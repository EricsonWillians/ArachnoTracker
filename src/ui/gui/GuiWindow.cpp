#include "GUI.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <condition_variable>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#ifndef ARACHNO_HAS_ALSA
#define ARACHNO_HAS_ALSA 0
#endif

#if ARACHNO_HAS_ALSA
#include <alsa/asoundlib.h>
#endif

#include "AppActions.h"
#include "PatchIO.h"
#include "GuiInput.h"

namespace arachno {

namespace {

AppActionRequest makeActionRequest(const std::string& actionId) {
    AppActionRequest request;
    request.actionId = actionId;
    return request;
}

struct TrackerWindowLayout {
    int margin = 12;
    int topPanelHeight = 170;
    int sidebarWidth = 300;
    int gridLeft = 12;
    int gridTop = 182;
    int gridWidth = 800;
    int gridHeight = 560;
    int sidebarLeft = 824;
    int sidebarHeight = 560;
    int rowNumberWidth = 56;
    int trackCols = 1;
    int trackWidth = 84;
    int rowHeight = 18;
    int visibleRows = 16;
    int verticalSplitterX = 820;
    int horizontalSplitterY = 180;
};

struct UiRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};

struct TrackHeaderHit {
    UiRect selectRect;
    UiRect muteRect;
    UiRect soloRect;
    int track = 0;
    std::string trackName;
    bool nameTruncated = false;
};

struct OrderSlotHit {
    UiRect rect;
    int pattern = -1;
    int startRow = 0;
    bool valid = false;
};

struct PianoKeyHit {
    UiRect rect;
    int midiNote = 60;
    bool black = false;
};

enum class InlinePromptKind {
    Inactive,
    OpenProjectPath,
    SaveProjectPath,
    ExportMixdownPath,
    ImportMidiPath,
    ImportPatchAsNewPath,
    ImportPatchReplacePath,
    ExportPatchPath,
    RenameInstrument,
    RenameTrack,
    SongLengthMinutes,
    PatternCreateSpec,
    PatternCloneName,
    TemporalPasteSpec
};

struct InlinePromptState {
    InlinePromptKind kind = InlinePromptKind::Inactive;
    bool active = false;
    std::string title;
    std::string hint;
    std::string value;
    int targetTrack = -1;
    int targetInstrument = -1;
};

struct FileBrowserEntry {
    std::string name;
    std::filesystem::path path;
    bool directory = false;
};

struct FileBrowserHit {
    UiRect rect;
    std::string role;
    int index = -1;
};

struct UnsavedDecisionPromptState {
    bool active = false;
    std::string title;
    std::string detail;
    AppActionRequest request;
};

struct TrackMetadataHit {
    UiRect rect;
    std::string role;
    int track = 0;
};

struct SongLengthHit {
    UiRect rect;
    std::string role;
};

struct MidiImportSettingHit {
    UiRect rect;
    std::string role;
};

struct SynthParamDef {
    std::string name;
    std::string label;
    double minimum = 0.0;
    double maximum = 1.0;
    double step = 0.01;
};

enum class AudioPerformanceMode {
    Auto,
    Live,
    Balanced,
    Heavy,
    Custom
};

struct SynthWindowHit {
    UiRect rect;
    std::string kind;
    std::string parameter;
    std::string oscillator;
    std::string wave;
    double delta = 0.0;
    double value = 0.0;
};

struct AudioTuningDialogHit {
    UiRect rect;
    std::string role;
    AudioPerformanceMode mode = AudioPerformanceMode::Auto;
    int delta = 0;
};

const char* audioPerformanceModeLabel(AudioPerformanceMode mode) {
    switch (mode) {
        case AudioPerformanceMode::Auto:
            return "AUTO";
        case AudioPerformanceMode::Live:
            return "LIVE";
        case AudioPerformanceMode::Balanced:
            return "BAL";
        case AudioPerformanceMode::Heavy:
            return "HEAVY";
        case AudioPerformanceMode::Custom:
            return "CUSTOM";
    }
    return "AUTO";
}

} // namespace

int runGuiWindow(
    ApplicationSession& session,
    std::ostream& output,
    const GuiShellOptions& options) {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        output << "GUI window unavailable: failed to open X11 display.\n";
        return 2;
    }

    const int screen = DefaultScreen(display);
    const Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        80,
        80,
        1280,
        800,
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen));
    XStoreName(display, window, "ArachnoTracker");
    XSelectInput(
        display,
        window,
        ExposureMask
            | KeyPressMask
            | StructureNotifyMask
            | ButtonPressMask
            | ButtonReleaseMask
            | PointerMotionMask);
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDelete, 1);
    XMapWindow(display, window);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, BlackPixel(display, screen));
    XSetBackground(display, gc, WhitePixel(display, screen));

    auto allocateColor = [&](const char* name, unsigned long fallback) -> unsigned long {
        Colormap cmap = DefaultColormap(display, screen);
        XColor exact;
        XColor chosen;
        if (XAllocNamedColor(display, cmap, name, &chosen, &exact) != 0) {
            return chosen.pixel;
        }
        return fallback;
    };

    struct UiThemePalette {
        unsigned long background = 0;
        unsigned long panel = 0;
        unsigned long gridHeader = 0;
        unsigned long gridLine = 0;
        unsigned long selection = 0;
        unsigned long cursor = 0;
        unsigned long text = 0;
        unsigned long mutedText = 0;
        unsigned long playhead = 0;
        unsigned long button = 0;
        unsigned long buttonActive = 0;
        unsigned long buttonLabel = 0;
        unsigned long buttonLabelActive = 0;
        unsigned long selectionText = 0;
        unsigned long cursorText = 0;
        unsigned long playheadText = 0;
        unsigned long activeTagText = 0;
        unsigned long pianoWhite = 0;
        unsigned long pianoBlack = 0;
    };
    enum class GuiThemeMode {
        Dos,
        HighContrast
    };

    const UiThemePalette dosTheme {
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#001408", BlackPixel(display, screen)),
        allocateColor("#008a00", WhitePixel(display, screen)),
        allocateColor("#04270f", BlackPixel(display, screen)),
        allocateColor("#00c94f", WhitePixel(display, screen)),
        allocateColor("#b8ffb8", WhitePixel(display, screen)),
        allocateColor("#66d680", WhitePixel(display, screen)),
        allocateColor("#083b18", WhitePixel(display, screen)),
        allocateColor("#001400", BlackPixel(display, screen)),
        allocateColor("#00a844", WhitePixel(display, screen)),
        allocateColor("#b8ffb8", WhitePixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#b8ffb8", WhitePixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#d4ffd4", WhitePixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#103810", BlackPixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen))
    };
    const UiThemePalette highContrastTheme {
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#111111", BlackPixel(display, screen)),
        allocateColor("#ffffff", WhitePixel(display, screen)),
        allocateColor("#222222", BlackPixel(display, screen)),
        allocateColor("#005fcc", WhitePixel(display, screen)),
        allocateColor("#ffffff", WhitePixel(display, screen)),
        allocateColor("#e6e6e6", WhitePixel(display, screen)),
        allocateColor("#0038a8", WhitePixel(display, screen)),
        allocateColor("#1a1a1a", BlackPixel(display, screen)),
        allocateColor("#00a6ff", WhitePixel(display, screen)),
        allocateColor("#ffffff", WhitePixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#ffffff", WhitePixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#ffffff", WhitePixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen)),
        allocateColor("#ffffff", WhitePixel(display, screen)),
        allocateColor("#000000", BlackPixel(display, screen))
    };
    GuiThemeMode themeMode = GuiThemeMode::Dos;

    XFontStruct* uiFont = XLoadQueryFont(display, "fixed");
    if (uiFont == nullptr) {
        uiFont = XLoadQueryFont(display, "9x15bold");
    }
    if (uiFont != nullptr) {
        XSetFont(display, gc, uiFont->fid);
    }

    AppActionResult lastAction;
    AppActionResult snapshotResult;
    int windowWidth = 1280;
    int windowHeight = 800;
    Pixmap trackerBackbuffer = 0;
    int trackerBackbufferWidth = 0;
    int trackerBackbufferHeight = 0;
    int armedInstrument = 0;
    int armedOctave = 4;
    float defaultVelocity = 0.80f;
    bool stepAdvance = true;
    bool followPlayback = true;
    int viewStartRow = 0;
    int requestedRowCount = std::max(1, options.snapshotGridRowCount);
    int topPanelHeightState = 210;
    int sidebarWidthState = 300;
    int activePatternRows = 64;
    TrackerWindowLayout layout;
    bool draggingSelection = false;
    bool resizingSidebar = false;
    bool resizingTopPanel = false;
    bool draggingPatternRows = false;
    int dragAnchorRow = 0;
    int dragAnchorTrack = 0;
    int patternResizeAnchorY = 0;
    int patternResizeStartRows = 64;
    auto lastPlaybackTick = std::chrono::steady_clock::now();

    std::vector<std::pair<UiRect, std::string>> transportButtons;
    std::vector<std::pair<UiRect, int>> instrumentHitTargets;
    std::vector<std::pair<UiRect, std::string>> instrumentControlHits;
    std::vector<std::pair<UiRect, int>> instrumentBrowserHitTargets;
    std::vector<TrackHeaderHit> trackHeaderHits;
    std::vector<OrderSlotHit> orderSlotHits;
    std::vector<std::pair<UiRect, std::string>> fileButtons;
    std::vector<std::pair<UiRect, GuiThemeMode>> themeButtons;
    std::vector<std::pair<UiRect, AudioPerformanceMode>> audioPerformanceButtons;
    std::vector<AudioTuningDialogHit> audioTuningDialogHits;
    std::vector<std::pair<UiRect, int>> octaveHitTargets;
    std::vector<PianoKeyHit> pianoKeyHits;
    std::vector<TrackMetadataHit> trackMetadataHits;
    std::vector<SongLengthHit> songLengthHits;
    std::vector<MidiImportSettingHit> midiImportSettingHits;
    std::vector<SynthWindowHit> synthWindowHits;
    UiRect patternRowsMinus;
    UiRect patternRowsPlus;
    UiRect patternRowsValue;
    UiRect gridTrackPrevButton;
    UiRect gridTrackNextButton;
    UiRect patternPrevButton;
    UiRect patternNextButton;
    UiRect patternValueButton;
    UiRect patternNewButton;
    UiRect patternCloneButton;
    UiRect patternDeleteButton;
    UiRect stepAdvanceButton;
    UiRect followPlaybackButton;
    UiRect instrumentListRect;
    UiRect instrumentBrowserListRect;
    UiRect instrumentBrowserAcceptButton;
    UiRect instrumentBrowserCancelButton;
    UiRect sidebarViewport;
    bool paintingNotes = false;
    int paintNoteMidi = 60;
    int lastPaintRow = -1;
    int lastPaintTrack = -1;
    bool keyboardSelectionActive = false;
    int keyboardSelectionAnchorRow = 0;
    int keyboardSelectionAnchorTrack = 0;
    auto manualScrollLockUntil = std::chrono::steady_clock::time_point {};
#if ARACHNO_HAS_ALSA
    snd_pcm_t* audioPcm = nullptr;
    bool audioPcmUsingFloat = false;
    std::thread audioAlsaWriterThread;
    std::mutex audioAlsaMutex;
    std::condition_variable audioAlsaCv;
    std::vector<float> audioAlsaQueue;
    std::size_t audioAlsaQueueCapacity = 0;
    std::size_t audioAlsaQueueRead = 0;
    std::size_t audioAlsaQueueSize = 0;
    bool audioAlsaThreadRunning = false;
    bool audioAlsaThreadStop = false;
    bool audioAlsaHealthy = true;
    std::size_t audioAlsaStartThresholdSamples = 0;
    std::size_t audioAlsaChunkSamples = 0;
#endif
    FILE* audioPipe = nullptr;
    bool audioOutputUsesAlsa = false;
    bool audioPipeUsingFloat = false;
    bool previousAudioStreamActive = false;
    std::vector<float> audioLeft;
    std::vector<float> audioRight;
    std::vector<float> audioInterleavedFloat;
    std::vector<short> audioInterleavedS16;
    std::vector<char> audioPipeBuffer;
    int audioFrameMin = 64;
    int audioFrameMax = 1024;
    int audioLoadClass = 0;
    AudioPerformanceMode audioPerformanceMode = AudioPerformanceMode::Auto;
    int audioCustomLevel = 0;
    auto lastAudioTuning = std::chrono::steady_clock::time_point {};
    bool audioTuningDialogActive = false;
    int instrumentListStart = 0;
    int instrumentListVisibleRows = 8;
    int sidebarScrollOffset = 0;
    int sidebarContentHeight = 0;
    int gridTrackStart = 0;
    int lastCursorTrackForGridFollow = -1;
    int pointerX = 0;
    int pointerY = 0;
    int hoveredTrackHeader = -1;
    bool instrumentBrowserActive = false;
    std::string instrumentBrowserQuery;
    int instrumentBrowserSelected = 0;
    int instrumentBrowserScroll = 0;
    InlinePromptState inlinePrompt;
    UiRect inlinePromptAcceptButton;
    UiRect inlinePromptCancelButton;
    bool inlinePromptButtonsVisible = false;
    std::filesystem::path fileBrowserDirectory;
    std::vector<FileBrowserEntry> fileBrowserEntries;
    std::vector<FileBrowserHit> fileBrowserHits;
    UiRect fileBrowserListRect;
    UiRect audioTuningDialogRect;
    int fileBrowserScroll = 0;
    int fileBrowserSelected = -1;
    UiRect synthInlinePromptAcceptButton;
    UiRect synthInlinePromptCancelButton;
    bool synthInlinePromptButtonsVisible = false;
    UnsavedDecisionPromptState unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>> unsavedPromptChoices;
    bool hasDeferredPostSaveAction = false;
    AppActionRequest deferredPostSaveAction;
    double targetSongLengthMinutes = 3.5;
    int midiImportRowsPerBeat = 4;
    int midiImportPatternRows = 64;
    bool midiImportSplitByTrack = true;
    bool synthWindowVisible = false;
    Window synthWindow = 0;
    GC synthGc = nullptr;
    Pixmap synthBackbuffer = 0;
    int synthBackbufferWidth = 0;
    int synthBackbufferHeight = 0;
    int synthWindowWidth = 980;
    int synthWindowHeight = 900;
    const int synthWindowMinWidth = 900;
    const int synthWindowMinHeight = 760;
    bool synthWindowNeedsRedraw = false;
    int synthPreviewMidi = 60;
    std::vector<PianoKeyHit> synthKeyboardHits;
    bool synthPointerDown = false;
    int synthLastPointerMidi = -1;
    int synthKeyboardBaseOctave = 2;
    int synthKeyboardVisibleOctaves = 4;
    int synthParamPage = 0;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> initFileBrowserFromPrompt;
    std::function<bool(const std::string&)> executeTemporalPasteSpec;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;

    auto activeSnapshot = [&]() -> AppSessionSnapshot {
        return snapshotResult.hasSessionSnapshot
            ? snapshotResult.sessionSnapshot
            : session.snapshot(std::max(0, viewStartRow), std::max(1, requestedRowCount));
    };

    auto ensureVisible = [&](int row) {
        if (row < viewStartRow) {
            viewStartRow = std::max(0, row);
        } else if (row >= viewStartRow + requestedRowCount) {
            viewStartRow = std::max(0, row - requestedRowCount + 1);
        }
    };

    auto lockManualScroll = [&](int milliseconds = 1600) {
        manualScrollLockUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    };

    auto releaseTrackerBackbuffer = [&]() {
        if (trackerBackbuffer != 0) {
            XFreePixmap(display, trackerBackbuffer);
            trackerBackbuffer = 0;
        }
        trackerBackbufferWidth = 0;
        trackerBackbufferHeight = 0;
    };

    auto ensureTrackerBackbuffer = [&]() {
        const int targetWidth = std::max(1, windowWidth);
        const int targetHeight = std::max(1, windowHeight);
        if (trackerBackbuffer != 0
            && trackerBackbufferWidth == targetWidth
            && trackerBackbufferHeight == targetHeight) {
            return;
        }
        releaseTrackerBackbuffer();
        trackerBackbuffer = XCreatePixmap(
            display,
            window,
            static_cast<unsigned int>(targetWidth),
            static_cast<unsigned int>(targetHeight),
            static_cast<unsigned int>(DefaultDepth(display, screen)));
        if (trackerBackbuffer != 0) {
            trackerBackbufferWidth = targetWidth;
            trackerBackbufferHeight = targetHeight;
        }
    };

#if ARACHNO_HAS_ALSA
    auto resetAlsaQueue = [&](int sampleRate) {
        audioAlsaQueueRead = 0;
        audioAlsaQueueSize = 0;
        const int queueFrames = std::max(2048, sampleRate / 3);
        audioAlsaQueueCapacity = static_cast<std::size_t>(queueFrames) * 2;
        audioAlsaQueue.assign(audioAlsaQueueCapacity, 0.0f);
        audioAlsaStartThresholdSamples = static_cast<std::size_t>(std::clamp(sampleRate / 150, 96, 768)) * 2;
        audioAlsaChunkSamples = static_cast<std::size_t>(std::clamp(sampleRate / 300, 64, 512)) * 2;
    };

    auto tuneAlsaQueueForLoad = [&](int sampleRate, int loadClass) {
        if (!audioAlsaThreadRunning || audioAlsaQueueCapacity == 0) {
            return;
        }
        const int startFrames = loadClass <= 0
            ? 192
            : (loadClass == 1 ? 512 : 1280);
        const int chunkFrames = loadClass <= 0
            ? 128
            : (loadClass == 1 ? 256 : 768);
        const std::size_t desiredStartSamples = static_cast<std::size_t>(std::clamp(startFrames, 64, 2048)) * 2;
        const std::size_t desiredChunkSamples = static_cast<std::size_t>(std::clamp(chunkFrames, 32, 1024)) * 2;
        std::lock_guard<std::mutex> lock(audioAlsaMutex);
        audioAlsaStartThresholdSamples = std::min(desiredStartSamples, audioAlsaQueueCapacity);
        audioAlsaChunkSamples = std::min(desiredChunkSamples, audioAlsaQueueCapacity);
        if (audioAlsaChunkSamples < 2) {
            audioAlsaChunkSamples = 2;
        }
        if (audioAlsaStartThresholdSamples < 2) {
            audioAlsaStartThresholdSamples = 2;
        }
        (void)sampleRate;
    };

    auto enqueueAlsaFrames = [&](const float* left, const float* right, int frames) -> bool {
        if (!audioOutputUsesAlsa || !audioAlsaThreadRunning || !audioAlsaHealthy || frames <= 0) {
            return false;
        }
        const std::size_t required = static_cast<std::size_t>(frames) * 2;
        if (required == 0 || required > audioAlsaQueueCapacity) {
            return false;
        }

        std::unique_lock<std::mutex> lock(audioAlsaMutex);
        if (audioAlsaThreadStop || !audioAlsaHealthy) {
            return false;
        }
        if (audioAlsaQueueSize + required > audioAlsaQueueCapacity) {
            const std::size_t overflow = (audioAlsaQueueSize + required) - audioAlsaQueueCapacity;
            audioAlsaQueueRead = (audioAlsaQueueRead + overflow) % audioAlsaQueueCapacity;
            audioAlsaQueueSize -= overflow;
        }
        std::size_t writeIndex = (audioAlsaQueueRead + audioAlsaQueueSize) % audioAlsaQueueCapacity;
        for (int index = 0; index < frames; ++index) {
            audioAlsaQueue[writeIndex] = left[static_cast<std::size_t>(index)];
            writeIndex = (writeIndex + 1) % audioAlsaQueueCapacity;
            audioAlsaQueue[writeIndex] = right[static_cast<std::size_t>(index)];
            writeIndex = (writeIndex + 1) % audioAlsaQueueCapacity;
        }
        audioAlsaQueueSize += required;
        lock.unlock();
        audioAlsaCv.notify_one();
        return true;
    };

    auto startAlsaWriterThread = [&](int sampleRate) {
        if (audioPcm == nullptr) {
            return false;
        }
        resetAlsaQueue(sampleRate);
        audioAlsaThreadStop = false;
        audioAlsaHealthy = true;
        audioAlsaThreadRunning = true;
        audioAlsaWriterThread = std::thread([&]() {
            std::vector<float> chunkFloat;
            std::vector<short> chunkS16;
            constexpr std::size_t maxChunkSamples = 4096;
            chunkFloat.reserve(maxChunkSamples);
            chunkS16.reserve(maxChunkSamples);
            bool primed = false;

            auto writeFramesToPcm = [&](const void* data, int frames) -> bool {
                int offset = 0;
                while (offset < frames) {
                    const snd_pcm_sframes_t written = snd_pcm_writei(
                        audioPcm,
                        audioPcmUsingFloat
                            ? static_cast<const void*>(
                                  static_cast<const float*>(data) + static_cast<std::size_t>(offset) * 2)
                            : static_cast<const void*>(
                                  static_cast<const short*>(data) + static_cast<std::size_t>(offset) * 2),
                        static_cast<snd_pcm_uframes_t>(frames - offset));
                    if (written > 0) {
                        offset += static_cast<int>(written);
                        continue;
                    }
                    if (written == -EAGAIN) {
                        (void)snd_pcm_wait(audioPcm, 5);
                        continue;
                    }
                    if (written == -EPIPE || written == -ESTRPIPE) {
                        if (snd_pcm_prepare(audioPcm) < 0) {
                            return false;
                        }
                        continue;
                    }
                    return false;
                }
                return true;
            };

            while (true) {
                {
                    std::unique_lock<std::mutex> lock(audioAlsaMutex);
                    const std::size_t startThreshold = std::max<std::size_t>(2, audioAlsaStartThresholdSamples);
                    audioAlsaCv.wait(lock, [&]() {
                        return audioAlsaThreadStop
                            || audioAlsaQueueSize >= (primed ? 2 : startThreshold);
                    });
                    if (audioAlsaThreadStop && audioAlsaQueueSize < 2) {
                        break;
                    }
                    primed = true;
                    const std::size_t requestedChunk = std::max<std::size_t>(2, audioAlsaChunkSamples);
                    const std::size_t toCopy = std::min(
                        maxChunkSamples,
                        std::max<std::size_t>(2, std::min(requestedChunk, audioAlsaQueueSize - (audioAlsaQueueSize % 2))));
                    chunkFloat.resize(toCopy);
                    for (std::size_t sample = 0; sample < toCopy; ++sample) {
                        chunkFloat[sample] = audioAlsaQueue[audioAlsaQueueRead];
                        audioAlsaQueueRead = (audioAlsaQueueRead + 1) % audioAlsaQueueCapacity;
                    }
                    audioAlsaQueueSize -= toCopy;
                    if (audioAlsaQueueSize < (startThreshold / 3)) {
                        primed = false;
                    }
                }

                const int frames = static_cast<int>(chunkFloat.size() / 2);
                if (frames <= 0) {
                    continue;
                }
                if (audioPcmUsingFloat) {
                    if (!writeFramesToPcm(chunkFloat.data(), frames)) {
                        std::lock_guard<std::mutex> lock(audioAlsaMutex);
                        audioAlsaHealthy = false;
                        break;
                    }
                } else {
                    chunkS16.resize(chunkFloat.size());
                    for (std::size_t index = 0; index < chunkFloat.size(); ++index) {
                        const float clamped = std::clamp(chunkFloat[index], -1.0f, 1.0f);
                        chunkS16[index] = static_cast<short>(std::lround(clamped * 32767.0f));
                    }
                    if (!writeFramesToPcm(chunkS16.data(), frames)) {
                        std::lock_guard<std::mutex> lock(audioAlsaMutex);
                        audioAlsaHealthy = false;
                        break;
                    }
                }
            }
        });
        return true;
    };
#endif

    auto audibleTrackCount = [&]() {
        const Song& song = session.song();
        bool soloActive = false;
        for (const Track& track : song.tracks) {
            if (track.solo) {
                soloActive = true;
                break;
            }
        }
        int count = 0;
        for (const Track& track : song.tracks) {
            if (track.muted) {
                continue;
            }
            if (soloActive && !track.solo) {
                continue;
            }
            ++count;
        }
        return std::max(1, count);
    };

    auto tuneRealtimeAudioForLoad = [&](int sampleRate) {
        const int tracks = audibleTrackCount();
        int loadClass = 0;
        int frameMin = 96;
        int frameMax = 1024;
        if (audioPerformanceMode == AudioPerformanceMode::Auto) {
            if (tracks > 28) {
                loadClass = 2;
            } else if (tracks > 12) {
                loadClass = 1;
            }
            if (loadClass <= 0) {
                frameMin = 96;
                frameMax = 1024;
            } else if (loadClass == 1) {
                frameMin = 256;
                frameMax = 2048;
            } else {
                frameMin = 536;
                frameMax = 3216;
            }
        } else if (audioPerformanceMode == AudioPerformanceMode::Live) {
            loadClass = 0;
            frameMin = 64;
            frameMax = 512;
        } else if (audioPerformanceMode == AudioPerformanceMode::Balanced) {
            loadClass = 1;
            frameMin = 256;
            frameMax = 2048;
        } else if (audioPerformanceMode == AudioPerformanceMode::Heavy) {
            loadClass = 2;
            frameMin = 536;
            frameMax = 3216;
        } else {
            const int level = std::clamp(audioCustomLevel, -24, 4096);
            if (level < 0) {
                frameMin = std::max(16, 64 + (level * 2));
                frameMax = std::clamp(frameMin * 4, 96, 768);
                loadClass = 0;
            } else if (level <= 128) {
                frameMin = 64 + (level * 2);
                frameMax = std::clamp(frameMin * 6, 256, 4096);
                loadClass = level <= 48 ? 0 : 1;
            } else if (level <= 1024) {
                frameMin = 320 + ((level - 128) * 3);
                frameMax = std::clamp(frameMin * 6, 1024, 12288);
                loadClass = level <= 320 ? 1 : 2;
            } else {
                frameMin = 3008 + ((level - 1024) * 4);
                frameMax = std::clamp(frameMin * 6, 2048, 32768);
                loadClass = 2;
            }
        }
        if (loadClass == audioLoadClass
            && frameMin == audioFrameMin
            && frameMax == audioFrameMax
            && std::chrono::steady_clock::now() - lastAudioTuning < std::chrono::milliseconds(180)) {
            return;
        }
        audioLoadClass = loadClass;
        audioFrameMin = frameMin;
        audioFrameMax = frameMax;
        lastAudioTuning = std::chrono::steady_clock::now();
#if ARACHNO_HAS_ALSA
        if (audioOutputUsesAlsa && audioPcm != nullptr) {
            tuneAlsaQueueForLoad(sampleRate, audioLoadClass);
            audioAlsaCv.notify_one();
        }
#else
        (void)sampleRate;
#endif
    };

    auto setAudioPerformanceMode = [&](AudioPerformanceMode mode, int sampleRate) {
        if (audioPerformanceMode == mode) {
            if (mode == AudioPerformanceMode::Custom) {
                tuneRealtimeAudioForLoad(sampleRate);
            }
            return;
        }
        audioPerformanceMode = mode;
        audioLoadClass = -1;
        lastAudioTuning = std::chrono::steady_clock::time_point {};
        tuneRealtimeAudioForLoad(sampleRate);
    };

    auto adjustAudioCustomLevel = [&](int delta, int sampleRate) {
        if (delta == 0) {
            return;
        }
        const int previous = audioCustomLevel;
        audioCustomLevel = std::clamp(audioCustomLevel + delta, -24, 4096);
        if (audioCustomLevel == previous && audioPerformanceMode == AudioPerformanceMode::Custom) {
            return;
        }
        audioPerformanceMode = AudioPerformanceMode::Custom;
        audioLoadClass = -1;
        lastAudioTuning = std::chrono::steady_clock::time_point {};
        tuneRealtimeAudioForLoad(sampleRate);
    };

    auto closeAudioOutput = [&]() {
#if ARACHNO_HAS_ALSA
        {
            std::lock_guard<std::mutex> lock(audioAlsaMutex);
            audioAlsaThreadStop = true;
        }
        audioAlsaCv.notify_all();
        if (audioAlsaWriterThread.joinable()) {
            audioAlsaWriterThread.join();
        }
        audioAlsaThreadRunning = false;
        audioAlsaThreadStop = false;
        audioAlsaQueue.clear();
        audioAlsaQueueCapacity = 0;
        audioAlsaQueueRead = 0;
        audioAlsaQueueSize = 0;
        audioAlsaHealthy = true;
        if (audioPcm != nullptr) {
            snd_pcm_drop(audioPcm);
            snd_pcm_close(audioPcm);
            audioPcm = nullptr;
        }
#endif
        if (audioPipe != nullptr) {
            ::pclose(audioPipe);
            audioPipe = nullptr;
        }
        audioOutputUsesAlsa = false;
    };

    auto openAudioOutput = [&](int sampleRate) -> bool {
        closeAudioOutput();
        if (sampleRate <= 0) {
            return false;
        }
        std::string forcedOutput;
        if (const char* env = std::getenv("ARACHNO_AUDIO_OUTPUT"); env != nullptr) {
            forcedOutput = env;
            std::transform(forcedOutput.begin(), forcedOutput.end(), forcedOutput.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
        }
        const bool forceAplay = forcedOutput == "aplay";
        const bool forceAlsa = forcedOutput == "alsa";

#if ARACHNO_HAS_ALSA
        auto tryOpenAlsa = [&](bool useFloat) -> bool {
            snd_pcm_t* pcm = nullptr;
            const int openResult = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
            if (openResult < 0 || pcm == nullptr) {
                return false;
            }
            snd_pcm_nonblock(pcm, 0);
            const snd_pcm_format_t format = useFloat ? SND_PCM_FORMAT_FLOAT_LE : SND_PCM_FORMAT_S16_LE;
            constexpr unsigned int latencyUs = 30000;
            const int paramResult = snd_pcm_set_params(
                pcm,
                format,
                SND_PCM_ACCESS_RW_INTERLEAVED,
                2,
                static_cast<unsigned int>(sampleRate),
                1,
                latencyUs);
            if (paramResult < 0) {
                snd_pcm_close(pcm);
                return false;
            }
            if (snd_pcm_prepare(pcm) < 0) {
                snd_pcm_close(pcm);
                return false;
            }
            audioPcm = pcm;
            audioPcmUsingFloat = useFloat;
            audioOutputUsesAlsa = true;
            if (!startAlsaWriterThread(sampleRate)) {
                snd_pcm_close(pcm);
                audioPcm = nullptr;
                audioOutputUsesAlsa = false;
                return false;
            }
            return true;
        };
        auto tryAlsaPreferred = [&]() {
            // Prefer S16 for broad device/plugin compatibility; float is fallback.
            return tryOpenAlsa(false) || tryOpenAlsa(true);
        };
#endif

        auto configureLowLatencyPipe = [&]() {
            if (audioPipe != nullptr) {
                setvbuf(audioPipe, nullptr, _IONBF, 0);
                audioPipeBuffer.clear();
            }
        };

        constexpr int liveBufferMicros = 30000;
        constexpr int livePeriodMicros = 5000;

        auto tryAplayPreferred = [&]() {
            {
                std::ostringstream cmd;
                cmd
                    << "aplay -q -t raw -f FLOAT_LE -c 2 -r " << sampleRate
                    << " -B " << liveBufferMicros
                    << " -F " << livePeriodMicros;
                audioPipe = ::popen(cmd.str().c_str(), "w");
                if (audioPipe != nullptr) {
                    configureLowLatencyPipe();
                    audioPipeUsingFloat = true;
                    return true;
                }
            }
            {
                std::ostringstream cmd;
                cmd
                    << "aplay -q -t raw -f S16_LE -c 2 -r " << sampleRate
                    << " -B " << liveBufferMicros
                    << " -F " << livePeriodMicros;
                audioPipe = ::popen(cmd.str().c_str(), "w");
                if (audioPipe != nullptr) {
                    configureLowLatencyPipe();
                    audioPipeUsingFloat = false;
                    return true;
                }
            }
            {
                std::ostringstream cmd;
                cmd << "aplay -q -t raw -f FLOAT_LE -c 2 -r " << sampleRate;
                audioPipe = ::popen(cmd.str().c_str(), "w");
                if (audioPipe != nullptr) {
                    configureLowLatencyPipe();
                    audioPipeUsingFloat = true;
                    return true;
                }
            }
            {
                std::ostringstream cmd;
                cmd << "aplay -q -t raw -f S16_LE -c 2 -r " << sampleRate;
                audioPipe = ::popen(cmd.str().c_str(), "w");
                if (audioPipe != nullptr) {
                    configureLowLatencyPipe();
                    audioPipeUsingFloat = false;
                    return true;
                }
            }
            return false;
        };

        if (forceAlsa) {
#if ARACHNO_HAS_ALSA
            return tryAlsaPreferred();
#else
            return false;
#endif
        }
        if (forceAplay) {
            return tryAplayPreferred();
        }

        // Auto mode: prefer the established aplay path first, ALSA direct second.
        if (tryAplayPreferred()) {
            return true;
        }
#if ARACHNO_HAS_ALSA
        if (tryAlsaPreferred()) {
            return true;
        }
#endif
        return false;
    };

    auto writeAudioOutput = [&](const float* left, const float* right, int frames) -> bool {
        if (frames <= 0) {
            return false;
        }
#if ARACHNO_HAS_ALSA
        if (audioOutputUsesAlsa && audioPcm != nullptr) {
            if (enqueueAlsaFrames(left, right, frames)) {
                return true;
            }
            closeAudioOutput();
            return false;
        }
#endif
        if (audioPipe == nullptr) {
            return false;
        }
        if (audioPipeUsingFloat) {
            const std::size_t sampleCount = static_cast<std::size_t>(frames) * 2;
            if (audioInterleavedFloat.size() < sampleCount) {
                audioInterleavedFloat.resize(sampleCount);
            }
            for (int index = 0; index < frames; ++index) {
                audioInterleavedFloat[static_cast<std::size_t>(index) * 2] = left[static_cast<std::size_t>(index)];
                audioInterleavedFloat[(static_cast<std::size_t>(index) * 2) + 1] = right[static_cast<std::size_t>(index)];
            }
            const std::size_t written = std::fwrite(
                audioInterleavedFloat.data(),
                sizeof(float),
                sampleCount,
                audioPipe);
            if (written != sampleCount) {
                closeAudioOutput();
                return false;
            }
        } else {
            const std::size_t sampleCount = static_cast<std::size_t>(frames) * 2;
            if (audioInterleavedS16.size() < sampleCount) {
                audioInterleavedS16.resize(sampleCount);
            }
            for (int index = 0; index < frames; ++index) {
                const float l = std::clamp(left[static_cast<std::size_t>(index)], -1.0f, 1.0f);
                const float r = std::clamp(right[static_cast<std::size_t>(index)], -1.0f, 1.0f);
                audioInterleavedS16[static_cast<std::size_t>(index) * 2] = static_cast<short>(std::lround(l * 32767.0f));
                audioInterleavedS16[(static_cast<std::size_t>(index) * 2) + 1] = static_cast<short>(std::lround(r * 32767.0f));
            }
            const std::size_t written = std::fwrite(
                audioInterleavedS16.data(),
                sizeof(short),
                sampleCount,
                audioPipe);
            if (written != sampleCount) {
                closeAudioOutput();
                return false;
            }
        }
        return true;
    };

    auto refreshSnapshot = [&]() {
        for (int attempt = 0; attempt < 2; ++attempt) {
            AppActionRequest snapshot;
            snapshot.actionId = "session.snapshot";
            snapshot.parameters = {
                {"grid_start_row", std::to_string(std::max(0, viewStartRow))},
                {"grid_row_count", std::to_string(std::max(1, requestedRowCount))}};
            snapshotResult = executeAppAction(session, snapshot);
            if (!snapshotResult.hasSessionSnapshot) {
                return;
            }

            const AppSessionSnapshot& snap = snapshotResult.sessionSnapshot;

            const bool autoFollowAllowed = followPlayback
                && std::chrono::steady_clock::now() >= manualScrollLockUntil;
            if (autoFollowAllowed
                && snap.playback.state == TransportState::Playing
                && !snap.playback.loop.enabled
                && snap.playback.position.pattern >= 0
                && snap.playback.position.pattern < static_cast<int>(snap.editor.patterns.size())
                && snap.playback.position.pattern != snap.editor.status.activePattern) {
                AppActionRequest followPattern;
                followPattern.actionId = "editor.navigation.pattern";
                followPattern.parameters = {{"index", std::to_string(snap.playback.position.pattern)}};
                const AppActionResult followResult = executeAppAction(session, followPattern);
                if (followResult.ok) {
                    continue;
                }
            }

            const int cursorTrack = std::max(0, snap.editor.status.cursorTrack);
            if (cursorTrack != lastCursorTrackForGridFollow) {
                if (cursorTrack < gridTrackStart) {
                    gridTrackStart = cursorTrack;
                } else if (layout.trackCols > 0 && cursorTrack >= gridTrackStart + layout.trackCols) {
                    gridTrackStart = cursorTrack - layout.trackCols + 1;
                }
                lastCursorTrackForGridFollow = cursorTrack;
            }

            for (const PatternSummary& pattern : snap.editor.patterns) {
                if (pattern.active) {
                    activePatternRows = std::max(8, pattern.rowCount);
                    break;
                }
            }
            if (autoFollowAllowed) {
                const int focusRow = (snap.playback.state == TransportState::Playing
                        && snap.playback.position.pattern == snap.editor.status.activePattern)
                    ? snap.playback.position.patternRow
                    : snap.editor.status.cursorRow;
                ensureVisible(std::max(0, focusRow));
            }
            const int maxStart = std::max(0, activePatternRows - 1);
            const int clamped = std::clamp(viewStartRow, 0, maxStart);
            if (clamped == viewStartRow || attempt == 1) {
                viewStartRow = clamped;
                return;
            }
            viewStartRow = clamped;
        }
    };

    auto runAction = [&](const AppActionRequest& request, bool refresh = true) {
        lastAction = executeAppAction(session, request);
        if (refresh) {
            refreshSnapshot();
        }
        return lastAction;
    };

    auto runSync = [&](const std::string& mode) {
        AppActionRequest sync;
        sync.actionId = "session.sync";
        sync.parameters = {
            {"checkpoint_name", options.checkpointName},
            {"create_if_missing", options.createCheckpointIfMissing ? "true" : "false"},
            {"mode", mode},
            {"max_events", std::to_string(options.maxEvents)},
            {"snapshot_grid_start_row", std::to_string(std::max(0, viewStartRow))},
            {"snapshot_grid_row_count", std::to_string(std::max(1, requestedRowCount))},
            {"update_checkpoint", "true"}};
        runAction(sync);
    };

    auto runEvents = [&]() {
        AppActionRequest events;
        events.actionId = "session.events";
        events.parameters = {{"max_events", std::to_string(options.maxEvents)}};
        runAction(events);
    };

    auto beginInlinePrompt = [&](
        InlinePromptKind kind,
        const std::string& title,
        const std::string& hint,
        const std::string& initialValue,
        int targetTrack = -1,
        int targetInstrument = -1) {
        inlinePrompt.kind = kind;
        inlinePrompt.active = true;
        inlinePrompt.title = title;
        inlinePrompt.hint = hint;
        inlinePrompt.value = initialValue;
        inlinePrompt.targetTrack = targetTrack;
        inlinePrompt.targetInstrument = targetInstrument;
        if (inlinePromptUsesFileBrowser(kind)) {
            initFileBrowserFromPrompt();
        } else {
            fileBrowserDirectory.clear();
            fileBrowserEntries.clear();
            fileBrowserHits.clear();
            fileBrowserScroll = 0;
            fileBrowserSelected = -1;
        }
    };

    auto clearInlinePrompt = [&]() {
        inlinePrompt = InlinePromptState {};
        inlinePromptButtonsVisible = false;
        synthInlinePromptButtonsVisible = false;
        fileBrowserDirectory.clear();
        fileBrowserEntries.clear();
        fileBrowserHits.clear();
        fileBrowserScroll = 0;
        fileBrowserSelected = -1;
    };

    auto isSynthInlinePromptKind = [&](InlinePromptKind kind) {
        return kind == InlinePromptKind::ImportPatchAsNewPath
            || kind == InlinePromptKind::ImportPatchReplacePath
            || kind == InlinePromptKind::ExportPatchPath
            || kind == InlinePromptKind::RenameInstrument;
    };

    inlinePromptUsesFileBrowser = [&](InlinePromptKind kind) {
        return kind == InlinePromptKind::OpenProjectPath
            || kind == InlinePromptKind::SaveProjectPath
            || kind == InlinePromptKind::ExportMixdownPath
            || kind == InlinePromptKind::ImportMidiPath
            || kind == InlinePromptKind::ImportPatchAsNewPath
            || kind == InlinePromptKind::ImportPatchReplacePath
            || kind == InlinePromptKind::ExportPatchPath;
    };

    auto fileBrowserExtensionFilter = [&](InlinePromptKind kind) {
        if (kind == InlinePromptKind::OpenProjectPath || kind == InlinePromptKind::SaveProjectPath) {
            return std::vector<std::string> {".arachno", ".mid", ".midi"};
        }
        if (kind == InlinePromptKind::ExportMixdownPath) {
            return std::vector<std::string> {".wav", ".mp3", ".ogg", ".mid", ".midi"};
        }
        if (kind == InlinePromptKind::ImportMidiPath) {
            return std::vector<std::string> {".mid", ".midi"};
        }
        if (kind == InlinePromptKind::ImportPatchAsNewPath
            || kind == InlinePromptKind::ImportPatchReplacePath
            || kind == InlinePromptKind::ExportPatchPath) {
            return std::vector<std::string> {".arachnopatch"};
        }
        return std::vector<std::string> {};
    };

    auto pathLower = [](const std::string& value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return lowered;
    };

    refreshFileBrowserEntries = [&]() {
        fileBrowserEntries.clear();
        fileBrowserHits.clear();
        fileBrowserSelected = -1;
        if (fileBrowserDirectory.empty()) {
            fileBrowserDirectory = std::filesystem::current_path();
        }
        std::error_code ec;
        const std::filesystem::path canonicalDir = std::filesystem::weakly_canonical(fileBrowserDirectory, ec);
        if (!ec && !canonicalDir.empty()) {
            fileBrowserDirectory = canonicalDir;
        }
        const std::vector<std::string> extensions = fileBrowserExtensionFilter(inlinePrompt.kind);
        std::vector<FileBrowserEntry> directories;
        std::vector<FileBrowserEntry> files;
        for (std::filesystem::directory_iterator it(fileBrowserDirectory, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            const std::filesystem::directory_entry& entry = *it;
            FileBrowserEntry item;
            item.path = entry.path();
            item.name = item.path.filename().string();
            item.directory = entry.is_directory(ec);
            if (ec || item.name.empty()) {
                ec.clear();
                continue;
            }
            if (item.directory) {
                directories.push_back(item);
            } else {
                if (!extensions.empty()) {
                    const std::string ext = pathLower(item.path.extension().string());
                    bool accepted = false;
                    for (const std::string& candidate : extensions) {
                        if (ext == candidate) {
                            accepted = true;
                            break;
                        }
                    }
                    if (!accepted) {
                        continue;
                    }
                }
                files.push_back(item);
            }
        }
        auto byName = [](const FileBrowserEntry& a, const FileBrowserEntry& b) {
            std::string left = a.name;
            std::string right = b.name;
            std::transform(left.begin(), left.end(), left.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            std::transform(right.begin(), right.end(), right.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return left < right;
        };
        std::sort(directories.begin(), directories.end(), byName);
        std::sort(files.begin(), files.end(), byName);
        fileBrowserEntries.reserve(directories.size() + files.size());
        fileBrowserEntries.insert(fileBrowserEntries.end(), directories.begin(), directories.end());
        fileBrowserEntries.insert(fileBrowserEntries.end(), files.begin(), files.end());
        fileBrowserScroll = std::clamp(fileBrowserScroll, 0, std::max(0, static_cast<int>(fileBrowserEntries.size()) - 1));
    };

    initFileBrowserFromPrompt = [&]() {
        fileBrowserScroll = 0;
        fileBrowserSelected = -1;
        std::filesystem::path seed;
        if (!inlinePrompt.value.empty()) {
            seed = std::filesystem::path(inlinePrompt.value);
        }
        std::error_code ec;
        if (seed.empty()) {
            fileBrowserDirectory = std::filesystem::current_path();
        } else if (std::filesystem::is_directory(seed, ec)) {
            fileBrowserDirectory = seed;
        } else {
            fileBrowserDirectory = seed.parent_path().empty() ? std::filesystem::current_path() : seed.parent_path();
        }
        if (!std::filesystem::exists(fileBrowserDirectory, ec) || !std::filesystem::is_directory(fileBrowserDirectory, ec)) {
            fileBrowserDirectory = std::filesystem::current_path();
        }
        refreshFileBrowserEntries();
        const std::string targetLeaf = seed.filename().string();
        if (!targetLeaf.empty()) {
            for (int index = 0; index < static_cast<int>(fileBrowserEntries.size()); ++index) {
                if (!fileBrowserEntries[static_cast<std::size_t>(index)].directory
                    && fileBrowserEntries[static_cast<std::size_t>(index)].name == targetLeaf) {
                    fileBrowserSelected = index;
                    break;
                }
            }
        }
    };

    auto cancelInlinePrompt = [&]() {
        if (inlinePrompt.active && inlinePrompt.kind == InlinePromptKind::SaveProjectPath) {
            hasDeferredPostSaveAction = false;
        }
        clearInlinePrompt();
    };

    auto clearUnsavedPrompt = [&]() {
        unsavedPrompt = UnsavedDecisionPromptState {};
        unsavedPromptChoices.clear();
    };

    auto runLifecycleAction = [&](const AppActionRequest& request) {
        const AppActionResult result = runAction(request);
        if (result.requiresUnsavedDecision) {
            unsavedPrompt.active = true;
            unsavedPrompt.request = request;
            unsavedPrompt.title = result.lifecyclePlan.unsavedPrompt.title.empty()
                ? "Unsaved changes"
                : result.lifecyclePlan.unsavedPrompt.title;
            unsavedPrompt.detail = result.lifecyclePlan.unsavedPrompt.detail.empty()
                ? result.lifecyclePlan.unsavedPrompt.message
                : result.lifecyclePlan.unsavedPrompt.detail;
            return;
        }
        if (result.requiresSaveAs) {
            hasDeferredPostSaveAction = true;
            deferredPostSaveAction = request;
            beginInlinePrompt(
                InlinePromptKind::SaveProjectPath,
                "Save project before continue",
                "Path to save current project",
                "gui_project.arachno");
        }
    };

    auto defaultProjectPath = [&](const AppSessionSnapshot& snap) {
        if (snap.hasProjectPath && !snap.projectPath.empty()) {
            return snap.projectPath;
        }
        return std::string("gui_project.arachno");
    };

    auto defaultMixdownPath = [&](const AppSessionSnapshot& snap) {
        if (snap.hasProjectPath && !snap.projectPath.empty()) {
            const std::filesystem::path path(snap.projectPath);
            const std::string stem = path.stem().string().empty() ? "mixdown" : path.stem().string();
            return (path.parent_path() / (stem + ".wav")).string();
        }
        return std::string("gui_mixdown.wav");
    };

    auto defaultMidiImportPath = [&](const AppSessionSnapshot& snap) {
        if (snap.hasProjectPath && !snap.projectPath.empty()) {
            const std::filesystem::path path(snap.projectPath);
            return (path.parent_path() / "import.mid").string();
        }
        return std::string("import.mid");
    };

    auto patchStemFromName = [&](const std::string& name) {
        std::string stem;
        stem.reserve(name.size());
        for (char ch : name) {
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (std::isalnum(uch)) {
                stem.push_back(static_cast<char>(std::tolower(uch)));
            } else if (ch == '_' || ch == '-' || ch == '.') {
                stem.push_back(ch);
            } else if (!stem.empty() && stem.back() != '_') {
                stem.push_back('_');
            }
        }
        if (stem.empty()) {
            stem = "patch";
        }
        return stem;
    };

    auto defaultPatchPath = [&](const AppSessionSnapshot& snap, const std::string& patchName) {
        const std::string stem = patchStemFromName(patchName);
        if (snap.hasProjectPath && !snap.projectPath.empty()) {
            const std::filesystem::path projectPath(snap.projectPath);
            return (projectPath.parent_path() / (stem + ".arachnopatch")).string();
        }
        return stem + ".arachnopatch";
    };

    std::function<void(int)> selectInstrument;
    std::function<bool(int, const SynthPatch&, bool)> applyPatchToInstrument;

    auto handleDeferredPostSave = [&]() {
        if (!hasDeferredPostSaveAction) {
            return;
        }
        AppActionRequest deferred = deferredPostSaveAction;
        hasDeferredPostSaveAction = false;
        deferred.unsavedChoice = UnsavedChangesChoice::NotNeeded;
        runLifecycleAction(deferred);
    };

    auto executeInlinePrompt = [&]() {
        if (!inlinePrompt.active) {
            return;
        }
        std::string value = trimCopy(inlinePrompt.value);
        auto sanitizePatternToken = [&](std::string token, const std::string& fallback) {
            token = trimCopy(token);
            std::string out;
            out.reserve(token.size());
            bool previousUnderscore = false;
            for (const unsigned char raw : token) {
                const char ch = static_cast<char>(raw);
                if (std::isspace(raw)) {
                    if (!out.empty() && !previousUnderscore) {
                        out.push_back('_');
                        previousUnderscore = true;
                    }
                    continue;
                }
                out.push_back(ch);
                previousUnderscore = ch == '_';
            }
            while (!out.empty() && out.back() == '_') {
                out.pop_back();
            }
            return out.empty() ? fallback : out;
        };
        if (inlinePromptUsesFileBrowser(inlinePrompt.kind) && !value.empty()) {
            std::error_code ec;
            const std::filesystem::path candidate(value);
            if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
                if (inlinePrompt.kind == InlinePromptKind::SaveProjectPath) {
                    value = (candidate / "project.arachno").string();
                } else if (inlinePrompt.kind == InlinePromptKind::ExportMixdownPath) {
                    value = (candidate / "mixdown.wav").string();
                } else if (inlinePrompt.kind == InlinePromptKind::OpenProjectPath
                    || inlinePrompt.kind == InlinePromptKind::ImportMidiPath
                    || inlinePrompt.kind == InlinePromptKind::ImportPatchAsNewPath
                    || inlinePrompt.kind == InlinePromptKind::ImportPatchReplacePath
                    || inlinePrompt.kind == InlinePromptKind::ExportPatchPath) {
                    lastAction.ok = false;
                    lastAction.error = "select a file, not a folder";
                    lastAction.actionId = "file.browser.select";
                    return;
                }
                inlinePrompt.value = value;
            }
        }
        if (inlinePrompt.kind == InlinePromptKind::OpenProjectPath) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "project.open";
                lastAction.error = "project path is empty";
                clearInlinePrompt();
                return;
            }
            AppActionRequest request;
            const std::string ext = pathLower(std::filesystem::path(value).extension().string());
            if (ext == ".mid" || ext == ".midi") {
                request.actionId = "import.midi";
                request.path = value;
                request.parameters = {
                    {"path", value},
                    {"rows_per_beat", std::to_string(std::clamp(midiImportRowsPerBeat, 1, 32))},
                    {"pattern_rows", std::to_string(std::clamp(midiImportPatternRows, 16, 8192))},
                    {"split_by_track", midiImportSplitByTrack ? "true" : "false"},
                    {"split_by_program", "true"},
                    {"preserve_tempo_map", "true"}};
            } else {
                request.actionId = "project.open";
                request.path = value;
                request.parameters = {{"path", value}};
            }
            clearInlinePrompt();
            runLifecycleAction(request);
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::SaveProjectPath) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "project.save_as";
                lastAction.error = "save path is empty";
                clearInlinePrompt();
                return;
            }
            AppActionRequest saveAs;
            saveAs.actionId = "project.save_as";
            saveAs.path = value;
            saveAs.parameters = {{"path", value}};
            clearInlinePrompt();
            runAction(saveAs);
            if (lastAction.ok) {
                handleDeferredPostSave();
            }
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::ExportMixdownPath) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "export.mixdown";
                lastAction.error = "export path is empty";
                clearInlinePrompt();
                return;
            }
            AppActionRequest exportMix;
            const std::string ext = pathLower(std::filesystem::path(value).extension().string());
            exportMix.actionId = (ext == ".mid" || ext == ".midi") ? "export.midi" : "export.mixdown";
            exportMix.path = value;
            exportMix.parameters = {{"path", value}};
            clearInlinePrompt();
            runAction(exportMix);
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::ImportMidiPath) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "import.midi";
                lastAction.error = "MIDI path is empty";
                clearInlinePrompt();
                return;
            }
            AppActionRequest importMidi;
            importMidi.actionId = "import.midi";
            importMidi.path = value;
            importMidi.parameters = {
                {"path", value},
                {"rows_per_beat", std::to_string(std::clamp(midiImportRowsPerBeat, 1, 32))},
                {"pattern_rows", std::to_string(std::clamp(midiImportPatternRows, 16, 8192))},
                {"split_by_track", midiImportSplitByTrack ? "true" : "false"},
                {"split_by_program", "true"},
                {"preserve_tempo_map", "true"}};
            clearInlinePrompt();
            runAction(importMidi);
            if (lastAction.ok
                && lastAction.hasMidiImportReport
                && !lastAction.midiImportReport.trackMappings.empty()) {
                selectInstrument(lastAction.midiImportReport.trackMappings.front().instrumentIndex);
            }
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::ImportPatchAsNewPath) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "script.import";
                lastAction.error = "patch path is empty";
                clearInlinePrompt();
                return;
            }
            AppActionRequest importPatch;
            importPatch.actionId = "script.import";
            importPatch.path = value;
            importPatch.parameters = {{"path", value}};
            clearInlinePrompt();
            runAction(importPatch);
            if (lastAction.ok && lastAction.hasScriptImportResult && lastAction.scriptImportResult.importedInstrument >= 0) {
                selectInstrument(lastAction.scriptImportResult.importedInstrument);
            }
            synthWindowNeedsRedraw = true;
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::ImportPatchReplacePath) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "editor.instrument.patch_replace";
                lastAction.error = "patch path is empty";
                clearInlinePrompt();
                return;
            }
            const int instrument = inlinePrompt.targetInstrument;
            clearInlinePrompt();
            try {
                const SynthPatch patch = loadPatch(value);
                const bool applied = applyPatchToInstrument(instrument, patch, false);
                if (applied) {
                    lastAction.ok = true;
                    lastAction.actionId = "editor.instrument.patch_replace";
                    lastAction.message = "Loaded patch into instrument";
                    lastAction.error.clear();
                } else if (lastAction.ok) {
                    lastAction.ok = false;
                    lastAction.actionId = "editor.instrument.patch_replace";
                    lastAction.error = "failed to apply patch";
                }
            } catch (const std::exception& error) {
                lastAction.ok = false;
                lastAction.actionId = "editor.instrument.patch_replace";
                lastAction.error = error.what();
            }
            synthWindowNeedsRedraw = true;
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::ExportPatchPath) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "editor.instrument.patch_export";
                lastAction.error = "export path is empty";
                clearInlinePrompt();
                return;
            }
            const int instrument = inlinePrompt.targetInstrument;
            clearInlinePrompt();
            try {
                const int count = static_cast<int>(session.song().instruments.size());
                if (instrument < 0 || instrument >= count) {
                    throw std::out_of_range("instrument index is out of range");
                }
                savePatch(session.song().instruments[static_cast<std::size_t>(instrument)].patch, value);
                lastAction.ok = true;
                lastAction.actionId = "editor.instrument.patch_export";
                lastAction.message = "Exported patch";
                lastAction.error.clear();
            } catch (const std::exception& error) {
                lastAction.ok = false;
                lastAction.actionId = "editor.instrument.patch_export";
                lastAction.error = error.what();
            }
            synthWindowNeedsRedraw = true;
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::RenameInstrument) {
            const int instrument = inlinePrompt.targetInstrument;
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "editor.instrument.rename";
                lastAction.error = "instrument name cannot be empty";
                clearInlinePrompt();
                return;
            }
            AppActionRequest rename;
            rename.actionId = "editor.instrument.rename";
            rename.parameters = {
                {"instrument", std::to_string(instrument)},
                {"name", value}};
            clearInlinePrompt();
            runAction(rename);
            synthWindowNeedsRedraw = true;
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::RenameTrack) {
            const AppSessionSnapshot snap = activeSnapshot();
            const int track = std::clamp(
                inlinePrompt.targetTrack,
                0,
                std::max(0, static_cast<int>(snap.editor.tracks.size()) - 1));
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "editor.track.rename";
                lastAction.error = "track name cannot be empty";
                clearInlinePrompt();
                return;
            }
            AppActionRequest rename;
            rename.actionId = "editor.track.rename";
            rename.parameters = {{"track", std::to_string(track)}, {"name", value}};
            clearInlinePrompt();
            runAction(rename);
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::SongLengthMinutes) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "arrangement.length_target";
                lastAction.error = "target minutes cannot be empty";
                clearInlinePrompt();
                return;
            }
            std::istringstream in(value);
            double minutes = 0.0;
            in >> minutes;
            if (!in || minutes <= 0.0) {
                lastAction.ok = false;
                lastAction.actionId = "arrangement.length_target";
                lastAction.error = "target minutes must be a positive number";
                clearInlinePrompt();
                return;
            }
            targetSongLengthMinutes = std::clamp(minutes, 0.1, 180.0);
            lastAction.ok = true;
            lastAction.actionId = "arrangement.length_target";
            lastAction.message = "target length updated";
            lastAction.error.clear();
            clearInlinePrompt();
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::PatternCreateSpec) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "editor.pattern.new";
                lastAction.error = "pattern spec is empty";
                clearInlinePrompt();
                return;
            }
            std::istringstream in(value);
            std::string rawName;
            int rows = std::clamp(activePatternRows, 8, 8192);
            int tracks = -1;
            if (!(in >> rawName)) {
                lastAction.ok = false;
                lastAction.actionId = "editor.pattern.new";
                lastAction.error = "pattern name is required";
                clearInlinePrompt();
                return;
            }
            if (in >> rows) {
                rows = std::clamp(rows, 8, 8192);
            } else {
                in.clear();
            }
            if (in >> tracks) {
                if (tracks <= 0) {
                    lastAction.ok = false;
                    lastAction.actionId = "editor.pattern.new";
                    lastAction.error = "tracks must be greater than zero";
                    clearInlinePrompt();
                    return;
                }
            }
            AppActionRequest create;
            create.actionId = "editor.pattern.new";
            create.parameters = {
                {"name", sanitizePatternToken(rawName, "Pattern")},
                {"rows", std::to_string(rows)}};
            if (tracks > 0) {
                create.parameters.emplace("tracks", std::to_string(tracks));
            }
            clearInlinePrompt();
            runAction(create);
            if (lastAction.ok) {
                keyboardSelectionActive = false;
                viewStartRow = 0;
            }
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::PatternCloneName) {
            AppActionRequest clone;
            clone.actionId = "editor.pattern.clone";
            const std::string name = sanitizePatternToken(value, "");
            if (!name.empty()) {
                clone.parameters = {{"name", name}};
            }
            clearInlinePrompt();
            runAction(clone);
            if (lastAction.ok) {
                keyboardSelectionActive = false;
                viewStartRow = 0;
            }
            return;
        }
        if (inlinePrompt.kind == InlinePromptKind::TemporalPasteSpec) {
            if (value.empty()) {
                lastAction.ok = false;
                lastAction.actionId = "editor.selection.paste_temporal";
                lastAction.error = "temporal paste spec is empty";
                clearInlinePrompt();
                return;
            }
            clearInlinePrompt();
            (void)executeTemporalPasteSpec(value);
            return;
        }
    };

    auto resolveUnsavedPrompt = [&](UnsavedChangesChoice choice) {
        if (!unsavedPrompt.active) {
            return;
        }
        AppActionRequest request = unsavedPrompt.request;
        clearUnsavedPrompt();
        if (choice == UnsavedChangesChoice::Cancel) {
            lastAction.ok = true;
            lastAction.actionId = request.actionId;
            lastAction.message = "action canceled";
            lastAction.error.clear();
            return;
        }
        if (choice == UnsavedChangesChoice::Save) {
            const AppSessionSnapshot snap = activeSnapshot();
            if (!snap.hasProjectPath || snap.projectPath.empty()) {
                hasDeferredPostSaveAction = true;
                deferredPostSaveAction = request;
                beginInlinePrompt(
                    InlinePromptKind::SaveProjectPath,
                    "Save project before continue",
                    "Path to save current project",
                    defaultProjectPath(snap));
                return;
            }
        }
        request.unsavedChoice = choice;
        runLifecycleAction(request);
    };

    auto ensureSynthWindow = [&]() -> bool {
        if (synthWindow != 0 && synthGc != nullptr) {
            return true;
        }
        if (synthWindow == 0) {
            synthWindow = XCreateSimpleWindow(
                display,
                RootWindow(display, screen),
                140,
                120,
                static_cast<unsigned int>(synthWindowWidth),
                static_cast<unsigned int>(synthWindowHeight),
                1,
                BlackPixel(display, screen),
                WhitePixel(display, screen));
            XStoreName(display, synthWindow, "ArachnoTracker Synth Designer");
            XSelectInput(
                display,
                synthWindow,
                ExposureMask
                    | KeyPressMask
                    | StructureNotifyMask
                    | ButtonPressMask
                    | ButtonReleaseMask
                    | PointerMotionMask);
            XSetWMProtocols(display, synthWindow, &wmDelete, 1);
        }
        if (synthGc == nullptr) {
            synthGc = XCreateGC(display, synthWindow, 0, nullptr);
            if (synthGc != nullptr) {
                XSetForeground(display, synthGc, BlackPixel(display, screen));
                XSetBackground(display, synthGc, WhitePixel(display, screen));
                if (uiFont != nullptr) {
                    XSetFont(display, synthGc, uiFont->fid);
                }
            }
        }
        return synthWindow != 0 && synthGc != nullptr;
    };

    auto releaseSynthBackbuffer = [&]() {
        if (synthBackbuffer != 0) {
            XFreePixmap(display, synthBackbuffer);
            synthBackbuffer = 0;
        }
        synthBackbufferWidth = 0;
        synthBackbufferHeight = 0;
    };

    auto ensureSynthBackbuffer = [&]() {
        if (synthWindow == 0) {
            releaseSynthBackbuffer();
            return;
        }
        const int targetWidth = std::max(1, synthWindowWidth);
        const int targetHeight = std::max(1, synthWindowHeight);
        if (synthBackbuffer != 0
            && synthBackbufferWidth == targetWidth
            && synthBackbufferHeight == targetHeight) {
            return;
        }
        releaseSynthBackbuffer();
        synthBackbuffer = XCreatePixmap(
            display,
            synthWindow,
            static_cast<unsigned int>(targetWidth),
            static_cast<unsigned int>(targetHeight),
            static_cast<unsigned int>(DefaultDepth(display, screen)));
        if (synthBackbuffer != 0) {
            synthBackbufferWidth = targetWidth;
            synthBackbufferHeight = targetHeight;
        }
    };

    auto setSynthWindowVisible = [&](bool visible) {
        if (visible) {
            if (!ensureSynthWindow()) {
                lastAction.ok = false;
                lastAction.actionId = "synth.window.open";
                lastAction.error = "failed to open synth designer window";
                return;
            }
            ensureSynthBackbuffer();
            XMapRaised(display, synthWindow);
            synthWindowVisible = true;
            synthKeyboardBaseOctave = std::clamp(armedOctave - 1, 0, std::max(0, 10 - synthKeyboardVisibleOctaves));
            synthPreviewMidi = std::clamp((armedOctave * 12) + (synthPreviewMidi % 12), 0, 127);
            paintNoteMidi = synthPreviewMidi;
            synthWindowNeedsRedraw = true;
        } else {
            if (synthWindow != 0) {
                XUnmapWindow(display, synthWindow);
            }
            synthWindowVisible = false;
            synthPointerDown = false;
            synthLastPointerMidi = -1;
        }
    };

    auto runFileButtonAction = [&](const std::string& actionId) {
        const AppSessionSnapshot snap = activeSnapshot();
        if (actionId == "synth.window.toggle") {
            setSynthWindowVisible(!synthWindowVisible);
            return;
        }
        if (actionId == "audio.tuning.toggle") {
            audioTuningDialogActive = !audioTuningDialogActive;
            return;
        }
        if (actionId == "project.new") {
            audioTuningDialogActive = false;
            runLifecycleAction(makeActionRequest("project.new"));
            return;
        }
        if (actionId == "project.save") {
            audioTuningDialogActive = false;
            if (snap.hasProjectPath) {
                runAction(makeActionRequest("project.save"));
            } else {
                beginInlinePrompt(
                    InlinePromptKind::SaveProjectPath,
                    "Save project",
                    "Path to save project",
                    defaultProjectPath(snap));
            }
            return;
        }
        if (actionId == "project.open") {
            audioTuningDialogActive = false;
            beginInlinePrompt(
                InlinePromptKind::OpenProjectPath,
                "Load project or MIDI",
                "Path to .arachno or .mid/.midi file",
                defaultProjectPath(snap));
            return;
        }
        if (actionId == "export.mixdown") {
            audioTuningDialogActive = false;
            beginInlinePrompt(
                InlinePromptKind::ExportMixdownPath,
                "Export audio or MIDI",
                "Output file (.wav/.mp3/.ogg/.mid)",
                defaultMixdownPath(snap));
            return;
        }
        if (actionId == "import.midi") {
            audioTuningDialogActive = false;
            beginInlinePrompt(
                InlinePromptKind::ImportMidiPath,
                "Import MIDI file",
                "Path to .mid/.midi file (uses sidebar MIDI import preset)",
                defaultMidiImportPath(snap));
        }
    };

    auto resizePatternRows = [&](int rows) {
        rows = std::clamp(rows, 8, 8192);
        if (rows == activePatternRows) {
            return;
        }
        AppActionRequest resize;
        resize.actionId = "editor.pattern.resize";
        resize.parameters = {{"rows", std::to_string(rows)}};
        runAction(resize);
    };

    auto setArmedOctave = [&](int octave) {
        armedOctave = std::clamp(octave, 0, 8);
        const int pitchClass = std::clamp(paintNoteMidi, 0, 127) % 12;
        paintNoteMidi = std::clamp((armedOctave * 12) + pitchClass, 0, 127);
    };

    auto clampSynthKeyboardBase = [&]() {
        const int maxStart = std::max(0, 10 - synthKeyboardVisibleOctaves);
        synthKeyboardBaseOctave = std::clamp(synthKeyboardBaseOctave, 0, maxStart);
    };

    auto ensureSynthKeyboardShowsMidi = [&](int midiNote) {
        const int clampedNote = std::clamp(midiNote, 0, 127);
        const int noteOctave = clampedNote / 12;
        if (noteOctave < synthKeyboardBaseOctave
            || noteOctave >= synthKeyboardBaseOctave + synthKeyboardVisibleOctaves) {
            synthKeyboardBaseOctave = noteOctave - (synthKeyboardVisibleOctaves / 2);
            clampSynthKeyboardBase();
        }
    };

    auto ensurePatternRowsForRow = [&](int row) {
        if (row < activePatternRows) {
            return;
        }
        int targetRows = std::max(8, activePatternRows);
        while (targetRows <= row && targetRows < 8192) {
            targetRows += (targetRows < 256 ? 16 : 32);
        }
        resizePatternRows(targetRows);
    };

    auto selectPatternIndex = [&](int index, bool resetViewStart) {
        const AppSessionSnapshot snap = activeSnapshot();
        const int count = static_cast<int>(snap.editor.patterns.size());
        if (count <= 0) {
            return false;
        }
        const int clamped = std::clamp(index, 0, count - 1);
        AppActionRequest pattern;
        pattern.actionId = "editor.navigation.pattern";
        pattern.parameters = {{"index", std::to_string(clamped)}};
        const AppActionResult nav = runAction(pattern);
        if (nav.ok) {
            keyboardSelectionActive = false;
            if (resetViewStart) {
                viewStartRow = 0;
            }
        }
        return nav.ok;
    };

    auto normalizedPatternNameToken = [&](std::string value, const std::string& fallback) {
        value = trimCopy(value);
        std::string out;
        out.reserve(value.size());
        bool previousUnderscore = false;
        for (const unsigned char raw : value) {
            const char ch = static_cast<char>(raw);
            if (std::isspace(raw)) {
                if (!out.empty() && !previousUnderscore) {
                    out.push_back('_');
                    previousUnderscore = true;
                }
                continue;
            }
            out.push_back(ch);
            previousUnderscore = ch == '_';
        }
        while (!out.empty() && out.back() == '_') {
            out.pop_back();
        }
        return out.empty() ? fallback : out;
    };

    auto beginPatternCreatePrompt = [&]() {
        const AppSessionSnapshot snap = activeSnapshot();
        const int nextNumber = std::max(1, static_cast<int>(snap.editor.patterns.size()) + 1);
        const int defaultRows = std::clamp(activePatternRows, 8, 8192);
        const int defaultTracks = std::max(1, snap.editor.activeGrid.trackCount);
        std::ostringstream initial;
        initial << "Pattern" << nextNumber << " " << defaultRows << " " << defaultTracks;
        beginInlinePrompt(
            InlinePromptKind::PatternCreateSpec,
            "Create pattern",
            "name rows [tracks] (example: Verse 64 4)",
            initial.str());
    };

    auto beginPatternClonePrompt = [&]() {
        const AppSessionSnapshot snap = activeSnapshot();
        const int count = static_cast<int>(snap.editor.patterns.size());
        std::string initial = "PatternCopy";
        if (count > 0) {
            const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
            initial = normalizedPatternNameToken(
                snap.editor.patterns[static_cast<std::size_t>(current)].name + "_copy",
                "PatternCopy");
        }
        beginInlinePrompt(
            InlinePromptKind::PatternCloneName,
            "Clone active pattern",
            "optional clone name token (single word)",
            initial);
    };

    auto deleteActivePattern = [&]() {
        AppActionRequest request;
        request.actionId = "editor.pattern.delete";
        const AppActionResult action = runAction(request);
        if (action.ok) {
            keyboardSelectionActive = false;
            viewStartRow = 0;
        }
        return action.ok;
    };

    auto beginTemporalPastePrompt = [&]() {
        beginInlinePrompt(
            InlinePromptKind::TemporalPasteSpec,
            "Rhythmic paste",
            "anchor repeats interval [offset] | anchor: cursor next_row next_beat next_bar next_sel abs:N | interval: beat bar sel rows:N",
            "next_bar 4 bar");
    };

    executeTemporalPasteSpec = [&](const std::string& specText) {
        AppSessionSnapshot snap = activeSnapshot();
        if (!snap.editor.clipboard.available) {
            (void)runAction(makeActionRequest("editor.selection.copy"));
            snap = activeSnapshot();
        }
        if (!snap.editor.clipboard.available) {
            lastAction.ok = false;
            lastAction.actionId = "editor.selection.paste_temporal";
            lastAction.error = "clipboard is empty (copy a selection first)";
            return false;
        }

        std::vector<std::string> tokens;
        {
            std::istringstream in(specText);
            std::string token;
            while (in >> token) {
                tokens.push_back(token);
            }
        }
        if (tokens.size() < 3) {
            lastAction.ok = false;
            lastAction.actionId = "editor.selection.paste_temporal";
            lastAction.error = "expected: anchor repeats interval [offset]";
            return false;
        }

        auto parsePositiveInt = [&](const std::string& text, int& value) {
            std::istringstream in(text);
            in >> value;
            return static_cast<bool>(in) && in.eof() && value > 0;
        };

        const int rowsPerBeat = std::max(1, snap.editor.status.rowsPerBeat);
        const int rowsPerBar = std::max(rowsPerBeat, rowsPerBeat * 4);
        const int selectionRows = std::max(1, snap.editor.status.selectionRows);
        const int clipboardRows = std::max(1, snap.editor.clipboard.rowCount);
        const int targetTrack = std::max(0, snap.editor.status.selectionStartTrack);
        const int cursorRow = std::max(0, snap.editor.status.cursorRow);

        auto parseRowUnit = [&](std::string token, int& value, bool allowZero = false) {
            token = lowerCopy(trimCopy(token));
            if (token.rfind("every_", 0) == 0) {
                token = token.substr(6);
            }
            if (token == "beat") {
                value = rowsPerBeat;
                return true;
            }
            if (token == "bar") {
                value = rowsPerBar;
                return true;
            }
            if (token == "sel" || token == "selection") {
                value = selectionRows;
                return true;
            }
            if (token.rfind("rows:", 0) == 0) {
                int parsed = 0;
                std::istringstream in(token.substr(5));
                in >> parsed;
                if (!in || !in.eof() || parsed < 0 || (!allowZero && parsed == 0)) {
                    return false;
                }
                value = parsed;
                return true;
            }
            int parsed = 0;
            std::istringstream in(token);
            in >> parsed;
            if (!in || !in.eof() || parsed < 0 || (!allowZero && parsed == 0)) {
                return false;
            }
            value = parsed;
            return true;
        };

        auto parseAnchor = [&](std::string token, int& value) {
            token = lowerCopy(trimCopy(token));
            if (token == "cursor") {
                value = cursorRow;
                return true;
            }
            if (token == "next_row" || token == "nextrow") {
                value = cursorRow + 1;
                return true;
            }
            if (token == "next_beat" || token == "nextbeat") {
                value = ((cursorRow / rowsPerBeat) + 1) * rowsPerBeat;
                return true;
            }
            if (token == "next_bar" || token == "nextbar") {
                value = ((cursorRow / rowsPerBar) + 1) * rowsPerBar;
                return true;
            }
            if (token == "next_sel" || token == "next_selection" || token == "nextsel") {
                value = cursorRow + selectionRows;
                return true;
            }
            if (token.rfind("abs:", 0) == 0) {
                int parsed = 0;
                std::istringstream in(token.substr(4));
                in >> parsed;
                if (!in || !in.eof() || parsed < 0) {
                    return false;
                }
                value = parsed;
                return true;
            }
            int parsed = 0;
            std::istringstream in(token);
            in >> parsed;
            if (!in || !in.eof() || parsed < 0) {
                return false;
            }
            value = parsed;
            return true;
        };

        int startRow = 0;
        if (!parseAnchor(tokens[0], startRow)) {
            lastAction.ok = false;
            lastAction.actionId = "editor.selection.paste_temporal";
            lastAction.error = "invalid anchor token";
            return false;
        }

        int repeats = 0;
        if (!parsePositiveInt(tokens[1], repeats)) {
            lastAction.ok = false;
            lastAction.actionId = "editor.selection.paste_temporal";
            lastAction.error = "repeats must be a positive integer";
            return false;
        }
        repeats = std::clamp(repeats, 1, 512);

        int intervalRows = 0;
        if (!parseRowUnit(tokens[2], intervalRows)) {
            lastAction.ok = false;
            lastAction.actionId = "editor.selection.paste_temporal";
            lastAction.error = "invalid interval token";
            return false;
        }

        int offsetRows = 0;
        if (tokens.size() >= 4) {
            if (!parseRowUnit(tokens[3], offsetRows, true)) {
                lastAction.ok = false;
                lastAction.actionId = "editor.selection.paste_temporal";
                lastAction.error = "invalid offset token";
                return false;
            }
        }

        const int firstRow = std::max(0, startRow + offsetRows);
        int pasted = 0;
        for (int index = 0; index < repeats; ++index) {
            const int row = firstRow + (index * intervalRows);
            ensurePatternRowsForRow(row + clipboardRows - 1);
            AppActionRequest paste;
            paste.actionId = "editor.selection.paste";
            paste.parameters = {
                {"row", std::to_string(row)},
                {"track", std::to_string(targetTrack)}};
            const AppActionResult action = runAction(paste);
            if (!action.ok) {
                lastAction.ok = false;
                lastAction.actionId = "editor.selection.paste_temporal";
                if (lastAction.error.empty()) {
                    lastAction.error = "paste failed";
                }
                return false;
            }
            ++pasted;
        }

        lastAction.ok = true;
        lastAction.actionId = "editor.selection.paste_temporal";
        std::ostringstream message;
        message << "Pasted " << pasted << "x every " << intervalRows << " rows";
        lastAction.message = message.str();
        lastAction.error.clear();
        return true;
    };

    selectInstrument = [&](int index) {
        const AppSessionSnapshot snap = activeSnapshot();
        const int count = static_cast<int>(snap.editor.instruments.size());
        if (count <= 0) {
            return;
        }
        armedInstrument = std::clamp(index, 0, count - 1);
        AppActionRequest inst;
        inst.actionId = "editor.step.instrument";
        inst.parameters = {{"index", std::to_string(armedInstrument)}};
        runAction(inst);
        if (armedInstrument < instrumentListStart) {
            instrumentListStart = armedInstrument;
        } else if (armedInstrument >= instrumentListStart + std::max(1, instrumentListVisibleRows)) {
            instrumentListStart = armedInstrument - std::max(1, instrumentListVisibleRows) + 1;
        }
        const int maxStart = std::max(0, count - std::max(1, instrumentListVisibleRows));
        instrumentListStart = std::clamp(instrumentListStart, 0, maxStart);
    };

    filteredInstrumentIndices = [&](const AppSessionSnapshot& snap) {
        std::vector<int> indices;
        const std::string query = lowerCopy(trimCopy(instrumentBrowserQuery));
        indices.reserve(snap.editor.instruments.size());
        for (const InstrumentSummary& instrument : snap.editor.instruments) {
            const std::string indexToken = std::to_string(instrument.index);
            const bool match = query.empty()
                || lowerCopy(instrument.name).find(query) != std::string::npos
                || indexToken.find(query) != std::string::npos;
            if (match) {
                indices.push_back(instrument.index);
            }
        }
        return indices;
    };

    auto openInstrumentBrowser = [&]() {
        const AppSessionSnapshot snap = activeSnapshot();
        audioTuningDialogActive = false;
        instrumentBrowserActive = true;
        instrumentBrowserQuery.clear();
        instrumentBrowserScroll = 0;
        instrumentBrowserSelected = 0;
        const std::vector<int> indices = filteredInstrumentIndices(snap);
        for (int row = 0; row < static_cast<int>(indices.size()); ++row) {
            if (indices[static_cast<std::size_t>(row)] == armedInstrument) {
                instrumentBrowserSelected = row;
                break;
            }
        }
    };

    auto closeInstrumentBrowser = [&](bool applySelection) {
        if (applySelection) {
            const AppSessionSnapshot snap = activeSnapshot();
            const std::vector<int> indices = filteredInstrumentIndices(snap);
            if (!indices.empty()) {
                const int row = std::clamp(
                    instrumentBrowserSelected,
                    0,
                    static_cast<int>(indices.size()) - 1);
                selectInstrument(indices[static_cast<std::size_t>(row)]);
            }
        }
        instrumentBrowserActive = false;
        instrumentBrowserQuery.clear();
        instrumentBrowserScroll = 0;
        instrumentBrowserSelected = 0;
        instrumentBrowserHitTargets.clear();
        instrumentBrowserListRect = UiRect {};
        instrumentBrowserAcceptButton = UiRect {};
        instrumentBrowserCancelButton = UiRect {};
    };

    auto cycleInstrumentBy = [&](int delta) {
        const AppSessionSnapshot snap = activeSnapshot();
        const int count = static_cast<int>(snap.editor.instruments.size());
        if (count <= 0) {
            return;
        }
        const int current = std::clamp(armedInstrument, 0, count - 1);
        const int next = (current + count + (delta % count)) % count;
        selectInstrument(next);
    };

    auto clampInstrumentListWindow = [&]() {
        const AppSessionSnapshot snap = activeSnapshot();
        const int count = static_cast<int>(snap.editor.instruments.size());
        const int maxStart = std::max(0, count - std::max(1, instrumentListVisibleRows));
        instrumentListStart = std::clamp(instrumentListStart, 0, maxStart);
    };

    auto scrollInstrumentList = [&](int delta) {
        instrumentListStart = std::max(0, instrumentListStart + delta);
        clampInstrumentListWindow();
    };

    auto auditionArmedInstrument = [&]() {
        const AppSessionSnapshot snap = activeSnapshot();
        const int count = static_cast<int>(snap.editor.instruments.size());
        if (count <= 0) {
            lastAction.ok = false;
            lastAction.actionId = "instrument.audition";
            lastAction.error = "no instruments available";
            return;
        }
        armedInstrument = std::clamp(armedInstrument, 0, count - 1);
        const AuditionResult audition = session.playback().auditionInstrument(
            armedInstrument,
            std::clamp(paintNoteMidi, 0, 127),
            defaultVelocity,
            0.35);
        lastAction.ok = audition.ok;
        lastAction.actionId = "instrument.audition";
        lastAction.message = audition.message;
        lastAction.error = audition.error;
    };

    auto auditionSynthPreviewMidi = [&](int midiNote) {
        synthPreviewMidi = std::clamp(midiNote, 0, 127);
        paintNoteMidi = synthPreviewMidi;
        ensureSynthKeyboardShowsMidi(synthPreviewMidi);
        auditionArmedInstrument();
        synthWindowNeedsRedraw = true;
    };

    const std::vector<SynthParamDef> synthParamDefs {
        {"osc_a_enabled", "OSC A ON", 0.0, 1.0, 1.0},
        {"osc_b_enabled", "OSC B ON", 0.0, 1.0, 1.0},
        {"osc_c_enabled", "OSC C ON", 0.0, 1.0, 1.0},
        {"osc_d_enabled", "OSC D ON", 0.0, 1.0, 1.0},
        {"oscillator_mix", "MIX", 0.0, 1.0, 0.02},
        {"oscillator_c_mix", "OSC C MIX", 0.0, 1.0, 0.02},
        {"oscillator_d_mix", "OSC D MIX", 0.0, 1.0, 0.02},
        {"detune_cents", "DETUNE", -48.0, 48.0, 0.5},
        {"detune_c_cents", "DETUNE C", -48.0, 48.0, 0.5},
        {"detune_d_cents", "DETUNE D", -48.0, 48.0, 0.5},
        {"pulse_width", "PULSE", 0.03, 0.97, 0.01},
        {"pwm_depth", "PWM", 0.0, 1.0, 0.02},
        {"unison_voices", "UNISON", 1.0, 8.0, 1.0},
        {"unison_detune_cents", "UNI DTN", 0.0, 40.0, 0.5},
        {"stereo_spread", "SPREAD", 0.0, 1.0, 0.02},
        {"sub_enabled", "SUB ON", 0.0, 1.0, 1.0},
        {"sub_oscillator", "SUB", 0.0, 1.0, 0.02},
        {"noise_enabled", "NOISE ON", 0.0, 1.0, 1.0},
        {"noise", "NOISE", 0.0, 1.0, 0.02},
        {"noise_tone", "NOI TONE", 0.0, 1.0, 0.02},
        {"pan", "PAN", -1.0, 1.0, 0.02},
        {"gain", "GAIN", 0.0, 1.0, 0.02},
        {"cutoff", "CUTOFF", 0.0, 1.0, 0.02},
        {"resonance", "RESO", 0.0, 1.0, 0.02},
        {"filter_envelope", "F-ENV", -1.0, 1.0, 0.02},
        {"lfo_filter_depth", "LFO->CUT", 0.0, 1.0, 0.02},
        {"lfo_pan_depth", "LFO->PAN", 0.0, 1.0, 0.02},
        {"filter_attack", "F A", 0.001, 4.0, 0.01},
        {"filter_decay", "F D", 0.001, 4.0, 0.01},
        {"filter_sustain", "F S", 0.0, 1.0, 0.02},
        {"filter_release", "F R", 0.001, 6.0, 0.02},
        {"drive", "DRIVE", 0.0, 1.0, 0.02},
        {"wavefold", "FOLD", 0.0, 1.0, 0.02},
        {"lfo_rate", "LFO", 0.05, 24.0, 0.1},
        {"vibrato_cents", "VIB", 0.0, 120.0, 1.0},
        {"tremolo_depth", "TREM", 0.0, 1.0, 0.02},
        {"pitch_envelope_semitones", "P ENV", -36.0, 36.0, 0.5},
        {"pitch_envelope_decay", "P DEC", 0.001, 2.0, 0.01},
        {"ring_enabled", "RING ON", 0.0, 1.0, 1.0},
        {"ring_mod", "RING", 0.0, 1.0, 0.02},
        {"fm_enabled", "FM ON", 0.0, 1.0, 1.0},
        {"fm_amount", "FM", 0.0, 1.0, 0.02},
        {"fm_ratio", "FM RAT", 0.1, 16.0, 0.1},
        {"fm_feedback", "FM FB", 0.0, 1.0, 0.02},
        {"hard_sync_enabled", "SYNC ON", 0.0, 1.0, 1.0},
        {"hard_sync", "SYNC", 0.0, 1.0, 0.02},
        {"chorus_enabled", "CHORUS ON", 0.0, 1.0, 1.0},
        {"chorus_mix", "CHORUS", 0.0, 1.0, 0.02},
        {"chorus_rate", "CH RATE", 0.05, 5.0, 0.05},
        {"chorus_depth", "CH DEP", 0.0, 1.0, 0.02},
        {"bit_crush_enabled", "CRUSH ON", 0.0, 1.0, 1.0},
        {"bit_crush", "CRUSH", 0.0, 1.0, 0.02},
        {"sample_rate_reduction", "SR RED", 0.0, 1.0, 0.02},
        {"comb_mix", "COMB MIX", 0.0, 1.0, 0.02},
        {"comb_time", "COMB T", 0.001, 0.5, 0.005},
        {"comb_feedback", "COMB FB", 0.0, 0.98, 0.02},
        {"high_pass", "HI PASS", 0.0, 1.0, 0.02},
        {"click", "CLICK", 0.0, 1.0, 0.02},
        {"transient_shape", "TR SHAPE", 0.0, 1.0, 0.02},
        {"transient_noise", "TR NOISE", 0.0, 1.0, 0.02},
        {"transient_pitch_semitones", "TR PITCH", -36.0, 36.0, 0.5},
        {"transient_pitch_decay", "TR P DEC", 0.001, 0.25, 0.005},
        {"transient_burst_count", "TR BURST", 1.0, 12.0, 1.0},
        {"transient_burst_spacing", "TR SPACE", 0.0005, 0.05, 0.0005},
        {"transient_burst_decay", "TR B DEC", 0.0, 1.0, 0.02},
        {"transient_tone", "TR TONE", 0.0, 1.0, 0.02},
        {"transient_decay", "TR DEC", 0.001, 0.25, 0.005},
        {"amp_attack", "A", 0.001, 4.0, 0.01},
        {"amp_decay", "D", 0.001, 4.0, 0.01},
        {"amp_sustain", "S", 0.0, 1.0, 0.02},
        {"amp_release", "R", 0.001, 6.0, 0.02},
    };

    auto synthParamBelongsToPage = [](const std::string& name, int page) {
        const std::string n = lowerCopy(name);
        const bool osc = n.rfind("osc_", 0) == 0 || n.find("oscillator_") != std::string::npos;
        const bool env = n.rfind("amp_", 0) == 0 || n.rfind("filter_", 0) == 0 || n.rfind("pitch_", 0) == 0;
        const bool mod = n.rfind("lfo_", 0) == 0 || n.find("vibrato") != std::string::npos || n.find("tremolo") != std::string::npos || n == "cutoff" || n == "resonance";
        const bool fx = n.find("chorus") != std::string::npos
            || n.find("ring") != std::string::npos
            || n.find("fm_") != std::string::npos
            || n.find("sync") != std::string::npos
            || n.find("drive") != std::string::npos
            || n.find("fold") != std::string::npos
            || n.find("crush") != std::string::npos
            || n.find("comb") != std::string::npos
            || n.find("high_pass") != std::string::npos
            || n.find("click") != std::string::npos
            || n.find("transient") != std::string::npos;

        if (page == 0) {
            return osc || n.find("detune") != std::string::npos || n == "pulse_width" || n == "pwm_depth"
                || n.find("sub") != std::string::npos || n.find("noise") != std::string::npos
                || n.find("unison") != std::string::npos || n == "stereo_spread" || n == "pan" || n == "gain";
        }
        if (page == 1) {
            return mod;
        }
        if (page == 2) {
            return fx;
        }
        return env;
    };

    auto clampInstrumentIndex = [&]() -> int {
        const int count = static_cast<int>(session.song().instruments.size());
        if (count <= 0) {
            return -1;
        }
        armedInstrument = std::clamp(armedInstrument, 0, count - 1);
        return armedInstrument;
    };

    auto getSynthParameterValue = [&](const SynthPatch& patch, const std::string& name) -> double {
        if (name == "osc_a_enabled") return patch.oscillatorAEnabled ? 1.0 : 0.0;
        if (name == "osc_b_enabled") return patch.oscillatorBEnabled ? 1.0 : 0.0;
        if (name == "osc_c_enabled") return patch.oscillatorCEnabled ? 1.0 : 0.0;
        if (name == "osc_d_enabled") return patch.oscillatorDEnabled ? 1.0 : 0.0;
        if (name == "oscillator_mix") return patch.oscillatorMix;
        if (name == "oscillator_c_mix") return patch.oscillatorCMix;
        if (name == "oscillator_d_mix") return patch.oscillatorDMix;
        if (name == "detune_cents") return patch.detuneCents;
        if (name == "detune_c_cents") return patch.detuneCCents;
        if (name == "detune_d_cents") return patch.detuneDCents;
        if (name == "pulse_width") return patch.pulseWidth;
        if (name == "pwm_depth") return patch.pwmDepth;
        if (name == "unison_detune_cents") return patch.unisonDetuneCents;
        if (name == "sub_enabled") return patch.subEnabled ? 1.0 : 0.0;
        if (name == "sub_oscillator") return patch.subOscillator;
        if (name == "noise_enabled") return patch.noiseEnabled ? 1.0 : 0.0;
        if (name == "noise") return patch.noise;
        if (name == "noise_tone") return patch.noiseTone;
        if (name == "pan") return patch.pan;
        if (name == "cutoff") return patch.cutoff;
        if (name == "resonance") return patch.resonance;
        if (name == "filter_envelope") return patch.filterEnvelopeAmount;
        if (name == "lfo_filter_depth") return patch.lfoFilterDepth;
        if (name == "lfo_pan_depth") return patch.lfoPanDepth;
        if (name == "filter_attack") return patch.filterEnvelope.attack;
        if (name == "filter_decay") return patch.filterEnvelope.decay;
        if (name == "filter_sustain") return patch.filterEnvelope.sustain;
        if (name == "filter_release") return patch.filterEnvelope.release;
        if (name == "drive") return patch.drive;
        if (name == "wavefold") return patch.wavefold;
        if (name == "lfo_rate") return patch.lfoRate;
        if (name == "vibrato_cents") return patch.vibratoCents;
        if (name == "tremolo_depth") return patch.tremoloDepth;
        if (name == "pitch_envelope_semitones") return patch.pitchEnvelopeSemitones;
        if (name == "pitch_envelope_decay") return patch.pitchEnvelopeDecay;
        if (name == "ring_enabled") return patch.ringEnabled ? 1.0 : 0.0;
        if (name == "ring_mod") return patch.ringMod;
        if (name == "fm_enabled") return patch.fmEnabled ? 1.0 : 0.0;
        if (name == "fm_amount") return patch.fmAmount;
        if (name == "fm_ratio") return patch.fmRatio;
        if (name == "fm_feedback") return patch.fmFeedback;
        if (name == "hard_sync_enabled") return patch.hardSyncEnabled ? 1.0 : 0.0;
        if (name == "hard_sync") return patch.hardSync;
        if (name == "chorus_enabled") return patch.chorusEnabled ? 1.0 : 0.0;
        if (name == "chorus_mix") return patch.chorusMix;
        if (name == "chorus_rate") return patch.chorusRate;
        if (name == "chorus_depth") return patch.chorusDepth;
        if (name == "bit_crush_enabled") return patch.bitCrushEnabled ? 1.0 : 0.0;
        if (name == "bit_crush") return patch.bitCrush;
        if (name == "sample_rate_reduction") return patch.sampleRateReduction;
        if (name == "comb_mix") return patch.combMix;
        if (name == "comb_time") return patch.combTime;
        if (name == "comb_feedback") return patch.combFeedback;
        if (name == "high_pass") return patch.highPass;
        if (name == "click") return patch.click;
        if (name == "transient_shape") return patch.transientShape;
        if (name == "transient_noise") return patch.transientNoise;
        if (name == "transient_pitch_semitones") return patch.transientPitchSemitones;
        if (name == "transient_pitch_decay") return patch.transientPitchDecay;
        if (name == "transient_burst_count") return static_cast<double>(patch.transientBurstCount);
        if (name == "transient_burst_spacing") return patch.transientBurstSpacing;
        if (name == "transient_burst_decay") return patch.transientBurstDecay;
        if (name == "transient_tone") return patch.transientTone;
        if (name == "transient_decay") return patch.transientDecay;
        if (name == "unison_voices") return static_cast<double>(patch.unisonVoices);
        if (name == "stereo_spread") return patch.stereoSpread;
        if (name == "gain") return patch.gain;
        if (name == "amp_attack") return patch.ampEnvelope.attack;
        if (name == "amp_decay") return patch.ampEnvelope.decay;
        if (name == "amp_sustain") return patch.ampEnvelope.sustain;
        if (name == "amp_release") return patch.ampEnvelope.release;
        return 0.0;
    };

    auto setSynthParameter = [&](int instrument, const std::string& param, double value, bool refresh = true) {
        AppActionRequest request;
        request.actionId = "editor.instrument.parameter";
        request.parameters = {
            {"instrument", std::to_string(instrument)},
            {"name", param},
            {"value", std::to_string(value)}};
        return runAction(request, refresh).ok;
    };

    auto setSynthWaveform = [&](int instrument, const std::string& oscillator, const std::string& wave, bool refresh = true) {
        AppActionRequest request;
        request.actionId = "editor.instrument.wave";
        request.parameters = {
            {"instrument", std::to_string(instrument)},
            {"oscillator", oscillator},
            {"wave", wave}};
        return runAction(request, refresh).ok;
    };

    applyPatchToInstrument = [&](int instrument, const SynthPatch& patch, bool preserveName) {
        const int count = static_cast<int>(session.song().instruments.size());
        if (instrument < 0 || instrument >= count) {
            lastAction.ok = false;
            lastAction.actionId = "editor.instrument.patch_apply";
            lastAction.error = "instrument index out of range";
            return false;
        }

        auto patchAction = [&](const AppActionRequest& request) {
            AppActionResult result = runAction(request, false);
            return result.ok;
        };
        if (!preserveName) {
            AppActionRequest rename;
            rename.actionId = "editor.instrument.rename";
            rename.parameters = {
                {"instrument", std::to_string(instrument)},
                {"name", patch.name.empty() ? "ImportedPatch" : patch.name}};
            if (!patchAction(rename)) {
                refreshSnapshot();
                return false;
            }
        }

        if (!setSynthWaveform(instrument, "A", lowerCopy(waveformName(patch.oscillatorA)), false)) {
            refreshSnapshot();
            return false;
        }
        if (!setSynthWaveform(instrument, "B", lowerCopy(waveformName(patch.oscillatorB)), false)) {
            refreshSnapshot();
            return false;
        }
        if (!setSynthWaveform(instrument, "C", lowerCopy(waveformName(patch.oscillatorC)), false)) {
            refreshSnapshot();
            return false;
        }
        if (!setSynthWaveform(instrument, "D", lowerCopy(waveformName(patch.oscillatorD)), false)) {
            refreshSnapshot();
            return false;
        }

        const std::vector<std::pair<std::string, double>> params {
            {"osc_a_enabled", patch.oscillatorAEnabled ? 1.0 : 0.0},
            {"osc_b_enabled", patch.oscillatorBEnabled ? 1.0 : 0.0},
            {"osc_c_enabled", patch.oscillatorCEnabled ? 1.0 : 0.0},
            {"osc_d_enabled", patch.oscillatorDEnabled ? 1.0 : 0.0},
            {"oscillator_mix", patch.oscillatorMix},
            {"oscillator_c_mix", patch.oscillatorCMix},
            {"oscillator_d_mix", patch.oscillatorDMix},
            {"detune_cents", patch.detuneCents},
            {"detune_c_cents", patch.detuneCCents},
            {"detune_d_cents", patch.detuneDCents},
            {"pulse_width", patch.pulseWidth},
            {"pwm_depth", patch.pwmDepth},
            {"fm_enabled", patch.fmEnabled ? 1.0 : 0.0},
            {"fm_amount", patch.fmAmount},
            {"fm_ratio", patch.fmRatio},
            {"fm_feedback", patch.fmFeedback},
            {"chorus_enabled", patch.chorusEnabled ? 1.0 : 0.0},
            {"chorus_mix", patch.chorusMix},
            {"chorus_rate", patch.chorusRate},
            {"chorus_depth", patch.chorusDepth},
            {"unison_voices", static_cast<double>(patch.unisonVoices)},
            {"unison_detune_cents", patch.unisonDetuneCents},
            {"stereo_spread", patch.stereoSpread},
            {"sub_enabled", patch.subEnabled ? 1.0 : 0.0},
            {"sub_oscillator", patch.subOscillator},
            {"noise_enabled", patch.noiseEnabled ? 1.0 : 0.0},
            {"noise", patch.noise},
            {"noise_tone", patch.noiseTone},
            {"cutoff", patch.cutoff},
            {"resonance", patch.resonance},
            {"filter_envelope", patch.filterEnvelopeAmount},
            {"lfo_filter_depth", patch.lfoFilterDepth},
            {"lfo_pan_depth", patch.lfoPanDepth},
            {"pitch_envelope_semitones", patch.pitchEnvelopeSemitones},
            {"pitch_envelope_decay", patch.pitchEnvelopeDecay},
            {"lfo_rate", patch.lfoRate},
            {"vibrato_cents", patch.vibratoCents},
            {"tremolo_depth", patch.tremoloDepth},
            {"ring_enabled", patch.ringEnabled ? 1.0 : 0.0},
            {"ring_mod", patch.ringMod},
            {"hard_sync_enabled", patch.hardSyncEnabled ? 1.0 : 0.0},
            {"hard_sync", patch.hardSync},
            {"drive", patch.drive},
            {"wavefold", patch.wavefold},
            {"bit_crush_enabled", patch.bitCrushEnabled ? 1.0 : 0.0},
            {"bit_crush", patch.bitCrush},
            {"sample_rate_reduction", patch.sampleRateReduction},
            {"comb_mix", patch.combMix},
            {"comb_time", patch.combTime},
            {"comb_feedback", patch.combFeedback},
            {"high_pass", patch.highPass},
            {"click", patch.click},
            {"transient_shape", patch.transientShape},
            {"transient_noise", patch.transientNoise},
            {"transient_pitch_semitones", patch.transientPitchSemitones},
            {"transient_pitch_decay", patch.transientPitchDecay},
            {"transient_burst_count", static_cast<double>(patch.transientBurstCount)},
            {"transient_burst_spacing", patch.transientBurstSpacing},
            {"transient_burst_decay", patch.transientBurstDecay},
            {"transient_tone", patch.transientTone},
            {"transient_decay", patch.transientDecay},
            {"gain", patch.gain},
            {"pan", patch.pan},
            {"amp_attack", patch.ampEnvelope.attack},
            {"amp_decay", patch.ampEnvelope.decay},
            {"amp_sustain", patch.ampEnvelope.sustain},
            {"amp_release", patch.ampEnvelope.release},
            {"filter_attack", patch.filterEnvelope.attack},
            {"filter_decay", patch.filterEnvelope.decay},
            {"filter_sustain", patch.filterEnvelope.sustain},
            {"filter_release", patch.filterEnvelope.release},
        };
        for (const auto& entry : params) {
            if (!setSynthParameter(instrument, entry.first, entry.second, false)) {
                refreshSnapshot();
                return false;
            }
        }

        refreshSnapshot();
        return true;
    };

    auto runTrackMetadataAction = [&](const std::string& role, int track) {
        const AppSessionSnapshot snap = activeSnapshot();
        if (snap.editor.tracks.empty()) {
            return;
        }
        const int trackIndex = std::clamp(track, 0, static_cast<int>(snap.editor.tracks.size()) - 1);
        const TrackStripSummary& summary = snap.editor.tracks[static_cast<std::size_t>(trackIndex)];
        if (role == "rename") {
            beginInlinePrompt(
                InlinePromptKind::RenameTrack,
                "Rename track",
                "Track name",
                summary.name,
                trackIndex);
            return;
        }
        AppActionRequest request;
        if (role == "vol_down" || role == "vol_up") {
            request.actionId = "editor.track.volume";
            const double delta = role == "vol_down" ? -0.05 : 0.05;
            const double value = std::clamp(summary.volume + delta, 0.0, 2.0);
            request.parameters = {
                {"track", std::to_string(trackIndex)},
                {"value", std::to_string(value)}};
        } else if (role == "pan_left" || role == "pan_right") {
            request.actionId = "editor.track.pan";
            const double delta = role == "pan_left" ? -0.05 : 0.05;
            const double value = std::clamp(summary.pan + delta, -1.0, 1.0);
            request.parameters = {
                {"track", std::to_string(trackIndex)},
                {"value", std::to_string(value)}};
        } else if (role == "mute") {
            request.actionId = "editor.track.mute";
            request.parameters = {
                {"track", std::to_string(trackIndex)},
                {"value", summary.muted ? "false" : "true"}};
        } else if (role == "solo") {
            request.actionId = "editor.track.solo";
            request.parameters = {
                {"track", std::to_string(trackIndex)},
                {"value", summary.solo ? "false" : "true"}};
        } else {
            return;
        }
        runAction(request);
    };

    auto buildSongToTargetSeconds = [&](double targetSeconds) {
        if (targetSeconds <= 1.0) {
            lastAction.ok = false;
            lastAction.actionId = "arrangement.build_length";
            lastAction.error = "target length must be greater than 1 second";
            return;
        }
        AppSessionSnapshot snap = activeSnapshot();
        if (snap.editor.patterns.empty()) {
            lastAction.ok = false;
            lastAction.actionId = "arrangement.build_length";
            lastAction.error = "no patterns available";
            return;
        }
        const int activePattern = std::clamp(
            snap.editor.status.activePattern,
            0,
            static_cast<int>(snap.editor.patterns.size()) - 1);
        int appended = 0;
        constexpr int kMaxAppend = 4096;
        double previousDuration = snap.editor.status.durationSeconds;
        while (snap.editor.status.durationSeconds + 0.01 < targetSeconds && appended < kMaxAppend) {
            AppActionRequest append;
            append.actionId = "editor.arrangement.append_order";
            append.parameters = {{"pattern", std::to_string(activePattern)}};
            runAction(append);
            snap = activeSnapshot();
            ++appended;
            if (snap.editor.status.durationSeconds <= previousDuration + 1e-6) {
                break;
            }
            previousDuration = snap.editor.status.durationSeconds;
        }
        if (appended <= 0) {
            lastAction.ok = false;
            lastAction.actionId = "arrangement.build_length";
            lastAction.error = "unable to extend arrangement";
            return;
        }
        std::ostringstream message;
        message.setf(std::ios::fixed);
        message.precision(2);
        message << "built arrangement to " << snap.editor.status.durationSeconds << "s";
        lastAction.ok = true;
        lastAction.actionId = "arrangement.build_length";
        lastAction.message = message.str();
        lastAction.error.clear();
    };

    auto trimSongToTargetSeconds = [&](double targetSeconds) {
        if (targetSeconds <= 1.0) {
            lastAction.ok = false;
            lastAction.actionId = "arrangement.trim_length";
            lastAction.error = "target length must be greater than 1 second";
            return;
        }
        AppSessionSnapshot snap = activeSnapshot();
        int removed = 0;
        constexpr int kMaxTrim = 4096;
        while (snap.editor.status.durationSeconds - 0.01 > targetSeconds
               && static_cast<int>(snap.editor.order.size()) > 1
               && removed < kMaxTrim) {
            AppActionRequest remove;
            remove.actionId = "editor.arrangement.remove_order";
            remove.parameters = {{"index", std::to_string(static_cast<int>(snap.editor.order.size()) - 1)}};
            runAction(remove);
            snap = activeSnapshot();
            ++removed;
        }
        if (removed <= 0) {
            lastAction.ok = false;
            lastAction.actionId = "arrangement.trim_length";
            lastAction.error = "nothing to trim";
            return;
        }
        std::ostringstream message;
        message.setf(std::ios::fixed);
        message.precision(2);
        message << "trimmed arrangement to " << snap.editor.status.durationSeconds << "s";
        lastAction.ok = true;
        lastAction.actionId = "arrangement.trim_length";
        lastAction.message = message.str();
        lastAction.error.clear();
    };

    auto moveCursor = [&](int row, int track, bool refresh = true) {
        ensurePatternRowsForRow(std::max(0, row));
        AppActionRequest move;
        move.actionId = "editor.navigation.move";
        move.parameters = {
            {"row", std::to_string(std::max(0, row))},
            {"track", std::to_string(std::max(0, track))}};
        return runAction(move, refresh);
    };

    auto paintNoteAt = [&](int row, int track, int midiNote) {
        const AppSessionSnapshot before = activeSnapshot();
        const int instrumentCount = static_cast<int>(before.editor.instruments.size());
        if (instrumentCount <= 0) {
            return;
        }

        armedInstrument = std::clamp(armedInstrument, 0, instrumentCount - 1);
        (void)moveCursor(row, track, false);
        AppActionRequest inst;
        inst.actionId = "editor.step.instrument";
        inst.parameters = {{"index", std::to_string(armedInstrument)}};
        (void)runAction(inst, false);

        AppActionRequest note;
        note.actionId = "editor.step.note";
        note.parameters = {
            {"note", midiNoteName(std::clamp(midiNote, 0, 127))},
            {"velocity", velocityText(defaultVelocity)}};
        AppActionResult noteResult = runAction(note, false);
        if (noteResult.ok) {
            (void)runAction(makeActionRequest("preview.cursor"), false);
        }
    };

    auto gridPositionToCell = [&](int x, int y, int& row, int& track) -> bool {
        if (x < layout.gridLeft || x >= layout.gridLeft + layout.gridWidth
            || y < layout.gridTop || y >= layout.gridTop + layout.gridHeight) {
            return false;
        }
        const int headerBottom = layout.gridTop + 26;
        if (y < headerBottom) {
            return false;
        }
        const int firstTrackX = layout.gridLeft + layout.rowNumberWidth + 4;
        if (x < firstTrackX || layout.trackWidth <= 0 || layout.rowHeight <= 0 || layout.trackCols <= 0) {
            return false;
        }
        const int col = (x - firstTrackX) / layout.trackWidth;
        const int rowOffset = (y - headerBottom) / layout.rowHeight;
        if (col < 0 || col >= layout.trackCols || rowOffset < 0 || rowOffset >= layout.visibleRows) {
            return false;
        }
        row = viewStartRow + rowOffset;
        track = gridTrackStart + col;
        return true;
    };

    runSync("force_snapshot");
    refreshSnapshot();
    (void)runAction(makeActionRequest("audio.runtime.start"), false);
    ensureTrackerBackbuffer();

    bool running = true;
    bool needsRedraw = true;
    auto lastRefresh = std::chrono::steady_clock::now();

    auto draw = [&]() {

        transportButtons.clear();
        instrumentHitTargets.clear();
        instrumentControlHits.clear();
        instrumentBrowserHitTargets.clear();
        trackHeaderHits.clear();
        orderSlotHits.clear();
        fileButtons.clear();
        themeButtons.clear();
        audioPerformanceButtons.clear();
        audioTuningDialogHits.clear();
        octaveHitTargets.clear();
        pianoKeyHits.clear();
        trackMetadataHits.clear();
        songLengthHits.clear();
        midiImportSettingHits.clear();
        fileBrowserHits.clear();
        inlinePromptButtonsVisible = false;
        unsavedPromptChoices.clear();
        patternRowsMinus = UiRect {};
        patternRowsPlus = UiRect {};
        patternRowsValue = UiRect {};
        gridTrackPrevButton = UiRect {};
        gridTrackNextButton = UiRect {};
        patternPrevButton = UiRect {};
        patternNextButton = UiRect {};
        patternValueButton = UiRect {};
        patternNewButton = UiRect {};
        patternCloneButton = UiRect {};
        patternDeleteButton = UiRect {};
        stepAdvanceButton = UiRect {};
        followPlaybackButton = UiRect {};
        instrumentListRect = UiRect {};
        instrumentBrowserListRect = UiRect {};
        instrumentBrowserAcceptButton = UiRect {};
        instrumentBrowserCancelButton = UiRect {};
        sidebarViewport = UiRect {};
        fileBrowserListRect = UiRect {};
        audioTuningDialogRect = UiRect {};

        const int lineHeight = 17;
        const int margin = 12;
        const int minimumTopPanelHeight = 196;
        topPanelHeightState = std::clamp(topPanelHeightState, minimumTopPanelHeight, std::max(minimumTopPanelHeight, windowHeight - 220));
        const int headerHeight = 124;
        const int statusHeight = std::max(70, topPanelHeightState - headerHeight - 8);
        const int gridTop = margin + headerHeight + statusHeight + 8;
        const int gridBottom = windowHeight - margin;
        const int gridHeight = std::max(120, gridBottom - gridTop);
        sidebarWidthState = std::clamp(sidebarWidthState, 220, std::max(240, windowWidth - 420));
        const int sidebarWidth = sidebarWidthState;
        const int gridWidth = std::max(300, windowWidth - (margin * 3) - sidebarWidth);
        const int gridLeft = margin;
        const int sidebarLeft = gridLeft + gridWidth + margin;

        layout.margin = margin;
        layout.topPanelHeight = headerHeight + statusHeight + 8;
        layout.sidebarWidth = sidebarWidth;
        layout.gridLeft = gridLeft;
        layout.gridTop = gridTop;
        layout.gridWidth = gridWidth;
        layout.gridHeight = gridHeight;
        layout.sidebarLeft = sidebarLeft;
        layout.sidebarHeight = gridHeight;
        layout.verticalSplitterX = sidebarLeft - (margin / 2);
        layout.horizontalSplitterY = gridTop - 4;

        const UiThemePalette& theme = themeMode == GuiThemeMode::HighContrast ? highContrastTheme : dosTheme;
        const unsigned long colorBackground = theme.background;
        const unsigned long colorPanel = theme.panel;
        const unsigned long colorGridHeader = theme.gridHeader;
        const unsigned long colorGridLine = theme.gridLine;
        const unsigned long colorSelection = theme.selection;
        const unsigned long colorCursor = theme.cursor;
        const unsigned long colorText = theme.text;
        const unsigned long colorMutedText = theme.mutedText;
        const unsigned long colorPlayhead = theme.playhead;
        const unsigned long colorButton = theme.button;
        const unsigned long colorButtonActive = theme.buttonActive;
        const unsigned long colorButtonLabel = theme.buttonLabel;
        const unsigned long colorButtonLabelActive = theme.buttonLabelActive;
        const unsigned long colorSelectionText = theme.selectionText;
        const unsigned long colorCursorText = theme.cursorText;
        const unsigned long colorPlayheadText = theme.playheadText;
        const unsigned long colorActiveTagText = theme.activeTagText;
        const Drawable drawTarget = trackerBackbuffer != 0 ? trackerBackbuffer : window;

        XSetForeground(display, gc, colorBackground);
        XFillRectangle(display, drawTarget, gc, 0, 0, static_cast<unsigned int>(windowWidth), static_cast<unsigned int>(windowHeight));

        auto drawFilledRect = [&](int x, int y, int w, int h, unsigned long color) {
            XSetForeground(display, gc, color);
            XFillRectangle(display, drawTarget, gc, x, y, static_cast<unsigned int>(w), static_cast<unsigned int>(h));
        };
        auto drawRect = [&](int x, int y, int w, int h, unsigned long color) {
            XSetForeground(display, gc, color);
            XDrawRectangle(display, drawTarget, gc, x, y, static_cast<unsigned int>(w), static_cast<unsigned int>(h));
        };
        auto drawText = [&](int x, int y, const std::string& text, unsigned long color) {
            XSetForeground(display, gc, color);
            XDrawString(display, drawTarget, gc, x, y, text.c_str(), static_cast<int>(text.size()));
        };
        auto drawButton = [&](const UiRect& rect, const std::string& label, bool active) {
            drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? colorButtonActive : colorButton);
            drawRect(rect.x, rect.y, rect.width, rect.height, colorGridLine);
            drawText(
                rect.x + 8,
                rect.y + rect.height - 8,
                label,
                active ? colorButtonLabelActive : colorButtonLabel);
        };
        auto textWidth = [&](const std::string& text) {
            if (uiFont != nullptr) {
                return XTextWidth(uiFont, text.c_str(), static_cast<int>(text.size()));
            }
            return static_cast<int>(text.size()) * 8;
        };
        auto fitText = [&](const std::string& text, int maxWidth) {
            if (maxWidth <= 10 || textWidth(text) <= maxWidth) {
                return text;
            }
            const std::string ellipsis = "...";
            std::string trimmed = text;
            while (!trimmed.empty() && textWidth(trimmed + ellipsis) > maxWidth) {
                trimmed.pop_back();
            }
            return trimmed.empty() ? ellipsis : (trimmed + ellipsis);
        };
        auto drawToolbarButton = [&](const UiRect& rect, const std::string& label, bool active) {
            drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? colorButtonActive : colorButton);
            drawRect(rect.x, rect.y, rect.width, rect.height, colorGridLine);
            const int tw = textWidth(label);
            const int tx = rect.x + std::max(6, (rect.width - tw) / 2);
            drawText(tx, rect.y + rect.height - 7, label, active ? colorButtonLabelActive : colorButtonLabel);
        };

        const AppSessionSnapshot snap = activeSnapshot();
        const PlaybackSnapshot playback = session.playback().snapshot();

        std::string playbackState = "stopped";
        if (playback.state == TransportState::Playing) {
            playbackState = "playing";
        } else if (playback.state == TransportState::Paused) {
            playbackState = "paused";
        }

        drawFilledRect(margin, margin, windowWidth - (margin * 2), headerHeight, colorPanel);
        drawRect(margin, margin, windowWidth - (margin * 2), headerHeight, colorGridLine);
        drawFilledRect(margin, margin + 34, windowWidth - (margin * 2), 1, colorGridLine);
        drawFilledRect(margin, margin + 76, windowWidth - (margin * 2), 1, colorGridLine);

        const int toolbarY = margin + 6;
        const int toolbarH = 22;
        const int toolbarGap = 6;
        drawText(margin + 10, toolbarY + 16, "ArachnoTracker", colorText);

        struct ToolbarItem {
            std::string label;
            std::string compactLabel;
            std::string actionId;
        };
        std::vector<ToolbarItem> fileItems {
            {"NEW", "N", "project.new"},
            {"LOAD", "L", "project.open"},
            {"SAVE", "S", "project.save"},
            {"TUNE", "TN", "audio.tuning.toggle"},
            {"PATCH", "PT", "synth.window.toggle"},
            {"EXPORT", "X", "export.mixdown"}};
        std::vector<ToolbarItem> transportItems {
            {"PATTERN", "PAT", "playback.play_pattern"},
            {"SONG", "SNG", "playback.play_song"},
            {"PAUSE", "PAU", "playback.pause"},
            {"STOP", "STP", "playback.stop"},
            {"PREVIEW", "PRV", "preview.cursor"}};
        const int groupGap = 10;
        const int rightLimit = windowWidth - margin - 10;
        const int brandRight = margin + 10 + textWidth("ArachnoTracker") + 16;
        int leftCursor = brandRight;
        int rightCursor = rightLimit;

        auto itemsWidth = [&](const std::vector<ToolbarItem>& items, bool compact, int padding, int minWidth, int gap) {
            int width = 0;
            for (std::size_t index = 0; index < items.size(); ++index) {
                const std::string& label = compact ? items[index].compactLabel : items[index].label;
                width += std::max(minWidth, textWidth(label) + padding);
                if (index + 1 < items.size()) {
                    width += gap;
                }
            }
            return width;
        };

        auto drawRightGroup = [&](const std::vector<ToolbarItem>& items, bool compact, int padding, int minWidth, int gap, bool transportGroup) -> bool {
            const int width = itemsWidth(items, compact, padding, minWidth, gap);
            const int startX = rightCursor - width;
            if (startX <= leftCursor + 8) {
                return false;
            }
            int x = startX;
            for (const ToolbarItem& item : items) {
                const std::string& label = compact ? item.compactLabel : item.label;
                const int buttonW = std::max(minWidth, textWidth(label) + padding);
                const UiRect rect {x, toolbarY + 1, buttonW, toolbarH};
                const bool active = (item.actionId == "playback.play_pattern"
                        && playback.state == TransportState::Playing
                        && playback.loop.enabled)
                    || ((item.actionId == "playback.play_song" || item.actionId == "playback.play")
                        && playback.state == TransportState::Playing
                        && !playback.loop.enabled)
                    || (item.actionId == "playback.pause" && playback.state == TransportState::Paused)
                    || (item.actionId == "playback.stop" && playback.state == TransportState::Stopped)
                    || (item.actionId == "theme.dos" && themeMode == GuiThemeMode::Dos)
                    || (item.actionId == "theme.high_contrast" && themeMode == GuiThemeMode::HighContrast)
                    || (item.actionId == "synth.window.toggle" && synthWindowVisible);
                drawToolbarButton(rect, label, active);
                if (item.actionId.rfind("theme.", 0) == 0) {
                    themeButtons.push_back({rect, item.actionId == "theme.dos" ? GuiThemeMode::Dos : GuiThemeMode::HighContrast});
                } else if (transportGroup) {
                    transportButtons.push_back({rect, item.actionId});
                } else {
                    fileButtons.push_back({rect, item.actionId});
                }
                x += buttonW + gap;
            }
            rightCursor = startX - groupGap;
            return true;
        };

        auto drawLeftGroup = [&](const std::vector<ToolbarItem>& items, bool compact, int padding, int minWidth, int gap) -> bool {
            const int width = itemsWidth(items, compact, padding, minWidth, gap);
            if (leftCursor + width >= rightCursor - 8) {
                return false;
            }
            int x = leftCursor;
            for (const ToolbarItem& item : items) {
                const std::string& label = compact ? item.compactLabel : item.label;
                const int buttonW = std::max(minWidth, textWidth(label) + padding);
                const UiRect rect {x, toolbarY + 1, buttonW, toolbarH};
                const bool active = (item.actionId == "synth.window.toggle" && synthWindowVisible)
                    || (item.actionId == "audio.tuning.toggle" && audioTuningDialogActive);
                drawToolbarButton(rect, label, active);
                fileButtons.push_back({rect, item.actionId});
                x += buttonW + gap;
            }
            leftCursor = x + groupGap;
            return true;
        };

        bool compact = false;
        bool veryCompact = false;
        int padding = 14;
        int minWidth = 54;
        int gap = toolbarGap;

        if (!drawRightGroup(transportItems, compact, padding, minWidth, gap, true)) {
            compact = true;
            padding = 10;
            minWidth = 40;
            gap = 4;
            if (!drawRightGroup(transportItems, compact, padding, minWidth, gap, true)) {
                std::vector<ToolbarItem> tinyTransport {
                    {"PATTERN", "PAT", "playback.play_pattern"},
                    {"SONG", "SNG", "playback.play_song"},
                    {"STOP", "STP", "playback.stop"}};
                (void)drawRightGroup(tinyTransport, true, padding, minWidth, gap, true);
                veryCompact = true;
            }
        }
        (void)veryCompact;

        if (!drawLeftGroup(fileItems, compact, padding, minWidth, gap)) {
            std::vector<ToolbarItem> compactFileItems {
                {"NEW", "N", "project.new"},
                {"LOAD", "L", "project.open"},
                {"SAVE", "S", "project.save"},
                {"TUNE", "TN", "audio.tuning.toggle"},
                {"PATCH", "PT", "synth.window.toggle"}};
            if (!drawLeftGroup(compactFileItems, true, 10, 36, 4)) {
                std::vector<ToolbarItem> tinyFileItems {
                    {"NEW", "N", "project.new"},
                    {"SAVE", "S", "project.save"},
                    {"TUNE", "TN", "audio.tuning.toggle"},
                    {"PATCH", "PT", "synth.window.toggle"}};
                (void)drawLeftGroup(tinyFileItems, true, 10, 34, 3);
            }
        }

        const int subtitleX = leftCursor;
        const int subtitleWidth = textWidth("Tracker Workbench");
        if (subtitleX + subtitleWidth + 10 < rightCursor) {
            drawText(subtitleX, toolbarY + 16, "Tracker Workbench", colorMutedText);
        }

        int audioPerfReservedLeft = windowWidth - margin - 8;
        {
            const int perfY = margin + 40;
            const int perfH = 18;
            const int perfGap = 3;
            const std::array<AudioPerformanceMode, 5> perfModes {
                AudioPerformanceMode::Auto,
                AudioPerformanceMode::Live,
                AudioPerformanceMode::Balanced,
                AudioPerformanceMode::Heavy,
                AudioPerformanceMode::Custom};
            int totalButtonsWidth = 0;
            std::array<int, 5> buttonWidths {};
            for (std::size_t index = 0; index < perfModes.size(); ++index) {
                const std::string label = audioPerformanceModeLabel(perfModes[index]);
                buttonWidths[index] = std::max(36, textWidth(label) + 12);
                totalButtonsWidth += buttonWidths[index];
                if (index + 1 < perfModes.size()) {
                    totalButtonsWidth += perfGap;
                }
            }
            const int tuneWidth = std::max(52, textWidth("TUNE") + 12);
            totalButtonsWidth += perfGap + tuneWidth;
            const int perfLabelWidth = textWidth("AUDIO");
            const int perfTotalWidth = perfLabelWidth + 8 + totalButtonsWidth;
            const int perfStartX = windowWidth - margin - 8 - perfTotalWidth;
            if (perfStartX > margin + 300) {
                std::string telemetryText;
                unsigned long telemetryColor = colorMutedText;
                {
                    std::ostringstream telemetry;
                    const char* outputBackend = audioOutputUsesAlsa
                        ? "ALSA"
                        : (audioPipe != nullptr ? "APLAY" : "OFF");
                    telemetry << outputBackend << " " << audioFrameMin << "-" << audioFrameMax;
                    if (audioPerformanceMode == AudioPerformanceMode::Custom) {
                        telemetry << " lvl " << audioCustomLevel;
                    }
#if ARACHNO_HAS_ALSA
                    if (audioOutputUsesAlsa && audioAlsaQueueCapacity > 0) {
                        std::size_t queued = 0;
                        std::size_t capacity = 0;
                        {
                            std::lock_guard<std::mutex> lock(audioAlsaMutex);
                            queued = audioAlsaQueueSize;
                            capacity = audioAlsaQueueCapacity;
                        }
                        if (capacity > 0) {
                            const int queuePct = static_cast<int>((queued * 100) / capacity);
                            telemetry << " q" << std::clamp(queuePct, 0, 100) << "%";
                            if (queuePct > 85) {
                                telemetryColor = colorCursor;
                            } else if (queuePct > 65) {
                                telemetryColor = colorText;
                            } else {
                                telemetryColor = colorMutedText;
                            }
                        }
                    }
#endif
                    telemetryText = telemetry.str();
                    if (!audioOutputUsesAlsa && audioPipe == nullptr) {
                        telemetryColor = colorMutedText;
                    }
                }
                drawText(perfStartX, perfY + 14, "AUDIO", colorMutedText);
                int x = perfStartX + perfLabelWidth + 8;
                for (std::size_t index = 0; index < perfModes.size(); ++index) {
                    const AudioPerformanceMode mode = perfModes[index];
                    const UiRect rect {x, perfY, buttonWidths[index], perfH};
                    const std::string label = audioPerformanceModeLabel(mode);
                    drawToolbarButton(rect, label, audioPerformanceMode == mode);
                    audioPerformanceButtons.push_back({rect, mode});
                    x += buttonWidths[index] + perfGap;
                }
                const UiRect tuneRect {x, perfY, tuneWidth, perfH};
                drawToolbarButton(tuneRect, "TUNE", audioTuningDialogActive);
                fileButtons.push_back({tuneRect, "audio.tuning.toggle"});
                const int telemetryY = perfY + perfH + 13;
                const int telemetryWidth = perfTotalWidth;
                drawText(perfStartX, telemetryY, fitText(telemetryText, telemetryWidth), telemetryColor);
                audioPerfReservedLeft = perfStartX - 10;
            }
        }

        const int hintMaxWidth = std::max(160, audioPerfReservedLeft - (margin + 10));
        const std::string hintLine1 = "Enter inserts selected note | Ctrl+0..8 octave | Arrows move | Shift+Arrows select | Space pattern play/stop";
        drawText(
            margin + 10,
            margin + 54,
            fitText(hintLine1, hintMaxWidth),
            colorMutedText);
        const std::string hintLine2 = "Transport: F5 song F6 pattern | F7 mode | Ctrl+F7 tuning | Wheel in tuning = infinite range";
        drawText(
            margin + 10,
            margin + 71,
            fitText(hintLine2, hintMaxWidth),
            colorMutedText);

        {
            const int patternCount = static_cast<int>(snap.editor.patterns.size());
            const int activePattern = patternCount > 0
                ? std::clamp(snap.editor.status.activePattern, 0, patternCount - 1)
                : 0;
            const std::string activePatternName = (patternCount > 0
                    && activePattern < static_cast<int>(snap.editor.patterns.size()))
                ? snap.editor.patterns[static_cast<std::size_t>(activePattern)].name
                : "None";

            drawText(margin + 10, margin + 92, "PATTERN", colorMutedText);
            const int patternRowY = margin + 96;
            const int rowLeft = margin + 84;
            const int rowRight = windowWidth - margin - 8;
            const int navW = 22;
            const int navGap = 4;
            const int actionGap = 4;
            const int actionW = 56;
            const int actionTotalW = (actionW * 3) + (actionGap * 2);
            const bool showPatternActions = (rowRight - rowLeft) >= (navW + navGap + 110 + navGap + navW + navGap + actionTotalW);
            patternPrevButton = UiRect {rowLeft, patternRowY, navW, 16};
            if (showPatternActions) {
                const int actionStart = rowRight - actionTotalW;
                patternNextButton = UiRect {actionStart - navGap - navW, patternRowY, navW, 16};
                patternValueButton = UiRect {
                    patternPrevButton.x + patternPrevButton.width + navGap,
                    patternRowY,
                    std::max(120, patternNextButton.x - navGap - (patternPrevButton.x + patternPrevButton.width + navGap)),
                    16};
                patternNewButton = UiRect {actionStart, patternRowY, actionW, 16};
                patternCloneButton = UiRect {actionStart + actionW + actionGap, patternRowY, actionW, 16};
                patternDeleteButton = UiRect {actionStart + (actionW + actionGap) * 2, patternRowY, actionW, 16};
            } else {
                patternNextButton = UiRect {rowRight - navW, patternRowY, navW, 16};
                patternValueButton = UiRect {
                    patternPrevButton.x + patternPrevButton.width + navGap,
                    patternRowY,
                    std::max(120, patternNextButton.x - navGap - (patternPrevButton.x + patternPrevButton.width + navGap)),
                    16};
            }
            drawButton(patternPrevButton, "<", false);
            drawButton(patternNextButton, ">", false);
            if (showPatternActions) {
                drawButton(patternNewButton, "NEW", false);
                drawButton(patternCloneButton, "CLONE", false);
                drawButton(patternDeleteButton, "DEL", false);
            }
            drawFilledRect(
                patternValueButton.x,
                patternValueButton.y,
                patternValueButton.width,
                patternValueButton.height,
                colorButton);
            drawRect(
                patternValueButton.x,
                patternValueButton.y,
                patternValueButton.width,
                patternValueButton.height,
                colorGridLine);
            std::ostringstream patternLabel;
            patternLabel << "P " << (activePattern + 1) << "/" << std::max(1, patternCount)
                         << "  " << activePatternName;
            drawText(
                patternValueButton.x + 6,
                patternValueButton.y + 12,
                fitText(patternLabel.str(), patternValueButton.width - 10),
                colorText);

            drawText(margin + 10, margin + 115, "ORDER", colorMutedText);
            const int slotY = margin + 120;
            const int slotLeft = margin + 64;
            const int slotWidth = 66;
            const int slotGap = 4;
            const int available = std::max(1, (windowWidth - slotLeft - margin - 8) / (slotWidth + slotGap));
            const int count = std::min(static_cast<int>(snap.editor.order.size()), available);
            for (int index = 0; index < count; ++index) {
                const OrderSlotSummary& slot = snap.editor.order[static_cast<std::size_t>(index)];
                const UiRect rect {slotLeft + (index * (slotWidth + slotGap)), slotY, slotWidth, 14};
                const bool active = slot.activePattern;
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? colorButtonActive : colorButton);
                drawRect(rect.x, rect.y, rect.width, rect.height, colorGridLine);
                std::ostringstream label;
                label << index << ":" << slot.pattern;
                drawText(
                    rect.x + 4,
                    rect.y + 11,
                    label.str(),
                    slot.missing ? colorMutedText : (active ? colorActiveTagText : colorText));
                orderSlotHits.push_back({rect, slot.pattern, slot.startRow, !slot.missing && slot.pattern >= 0});
            }
        }

        drawFilledRect(margin, margin + headerHeight + 8, windowWidth - (margin * 2), statusHeight, colorPanel);
        drawRect(margin, margin + headerHeight + 8, windowWidth - (margin * 2), statusHeight, colorGridLine);
        drawFilledRect(layout.verticalSplitterX, gridTop, 5, gridHeight, colorGridLine);
        drawFilledRect(margin, layout.horizontalSplitterY, windowWidth - (margin * 2), 3, colorGridLine);

        int y = margin + headerHeight + 28;
        auto printLine = [&](const std::string& line) {
            drawText(margin + 10, y, line, colorText);
            y += lineHeight;
        };

        std::ostringstream status;
        status
            << "Project: " << (snap.hasProjectPath ? snap.projectPath : "<untitled>")
            << "  Dirty: " << (snap.dirty ? "yes" : "no")
            << "  Playback: " << playbackState
            << "  Row: " << snap.playback.position.patternRow
            << "  Pattern: " << snap.playback.position.pattern;
        printLine(status.str());

        std::ostringstream cursor;
        cursor
            << "Cursor: pattern " << snap.editor.status.activePattern
            << " row " << snap.editor.status.cursorRow
            << " track " << snap.editor.status.cursorTrack
            << "  Selection: " << snap.editor.status.selectionRows
            << "x" << snap.editor.status.selectionTracks;
        printLine(cursor.str());

        std::ostringstream counts;
        counts
            << "Patterns: " << snap.editor.patterns.size()
            << "  Tracks: " << snap.editor.tracks.size()
            << "  Instruments: " << snap.editor.instruments.size()
            << "  Audio: " << (snap.audio.active ? "active" : "inactive")
            << " (" << audioBackendName(snap.audio.backend) << ")";
        printLine(counts.str());

        std::ostringstream trackerState;
        trackerState
            << "Armed instrument: " << armedInstrument
            << "  Octave: " << armedOctave
            << "  Velocity: " << static_cast<int>(defaultVelocity * 100.0f)
            << "  Step advance: " << (stepAdvance ? "on" : "off")
            << "  Follow playback: " << (followPlayback ? "on" : "off")
            << "  Audio mode: " << audioPerformanceModeLabel(audioPerformanceMode);
        if (audioPerformanceMode == AudioPerformanceMode::Custom) {
            trackerState << " (" << audioCustomLevel << ")";
        }
        printLine(trackerState.str());

        drawFilledRect(gridLeft, gridTop, gridWidth, gridHeight, colorPanel);
        drawRect(gridLeft, gridTop, gridWidth, gridHeight, colorGridLine);

        const PatternGrid& grid = snap.editor.activeGrid;
        const int rowNumberWidth = 56;
        const int totalTrackCols = std::max(1, grid.trackCount);
        const int trackWidth = 84;
        const int visibleTrackCols = std::max(1, (gridWidth - rowNumberWidth - 6) / trackWidth);
        const int maxTrackStart = std::max(0, totalTrackCols - visibleTrackCols);
        gridTrackStart = std::clamp(gridTrackStart, 0, maxTrackStart);
        const int gridHeaderY = gridTop + 22;
        const int rowHeight = 18;
        const int maxRows = std::max(1, (gridHeight - 36) / rowHeight);
        const int visibleRows = std::min(grid.rowCount, maxRows);
        layout.rowNumberWidth = rowNumberWidth;
        layout.trackCols = visibleTrackCols;
        layout.trackWidth = trackWidth;
        layout.rowHeight = rowHeight;
        layout.visibleRows = visibleRows;
        requestedRowCount = std::max(1, maxRows);

        drawFilledRect(gridLeft + 1, gridTop + 1, gridWidth - 2, 24, colorGridHeader);
        drawText(gridLeft + 8, gridHeaderY, "ROW", colorText);
        if (totalTrackCols > visibleTrackCols) {
            const UiRect leftRect {gridLeft + 4, gridTop + 4, 12, 16};
            const UiRect rightRect {gridLeft + rowNumberWidth - 16, gridTop + 4, 12, 16};
            drawButton(leftRect, "<", false);
            drawButton(rightRect, ">", false);
            gridTrackPrevButton = leftRect;
            gridTrackNextButton = rightRect;
            std::ostringstream range;
            range << (gridTrackStart + 1) << "-" << std::min(totalTrackCols, gridTrackStart + visibleTrackCols)
                  << "/" << totalTrackCols;
            drawText(gridLeft + 18, gridHeaderY, fitText(range.str(), rowNumberWidth - 38), colorMutedText);
        }
        for (int col = 0; col < visibleTrackCols; ++col) {
            const int track = gridTrackStart + col;
            if (track >= totalTrackCols) {
                break;
            }
            const int x = gridLeft + rowNumberWidth + 4 + (col * trackWidth);
            const TrackStripSummary* trackSummary = track < static_cast<int>(snap.editor.tracks.size())
                ? &snap.editor.tracks[static_cast<std::size_t>(track)]
                : nullptr;
            const std::string trackName = track < static_cast<int>(grid.trackNames.size())
                ? grid.trackNames[static_cast<std::size_t>(track)]
                : ("T" + std::to_string(track));
            const int nameWidth = std::max(12, trackWidth - 34);
            const std::string shownName = fitText(trackName, nameWidth);
            const bool nameTruncated = shownName != trackName;
            drawText(x + 4, gridHeaderY, shownName, colorText);
            const UiRect muteRect {x + trackWidth - 28, gridTop + 5, 11, 11};
            const UiRect soloRect {x + trackWidth - 14, gridTop + 5, 11, 11};
            drawFilledRect(muteRect.x, muteRect.y, muteRect.width, muteRect.height,
                (trackSummary != nullptr && trackSummary->muted) ? colorButtonActive : colorButton);
            drawFilledRect(soloRect.x, soloRect.y, soloRect.width, soloRect.height,
                (trackSummary != nullptr && trackSummary->solo) ? colorButtonActive : colorButton);
            drawRect(muteRect.x, muteRect.y, muteRect.width, muteRect.height, colorGridLine);
            drawRect(soloRect.x, soloRect.y, soloRect.width, soloRect.height, colorGridLine);
            drawText(
                muteRect.x + 2,
                muteRect.y + 9,
                "M",
                (trackSummary != nullptr && trackSummary->muted) ? colorActiveTagText : colorText);
            drawText(
                soloRect.x + 3,
                soloRect.y + 9,
                "S",
                (trackSummary != nullptr && trackSummary->solo) ? colorActiveTagText : colorText);
            TrackHeaderHit hit;
            hit.selectRect = UiRect {x, gridTop + 1, trackWidth, 24};
            hit.muteRect = muteRect;
            hit.soloRect = soloRect;
            hit.track = track;
            hit.trackName = trackName;
            hit.nameTruncated = nameTruncated;
            trackHeaderHits.push_back(hit);
            drawRect(x, gridTop + 1, trackWidth, gridHeight - 2, colorGridLine);
        }

        auto formatCell = [](const PatternGridCell& cell) {
            if (!cell.hasNote) {
                return std::string("... .. ..");
            }
            char buffer[32];
            const int velocity = static_cast<int>(cell.velocity * 100.0f);
            std::snprintf(buffer, sizeof(buffer), "%-3s %02d %02d", cell.noteName.c_str(), cell.instrument, velocity);
            return std::string(buffer);
        };

        const bool playingHere = snap.playback.state == TransportState::Playing
            && snap.playback.position.pattern == snap.editor.status.activePattern;

        for (int rowOffset = 0; rowOffset < visibleRows; ++rowOffset) {
            const int yTop = gridTop + 26 + (rowOffset * rowHeight);
            const int textY = yTop + 13;
            const int absoluteRow = grid.startRow + rowOffset;
            bool rowOnPlayhead = false;
            if (playingHere && absoluteRow == snap.playback.position.patternRow) {
                drawFilledRect(gridLeft + 1, yTop + 1, gridWidth - 2, rowHeight - 2, colorPlayhead);
                rowOnPlayhead = true;
            }
            char rowBuf[16];
            std::snprintf(rowBuf, sizeof(rowBuf), "%03d", absoluteRow);
            drawText(gridLeft + 8, textY, rowBuf, rowOnPlayhead ? colorPlayheadText : colorMutedText);

            for (int col = 0; col < visibleTrackCols; ++col) {
                const int track = gridTrackStart + col;
                if (track >= totalTrackCols) {
                    break;
                }
                const int cellIndex = (rowOffset * totalTrackCols) + track;
                if (cellIndex < 0 || cellIndex >= static_cast<int>(grid.cells.size())) {
                    continue;
                }
                const PatternGridCell& cell = grid.cells[static_cast<std::size_t>(cellIndex)];
                const int x = gridLeft + rowNumberWidth + 4 + (col * trackWidth);
                if (cell.selected) {
                    drawFilledRect(x + 1, yTop + 1, trackWidth - 2, rowHeight - 2, colorSelection);
                }
                if (cell.cursor) {
                    drawFilledRect(x + 1, yTop + 1, trackWidth - 2, rowHeight - 2, colorCursor);
                }
                const unsigned long cellTextColor = cell.cursor
                    ? colorCursorText
                    : (cell.selected ? colorSelectionText : (rowOnPlayhead ? colorPlayheadText : colorText));
                drawText(x + 4, textY, formatCell(cell), cellTextColor);
                if (cell.cursor) {
                    drawRect(x + 1, yTop + 1, trackWidth - 2, rowHeight - 2, colorCursorText);
                }
            }
        }

        if (hoveredTrackHeader >= 0 && hoveredTrackHeader < static_cast<int>(trackHeaderHits.size())) {
            const TrackHeaderHit& hover = trackHeaderHits[static_cast<std::size_t>(hoveredTrackHeader)];
            if (hover.nameTruncated && !hover.trackName.empty()) {
                const int tooltipPad = 6;
                const int tooltipW = std::max(80, textWidth(hover.trackName) + (tooltipPad * 2));
                const int tooltipH = 20;
                int tx = pointerX + 14;
                int ty = pointerY + 14;
                tx = std::clamp(tx, margin + 2, windowWidth - tooltipW - margin - 2);
                ty = std::clamp(ty, margin + 2, windowHeight - tooltipH - margin - 2);
                drawFilledRect(tx, ty, tooltipW, tooltipH, colorPanel);
                drawRect(tx, ty, tooltipW, tooltipH, colorGridLine);
                drawText(tx + tooltipPad, ty + 14, hover.trackName, colorText);
            }
        }

        drawFilledRect(sidebarLeft, gridTop, sidebarWidth, gridHeight, colorPanel);
        drawRect(sidebarLeft, gridTop, sidebarWidth, gridHeight, colorGridLine);
        sidebarViewport = UiRect {sidebarLeft + 1, gridTop + 1, sidebarWidth - 2, gridHeight - 2};
        const int sidebarViewportHeight = std::max(1, sidebarViewport.height - 8);
        const int maxSidebarScroll = std::max(0, sidebarContentHeight - sidebarViewportHeight);
        sidebarScrollOffset = std::clamp(sidebarScrollOffset, 0, maxSidebarScroll);
        const int sidebarContentStartY = gridTop + 20;
        int sy = sidebarContentStartY - sidebarScrollOffset;
        XRectangle sidebarClip;
        sidebarClip.x = static_cast<short>(sidebarViewport.x);
        sidebarClip.y = static_cast<short>(sidebarViewport.y);
        sidebarClip.width = static_cast<unsigned short>(std::max(0, sidebarViewport.width));
        sidebarClip.height = static_cast<unsigned short>(std::max(0, sidebarViewport.height));
        XSetClipRectangles(display, gc, 0, 0, &sidebarClip, 1, Unsorted);
        auto sidebarLine = [&](const std::string& line, bool muted = false) {
            drawText(sidebarLeft + 8, sy, line, muted ? colorMutedText : colorText);
            sy += 16;
        };

        sidebarLine("ACTIVE STEP");
        std::ostringstream step;
        step << "Row " << snap.editor.activeStep.row << " Track " << snap.editor.activeStep.trackName;
        sidebarLine(step.str(), true);
        sidebarLine(
            snap.editor.activeStep.hasNote
                ? ("Note " + snap.editor.activeStep.noteName + "  Inst " + std::to_string(snap.editor.activeStep.instrument))
                : "Note ---  Inst --");
        {
            std::ostringstream vel;
            vel << "Velocity " << static_cast<int>(snap.editor.activeStep.velocity * 100.0f)
                << "  Gate " << snap.editor.activeStep.gateRows;
            sidebarLine(vel.str(), true);
        }
        sidebarLine("NOTE KEYS");
        sidebarLine("Z S X D C V G B H N J M", true);
        sidebarLine("Q 2 W 3 E R 5 T 6 Y 7 U", true);
        sidebarLine("Paint note " + midiNoteName(paintNoteMidi), true);
        sidebarLine("OCTAVE");
        {
            const int octaveButtonY = sy - 12;
            const UiRect minusRect {sidebarLeft + 8, octaveButtonY, 20, 20};
            const UiRect plusRect {sidebarLeft + 220, octaveButtonY, 20, 20};
            drawButton(minusRect, "-", false);
            drawButton(plusRect, "+", false);
            octaveHitTargets.push_back({minusRect, -1});
            octaveHitTargets.push_back({plusRect, 100});

            int ox = sidebarLeft + 32;
            for (int octave = 0; octave <= 8; ++octave) {
                const UiRect rect {ox, octaveButtonY, 20, 20};
                drawButton(rect, std::to_string(octave), octave == armedOctave);
                octaveHitTargets.push_back({rect, octave});
                ox += 21;
            }
            sy += 22;
        }
        sidebarLine("PIANO");
        {
            const int whiteKeyWidth = 16;
            const int whiteKeyHeight = 42;
            const int blackKeyWidth = 10;
            const int blackKeyHeight = 24;
            const int pianoX = sidebarLeft + 8;
            const int pianoY = sy - 12;
            const int baseMidi = std::clamp(armedOctave * 12, 0, 115);
            const int whiteSemitones[7] = {0, 2, 4, 5, 7, 9, 11};
            const int blackSemitones[5] = {1, 3, 6, 8, 10};
            const int blackXOffsets[5] = {11, 27, 59, 75, 91};

            for (int white = 0; white < 7; ++white) {
                const int midi = std::clamp(baseMidi + whiteSemitones[white], 0, 127);
                const UiRect rect {pianoX + (white * whiteKeyWidth), pianoY, whiteKeyWidth, whiteKeyHeight};
                const bool active = midi == paintNoteMidi;
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? colorButtonActive : theme.pianoWhite);
                drawRect(rect.x, rect.y, rect.width, rect.height, colorGridLine);
                pianoKeyHits.push_back({rect, midi, false});
            }
            for (int black = 0; black < 5; ++black) {
                const int midi = std::clamp(baseMidi + blackSemitones[black], 0, 127);
                const UiRect rect {pianoX + blackXOffsets[black], pianoY, blackKeyWidth, blackKeyHeight};
                const bool active = midi == paintNoteMidi;
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? colorButtonActive : theme.pianoBlack);
                drawRect(rect.x, rect.y, rect.width, rect.height, colorGridLine);
                pianoKeyHits.push_back({rect, midi, true});
            }
            drawText(pianoX + 120, pianoY + 18, midiNoteName(baseMidi), colorMutedText);
            drawText(pianoX + 120, pianoY + 34, midiNoteName(std::clamp(baseMidi + 12, 0, 127)), colorMutedText);
            sy += 46;
        }
        sidebarLine("");
        sidebarLine("PATTERN ROWS");
        patternRowsMinus = UiRect {sidebarLeft + 8, sy - 12, 20, 20};
        patternRowsValue = UiRect {sidebarLeft + 32, sy - 12, 88, 20};
        patternRowsPlus = UiRect {sidebarLeft + 124, sy - 12, 20, 20};
        drawButton(patternRowsMinus, "-", false);
        drawButton(patternRowsValue, std::to_string(activePatternRows), false);
        drawButton(patternRowsPlus, "+", false);
        sy += 20;
        sidebarLine("Drag row count or Ctrl+Wheel", true);

        stepAdvanceButton = UiRect {sidebarLeft + 8, sy - 10, 112, 20};
        followPlaybackButton = UiRect {sidebarLeft + 126, sy - 10, 112, 20};
        drawButton(stepAdvanceButton, stepAdvance ? "STEP ON" : "STEP OFF", stepAdvance);
        drawButton(followPlaybackButton, followPlayback ? "FOLLOW ON" : "FOLLOW OFF", followPlayback);
        sy += 18;

        sidebarLine("");
        sidebarLine("TRACK METADATA");
        if (!snap.editor.tracks.empty()) {
            const int selectedTrack = std::clamp(
                snap.editor.status.cursorTrack,
                0,
                static_cast<int>(snap.editor.tracks.size()) - 1);
            const TrackStripSummary& selected = snap.editor.tracks[static_cast<std::size_t>(selectedTrack)];
            {
                std::ostringstream trackLine;
                trackLine
                    << "T" << selectedTrack
                    << "  "
                    << selected.name;
                sidebarLine(trackLine.str());
            }
            {
                std::ostringstream mixLine;
                mixLine.setf(std::ios::fixed);
                mixLine.precision(2);
                mixLine
                    << "Vol " << selected.volume
                    << "  Pan " << selected.pan
                    << "  "
                    << (selected.muted ? "M" : "-")
                    << (selected.solo ? "S" : "-");
                sidebarLine(mixLine.str(), true);
            }
            const int buttonY = sy - 12;
            const UiRect renameRect {sidebarLeft + 8, buttonY, 64, 20};
            const UiRect volDownRect {sidebarLeft + 76, buttonY, 28, 20};
            const UiRect volUpRect {sidebarLeft + 108, buttonY, 28, 20};
            const UiRect panLeftRect {sidebarLeft + 140, buttonY, 28, 20};
            const UiRect panRightRect {sidebarLeft + 172, buttonY, 28, 20};
            const UiRect muteRect {sidebarLeft + 204, buttonY, 40, 20};
            const UiRect soloRect {sidebarLeft + 248, buttonY, 40, 20};
            drawButton(renameRect, "RENAME", false);
            drawButton(volDownRect, "V-", false);
            drawButton(volUpRect, "V+", false);
            drawButton(panLeftRect, "P<", false);
            drawButton(panRightRect, "P>", false);
            drawButton(muteRect, "MUTE", selected.muted);
            drawButton(soloRect, "SOLO", selected.solo);
            trackMetadataHits.push_back({renameRect, "rename", selectedTrack});
            trackMetadataHits.push_back({volDownRect, "vol_down", selectedTrack});
            trackMetadataHits.push_back({volUpRect, "vol_up", selectedTrack});
            trackMetadataHits.push_back({panLeftRect, "pan_left", selectedTrack});
            trackMetadataHits.push_back({panRightRect, "pan_right", selectedTrack});
            trackMetadataHits.push_back({muteRect, "mute", selectedTrack});
            trackMetadataHits.push_back({soloRect, "solo", selectedTrack});
            sy += 22;
            sidebarLine("Keys: [ / ] prev-next  Shift+1..0 direct  \\ audition", true);
        } else {
            sidebarLine("No tracks", true);
        }

        sidebarLine("");
        sidebarLine("SONG LENGTH");
        {
            std::ostringstream lengthLine;
            lengthLine.setf(std::ios::fixed);
            lengthLine.precision(2);
            lengthLine << "Current " << (snap.editor.status.durationSeconds / 60.0) << "m";
            sidebarLine(lengthLine.str(), true);
        }
        {
            std::ostringstream targetLine;
            targetLine.setf(std::ios::fixed);
            targetLine.precision(2);
            targetLine << "Target  " << targetSongLengthMinutes << "m";
            sidebarLine(targetLine.str(), true);
        }
        {
            const int buttonY = sy - 12;
            const UiRect minusRect {sidebarLeft + 8, buttonY, 24, 20};
            const UiRect plusRect {sidebarLeft + 36, buttonY, 24, 20};
            const UiRect setRect {sidebarLeft + 64, buttonY, 40, 20};
            const UiRect buildRect {sidebarLeft + 108, buttonY, 56, 20};
            const UiRect trimRect {sidebarLeft + 168, buttonY, 56, 20};
            drawButton(minusRect, "-", false);
            drawButton(plusRect, "+", false);
            drawButton(setRect, "SET", false);
            drawButton(buildRect, "BUILD", false);
            drawButton(trimRect, "TRIM", false);
            songLengthHits.push_back({minusRect, "target_down"});
            songLengthHits.push_back({plusRect, "target_up"});
            songLengthHits.push_back({setRect, "target_set"});
            songLengthHits.push_back({buildRect, "build"});
            songLengthHits.push_back({trimRect, "trim"});
            sy += 22;
        }
        sidebarLine("Build extends order; trim removes tail order", true);

        sidebarLine("");
        sidebarLine("MIDI IMPORT PRESET");
        {
            std::ostringstream line;
            line << "Rows/beat " << midiImportRowsPerBeat;
            sidebarLine(line.str(), true);
            const int buttonY = sy - 12;
            const UiRect rpbDownRect {sidebarLeft + 8, buttonY, 24, 20};
            const UiRect rpbUpRect {sidebarLeft + 36, buttonY, 24, 20};
            drawButton(rpbDownRect, "-", false);
            drawButton(rpbUpRect, "+", false);
            midiImportSettingHits.push_back({rpbDownRect, "rpb_down"});
            midiImportSettingHits.push_back({rpbUpRect, "rpb_up"});
            sy += 22;
        }
        {
            std::ostringstream line;
            line << "Pattern rows " << midiImportPatternRows;
            sidebarLine(line.str(), true);
            const int buttonY = sy - 12;
            const UiRect rowsDownRect {sidebarLeft + 8, buttonY, 24, 20};
            const UiRect rowsUpRect {sidebarLeft + 36, buttonY, 24, 20};
            drawButton(rowsDownRect, "-", false);
            drawButton(rowsUpRect, "+", false);
            midiImportSettingHits.push_back({rowsDownRect, "rows_down"});
            midiImportSettingHits.push_back({rowsUpRect, "rows_up"});
            sy += 22;
        }
        {
            const int buttonY = sy - 12;
            const UiRect splitRect {sidebarLeft + 8, buttonY, 116, 20};
            const UiRect importRect {sidebarLeft + 128, buttonY, 116, 20};
            drawButton(splitRect, midiImportSplitByTrack ? "SPLIT TRACKS" : "MERGE LANES", midiImportSplitByTrack);
            drawButton(importRect, "IMPORT MIDI", false);
            midiImportSettingHits.push_back({splitRect, "split_toggle"});
            midiImportSettingHits.push_back({importRect, "import"});
            sy += 22;
        }
        sidebarLine("Use Ctrl+M to open picker", true);

        sidebarLine("");
        sidebarLine("INSTRUMENTS");
        const int listRows = std::max(3, (gridTop + gridHeight - sy - 142) / 16);
        instrumentListVisibleRows = listRows;
        clampInstrumentListWindow();
        const int instrumentCount = static_cast<int>(snap.editor.instruments.size());
        const UiRect instPrevRect {sidebarLeft + 8, sy - 12, 28, 20};
        const UiRect instNextRect {sidebarLeft + 40, sy - 12, 28, 20};
        const UiRect instAudRect {sidebarLeft + 72, sy - 12, 48, 20};
        const UiRect instBrowseRect {sidebarLeft + 124, sy - 12, 84, 20};
        drawButton(instPrevRect, "<", false);
        drawButton(instNextRect, ">", false);
        drawButton(instAudRect, "AUD", false);
        drawButton(instBrowseRect, "BROWSE", false);
        instrumentControlHits.push_back({instPrevRect, "prev"});
        instrumentControlHits.push_back({instNextRect, "next"});
        instrumentControlHits.push_back({instAudRect, "audition"});
        instrumentControlHits.push_back({instBrowseRect, "browse"});
        if (instrumentCount > 0) {
            const int viewStart = std::clamp(instrumentListStart, 0, std::max(0, instrumentCount - 1));
            const int viewEnd = std::min(instrumentCount, viewStart + listRows);
            std::ostringstream range;
            range << (viewStart + 1) << "-" << viewEnd << " / " << instrumentCount;
            drawText(sidebarLeft + 214, sy, range.str(), colorMutedText);
        } else {
            drawText(sidebarLeft + 214, sy, "0 instruments", colorMutedText);
        }
        sy += 20;
        const int listTop = sy - 12;
        const int listHeight = listRows * 16;
        instrumentListRect = UiRect {sidebarLeft + 6, listTop, sidebarWidth - 14, listHeight};

        const int viewStart = std::clamp(instrumentListStart, 0, std::max(0, instrumentCount - 1));
        const int viewEnd = std::min(instrumentCount, viewStart + listRows);
        for (int index = viewStart; index < viewEnd; ++index) {
            const InstrumentSummary& instrument = snap.editor.instruments[static_cast<std::size_t>(index)];
            const UiRect hit {sidebarLeft + 6, sy - 12, sidebarWidth - 14, 16};
            if (index == armedInstrument || instrument.active) {
                drawFilledRect(hit.x, hit.y + 1, hit.width, hit.height - 2, colorSelection);
            }
            std::ostringstream line;
            line
                << (index == armedInstrument ? "*" : " ")
                << (instrument.active ? ">" : " ")
                << " "
                << (index < 10 ? "0" : "") << index
                << " "
                << instrument.name
                << " (" << instrument.noteUseCount << ")";
            drawText(sidebarLeft + 8, sy, line.str(), index == armedInstrument ? colorText : colorMutedText);
            instrumentHitTargets.push_back({hit, index});
            sy += 16;
        }
        if (instrumentCount > viewEnd) {
            drawText(sidebarLeft + 8, sy, "...", colorMutedText);
            sy += 16;
        }
        sidebarLine("Keys: [ / ] prev-next | Ctrl+Up/Down cycle", true);
        sidebarLine("Ctrl+Shift+I browser | Alt+0..9 direct | \\ audition", true);

        sidebarLine("");
        sidebarLine("LAST ACTION");
        if (!lastAction.actionId.empty()) {
            sidebarLine(lastAction.actionId, true);
            sidebarLine(lastAction.ok ? "ok" : "error");
            if (!lastAction.error.empty()) {
                sidebarLine(lastAction.error, true);
            } else if (!lastAction.message.empty()) {
                sidebarLine(lastAction.message, true);
            }
        } else {
            sidebarLine("<none>", true);
        }
        if (lastAction.hasMidiImportReport) {
            const MidiImportReport& report = lastAction.midiImportReport;
            sidebarLine("");
            sidebarLine("MIDI IMPORT");
            {
                std::ostringstream summary;
                summary << "Fmt " << report.midiFormat << "  TPQ " << report.ticksPerQuarterNote;
                sidebarLine(summary.str(), true);
            }
            {
                std::ostringstream counts;
                counts << "Tracks " << report.importedTrackCount
                       << " Inst " << report.trackMappings.size()
                       << " Notes " << report.importedNoteCount;
                sidebarLine(counts.str(), true);
            }
            const int mappingCount = static_cast<int>(report.trackMappings.size());
            const int shown = std::min(6, mappingCount);
            for (int index = 0; index < shown; ++index) {
                const MidiImportTrackMapping& mapping = report.trackMappings[static_cast<std::size_t>(index)];
                std::ostringstream line;
                line << "T" << mapping.trackIndex
                     << ">" << "I" << mapping.instrumentIndex
                     << " ch" << mapping.midiChannel
                     << " pg" << mapping.dominantProgram;
                sidebarLine(line.str(), true);
            }
            if (mappingCount > shown) {
                sidebarLine("... +" + std::to_string(mappingCount - shown) + " lanes", true);
            }
        }
        sidebarLine("");
        sidebarLine("MESSAGE");
        sidebarLine("[" + std::string(appMessageSeverityName(snap.lastMessage.severity)) + "]", true);
        sidebarLine(snap.lastMessage.text);
        XSetClipMask(display, gc, None);
        sidebarContentHeight = std::max(0, (sy + sidebarScrollOffset) - sidebarContentStartY);
        if (sidebarContentHeight > sidebarViewportHeight) {
            const int trackX = sidebarLeft + sidebarWidth - 8;
            const int trackY = gridTop + 6;
            const int trackH = std::max(12, gridHeight - 12);
            drawFilledRect(trackX, trackY, 4, trackH, colorGridLine);
            const double visibleRatio = static_cast<double>(sidebarViewportHeight)
                / static_cast<double>(std::max(1, sidebarContentHeight));
            const int thumbH = std::max(16, static_cast<int>(std::lround(static_cast<double>(trackH) * visibleRatio)));
            const double scrollRatio = static_cast<double>(sidebarScrollOffset)
                / static_cast<double>(std::max(1, sidebarContentHeight - sidebarViewportHeight));
            const int thumbTravel = std::max(0, trackH - thumbH);
            const int thumbY = trackY + static_cast<int>(std::lround(scrollRatio * static_cast<double>(thumbTravel)));
            drawFilledRect(trackX, thumbY, 4, thumbH, colorButtonActive);
        }

        auto drawModalPanel = [&](int width, int height, int& x, int& y) {
            x = std::max(16, (windowWidth - width) / 2);
            y = std::max(16, (windowHeight - height) / 2);
            drawFilledRect(x, y, width, height, colorPanel);
            drawRect(x, y, width, height, colorGridLine);
        };

        if (unsavedPrompt.active) {
            const int modalW = std::min(windowWidth - 40, 560);
            const int modalH = 170;
            int modalX = 0;
            int modalY = 0;
            drawModalPanel(modalW, modalH, modalX, modalY);
            drawText(modalX + 12, modalY + 24, unsavedPrompt.title.empty() ? "Unsaved changes" : unsavedPrompt.title, colorText);
            drawText(
                modalX + 12,
                modalY + 46,
                unsavedPrompt.detail.empty() ? "Save changes before continuing?" : unsavedPrompt.detail,
                colorMutedText);
            const UiRect saveRect {modalX + 12, modalY + modalH - 34, 86, 22};
            const UiRect discardRect {modalX + 106, modalY + modalH - 34, 102, 22};
            const UiRect cancelRect {modalX + 216, modalY + modalH - 34, 86, 22};
            drawButton(saveRect, "SAVE", false);
            drawButton(discardRect, "DISCARD", false);
            drawButton(cancelRect, "CANCEL", false);
            unsavedPromptChoices.push_back({saveRect, UnsavedChangesChoice::Save});
            unsavedPromptChoices.push_back({discardRect, UnsavedChangesChoice::Discard});
            unsavedPromptChoices.push_back({cancelRect, UnsavedChangesChoice::Cancel});
        } else if (instrumentBrowserActive) {
            const int modalW = std::min(windowWidth - 40, 760);
            const int modalH = std::min(windowHeight - 40, 520);
            int modalX = 0;
            int modalY = 0;
            drawModalPanel(modalW, modalH, modalX, modalY);
            drawText(modalX + 12, modalY + 24, "Instrument Browser", colorText);
            drawText(
                modalX + 12,
                modalY + 44,
                "Type to filter by name/index | Enter choose | Up/Down navigate | Ctrl+Up/Down cycle | Esc cancel",
                colorMutedText);

            const UiRect queryRect {modalX + 12, modalY + 54, modalW - 24, 22};
            drawFilledRect(queryRect.x, queryRect.y, queryRect.width, queryRect.height, colorBackground);
            drawRect(queryRect.x, queryRect.y, queryRect.width, queryRect.height, colorGridLine);
            drawText(queryRect.x + 6, queryRect.y + 15, instrumentBrowserQuery.empty() ? "<all>" : instrumentBrowserQuery, colorText);

            instrumentBrowserListRect = UiRect {modalX + 12, modalY + 84, modalW - 24, modalH - 142};
            drawFilledRect(
                instrumentBrowserListRect.x,
                instrumentBrowserListRect.y,
                instrumentBrowserListRect.width,
                instrumentBrowserListRect.height,
                colorBackground);
            drawRect(
                instrumentBrowserListRect.x,
                instrumentBrowserListRect.y,
                instrumentBrowserListRect.width,
                instrumentBrowserListRect.height,
                colorGridLine);

            const std::vector<int> filtered = filteredInstrumentIndices(snap);
            const int rowHeight = 18;
            const int visibleRows = std::max(1, (instrumentBrowserListRect.height - 4) / rowHeight);
            const int maxScroll = std::max(0, static_cast<int>(filtered.size()) - visibleRows);
            instrumentBrowserSelected = std::clamp(
                instrumentBrowserSelected,
                0,
                std::max(0, static_cast<int>(filtered.size()) - 1));
            instrumentBrowserScroll = std::clamp(instrumentBrowserScroll, 0, maxScroll);
            if (instrumentBrowserSelected < instrumentBrowserScroll) {
                instrumentBrowserScroll = instrumentBrowserSelected;
            } else if (instrumentBrowserSelected >= instrumentBrowserScroll + visibleRows) {
                instrumentBrowserScroll = std::max(0, instrumentBrowserSelected - visibleRows + 1);
            }

            if (filtered.empty()) {
                drawText(
                    instrumentBrowserListRect.x + 8,
                    instrumentBrowserListRect.y + 18,
                    "No instruments match this filter.",
                    colorMutedText);
            } else {
                for (int row = 0; row < visibleRows; ++row) {
                    const int filteredRow = instrumentBrowserScroll + row;
                    if (filteredRow >= static_cast<int>(filtered.size())) {
                        break;
                    }
                    const int instrumentIndex = filtered[static_cast<std::size_t>(filteredRow)];
                    if (instrumentIndex < 0 || instrumentIndex >= static_cast<int>(snap.editor.instruments.size())) {
                        continue;
                    }
                    const InstrumentSummary& instrument = snap.editor.instruments[static_cast<std::size_t>(instrumentIndex)];
                    const UiRect rowRect {
                        instrumentBrowserListRect.x + 2,
                        instrumentBrowserListRect.y + 2 + (row * rowHeight),
                        instrumentBrowserListRect.width - 4,
                        rowHeight};
                    const bool selected = filteredRow == instrumentBrowserSelected;
                    if (selected) {
                        drawFilledRect(rowRect.x, rowRect.y, rowRect.width, rowRect.height, colorSelection);
                    }
                    std::ostringstream line;
                    line << (instrumentIndex < 10 ? "0" : "") << instrumentIndex
                         << "  " << instrument.name
                         << "  (" << instrument.noteUseCount << ")";
                    drawText(
                        rowRect.x + 6,
                        rowRect.y + 13,
                        fitText(line.str(), rowRect.width - 12),
                        selected ? colorSelectionText : colorText);
                    instrumentBrowserHitTargets.push_back({rowRect, filteredRow});
                }
            }

            instrumentBrowserAcceptButton = UiRect {modalX + 12, modalY + modalH - 34, 86, 22};
            instrumentBrowserCancelButton = UiRect {modalX + 106, modalY + modalH - 34, 86, 22};
            drawButton(instrumentBrowserAcceptButton, "SELECT", false);
            drawButton(instrumentBrowserCancelButton, "CANCEL", false);
        } else if (audioTuningDialogActive) {
            const int modalW = std::min(windowWidth - 40, 760);
            const int modalH = 244;
            int modalX = 0;
            int modalY = 0;
            drawModalPanel(modalW, modalH, modalX, modalY);
            audioTuningDialogRect = UiRect {modalX, modalY, modalW, modalH};
            drawText(modalX + 12, modalY + 24, "Audio Performance Tuning", colorText);
            drawText(
                modalX + 12,
                modalY + 44,
                "Wheel or +/- controls to stretch from ultra-low latency to ultra-heavy buffering.",
                colorMutedText);

            const int modeY = modalY + 56;
            const int modeH = 20;
            const int modeGap = 4;
            const std::array<AudioPerformanceMode, 5> modes {
                AudioPerformanceMode::Auto,
                AudioPerformanceMode::Live,
                AudioPerformanceMode::Balanced,
                AudioPerformanceMode::Heavy,
                AudioPerformanceMode::Custom};
            int modeX = modalX + 12;
            for (AudioPerformanceMode mode : modes) {
                const std::string label = audioPerformanceModeLabel(mode);
                const int width = std::max(62, textWidth(label) + 14);
                const UiRect rect {modeX, modeY, width, modeH};
                drawToolbarButton(rect, label, audioPerformanceMode == mode);
                AudioTuningDialogHit hit;
                hit.rect = rect;
                hit.role = "mode";
                hit.mode = mode;
                audioTuningDialogHits.push_back(hit);
                modeX += width + modeGap;
            }

            const UiRect valueRect {modalX + 12, modalY + 84, modalW - 24, 40};
            drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, colorBackground);
            drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, colorGridLine);
            std::ostringstream valueLine;
            valueLine << "Custom level " << audioCustomLevel
                      << "  |  Frame window " << audioFrameMin << "-" << audioFrameMax;
            drawText(valueRect.x + 8, valueRect.y + 17, fitText(valueLine.str(), valueRect.width - 12), colorText);
            drawText(valueRect.x + 8, valueRect.y + 33, "Wheel Up = lower latency, Wheel Down = heavier buffering", colorMutedText);

            const int controlsY = modalY + 134;
            const int controlH = 22;
            const int controlGap = 6;
            const std::array<int, 8> deltas {-250, -50, -10, -1, 1, 10, 50, 250};
            const std::array<const char*, 8> deltaLabels {"-250", "-50", "-10", "-1", "+1", "+10", "+50", "+250"};
            int controlX = modalX + 12;
            for (std::size_t index = 0; index < deltas.size(); ++index) {
                const UiRect rect {controlX, controlsY, 58, controlH};
                drawButton(rect, deltaLabels[index], false);
                AudioTuningDialogHit hit;
                hit.rect = rect;
                hit.role = "delta";
                hit.delta = deltas[index];
                audioTuningDialogHits.push_back(hit);
                controlX += rect.width + controlGap;
            }
            const UiRect resetRect {controlX + 8, controlsY, 72, controlH};
            drawButton(resetRect, "RESET", false);
            {
                AudioTuningDialogHit hit;
                hit.rect = resetRect;
                hit.role = "reset";
                audioTuningDialogHits.push_back(hit);
            }

            const UiRect closeRect {modalX + modalW - 92, modalY + modalH - 32, 80, 20};
            drawButton(closeRect, "CLOSE", false);
            {
                AudioTuningDialogHit hit;
                hit.rect = closeRect;
                hit.role = "close";
                audioTuningDialogHits.push_back(hit);
            }

            drawText(
                modalX + 12,
                modalY + modalH - 12,
                "Ctrl+F7 opens this panel | F7 cycles preset modes",
                colorMutedText);
        } else if (inlinePrompt.active && !(synthWindowVisible && isSynthInlinePromptKind(inlinePrompt.kind))) {
            const bool browserMode = inlinePromptUsesFileBrowser(inlinePrompt.kind);
            const int modalW = browserMode ? std::min(windowWidth - 40, 920) : std::min(windowWidth - 40, 700);
            const int modalH = browserMode ? std::min(windowHeight - 40, 560) : 140;
            int modalX = 0;
            int modalY = 0;
            drawModalPanel(modalW, modalH, modalX, modalY);
            drawText(modalX + 12, modalY + 24, inlinePrompt.title, colorText);
            drawText(modalX + 12, modalY + 44, inlinePrompt.hint, colorMutedText);
            if (browserMode) {
                if (fileBrowserEntries.empty()) {
                    refreshFileBrowserEntries();
                }
                const UiRect homeRect {modalX + 12, modalY + 54, 64, 22};
                const UiRect upRect {modalX + 82, modalY + 54, 48, 22};
                const UiRect refreshRect {modalX + 136, modalY + 54, 88, 22};
                const UiRect dirRect {modalX + 230, modalY + 54, modalW - 242, 22};
                drawButton(homeRect, "HOME", false);
                drawButton(upRect, "UP", false);
                drawButton(refreshRect, "REFRESH", false);
                drawFilledRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, colorBackground);
                drawRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, colorGridLine);
                drawText(dirRect.x + 6, dirRect.y + 15, fitText(fileBrowserDirectory.string(), dirRect.width - 10), colorText);
                fileBrowserHits.push_back({homeRect, "nav_home", -1});
                fileBrowserHits.push_back({upRect, "nav_up", -1});
                fileBrowserHits.push_back({refreshRect, "nav_refresh", -1});

                fileBrowserListRect = UiRect {modalX + 12, modalY + 82, modalW - 24, modalH - 156};
                drawFilledRect(fileBrowserListRect.x, fileBrowserListRect.y, fileBrowserListRect.width, fileBrowserListRect.height, colorBackground);
                drawRect(fileBrowserListRect.x, fileBrowserListRect.y, fileBrowserListRect.width, fileBrowserListRect.height, colorGridLine);
                const int rowHeight = 18;
                const int visibleRows = std::max(1, (fileBrowserListRect.height - 4) / rowHeight);
                const int maxScroll = std::max(0, static_cast<int>(fileBrowserEntries.size()) - visibleRows);
                fileBrowserScroll = std::clamp(fileBrowserScroll, 0, maxScroll);
                for (int row = 0; row < visibleRows; ++row) {
                    const int index = fileBrowserScroll + row;
                    if (index >= static_cast<int>(fileBrowserEntries.size())) {
                        break;
                    }
                    const FileBrowserEntry& entry = fileBrowserEntries[static_cast<std::size_t>(index)];
                    const UiRect rowRect {
                        fileBrowserListRect.x + 2,
                        fileBrowserListRect.y + 2 + (row * rowHeight),
                        fileBrowserListRect.width - 4,
                        rowHeight};
                    const bool selected = index == fileBrowserSelected;
                    if (selected) {
                        drawFilledRect(rowRect.x, rowRect.y, rowRect.width, rowRect.height, colorSelection);
                    }
                    drawText(
                        rowRect.x + 6,
                        rowRect.y + 13,
                        std::string(entry.directory ? "[DIR] " : "      ") + entry.name,
                        selected ? colorSelectionText : colorText);
                    fileBrowserHits.push_back({rowRect, "entry", index});
                }
                if (static_cast<int>(fileBrowserEntries.size()) > visibleRows) {
                    const int scrollTrackX = fileBrowserListRect.x + fileBrowserListRect.width - 8;
                    const int scrollTrackY = fileBrowserListRect.y + 2;
                    const int scrollTrackH = fileBrowserListRect.height - 4;
                    drawFilledRect(scrollTrackX, scrollTrackY, 4, scrollTrackH, colorGridLine);
                    const double visibleRatio = static_cast<double>(visibleRows)
                        / static_cast<double>(std::max(1, static_cast<int>(fileBrowserEntries.size())));
                    const int thumbH = std::max(16, static_cast<int>(std::lround(static_cast<double>(scrollTrackH) * visibleRatio)));
                    const int thumbTravel = std::max(0, scrollTrackH - thumbH);
                    const double scrollRatio = static_cast<double>(fileBrowserScroll)
                        / static_cast<double>(std::max(1, maxScroll));
                    const int thumbY = scrollTrackY + static_cast<int>(std::lround(scrollRatio * static_cast<double>(thumbTravel)));
                    drawFilledRect(scrollTrackX, thumbY, 4, thumbH, colorButtonActive);
                }

                const UiRect valueRect {modalX + 12, modalY + modalH - 66, modalW - 24, 24};
                drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, colorBackground);
                drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, colorGridLine);
                drawText(valueRect.x + 8, valueRect.y + 16, inlinePrompt.value + "_", colorText);
                fileBrowserHits.push_back({valueRect, "value", -1});
            } else {
                const UiRect valueRect {modalX + 12, modalY + 52, modalW - 24, 30};
                drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, colorBackground);
                drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, colorGridLine);
                drawText(valueRect.x + 8, valueRect.y + 20, inlinePrompt.value + "_", colorText);
            }
            inlinePromptAcceptButton = UiRect {modalX + modalW - 200, modalY + modalH - 34, 88, 22};
            inlinePromptCancelButton = UiRect {modalX + modalW - 104, modalY + modalH - 34, 88, 22};
            drawButton(inlinePromptAcceptButton, "APPLY", false);
            drawButton(inlinePromptCancelButton, "CANCEL", false);
            inlinePromptButtonsVisible = true;
        }

        if (trackerBackbuffer != 0) {
            XCopyArea(
                display,
                trackerBackbuffer,
                window,
                gc,
                0,
                0,
                static_cast<unsigned int>(windowWidth),
                static_cast<unsigned int>(windowHeight),
                0,
                0);
        }
        XFlush(display);
    };

    auto findSynthParamDef = [&](const std::string& name) -> const SynthParamDef* {
        for (const SynthParamDef& def : synthParamDefs) {
            if (def.name == name) {
                return &def;
            }
        }
        return nullptr;
    };

    auto drawSynthWindow = [&]() {
        if (!synthWindowVisible || synthWindow == 0 || synthGc == nullptr) {
            return;
        }
        ensureSynthBackbuffer();
        synthWindowHits.clear();
        synthKeyboardHits.clear();
        fileBrowserHits.clear();
        fileBrowserListRect = UiRect {};
        const UiThemePalette& theme = themeMode == GuiThemeMode::HighContrast ? highContrastTheme : dosTheme;
        const Drawable synthDrawTarget = synthBackbuffer != 0 ? synthBackbuffer : synthWindow;

        auto drawFilledRect = [&](int x, int y, int w, int h, unsigned long color) {
            XSetForeground(display, synthGc, color);
            XFillRectangle(display, synthDrawTarget, synthGc, x, y, static_cast<unsigned int>(w), static_cast<unsigned int>(h));
        };
        auto drawRect = [&](int x, int y, int w, int h, unsigned long color) {
            XSetForeground(display, synthGc, color);
            XDrawRectangle(display, synthDrawTarget, synthGc, x, y, static_cast<unsigned int>(w), static_cast<unsigned int>(h));
        };
        auto drawText = [&](int x, int y, const std::string& text, unsigned long color) {
            XSetForeground(display, synthGc, color);
            XDrawString(display, synthDrawTarget, synthGc, x, y, text.c_str(), static_cast<int>(text.size()));
        };
        auto drawButton = [&](const UiRect& rect, const std::string& label, bool active) {
            drawFilledRect(rect.x, rect.y, rect.width, rect.height, active ? theme.buttonActive : theme.button);
            drawRect(rect.x, rect.y, rect.width, rect.height, theme.gridLine);
            drawText(rect.x + 5, rect.y + 14, label, active ? theme.buttonLabelActive : theme.buttonLabel);
        };

        drawFilledRect(0, 0, synthWindowWidth, synthWindowHeight, theme.background);
        drawFilledRect(8, 8, synthWindowWidth - 16, synthWindowHeight - 16, theme.panel);
        drawRect(8, 8, synthWindowWidth - 16, synthWindowHeight - 16, theme.gridLine);
        drawFilledRect(9, 9, synthWindowWidth - 18, 32, theme.gridHeader);
        drawText(16, 30, "SYNTH DESIGNER", theme.text);
        drawText(164, 30, "| PATCH WORKBENCH", theme.mutedText);

        const int instrument = clampInstrumentIndex();
        if (instrument < 0 || instrument >= static_cast<int>(session.song().instruments.size())) {
            drawText(16, 52, "No instruments available.", theme.mutedText);
            const UiRect addRect {16, 64, 120, 24};
            drawButton(addRect, "NEW INSTR", false);
            synthWindowHits.push_back({addRect, "inst_new", "", "", "", 0.0});
            if (synthBackbuffer != 0) {
                XCopyArea(
                    display,
                    synthBackbuffer,
                    synthWindow,
                    synthGc,
                    0,
                    0,
                    static_cast<unsigned int>(synthWindowWidth),
                    static_cast<unsigned int>(synthWindowHeight),
                    0,
                    0);
            }
            XFlush(display);
            return;
        }

        const SynthPatch& patch = session.song().instruments[static_cast<std::size_t>(instrument)].patch;
        auto textWidth = [&](const std::string& text) {
            if (uiFont != nullptr) {
                return XTextWidth(uiFont, text.c_str(), static_cast<int>(text.size()));
            }
            return static_cast<int>(text.size()) * 8;
        };
        auto fitText = [&](const std::string& text, int maxWidth) {
            if (maxWidth <= 10 || textWidth(text) <= maxWidth) {
                return text;
            }
            const std::string ellipsis = "...";
            std::string trimmed = text;
            while (!trimmed.empty() && textWidth(trimmed + ellipsis) > maxWidth) {
                trimmed.pop_back();
            }
            return trimmed.empty() ? ellipsis : (trimmed + ellipsis);
        };

        const int panelLeft = 16;
        const int panelRight = std::max(panelLeft + 120, synthWindowWidth - 16);
        const bool compactTop = synthWindowWidth < 980;
        const int navY = 64;
        const int buttonH = 22;
        const int buttonGap = 4;

        std::ostringstream instLabel;
        instLabel << "INST " << instrument << "  " << patch.name;
        drawText(panelLeft, 54, fitText(instLabel.str(), panelRight - panelLeft - 4), theme.text);

        const UiRect closeRect {panelRight - (compactTop ? 62 : 70), navY, compactTop ? 62 : 70, buttonH};
        drawButton(closeRect, "CLOSE", false);
        synthWindowHits.push_back({closeRect, "window_close", "", "", "", 0.0});

        int navX = panelLeft;
        auto addNavButton = [&](const std::string& label, int width, const std::string& kind) {
            if (navX + width > closeRect.x - 6) {
                return false;
            }
            const UiRect rect {navX, navY, width, buttonH};
            drawButton(rect, label, false);
            synthWindowHits.push_back({rect, kind, "", "", "", 0.0});
            navX += width + buttonGap;
            return true;
        };
        (void)addNavButton("<", 30, "inst_prev");
        (void)addNavButton(">", 30, "inst_next");
        (void)addNavButton("NEW", compactTop ? 52 : 64, "inst_new");
        (void)addNavButton("CLONE", compactTop ? 60 : 70, "inst_clone");
        (void)addNavButton("RENAME", compactTop ? 70 : 80, "inst_rename");
        (void)addNavButton("AUD", compactTop ? 52 : 60, "inst_aud");

        const int filesY = navY + buttonH + 6;
        int fileX = panelLeft;
        auto addFileButton = [&](const std::string& label, int width, const std::string& kind) {
            const UiRect rect {fileX, filesY, width, buttonH};
            drawButton(rect, label, false);
            synthWindowHits.push_back({rect, kind, "", "", "", 0.0});
            fileX += width + buttonGap;
            return rect;
        };
        addFileButton(compactTop ? "ADD" : "ADD PATCH", compactTop ? 62 : 106, "patch_import_new");
        addFileButton(compactTop ? "LOAD" : "LOAD PATCH", compactTop ? 66 : 112, "patch_import_replace");
        const UiRect exportRect = addFileButton(compactTop ? "SAVE" : "SAVE PATCH", compactTop ? 66 : 112, "patch_export");

        const std::array<std::pair<const char*, int>, 4> paramPages {{
            {compactTop ? "SND" : "SOUND", 0},
            {"MOD", 1},
            {"FX", 2},
            {"ENV", 3},
        }};
        int pageW = compactTop ? 48 : 70;
        int pageX = panelRight - ((pageW + buttonGap) * static_cast<int>(paramPages.size()));
        if (pageX <= exportRect.x + exportRect.width + 8) {
            pageX = exportRect.x + exportRect.width + 12;
            pageW = compactTop ? 44 : 56;
        }
        for (const auto& page : paramPages) {
            const UiRect pageRect {pageX, filesY, pageW, buttonH};
            const bool active = synthParamPage == page.second;
            drawButton(pageRect, page.first, active);
            synthWindowHits.push_back({pageRect, "param_page", "", "", "", static_cast<double>(page.second)});
            pageX += pageW + buttonGap;
        }

        const int oscTop = filesY + buttonH + 10;
        const int oscRowHeight = 24;
        const int oscToggleWidth = 46;
        const int oscNameX = panelLeft;
        const int oscWaveStartX = panelLeft + 60;
        const int oscToggleX = panelRight - oscToggleWidth;
        const int oscWaveGap = 4;
        drawText(oscNameX, oscTop + 14, "OSC A", theme.text);
        drawText(oscNameX, oscTop + 14 + oscRowHeight, "OSC B", theme.text);
        drawText(oscNameX, oscTop + 14 + (oscRowHeight * 2), "OSC C", theme.text);
        drawText(oscNameX, oscTop + 14 + (oscRowHeight * 3), "OSC D", theme.text);
        const UiRect oscAOnRect {oscToggleX, oscTop, oscToggleWidth, 20};
        const UiRect oscBOnRect {oscToggleX, oscTop + oscRowHeight, oscToggleWidth, 20};
        const UiRect oscCOnRect {oscToggleX, oscTop + (oscRowHeight * 2), oscToggleWidth, 20};
        const UiRect oscDOnRect {oscToggleX, oscTop + (oscRowHeight * 3), oscToggleWidth, 20};
        drawButton(oscAOnRect, patch.oscillatorAEnabled ? "ON" : "OFF", patch.oscillatorAEnabled);
        drawButton(oscBOnRect, patch.oscillatorBEnabled ? "ON" : "OFF", patch.oscillatorBEnabled);
        drawButton(oscCOnRect, patch.oscillatorCEnabled ? "ON" : "OFF", patch.oscillatorCEnabled);
        drawButton(oscDOnRect, patch.oscillatorDEnabled ? "ON" : "OFF", patch.oscillatorDEnabled);
        synthWindowHits.push_back({oscAOnRect, "param_delta", "osc_a_enabled", "", "", patch.oscillatorAEnabled ? -1.0 : 1.0});
        synthWindowHits.push_back({oscBOnRect, "param_delta", "osc_b_enabled", "", "", patch.oscillatorBEnabled ? -1.0 : 1.0});
        synthWindowHits.push_back({oscCOnRect, "param_delta", "osc_c_enabled", "", "", patch.oscillatorCEnabled ? -1.0 : 1.0});
        synthWindowHits.push_back({oscDOnRect, "param_delta", "osc_d_enabled", "", "", patch.oscillatorDEnabled ? -1.0 : 1.0});
        const std::vector<std::pair<std::string, std::string>> waves {
            {"sine", "SIN"},
            {"square", "SQR"},
            {"saw", "SAW"},
            {"triangle", "TRI"},
            {"noise", "NOI"},
        };
        const std::string oscAName = lowerCopy(waveformName(patch.oscillatorA));
        const std::string oscBName = lowerCopy(waveformName(patch.oscillatorB));
        const std::string oscCName = lowerCopy(waveformName(patch.oscillatorC));
        const std::string oscDName = lowerCopy(waveformName(patch.oscillatorD));
        const int waveColumns = static_cast<int>(waves.size());
        const int totalWaveGap = (waveColumns - 1) * oscWaveGap;
        const int waveAvailable = std::max(140, oscToggleX - oscWaveStartX - 8);
        const int waveButtonWidth = std::max(34, std::min(52, (waveAvailable - totalWaveGap) / waveColumns));
        int wx = oscWaveStartX;
        for (const auto& wave : waves) {
            const UiRect aRect {wx, oscTop, waveButtonWidth, 20};
            const UiRect bRect {wx, oscTop + oscRowHeight, waveButtonWidth, 20};
            const UiRect cRect {wx, oscTop + (oscRowHeight * 2), waveButtonWidth, 20};
            const UiRect dRect {wx, oscTop + (oscRowHeight * 3), waveButtonWidth, 20};
            drawButton(aRect, wave.second, wave.first == oscAName);
            drawButton(bRect, wave.second, wave.first == oscBName);
            drawButton(cRect, wave.second, wave.first == oscCName);
            drawButton(dRect, wave.second, wave.first == oscDName);
            synthWindowHits.push_back({aRect, "wave", "", "A", wave.first});
            synthWindowHits.push_back({bRect, "wave", "", "B", wave.first});
            synthWindowHits.push_back({cRect, "wave", "", "C", wave.first});
            synthWindowHits.push_back({dRect, "wave", "", "D", wave.first});
            wx += waveButtonWidth + oscWaveGap;
        }

        const int keyboardTop = oscTop + (oscRowHeight * 4) + 26;
        const int keyboardLeft = 16;
        const int whiteKeyWidth = 18;
        const int whiteKeyHeight = 78;
        const int blackKeyWidth = 12;
        const int blackKeyHeight = 48;
        const int keyboardRightBudget = std::max(keyboardLeft + 7 * whiteKeyWidth, panelRight - 182);
        const int maxVisibleWhiteKeys = std::max(14, (keyboardRightBudget - keyboardLeft) / whiteKeyWidth);
        const int maxVisibleOctaves = std::max(2, std::min(6, maxVisibleWhiteKeys / 7));
        synthKeyboardVisibleOctaves = std::clamp(synthKeyboardVisibleOctaves, 2, maxVisibleOctaves);
        const int whiteKeys = synthKeyboardVisibleOctaves * 7;
        const int keyboardWidth = whiteKeys * whiteKeyWidth;
        const int maxKeyboardStart = std::max(0, 10 - synthKeyboardVisibleOctaves);
        synthKeyboardBaseOctave = std::clamp(synthKeyboardBaseOctave, 0, maxKeyboardStart);
        const int baseMidi = std::clamp(synthKeyboardBaseOctave * 12, 0, 120);
        drawText(16, keyboardTop - 8, "AUDITION KEYS", theme.mutedText);
        const UiRect kbDownRect {keyboardLeft + keyboardWidth + 12, keyboardTop - 18, 26, 18};
        const UiRect kbUpRect {keyboardLeft + keyboardWidth + 42, keyboardTop - 18, 26, 18};
        const UiRect kbCenterRect {keyboardLeft + keyboardWidth + 72, keyboardTop - 18, 48, 18};
        drawButton(kbDownRect, "<", false);
        drawButton(kbUpRect, ">", false);
        drawButton(kbCenterRect, "SYNC", false);
        synthWindowHits.push_back({kbDownRect, "kb_octave_down", "", "", "", 0.0});
        synthWindowHits.push_back({kbUpRect, "kb_octave_up", "", "", "", 0.0});
        synthWindowHits.push_back({kbCenterRect, "kb_octave_sync", "", "", "", 0.0});

        drawFilledRect(keyboardLeft, keyboardTop, keyboardWidth, whiteKeyHeight, theme.button);
        drawRect(keyboardLeft, keyboardTop, keyboardWidth, whiteKeyHeight, theme.gridLine);
        const std::array<int, 7> whiteSemitones = {0, 2, 4, 5, 7, 9, 11};
        const std::array<int, 5> blackSemitones = {1, 3, 6, 8, 10};
        const std::array<int, 5> blackBeforeWhiteIndex = {0, 1, 3, 4, 5};
        for (int octave = 0; octave < synthKeyboardVisibleOctaves; ++octave) {
            const int octaveWhiteOffset = octave * 7;
            const int octaveMidi = baseMidi + (octave * 12);
            for (int degree = 0; degree < 7; ++degree) {
                const int midi = std::clamp(octaveMidi + whiteSemitones[static_cast<std::size_t>(degree)], 0, 127);
                const UiRect keyRect {
                    keyboardLeft + ((octaveWhiteOffset + degree) * whiteKeyWidth),
                    keyboardTop,
                    whiteKeyWidth,
                    whiteKeyHeight};
                const bool active = midi == synthPreviewMidi;
                drawFilledRect(
                    keyRect.x + 1,
                    keyRect.y + 1,
                    keyRect.width - 2,
                    keyRect.height - 2,
                    active ? theme.selection : theme.pianoWhite);
                drawRect(keyRect.x, keyRect.y, keyRect.width, keyRect.height, theme.gridLine);
                synthKeyboardHits.push_back({keyRect, midi, false});
                if (degree == 0) {
                    drawText(keyRect.x + 2, keyRect.y + whiteKeyHeight - 6, "C" + std::to_string(synthKeyboardBaseOctave + octave), theme.mutedText);
                }
            }
            for (std::size_t blackIndex = 0; blackIndex < blackSemitones.size(); ++blackIndex) {
                const int midi = std::clamp(octaveMidi + blackSemitones[blackIndex], 0, 127);
                const int whiteIndexBefore = octaveWhiteOffset + blackBeforeWhiteIndex[blackIndex];
                const int boundaryX = keyboardLeft + ((whiteIndexBefore + 1) * whiteKeyWidth);
                const UiRect keyRect {
                    boundaryX - (blackKeyWidth / 2),
                    keyboardTop,
                    blackKeyWidth,
                    blackKeyHeight};
                const bool active = midi == synthPreviewMidi;
                drawFilledRect(
                    keyRect.x + 1,
                    keyRect.y + 1,
                    keyRect.width - 2,
                    keyRect.height - 2,
                    active ? theme.buttonActive : theme.pianoBlack);
                drawRect(keyRect.x, keyRect.y, keyRect.width, keyRect.height, theme.gridLine);
                synthKeyboardHits.push_back({keyRect, midi, true});
            }
        }
        drawText(keyboardLeft + keyboardWidth + 16, keyboardTop + 12, "Selected", theme.mutedText);
        drawText(keyboardLeft + keyboardWidth + 16, keyboardTop + 30, midiNoteName(synthPreviewMidi), theme.text);
        drawText(keyboardLeft + keyboardWidth + 16, keyboardTop + 46, "Range", theme.mutedText);
        drawText(
            keyboardLeft + keyboardWidth + 16,
            keyboardTop + 62,
            "C" + std::to_string(synthKeyboardBaseOctave) + "..C"
                + std::to_string(synthKeyboardBaseOctave + synthKeyboardVisibleOctaves),
            theme.text);
        drawText(keyboardLeft + keyboardWidth + 16, keyboardTop + 78, "Mouse click/drag = preview", theme.mutedText);
        drawText(keyboardLeft + keyboardWidth + 16, keyboardTop + 94, "Z..M/Q..U + Enter/Space", theme.mutedText);

        const int controlsTop = keyboardTop + whiteKeyHeight + 22;
        const std::array<const char*, 4> pageTitles {{
            "SOUND SHAPING",
            "MODULATION",
            "FX / COLOR",
            "ENVELOPES",
        }};
        drawText(16, controlsTop - 8, pageTitles[static_cast<std::size_t>(std::clamp(synthParamPage, 0, 3))], theme.mutedText);
        std::vector<const SynthParamDef*> visibleDefs;
        visibleDefs.reserve(synthParamDefs.size());
        for (const SynthParamDef& def : synthParamDefs) {
            if (synthParamBelongsToPage(def.name, synthParamPage)) {
                visibleDefs.push_back(&def);
            }
        }
        if (visibleDefs.empty()) {
            for (const SynthParamDef& def : synthParamDefs) {
                visibleDefs.push_back(&def);
            }
        }
        const int columns = 2;
        const int columnWidth = (synthWindowWidth - 34) / columns;
        const int rowHeight = 26;
        for (int index = 0; index < static_cast<int>(visibleDefs.size()); ++index) {
            const SynthParamDef& def = *visibleDefs[static_cast<std::size_t>(index)];
            const int col = index % columns;
            const int row = index / columns;
            const int x = 16 + (col * columnWidth);
            const int y = controlsTop + (row * rowHeight);
            const double value = getSynthParameterValue(patch, def.name);
            std::ostringstream valueText;
            valueText.setf(std::ios::fixed);
            valueText.precision(def.step >= 1.0 ? 0 : 2);
            valueText << value;
            drawText(x, y + 14, def.label, theme.text);
            drawText(x + 64, y + 14, valueText.str(), theme.mutedText);
            const UiRect sliderRect {x + 104, y + 2, 112, 16};
            drawRect(sliderRect.x, sliderRect.y, sliderRect.width, sliderRect.height, theme.gridLine);
            const double ratio = (value - def.minimum) / std::max(0.0001, def.maximum - def.minimum);
            const int filled = std::clamp(static_cast<int>(std::lround(ratio * (sliderRect.width - 2))), 0, sliderRect.width - 2);
            drawFilledRect(sliderRect.x + 1, sliderRect.y + 1, filled, sliderRect.height - 2, theme.buttonActive);
            const UiRect minusRect {x + 220, y, 22, 20};
            const UiRect plusRect {x + 246, y, 22, 20};
            drawButton(minusRect, "-", false);
            drawButton(plusRect, "+", false);
            synthWindowHits.push_back({minusRect, "param_delta", def.name, "", "", -def.step});
            synthWindowHits.push_back({plusRect, "param_delta", def.name, "", "", def.step});
            synthWindowHits.push_back({sliderRect, "param_slider", def.name, "", "", 0.0});
        }

        drawText(16, synthWindowHeight - 18, "Patch files load/save as .arachnopatch. Imported/custom instruments are persisted with project save.", theme.mutedText);
        if (inlinePrompt.active && isSynthInlinePromptKind(inlinePrompt.kind)) {
            const bool browserMode = inlinePromptUsesFileBrowser(inlinePrompt.kind);
            const int modalW = browserMode ? std::min(synthWindowWidth - 40, 760) : std::min(synthWindowWidth - 40, 620);
            const int modalH = browserMode ? std::min(synthWindowHeight - 40, 460) : 136;
            const int modalX = std::max(12, (synthWindowWidth - modalW) / 2);
            const int modalY = std::max(12, (synthWindowHeight - modalH) / 2);
            drawFilledRect(modalX, modalY, modalW, modalH, theme.panel);
            drawRect(modalX, modalY, modalW, modalH, theme.gridLine);
            drawText(modalX + 12, modalY + 24, inlinePrompt.title, theme.text);
            drawText(modalX + 12, modalY + 42, inlinePrompt.hint, theme.mutedText);
            if (browserMode) {
                if (fileBrowserEntries.empty()) {
                    refreshFileBrowserEntries();
                }
                const UiRect homeRect {modalX + 12, modalY + 54, 56, 22};
                const UiRect upRect {modalX + 72, modalY + 54, 46, 22};
                const UiRect refreshRect {modalX + 122, modalY + 54, 72, 22};
                const UiRect dirRect {modalX + 198, modalY + 54, modalW - 210, 22};
                drawButton(homeRect, "HOME", false);
                drawButton(upRect, "UP", false);
                drawButton(refreshRect, "REFRESH", false);
                drawFilledRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, theme.background);
                drawRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, theme.gridLine);
                drawText(dirRect.x + 6, dirRect.y + 15, fitText(fileBrowserDirectory.string(), dirRect.width - 10), theme.text);
                fileBrowserHits.push_back({homeRect, "nav_home", -1});
                fileBrowserHits.push_back({upRect, "nav_up", -1});
                fileBrowserHits.push_back({refreshRect, "nav_refresh", -1});

                fileBrowserListRect = UiRect {modalX + 12, modalY + 82, modalW - 24, modalH - 156};
                drawFilledRect(fileBrowserListRect.x, fileBrowserListRect.y, fileBrowserListRect.width, fileBrowserListRect.height, theme.background);
                drawRect(fileBrowserListRect.x, fileBrowserListRect.y, fileBrowserListRect.width, fileBrowserListRect.height, theme.gridLine);
                const int rowHeight = 18;
                const int visibleRows = std::max(1, (fileBrowserListRect.height - 4) / rowHeight);
                const int maxScroll = std::max(0, static_cast<int>(fileBrowserEntries.size()) - visibleRows);
                fileBrowserScroll = std::clamp(fileBrowserScroll, 0, maxScroll);
                for (int row = 0; row < visibleRows; ++row) {
                    const int index = fileBrowserScroll + row;
                    if (index >= static_cast<int>(fileBrowserEntries.size())) {
                        break;
                    }
                    const FileBrowserEntry& entry = fileBrowserEntries[static_cast<std::size_t>(index)];
                    const UiRect rowRect {
                        fileBrowserListRect.x + 2,
                        fileBrowserListRect.y + 2 + (row * rowHeight),
                        fileBrowserListRect.width - 4,
                        rowHeight};
                    const bool selected = index == fileBrowserSelected;
                    if (selected) {
                        drawFilledRect(rowRect.x, rowRect.y, rowRect.width, rowRect.height, theme.selection);
                    }
                    drawText(
                        rowRect.x + 6,
                        rowRect.y + 13,
                        std::string(entry.directory ? "[DIR] " : "      ") + entry.name,
                        selected ? theme.selectionText : theme.text);
                    fileBrowserHits.push_back({rowRect, "entry", index});
                }
                const UiRect valueRect {modalX + 12, modalY + modalH - 66, modalW - 24, 24};
                drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, theme.background);
                drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, theme.gridLine);
                drawText(valueRect.x + 8, valueRect.y + 16, inlinePrompt.value + "_", theme.text);
                fileBrowserHits.push_back({valueRect, "value", -1});
            } else {
                const UiRect valueRect {modalX + 12, modalY + 50, modalW - 24, 30};
                drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, theme.background);
                drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, theme.gridLine);
                drawText(valueRect.x + 8, valueRect.y + 20, inlinePrompt.value + "_", theme.text);
            }
            synthInlinePromptAcceptButton = UiRect {modalX + modalW - 196, modalY + modalH - 34, 88, 22};
            synthInlinePromptCancelButton = UiRect {modalX + modalW - 102, modalY + modalH - 34, 88, 22};
            drawButton(synthInlinePromptAcceptButton, "APPLY", false);
            drawButton(synthInlinePromptCancelButton, "CANCEL", false);
            synthInlinePromptButtonsVisible = true;
        } else {
            synthInlinePromptButtonsVisible = false;
        }
        if (synthBackbuffer != 0) {
            XCopyArea(
                display,
                synthBackbuffer,
                synthWindow,
                synthGc,
                0,
                0,
                static_cast<unsigned int>(synthWindowWidth),
                static_cast<unsigned int>(synthWindowHeight),
                0,
                0);
        }
        XFlush(display);
    };

    auto triggerSynthKeyboardPointer = [&](int mx, int my, bool allowRetrigger) -> bool {
        for (auto keyIt = synthKeyboardHits.rbegin(); keyIt != synthKeyboardHits.rend(); ++keyIt) {
            if (!keyIt->rect.contains(mx, my)) {
                continue;
            }
            if (allowRetrigger || keyIt->midiNote != synthLastPointerMidi) {
                synthLastPointerMidi = keyIt->midiNote;
                auditionSynthPreviewMidi(keyIt->midiNote);
            }
            return true;
        }
        return false;
    };

    auto handleSynthWindowClick = [&](int mx, int my) {
        for (auto hitIt = synthWindowHits.rbegin(); hitIt != synthWindowHits.rend(); ++hitIt) {
            const SynthWindowHit& hit = *hitIt;
            if (!hit.rect.contains(mx, my)) {
                continue;
            }
            const int instrument = clampInstrumentIndex();
            if (hit.kind == "window_close") {
                setSynthWindowVisible(false);
            } else if (hit.kind == "inst_prev") {
                if (instrument >= 0) {
                    selectInstrument(instrument - 1);
                }
            } else if (hit.kind == "inst_next") {
                if (instrument >= 0) {
                    selectInstrument(instrument + 1);
                }
            } else if (hit.kind == "inst_new") {
                AppActionRequest request;
                request.actionId = "editor.instrument.new";
                request.parameters = {{"name", "NewPatch"}};
                runAction(request);
                selectInstrument(static_cast<int>(session.song().instruments.size()) - 1);
            } else if (hit.kind == "inst_clone") {
                if (instrument >= 0) {
                    AppActionRequest request;
                    request.actionId = "editor.instrument.clone";
                    request.parameters = {{"source_index", std::to_string(instrument)}};
                    runAction(request);
                    selectInstrument(static_cast<int>(session.song().instruments.size()) - 1);
                }
            } else if (hit.kind == "inst_rename") {
                if (instrument >= 0) {
                    beginInlinePrompt(
                        InlinePromptKind::RenameInstrument,
                        "Rename instrument",
                        "New patch name",
                        session.song().instruments[static_cast<std::size_t>(instrument)].patch.name,
                        -1,
                        instrument);
                }
            } else if (hit.kind == "inst_aud") {
                auditionArmedInstrument();
            } else if (hit.kind == "patch_import_new") {
                const AppSessionSnapshot snap = activeSnapshot();
                beginInlinePrompt(
                    InlinePromptKind::ImportPatchAsNewPath,
                    "Import patch as new instrument",
                    "Path to .arachnopatch",
                    defaultPatchPath(snap, patchStemFromName("imported_patch")));
            } else if (hit.kind == "patch_import_replace") {
                if (instrument >= 0) {
                    const AppSessionSnapshot snap = activeSnapshot();
                    beginInlinePrompt(
                        InlinePromptKind::ImportPatchReplacePath,
                        "Load patch into selected instrument",
                        "Path to .arachnopatch",
                        defaultPatchPath(
                            snap,
                            session.song().instruments[static_cast<std::size_t>(instrument)].patch.name),
                        -1,
                        instrument);
                }
            } else if (hit.kind == "patch_export") {
                if (instrument >= 0) {
                    const AppSessionSnapshot snap = activeSnapshot();
                    beginInlinePrompt(
                        InlinePromptKind::ExportPatchPath,
                        "Export selected instrument patch",
                        "Output .arachnopatch path",
                        defaultPatchPath(
                            snap,
                            session.song().instruments[static_cast<std::size_t>(instrument)].patch.name),
                        -1,
                        instrument);
                }
            } else if (hit.kind == "kb_octave_down") {
                synthKeyboardBaseOctave = std::max(0, synthKeyboardBaseOctave - 1);
            } else if (hit.kind == "kb_octave_up") {
                synthKeyboardBaseOctave = std::min(std::max(0, 10 - synthKeyboardVisibleOctaves), synthKeyboardBaseOctave + 1);
            } else if (hit.kind == "kb_octave_sync") {
                synthKeyboardBaseOctave = std::clamp(armedOctave - 1, 0, std::max(0, 10 - synthKeyboardVisibleOctaves));
            } else if (hit.kind == "param_page") {
                synthParamPage = std::clamp(static_cast<int>(std::lround(hit.value)), 0, 3);
            } else if (hit.kind == "wave") {
                if (instrument >= 0) {
                    setSynthWaveform(instrument, hit.oscillator, hit.wave);
                }
            } else if (hit.kind == "key_note") {
                if (instrument >= 0) {
                    const int midi = std::clamp(static_cast<int>(std::lround(hit.value)), 0, 127);
                    synthLastPointerMidi = midi;
                    auditionSynthPreviewMidi(midi);
                }
            } else if (hit.kind == "param_delta") {
                if (instrument >= 0) {
                    const SynthParamDef* def = findSynthParamDef(hit.parameter);
                    if (def != nullptr) {
                        const SynthPatch& patch = session.song().instruments[static_cast<std::size_t>(instrument)].patch;
                        double value = getSynthParameterValue(patch, def->name) + hit.delta;
                        value = std::clamp(value, def->minimum, def->maximum);
                        if (def->step >= 1.0) {
                            value = std::round(value);
                        }
                        setSynthParameter(instrument, def->name, value);
                    }
                }
            } else if (hit.kind == "param_slider") {
                if (instrument >= 0) {
                    const SynthParamDef* def = findSynthParamDef(hit.parameter);
                    if (def != nullptr) {
                        const double ratio = static_cast<double>(mx - hit.rect.x)
                            / static_cast<double>(std::max(1, hit.rect.width));
                        double value = def->minimum + (std::clamp(ratio, 0.0, 1.0) * (def->maximum - def->minimum));
                        if (def->step >= 1.0) {
                            value = std::round(value);
                        }
                        setSynthParameter(instrument, def->name, value);
                    }
                }
            }
            synthWindowNeedsRedraw = true;
            needsRedraw = true;
            break;
        }
    };

    while (running) {
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            const Window eventWindow = event.xany.window;

            if (eventWindow == synthWindow && synthWindow != 0) {
                if (event.type == Expose) {
                    synthWindowNeedsRedraw = true;
                    continue;
                }
                if (event.type == ConfigureNotify) {
                    synthWindowWidth = std::max(synthWindowMinWidth, event.xconfigure.width);
                    synthWindowHeight = std::max(synthWindowMinHeight, event.xconfigure.height);
                    if (event.xconfigure.width < synthWindowMinWidth || event.xconfigure.height < synthWindowMinHeight) {
                        XResizeWindow(
                            display,
                            synthWindow,
                            static_cast<unsigned int>(synthWindowWidth),
                            static_cast<unsigned int>(synthWindowHeight));
                    }
                    ensureSynthBackbuffer();
                    synthWindowNeedsRedraw = true;
                    continue;
                }
                if (event.type == ClientMessage) {
                    if (static_cast<Atom>(event.xclient.data.l[0]) == wmDelete) {
                        setSynthWindowVisible(false);
                    }
                    continue;
                }
                if (event.type == ButtonPress) {
                    if (inlinePrompt.active && isSynthInlinePromptKind(inlinePrompt.kind)) {
                        const bool browserMode = inlinePromptUsesFileBrowser(inlinePrompt.kind);
                        bool handled = false;
                        if (event.xbutton.button == Button1) {
                            if (synthInlinePromptButtonsVisible
                                && synthInlinePromptAcceptButton.contains(event.xbutton.x, event.xbutton.y)) {
                                executeInlinePrompt();
                                handled = true;
                            } else if (synthInlinePromptButtonsVisible
                                && synthInlinePromptCancelButton.contains(event.xbutton.x, event.xbutton.y)) {
                                cancelInlinePrompt();
                                handled = true;
                            } else if (browserMode) {
                                for (const FileBrowserHit& hit : fileBrowserHits) {
                                    if (!hit.rect.contains(event.xbutton.x, event.xbutton.y)) {
                                        continue;
                                    }
                                    if (hit.role == "nav_home") {
                                        const char* home = std::getenv("HOME");
                                        if (home != nullptr && *home != '\0') {
                                            fileBrowserDirectory = std::filesystem::path(home);
                                            refreshFileBrowserEntries();
                                        }
                                    } else if (hit.role == "nav_up") {
                                        const std::filesystem::path parent = fileBrowserDirectory.parent_path();
                                        if (!parent.empty()) {
                                            fileBrowserDirectory = parent;
                                            refreshFileBrowserEntries();
                                        }
                                    } else if (hit.role == "nav_refresh") {
                                        refreshFileBrowserEntries();
                                    } else if (hit.role == "entry"
                                        && hit.index >= 0
                                        && hit.index < static_cast<int>(fileBrowserEntries.size())) {
                                        fileBrowserSelected = hit.index;
                                        const FileBrowserEntry& entry = fileBrowserEntries[static_cast<std::size_t>(hit.index)];
                                        if (entry.directory) {
                                            fileBrowserDirectory = entry.path;
                                            refreshFileBrowserEntries();
                                            inlinePrompt.value = fileBrowserDirectory.string();
                                        } else {
                                            inlinePrompt.value = entry.path.string();
                                        }
                                    }
                                    handled = true;
                                    break;
                                }
                            }
                        } else if ((event.xbutton.button == Button4 || event.xbutton.button == Button5) && browserMode) {
                            if (fileBrowserListRect.contains(event.xbutton.x, event.xbutton.y)) {
                                const int delta = event.xbutton.button == Button4 ? -1 : 1;
                                fileBrowserScroll = std::max(0, fileBrowserScroll + delta);
                                handled = true;
                            }
                        }
                        if (handled || browserMode) {
                            synthWindowNeedsRedraw = true;
                            needsRedraw = true;
                            continue;
                        }
                    }
                    if (event.xbutton.button == Button1) {
                        synthPointerDown = true;
                        synthLastPointerMidi = -1;
                        if (triggerSynthKeyboardPointer(event.xbutton.x, event.xbutton.y, false)) {
                            needsRedraw = true;
                            synthWindowNeedsRedraw = true;
                            continue;
                        }
                        handleSynthWindowClick(event.xbutton.x, event.xbutton.y);
                    }
                    continue;
                }
                if (event.type == ButtonRelease) {
                    if (event.xbutton.button == Button1) {
                        synthPointerDown = false;
                        synthLastPointerMidi = -1;
                    }
                    continue;
                }
                if (event.type == MotionNotify) {
                    if (inlinePrompt.active && isSynthInlinePromptKind(inlinePrompt.kind)) {
                        continue;
                    }
                    if (synthPointerDown && (event.xmotion.state & Button1Mask) != 0) {
                        if (triggerSynthKeyboardPointer(event.xmotion.x, event.xmotion.y, false)) {
                            needsRedraw = true;
                            synthWindowNeedsRedraw = true;
                        }
                    }
                    continue;
                }
                if (event.type == KeyPress) {
                    KeySym key = XLookupKeysym(&event.xkey, 0);
                    const bool ctrlDown = (event.xkey.state & ControlMask) != 0;
                    const bool altDown = (event.xkey.state & Mod1Mask) != 0;
                    char lookupBuffer[16];
                    const int lookupCount = XLookupString(
                        &event.xkey,
                        lookupBuffer,
                        static_cast<int>(sizeof(lookupBuffer)),
                        nullptr,
                        nullptr);
                    if (inlinePrompt.active && isSynthInlinePromptKind(inlinePrompt.kind)) {
                        const bool browserMode = inlinePromptUsesFileBrowser(inlinePrompt.kind);
                        bool consumed = false;
                        if (key == XK_Escape) {
                            cancelInlinePrompt();
                            consumed = true;
                        } else if (key == XK_Return || key == XK_KP_Enter) {
                            if (browserMode
                                && fileBrowserSelected >= 0
                                && fileBrowserSelected < static_cast<int>(fileBrowserEntries.size())
                                && fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)].directory) {
                                fileBrowserDirectory = fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)].path;
                                refreshFileBrowserEntries();
                                inlinePrompt.value = fileBrowserDirectory.string();
                            } else {
                                executeInlinePrompt();
                            }
                            consumed = true;
                        } else if (browserMode && (key == XK_Up || key == XK_KP_Up)) {
                            if (fileBrowserEntries.empty()) {
                                refreshFileBrowserEntries();
                            }
                            if (!fileBrowserEntries.empty()) {
                                const int previous = fileBrowserSelected < 0 ? 0 : fileBrowserSelected;
                                fileBrowserSelected = std::max(0, previous - 1);
                                if (fileBrowserSelected < fileBrowserScroll) {
                                    fileBrowserScroll = fileBrowserSelected;
                                }
                                inlinePrompt.value = fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)].path.string();
                            }
                            consumed = true;
                        } else if (browserMode && (key == XK_Down || key == XK_KP_Down)) {
                            if (fileBrowserEntries.empty()) {
                                refreshFileBrowserEntries();
                            }
                            if (!fileBrowserEntries.empty()) {
                                const int previous = fileBrowserSelected < 0 ? -1 : fileBrowserSelected;
                                fileBrowserSelected = std::min(static_cast<int>(fileBrowserEntries.size()) - 1, previous + 1);
                                const int visibleRows = std::max(1, (fileBrowserListRect.height - 4) / 18);
                                if (fileBrowserSelected >= fileBrowserScroll + visibleRows) {
                                    fileBrowserScroll = std::max(0, fileBrowserSelected - visibleRows + 1);
                                }
                                inlinePrompt.value = fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)].path.string();
                            }
                            consumed = true;
                        } else if (key == XK_BackSpace) {
                            if (!inlinePrompt.value.empty()) {
                                inlinePrompt.value.pop_back();
                            }
                            fileBrowserSelected = -1;
                            consumed = true;
                        } else if (key == XK_Delete) {
                            inlinePrompt.value.clear();
                            fileBrowserSelected = -1;
                            consumed = true;
                        } else if (!ctrlDown && !altDown && lookupCount > 0) {
                            for (int index = 0; index < lookupCount; ++index) {
                                const unsigned char ch = static_cast<unsigned char>(lookupBuffer[index]);
                                if (ch >= 32 && ch <= 126) {
                                    inlinePrompt.value.push_back(static_cast<char>(ch));
                                }
                            }
                            fileBrowserSelected = -1;
                            consumed = true;
                        }
                        if (consumed) {
                            synthWindowNeedsRedraw = true;
                            needsRedraw = true;
                            continue;
                        }
                    }
                    if (key == XK_Escape) {
                        setSynthWindowVisible(false);
                    } else if (key == XK_Left) {
                        selectInstrument(armedInstrument - 1);
                    } else if (key == XK_Right) {
                        selectInstrument(armedInstrument + 1);
                    } else if (key == XK_space || key == XK_Return) {
                        auditionSynthPreviewMidi(synthPreviewMidi);
                    } else {
                        int midiNote = 0;
                        if (!ctrlDown && trackerKeyToMidi(key, armedOctave, midiNote)) {
                            auditionSynthPreviewMidi(midiNote);
                        } else if (ctrlDown) {
                            const int octaveDigit = digitKeyToInt(key);
                            if (octaveDigit >= 0 && octaveDigit <= 8) {
                                setArmedOctave(octaveDigit);
                                synthPreviewMidi = std::clamp((armedOctave * 12) + (synthPreviewMidi % 12), 0, 127);
                                paintNoteMidi = synthPreviewMidi;
                                ensureSynthKeyboardShowsMidi(synthPreviewMidi);
                            }
                        } else if (key == XK_minus || key == XK_KP_Subtract) {
                            setArmedOctave(armedOctave - 1);
                            synthPreviewMidi = std::clamp((armedOctave * 12) + (synthPreviewMidi % 12), 0, 127);
                            paintNoteMidi = synthPreviewMidi;
                            ensureSynthKeyboardShowsMidi(synthPreviewMidi);
                        } else if (key == XK_equal || key == XK_plus || key == XK_KP_Add) {
                            setArmedOctave(armedOctave + 1);
                            synthPreviewMidi = std::clamp((armedOctave * 12) + (synthPreviewMidi % 12), 0, 127);
                            paintNoteMidi = synthPreviewMidi;
                            ensureSynthKeyboardShowsMidi(synthPreviewMidi);
                        }
                    }
                    synthWindowNeedsRedraw = true;
                    needsRedraw = true;
                    continue;
                }
            }

            if (eventWindow != window) {
                continue;
            }

            if (event.type == Expose) {
                needsRedraw = true;
                continue;
            }
            if (event.type == ConfigureNotify) {
                windowWidth = event.xconfigure.width;
                windowHeight = event.xconfigure.height;
                ensureTrackerBackbuffer();
                needsRedraw = true;
                continue;
            }
            if (event.type == ClientMessage) {
                if (static_cast<Atom>(event.xclient.data.l[0]) == wmDelete) {
                    running = false;
                }
                continue;
            }
            if (event.type == ButtonPress) {
                const int mx = event.xbutton.x;
                const int my = event.xbutton.y;
                pointerX = mx;
                pointerY = my;
                const bool ctrlDown = (event.xbutton.state & ControlMask) != 0;
                const bool shiftDown = (event.xbutton.state & ShiftMask) != 0;
                const bool altDown = (event.xbutton.state & Mod1Mask) != 0;

                if (event.xbutton.button == Button1) {
                    if (unsavedPrompt.active) {
                        bool handled = false;
                        for (const auto& choice : unsavedPromptChoices) {
                            if (!choice.first.contains(mx, my)) {
                                continue;
                            }
                            resolveUnsavedPrompt(choice.second);
                            handled = true;
                            break;
                        }
                        if (!handled) {
                            resolveUnsavedPrompt(UnsavedChangesChoice::Cancel);
                        }
                        needsRedraw = true;
                        continue;
                    }
                }
                if (instrumentBrowserActive) {
                    if (event.xbutton.button == Button1) {
                        if (instrumentBrowserAcceptButton.contains(mx, my)) {
                            closeInstrumentBrowser(true);
                        } else if (instrumentBrowserCancelButton.contains(mx, my)) {
                            closeInstrumentBrowser(false);
                        } else {
                            for (const auto& hit : instrumentBrowserHitTargets) {
                                if (!hit.first.contains(mx, my)) {
                                    continue;
                                }
                                instrumentBrowserSelected = hit.second;
                                break;
                            }
                        }
                    } else if (event.xbutton.button == Button4 || event.xbutton.button == Button5) {
                        if (instrumentBrowserListRect.contains(mx, my)) {
                            const int delta = event.xbutton.button == Button4 ? -1 : 1;
                            instrumentBrowserScroll = std::max(0, instrumentBrowserScroll + delta);
                        }
                    }
                    needsRedraw = true;
                    continue;
                }
                if (audioTuningDialogActive) {
                    const PlaybackSnapshot playback = session.playback().snapshot();
                    if (event.xbutton.button == Button1) {
                        bool handled = false;
                        for (const AudioTuningDialogHit& hit : audioTuningDialogHits) {
                            if (!hit.rect.contains(mx, my)) {
                                continue;
                            }
                            if (hit.role == "close") {
                                audioTuningDialogActive = false;
                            } else if (hit.role == "mode") {
                                setAudioPerformanceMode(hit.mode, playback.sampleRate);
                            } else if (hit.role == "delta") {
                                adjustAudioCustomLevel(hit.delta, playback.sampleRate);
                            } else if (hit.role == "reset") {
                                audioCustomLevel = 0;
                                setAudioPerformanceMode(AudioPerformanceMode::Custom, playback.sampleRate);
                            }
                            handled = true;
                            break;
                        }
                        if (!handled && !audioTuningDialogRect.contains(mx, my)) {
                            audioTuningDialogActive = false;
                        }
                    } else if (event.xbutton.button == Button4 || event.xbutton.button == Button5) {
                        if (audioTuningDialogRect.contains(mx, my)) {
                            int delta = event.xbutton.button == Button4 ? -1 : 1;
                            if (shiftDown) {
                                delta *= 10;
                            } else if (ctrlDown) {
                                delta *= 50;
                            }
                            adjustAudioCustomLevel(delta, playback.sampleRate);
                        }
                    }
                    needsRedraw = true;
                    continue;
                }
                if (inlinePrompt.active && !(synthWindowVisible && isSynthInlinePromptKind(inlinePrompt.kind))) {
                    const bool browserMode = inlinePromptUsesFileBrowser(inlinePrompt.kind);
                    bool handled = false;
                    if (event.xbutton.button == Button1) {
                        if (inlinePromptButtonsVisible && inlinePromptAcceptButton.contains(mx, my)) {
                            executeInlinePrompt();
                            handled = true;
                        } else if (inlinePromptButtonsVisible && inlinePromptCancelButton.contains(mx, my)) {
                            cancelInlinePrompt();
                            handled = true;
                        } else if (browserMode) {
                            for (const FileBrowserHit& hit : fileBrowserHits) {
                                if (!hit.rect.contains(mx, my)) {
                                    continue;
                                }
                                if (hit.role == "nav_home") {
                                    const char* home = std::getenv("HOME");
                                    if (home != nullptr && *home != '\0') {
                                        fileBrowserDirectory = std::filesystem::path(home);
                                        refreshFileBrowserEntries();
                                    }
                                } else if (hit.role == "nav_up") {
                                    const std::filesystem::path parent = fileBrowserDirectory.parent_path();
                                    if (!parent.empty()) {
                                        fileBrowserDirectory = parent;
                                        refreshFileBrowserEntries();
                                    }
                                } else if (hit.role == "nav_refresh") {
                                    refreshFileBrowserEntries();
                                } else if (hit.role == "entry"
                                    && hit.index >= 0
                                    && hit.index < static_cast<int>(fileBrowserEntries.size())) {
                                    fileBrowserSelected = hit.index;
                                    const FileBrowserEntry& entry = fileBrowserEntries[static_cast<std::size_t>(hit.index)];
                                    if (entry.directory) {
                                        fileBrowserDirectory = entry.path;
                                        refreshFileBrowserEntries();
                                        inlinePrompt.value = fileBrowserDirectory.string();
                                    } else {
                                        inlinePrompt.value = entry.path.string();
                                    }
                                }
                                handled = true;
                                break;
                            }
                        }
                    } else if ((event.xbutton.button == Button4 || event.xbutton.button == Button5) && browserMode) {
                        if (fileBrowserListRect.contains(mx, my)) {
                            const int delta = event.xbutton.button == Button4 ? -1 : 1;
                            fileBrowserScroll = std::max(0, fileBrowserScroll + delta);
                            handled = true;
                        }
                    }
                    if (handled || browserMode || event.xbutton.button != Button1) {
                        needsRedraw = true;
                        continue;
                    }
                } else if (unsavedPrompt.active) {
                    needsRedraw = true;
                    continue;
                }

                if (std::abs(mx - layout.verticalSplitterX) <= 6) {
                    resizingSidebar = true;
                    continue;
                }
                if (std::abs(my - layout.horizontalSplitterY) <= 6) {
                    resizingTopPanel = true;
                    continue;
                }

                if (event.xbutton.button == Button4 || event.xbutton.button == Button5) {
                    if (shiftDown
                        && mx >= layout.gridLeft
                        && mx < layout.gridLeft + layout.gridWidth
                        && my >= layout.gridTop
                        && my < layout.gridTop + layout.gridHeight) {
                        const int horizontalDelta = event.xbutton.button == Button4 ? -1 : 1;
                        const AppSessionSnapshot snap = activeSnapshot();
                        const int totalTrackCols = std::max(1, snap.editor.activeGrid.trackCount);
                        const int maxTrackStart = std::max(0, totalTrackCols - std::max(1, layout.trackCols));
                        gridTrackStart = std::clamp(gridTrackStart + horizontalDelta, 0, maxTrackStart);
                        needsRedraw = true;
                        continue;
                    }
                    const int delta = event.xbutton.button == Button4 ? -4 : 4;
                    if (!ctrlDown && sidebarViewport.contains(mx, my)) {
                        if (instrumentListRect.contains(mx, my)) {
                            const int listDelta = event.xbutton.button == Button4 ? -1 : 1;
                            scrollInstrumentList(listDelta);
                        } else {
                            const int scrollDelta = event.xbutton.button == Button4 ? -28 : 28;
                            const int sidebarViewportHeight = std::max(1, sidebarViewport.height - 8);
                            const int maxSidebarScroll = std::max(0, sidebarContentHeight - sidebarViewportHeight);
                            sidebarScrollOffset = std::clamp(sidebarScrollOffset + scrollDelta, 0, maxSidebarScroll);
                        }
                        needsRedraw = true;
                        continue;
                    }
                    if (!ctrlDown && instrumentListRect.contains(mx, my)) {
                        const int listDelta = event.xbutton.button == Button4 ? -1 : 1;
                        scrollInstrumentList(listDelta);
                        needsRedraw = true;
                        continue;
                    }
                    if (ctrlDown) {
                        resizePatternRows(activePatternRows + (delta * -2));
                    } else {
                        lockManualScroll();
                        viewStartRow = std::max(0, viewStartRow + delta);
                        refreshSnapshot();
                    }
                    needsRedraw = true;
                    continue;
                }

                if (event.xbutton.button == Button1) {
                    bool consumed = false;
                    const bool pointerInSidebar = sidebarViewport.contains(mx, my);
                    for (const auto& entry : transportButtons) {
                        if (entry.first.contains(mx, my)) {
                            runAction(makeActionRequest(entry.second));
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed) {
                        for (const auto& entry : fileButtons) {
                            if (!entry.first.contains(mx, my)) {
                                continue;
                            }
                            runFileButtonAction(entry.second);
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed) {
                        for (const auto& entry : themeButtons) {
                            if (!entry.first.contains(mx, my)) {
                                continue;
                            }
                            themeMode = entry.second;
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed) {
                        for (const auto& entry : audioPerformanceButtons) {
                            if (!entry.first.contains(mx, my)) {
                                continue;
                            }
                            const PlaybackSnapshot playback = session.playback().snapshot();
                            setAudioPerformanceMode(entry.second, playback.sampleRate);
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed) {
                        if (gridTrackPrevButton.contains(mx, my) || gridTrackNextButton.contains(mx, my)) {
                            const int direction = gridTrackPrevButton.contains(mx, my) ? -1 : 1;
                            const AppSessionSnapshot snap = activeSnapshot();
                            const int totalTrackCols = std::max(1, snap.editor.activeGrid.trackCount);
                            const int maxTrackStart = std::max(0, totalTrackCols - std::max(1, layout.trackCols));
                            gridTrackStart = std::clamp(gridTrackStart + direction, 0, maxTrackStart);
                            consumed = true;
                        }
                    }
                    if (!consumed) {
                        if (patternNewButton.contains(mx, my)) {
                            beginPatternCreatePrompt();
                            consumed = true;
                        } else if (patternCloneButton.contains(mx, my)) {
                            beginPatternClonePrompt();
                            consumed = true;
                        } else if (patternDeleteButton.contains(mx, my)) {
                            (void)deleteActivePattern();
                            consumed = true;
                        }
                    }
                    if (!consumed) {
                        if (patternPrevButton.contains(mx, my) || patternNextButton.contains(mx, my)) {
                            const AppSessionSnapshot snap = activeSnapshot();
                            const int count = static_cast<int>(snap.editor.patterns.size());
                            if (count > 0) {
                                const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
                                const int next = patternPrevButton.contains(mx, my)
                                    ? (current + count - 1) % count
                                    : (current + 1) % count;
                                (void)selectPatternIndex(next, true);
                            }
                            consumed = true;
                        }
                    }
                    if (!consumed) {
                        for (const OrderSlotHit& slot : orderSlotHits) {
                            if (!slot.rect.contains(mx, my)) {
                                continue;
                            }
                            if (slot.valid) {
                                (void)selectPatternIndex(slot.pattern, true);
                            }
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed) {
                        for (const TrackHeaderHit& hit : trackHeaderHits) {
                            if (hit.muteRect.contains(mx, my) || hit.soloRect.contains(mx, my) || hit.selectRect.contains(mx, my)) {
                                const AppSessionSnapshot snap = activeSnapshot();
                                const TrackStripSummary* summary = hit.track < static_cast<int>(snap.editor.tracks.size())
                                    ? &snap.editor.tracks[static_cast<std::size_t>(hit.track)]
                                    : nullptr;
                                if (hit.muteRect.contains(mx, my) && summary != nullptr) {
                                    AppActionRequest mute;
                                    mute.actionId = "editor.track.mute";
                                    mute.parameters = {
                                        {"track", std::to_string(hit.track)},
                                        {"value", summary->muted ? "false" : "true"}};
                                    (void)runAction(mute);
                                } else if (hit.soloRect.contains(mx, my) && summary != nullptr) {
                                    AppActionRequest solo;
                                    solo.actionId = "editor.track.solo";
                                    solo.parameters = {
                                        {"track", std::to_string(hit.track)},
                                        {"value", summary->solo ? "false" : "true"}};
                                    (void)runAction(solo);
                                } else {
                                    const int cursorRow = snap.editor.status.cursorRow;
                                    (void)moveCursor(cursorRow, hit.track);
                                }
                                consumed = true;
                                break;
                            }
                        }
                    }
                    if (!consumed && pointerInSidebar) {
                        for (const auto& octaveHit : octaveHitTargets) {
                            if (!octaveHit.first.contains(mx, my)) {
                                continue;
                            }
                            if (octaveHit.second == -1) {
                                setArmedOctave(armedOctave - 1);
                            } else if (octaveHit.second == 100) {
                                setArmedOctave(armedOctave + 1);
                            } else {
                                setArmedOctave(octaveHit.second);
                            }
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed && pointerInSidebar) {
                        for (const PianoKeyHit& hit : pianoKeyHits) {
                            if (!hit.black || !hit.rect.contains(mx, my)) {
                                continue;
                            }
                            paintNoteMidi = hit.midiNote;
                            const AppSessionSnapshot snap = activeSnapshot();
                            paintNoteAt(snap.editor.status.cursorRow, snap.editor.status.cursorTrack, paintNoteMidi);
                            if (stepAdvance) {
                                ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                                (void)runAction(makeActionRequest("editor.navigation.down"));
                            } else {
                                refreshSnapshot();
                            }
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed && pointerInSidebar) {
                        for (const PianoKeyHit& hit : pianoKeyHits) {
                            if (hit.black || !hit.rect.contains(mx, my)) {
                                continue;
                            }
                            paintNoteMidi = hit.midiNote;
                            const AppSessionSnapshot snap = activeSnapshot();
                            paintNoteAt(snap.editor.status.cursorRow, snap.editor.status.cursorTrack, paintNoteMidi);
                            if (stepAdvance) {
                                ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                                (void)runAction(makeActionRequest("editor.navigation.down"));
                            } else {
                                refreshSnapshot();
                            }
                            consumed = true;
                            break;
                        }
                    }
                    if (!consumed && pointerInSidebar) {
                        for (const TrackMetadataHit& hit : trackMetadataHits) {
                            if (!hit.rect.contains(mx, my)) {
                                continue;
                            }
                            runTrackMetadataAction(hit.role, hit.track);
                            consumed = true;
                            break;
                        }
                    }
                    if (consumed) {
                        needsRedraw = true;
                        continue;
                    }
                    if (pointerInSidebar && patternRowsMinus.contains(mx, my)) {
                        resizePatternRows(activePatternRows - 8);
                        needsRedraw = true;
                        continue;
                    }
                    if (pointerInSidebar && patternRowsPlus.contains(mx, my)) {
                        resizePatternRows(activePatternRows + 8);
                        needsRedraw = true;
                        continue;
                    }
                    if (pointerInSidebar && patternRowsValue.contains(mx, my)) {
                        draggingPatternRows = true;
                        patternResizeAnchorY = my;
                        patternResizeStartRows = activePatternRows;
                        needsRedraw = true;
                        continue;
                    }
                    if (pointerInSidebar && stepAdvanceButton.contains(mx, my)) {
                        stepAdvance = !stepAdvance;
                        needsRedraw = true;
                        continue;
                    }
                    if (pointerInSidebar && followPlaybackButton.contains(mx, my)) {
                        followPlayback = !followPlayback;
                        needsRedraw = true;
                        continue;
                    }
                    if (pointerInSidebar) for (const SongLengthHit& hit : songLengthHits) {
                        if (!hit.rect.contains(mx, my)) {
                            continue;
                        }
                        if (hit.role == "target_down") {
                            targetSongLengthMinutes = std::max(0.1, targetSongLengthMinutes - 0.25);
                        } else if (hit.role == "target_up") {
                            targetSongLengthMinutes = std::min(180.0, targetSongLengthMinutes + 0.25);
                        } else if (hit.role == "target_set") {
                            std::ostringstream initial;
                            initial.setf(std::ios::fixed);
                            initial.precision(2);
                            initial << targetSongLengthMinutes;
                            beginInlinePrompt(
                                InlinePromptKind::SongLengthMinutes,
                                "Set track length (minutes)",
                                "Example: 4.50",
                                initial.str());
                        } else if (hit.role == "build") {
                            buildSongToTargetSeconds(targetSongLengthMinutes * 60.0);
                        } else if (hit.role == "trim") {
                            trimSongToTargetSeconds(targetSongLengthMinutes * 60.0);
                        }
                        needsRedraw = true;
                        consumed = true;
                        break;
                    }
                    if (consumed) {
                        continue;
                    }
                    if (pointerInSidebar) for (const MidiImportSettingHit& hit : midiImportSettingHits) {
                        if (!hit.rect.contains(mx, my)) {
                            continue;
                        }
                        if (hit.role == "rpb_down") {
                            midiImportRowsPerBeat = std::max(1, midiImportRowsPerBeat - 1);
                        } else if (hit.role == "rpb_up") {
                            midiImportRowsPerBeat = std::min(32, midiImportRowsPerBeat + 1);
                        } else if (hit.role == "rows_down") {
                            midiImportPatternRows = std::max(16, midiImportPatternRows - 16);
                        } else if (hit.role == "rows_up") {
                            midiImportPatternRows = std::min(8192, midiImportPatternRows + 16);
                        } else if (hit.role == "split_toggle") {
                            midiImportSplitByTrack = !midiImportSplitByTrack;
                        } else if (hit.role == "import") {
                            runFileButtonAction("import.midi");
                        }
                        needsRedraw = true;
                        consumed = true;
                        break;
                    }
                    if (consumed) {
                        continue;
                    }
                    if (pointerInSidebar) for (const auto& target : instrumentControlHits) {
                        if (!target.first.contains(mx, my)) {
                            continue;
                        }
                        if (target.second == "prev") {
                            const AppSessionSnapshot snap = activeSnapshot();
                            const int count = static_cast<int>(snap.editor.instruments.size());
                            if (count > 0) {
                                armedInstrument = (armedInstrument + count - 1) % count;
                                selectInstrument(armedInstrument);
                            }
                        } else if (target.second == "next") {
                            const AppSessionSnapshot snap = activeSnapshot();
                            const int count = static_cast<int>(snap.editor.instruments.size());
                            if (count > 0) {
                                armedInstrument = (armedInstrument + 1) % count;
                                selectInstrument(armedInstrument);
                            }
                        } else if (target.second == "audition") {
                            auditionArmedInstrument();
                        } else if (target.second == "browse") {
                            openInstrumentBrowser();
                        }
                        needsRedraw = true;
                        consumed = true;
                        break;
                    }
                    if (consumed) {
                        continue;
                    }
                    if (pointerInSidebar) for (const auto& target : instrumentHitTargets) {
                        if (target.first.contains(mx, my)) {
                            selectInstrument(target.second);
                            needsRedraw = true;
                            consumed = true;
                            break;
                        }
                    }
                    if (consumed) {
                        continue;
                    }
                }

                int row = 0;
                int track = 0;
                if (event.xbutton.button == Button1 && gridPositionToCell(mx, my, row, track)) {
                    keyboardSelectionActive = false;
                    if (altDown) {
                        paintNoteAt(row, track, paintNoteMidi);
                        paintingNotes = true;
                        lastPaintRow = row;
                        lastPaintTrack = track;
                        refreshSnapshot();
                        needsRedraw = true;
                        continue;
                    }
                    (void)moveCursor(row, track);
                    draggingSelection = !shiftDown;
                    dragAnchorRow = std::max(0, row);
                    dragAnchorTrack = std::max(0, track);
                    if (shiftDown) {
                        AppActionRequest select;
                        select.actionId = "editor.selection.select";
                        select.parameters = {
                            {"row", std::to_string(std::min(dragAnchorRow, row))},
                            {"track", std::to_string(std::min(dragAnchorTrack, track))},
                            {"rows", "1"},
                            {"tracks", "1"}};
                        runAction(select);
                    }
                    needsRedraw = true;
                    continue;
                }
                if (event.xbutton.button == Button2 && gridPositionToCell(mx, my, row, track)) {
                    (void)moveCursor(row, track);
                    runAction(makeActionRequest("preview.cursor"));
                    needsRedraw = true;
                    continue;
                }
                if (event.xbutton.button == Button3 && gridPositionToCell(mx, my, row, track)) {
                    keyboardSelectionActive = false;
                    (void)moveCursor(row, track);
                    runAction(makeActionRequest("editor.step.clear"));
                    needsRedraw = true;
                    continue;
                }
            }
            if (event.type == ButtonRelease) {
                resizingSidebar = false;
                resizingTopPanel = false;
                draggingSelection = false;
                draggingPatternRows = false;
                paintingNotes = false;
                lastPaintRow = -1;
                lastPaintTrack = -1;
                continue;
            }
            if (event.type == MotionNotify) {
                const int mx = event.xmotion.x;
                const int my = event.xmotion.y;
                pointerX = mx;
                pointerY = my;
                if (resizingSidebar) {
                    const int newSidebar = windowWidth - mx - (layout.margin * 2);
                    sidebarWidthState = std::clamp(newSidebar, 220, std::max(240, windowWidth - 420));
                    needsRedraw = true;
                    continue;
                }
                if (resizingTopPanel) {
                    topPanelHeightState = std::clamp(my - layout.margin + 4, 196, std::max(196, windowHeight - 220));
                    needsRedraw = true;
                    continue;
                }
                if (draggingPatternRows && (event.xmotion.state & Button1Mask) != 0) {
                    const int delta = (patternResizeAnchorY - my) / 5;
                    const int targetRows = patternResizeStartRows + (delta * 4);
                    resizePatternRows(targetRows);
                    needsRedraw = true;
                    continue;
                }
                if (paintingNotes && (event.xmotion.state & Button1Mask) != 0) {
                    int row = 0;
                    int track = 0;
                    if (gridPositionToCell(mx, my, row, track) && (row != lastPaintRow || track != lastPaintTrack)) {
                        paintNoteAt(row, track, paintNoteMidi);
                        lastPaintRow = row;
                        lastPaintTrack = track;
                        refreshSnapshot();
                        needsRedraw = true;
                    }
                    continue;
                }
                if (draggingSelection && (event.xmotion.state & Button1Mask) != 0) {
                    int row = 0;
                    int track = 0;
                    if (gridPositionToCell(mx, my, row, track)) {
                        const int startRow = std::min(dragAnchorRow, row);
                        const int startTrack = std::min(dragAnchorTrack, track);
                        const int rows = std::abs(row - dragAnchorRow) + 1;
                        const int tracks = std::abs(track - dragAnchorTrack) + 1;
                        AppActionRequest select;
                        select.actionId = "editor.selection.select";
                        select.parameters = {
                            {"row", std::to_string(startRow)},
                            {"track", std::to_string(startTrack)},
                            {"rows", std::to_string(rows)},
                            {"tracks", std::to_string(tracks)}};
                        runAction(select);
                        needsRedraw = true;
                    }
                    continue;
                }
                int hovered = -1;
                for (int index = 0; index < static_cast<int>(trackHeaderHits.size()); ++index) {
                    if (trackHeaderHits[static_cast<std::size_t>(index)].selectRect.contains(mx, my)) {
                        hovered = index;
                        break;
                    }
                }
                if (hovered != hoveredTrackHeader) {
                    hoveredTrackHeader = hovered;
                    needsRedraw = true;
                }
            }
            if (event.type != KeyPress) {
                continue;
            }

            KeySym key0 = XLookupKeysym(&event.xkey, 0);
            KeySym key1 = XLookupKeysym(&event.xkey, 1);
            KeySym key = NoSymbol;
            char lookupBuffer[16];
            const int lookupCount = XLookupString(
                &event.xkey,
                lookupBuffer,
                static_cast<int>(sizeof(lookupBuffer)),
                &key,
                nullptr);
            if (key == NoSymbol) {
                key = key0;
            }
            if (key == NoSymbol) {
                key = key1;
            }
            auto keyMatches = [&](KeySym target) {
                return key == target || key0 == target || key1 == target;
            };
            auto resolvedDigit = [&]() -> int {
                int digit = digitKeyToInt(key);
                if (digit < 0) {
                    digit = digitKeyToInt(key0);
                }
                if (digit < 0) {
                    digit = digitKeyToInt(key1);
                }
                return digit;
            };
            const bool ctrlDown = (event.xkey.state & ControlMask) != 0;
            const bool shiftDown = (event.xkey.state & ShiftMask) != 0;
            const bool altDown = (event.xkey.state & Mod1Mask) != 0;

            if (unsavedPrompt.active) {
                const KeySym normalized = normalizeLetterKey(key);
                if (normalized == XK_s) {
                    resolveUnsavedPrompt(UnsavedChangesChoice::Save);
                } else if (normalized == XK_d) {
                    resolveUnsavedPrompt(UnsavedChangesChoice::Discard);
                } else {
                    resolveUnsavedPrompt(UnsavedChangesChoice::Cancel);
                }
                needsRedraw = true;
                continue;
            }

            if (audioTuningDialogActive) {
                const PlaybackSnapshot playback = session.playback().snapshot();
                bool consumed = false;
                if (key == XK_Escape || keyMatches(XK_Return) || keyMatches(XK_KP_Enter)) {
                    audioTuningDialogActive = false;
                    consumed = true;
                } else if (keyMatches(XK_Left) || keyMatches(XK_KP_Left) || key == XK_minus || key == XK_KP_Subtract) {
                    adjustAudioCustomLevel(shiftDown ? -10 : -1, playback.sampleRate);
                    consumed = true;
                } else if (keyMatches(XK_Right) || keyMatches(XK_KP_Right) || key == XK_equal || key == XK_plus || key == XK_KP_Add) {
                    adjustAudioCustomLevel(shiftDown ? 10 : 1, playback.sampleRate);
                    consumed = true;
                } else if (keyMatches(XK_Page_Up)) {
                    adjustAudioCustomLevel(-50, playback.sampleRate);
                    consumed = true;
                } else if (keyMatches(XK_Page_Down)) {
                    adjustAudioCustomLevel(50, playback.sampleRate);
                    consumed = true;
                } else if (ctrlDown && key == XK_F7) {
                    setAudioPerformanceMode(AudioPerformanceMode::Custom, playback.sampleRate);
                    consumed = true;
                } else {
                    const int digit = resolvedDigit();
                    if (digit >= 1 && digit <= 5) {
                        const std::array<AudioPerformanceMode, 5> modes {
                            AudioPerformanceMode::Auto,
                            AudioPerformanceMode::Live,
                            AudioPerformanceMode::Balanced,
                            AudioPerformanceMode::Heavy,
                            AudioPerformanceMode::Custom};
                        setAudioPerformanceMode(modes[static_cast<std::size_t>(digit - 1)], playback.sampleRate);
                        consumed = true;
                    }
                }
                if (consumed) {
                    needsRedraw = true;
                    continue;
                }
            }

            if (inlinePrompt.active && !(synthWindowVisible && isSynthInlinePromptKind(inlinePrompt.kind))) {
                const bool browserMode = inlinePromptUsesFileBrowser(inlinePrompt.kind);
                bool consumed = false;
                if (key == XK_Escape) {
                    cancelInlinePrompt();
                    consumed = true;
                } else if (keyMatches(XK_Return) || keyMatches(XK_KP_Enter)) {
                    if (browserMode
                        && fileBrowserSelected >= 0
                        && fileBrowserSelected < static_cast<int>(fileBrowserEntries.size())
                        && fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)].directory) {
                        fileBrowserDirectory = fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)].path;
                        refreshFileBrowserEntries();
                        inlinePrompt.value = fileBrowserDirectory.string();
                    } else {
                        executeInlinePrompt();
                    }
                    consumed = true;
                } else if (browserMode && (keyMatches(XK_Up) || keyMatches(XK_KP_Up))) {
                    if (fileBrowserEntries.empty()) {
                        refreshFileBrowserEntries();
                    }
                    if (!fileBrowserEntries.empty()) {
                        const int previous = fileBrowserSelected < 0 ? 0 : fileBrowserSelected;
                        fileBrowserSelected = std::max(0, previous - 1);
                        if (fileBrowserSelected < fileBrowserScroll) {
                            fileBrowserScroll = fileBrowserSelected;
                        }
                        const FileBrowserEntry& entry = fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)];
                        inlinePrompt.value = entry.path.string();
                    }
                    consumed = true;
                } else if (browserMode && (keyMatches(XK_Down) || keyMatches(XK_KP_Down))) {
                    if (fileBrowserEntries.empty()) {
                        refreshFileBrowserEntries();
                    }
                    if (!fileBrowserEntries.empty()) {
                        const int previous = fileBrowserSelected < 0 ? -1 : fileBrowserSelected;
                        fileBrowserSelected = std::min(static_cast<int>(fileBrowserEntries.size()) - 1, previous + 1);
                        const int visibleRows = std::max(1, (fileBrowserListRect.height - 4) / 18);
                        if (fileBrowserSelected >= fileBrowserScroll + visibleRows) {
                            fileBrowserScroll = std::max(0, fileBrowserSelected - visibleRows + 1);
                        }
                        const FileBrowserEntry& entry = fileBrowserEntries[static_cast<std::size_t>(fileBrowserSelected)];
                        inlinePrompt.value = entry.path.string();
                    }
                    consumed = true;
                } else if (keyMatches(XK_BackSpace)) {
                    if (!inlinePrompt.value.empty()) {
                        inlinePrompt.value.pop_back();
                    }
                    fileBrowserSelected = -1;
                    consumed = true;
                } else if (key == XK_Delete) {
                    inlinePrompt.value.clear();
                    fileBrowserSelected = -1;
                    consumed = true;
                } else if (!ctrlDown && !altDown && lookupCount > 0) {
                    for (int index = 0; index < lookupCount; ++index) {
                        const unsigned char ch = static_cast<unsigned char>(lookupBuffer[index]);
                        if (ch >= 32 && ch <= 126) {
                            inlinePrompt.value.push_back(static_cast<char>(ch));
                        }
                    }
                    fileBrowserSelected = -1;
                    consumed = true;
                }
                if (consumed) {
                    needsRedraw = true;
                    continue;
                }
                needsRedraw = true;
                continue;
            }

            if (instrumentBrowserActive) {
                bool consumed = false;
                if (key == XK_Escape) {
                    closeInstrumentBrowser(false);
                    consumed = true;
                } else if (keyMatches(XK_Return) || keyMatches(XK_KP_Enter)) {
                    closeInstrumentBrowser(true);
                    consumed = true;
                } else if (ctrlDown && (keyMatches(XK_Up) || keyMatches(XK_KP_Up))) {
                    cycleInstrumentBy(-1);
                    const AppSessionSnapshot snap = activeSnapshot();
                    const std::vector<int> filtered = filteredInstrumentIndices(snap);
                    for (int row = 0; row < static_cast<int>(filtered.size()); ++row) {
                        if (filtered[static_cast<std::size_t>(row)] == armedInstrument) {
                            instrumentBrowserSelected = row;
                            break;
                        }
                    }
                    consumed = true;
                } else if (ctrlDown && (keyMatches(XK_Down) || keyMatches(XK_KP_Down))) {
                    cycleInstrumentBy(1);
                    const AppSessionSnapshot snap = activeSnapshot();
                    const std::vector<int> filtered = filteredInstrumentIndices(snap);
                    for (int row = 0; row < static_cast<int>(filtered.size()); ++row) {
                        if (filtered[static_cast<std::size_t>(row)] == armedInstrument) {
                            instrumentBrowserSelected = row;
                            break;
                        }
                    }
                    consumed = true;
                } else if (keyMatches(XK_Up) || keyMatches(XK_KP_Up)) {
                    instrumentBrowserSelected = std::max(0, instrumentBrowserSelected - 1);
                    consumed = true;
                } else if (keyMatches(XK_Down) || keyMatches(XK_KP_Down)) {
                    const AppSessionSnapshot snap = activeSnapshot();
                    const int count = static_cast<int>(filteredInstrumentIndices(snap).size());
                    instrumentBrowserSelected = std::min(std::max(0, count - 1), instrumentBrowserSelected + 1);
                    consumed = true;
                } else if (keyMatches(XK_Page_Up) || keyMatches(XK_KP_Page_Up)) {
                    instrumentBrowserSelected = std::max(0, instrumentBrowserSelected - 8);
                    consumed = true;
                } else if (keyMatches(XK_Page_Down) || keyMatches(XK_KP_Page_Down)) {
                    const AppSessionSnapshot snap = activeSnapshot();
                    const int count = static_cast<int>(filteredInstrumentIndices(snap).size());
                    instrumentBrowserSelected = std::min(std::max(0, count - 1), instrumentBrowserSelected + 8);
                    consumed = true;
                } else if (keyMatches(XK_BackSpace)) {
                    if (!instrumentBrowserQuery.empty()) {
                        instrumentBrowserQuery.pop_back();
                    }
                    instrumentBrowserSelected = 0;
                    instrumentBrowserScroll = 0;
                    consumed = true;
                } else if (key == XK_Delete) {
                    instrumentBrowserQuery.clear();
                    instrumentBrowserSelected = 0;
                    instrumentBrowserScroll = 0;
                    consumed = true;
                } else if (!ctrlDown && !altDown && lookupCount > 0) {
                    for (int index = 0; index < lookupCount; ++index) {
                        const unsigned char ch = static_cast<unsigned char>(lookupBuffer[index]);
                        if (ch >= 32 && ch <= 126) {
                            instrumentBrowserQuery.push_back(static_cast<char>(ch));
                        }
                    }
                    instrumentBrowserSelected = 0;
                    instrumentBrowserScroll = 0;
                    consumed = true;
                }
                if (consumed) {
                    needsRedraw = true;
                    continue;
                }
            }

            if (key == XK_Escape || (ctrlDown && normalizeLetterKey(key) == XK_q)) {
                running = false;
                continue;
            }

            if (synthWindowVisible && !ctrlDown && !altDown) {
                int midiNote = 0;
                if (trackerKeyToMidi(key, armedOctave, midiNote)) {
                    auditionSynthPreviewMidi(midiNote);
                    needsRedraw = true;
                    continue;
                }
                if (keyMatches(XK_Return) || keyMatches(XK_KP_Enter) || key == XK_space) {
                    auditionSynthPreviewMidi(synthPreviewMidi);
                    needsRedraw = true;
                    continue;
                }
                if (key == XK_minus || key == XK_KP_Subtract) {
                    setArmedOctave(armedOctave - 1);
                    synthPreviewMidi = std::clamp((armedOctave * 12) + (synthPreviewMidi % 12), 0, 127);
                    paintNoteMidi = synthPreviewMidi;
                    ensureSynthKeyboardShowsMidi(synthPreviewMidi);
                    synthWindowNeedsRedraw = true;
                    needsRedraw = true;
                    continue;
                }
                if (key == XK_equal || key == XK_plus || key == XK_KP_Add) {
                    setArmedOctave(armedOctave + 1);
                    synthPreviewMidi = std::clamp((armedOctave * 12) + (synthPreviewMidi % 12), 0, 127);
                    paintNoteMidi = synthPreviewMidi;
                    ensureSynthKeyboardShowsMidi(synthPreviewMidi);
                    synthWindowNeedsRedraw = true;
                    needsRedraw = true;
                    continue;
                }
            }

            if (shiftDown && !ctrlDown && !altDown) {
                const int digit = resolvedDigit();
                if (digit >= 0) {
                    const int mapped = digit == 0 ? 9 : digit - 1;
                    selectInstrument(mapped);
                    needsRedraw = true;
                    continue;
                }
            }

            if (!ctrlDown && !altDown) {
                int midiNote = 0;
                if (trackerKeyToMidi(key, armedOctave, midiNote)) {
                    paintNoteMidi = midiNote;
                    const AppSessionSnapshot snap = activeSnapshot();
                    const int instrumentCount = static_cast<int>(snap.editor.instruments.size());
                    if (instrumentCount > 0) {
                        armedInstrument = std::clamp(armedInstrument, 0, instrumentCount - 1);
                        AppActionRequest inst;
                        inst.actionId = "editor.step.instrument";
                        inst.parameters = {{"index", std::to_string(armedInstrument)}};
                        AppActionResult setInstrument = runAction(inst);
                        if (setInstrument.ok) {
                            AppActionRequest note;
                            note.actionId = "editor.step.note";
                            note.parameters = {
                                {"note", midiNoteName(midiNote)},
                                {"velocity", velocityText(defaultVelocity)}};
                            AppActionResult noteResult = runAction(note);
                            if (noteResult.ok) {
                                (void)runAction(makeActionRequest("preview.cursor"), false);
                                if (stepAdvance) {
                                    ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                                    (void)runAction(makeActionRequest("editor.navigation.down"));
                                }
                            }
                        }
                    }
                    needsRedraw = true;
                    continue;
                }
            }

            if (key == XK_F5) {
                runAction(makeActionRequest("playback.play_song"));
                needsRedraw = true;
                continue;
            }
            if (key == XK_F6) {
                runAction(makeActionRequest("playback.play_pattern"));
                needsRedraw = true;
                continue;
            }
            if (key == XK_F7) {
                const PlaybackSnapshot playback = session.playback().snapshot();
                if (ctrlDown) {
                    audioTuningDialogActive = !audioTuningDialogActive;
                    if (audioTuningDialogActive) {
                        setAudioPerformanceMode(AudioPerformanceMode::Custom, playback.sampleRate);
                    }
                    needsRedraw = true;
                    continue;
                }
                const std::array<AudioPerformanceMode, 4> modes {
                    AudioPerformanceMode::Auto,
                    AudioPerformanceMode::Live,
                    AudioPerformanceMode::Balanced,
                    AudioPerformanceMode::Heavy};
                std::size_t modeIndex = 0;
                bool foundMode = false;
                for (std::size_t index = 0; index < modes.size(); ++index) {
                    if (modes[index] == audioPerformanceMode) {
                        modeIndex = index;
                        foundMode = true;
                        break;
                    }
                }
                if (!foundMode) {
                    modeIndex = shiftDown ? modes.size() - 1 : 0;
                } else if (shiftDown) {
                    modeIndex = (modeIndex + modes.size() - 1) % modes.size();
                } else {
                    modeIndex = (modeIndex + 1) % modes.size();
                }
                setAudioPerformanceMode(modes[modeIndex], playback.sampleRate);
                needsRedraw = true;
                continue;
            }
            if (key == XK_space) {
                if (ctrlDown) {
                    runAction(makeActionRequest("preview.cursor"));
                } else if (shiftDown) {
                    runAction(makeActionRequest("playback.pause"));
                } else {
                    const TransportState state = session.playback().snapshot().state;
                    runAction(makeActionRequest(
                        state == TransportState::Playing ? "playback.stop" : "playback.play_pattern"));
                }
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_z) {
                runAction(makeActionRequest(shiftDown ? "editor.history.redo" : "editor.history.undo"));
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_y) {
                runAction(makeActionRequest("editor.history.redo"));
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_r) {
                runSync("delta_with_fallback");
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_f) {
                runSync("force_snapshot");
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && !shiftDown && normalizeLetterKey(key) == XK_e) {
                runEvents();
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_h) {
                themeMode = themeMode == GuiThemeMode::Dos
                    ? GuiThemeMode::HighContrast
                    : GuiThemeMode::Dos;
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && shiftDown && normalizeLetterKey(key) == XK_i) {
                openInstrumentBrowser();
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && (keyMatches(XK_Up) || keyMatches(XK_KP_Up))) {
                cycleInstrumentBy(-1);
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && (keyMatches(XK_Down) || keyMatches(XK_KP_Down))) {
                cycleInstrumentBy(1);
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_n && !shiftDown) {
                runFileButtonAction("project.new");
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_o) {
                runFileButtonAction("project.open");
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_s && !shiftDown) {
                runFileButtonAction("project.save");
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && shiftDown && normalizeLetterKey(key) == XK_s) {
                const AppSessionSnapshot snap = activeSnapshot();
                beginInlinePrompt(
                    InlinePromptKind::SaveProjectPath,
                    "Save project as",
                    "Path to save project",
                    defaultProjectPath(snap));
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && shiftDown && normalizeLetterKey(key) == XK_e) {
                runFileButtonAction("export.mixdown");
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && shiftDown && normalizeLetterKey(key) == XK_m) {
                midiImportSplitByTrack = !midiImportSplitByTrack;
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && !shiftDown && normalizeLetterKey(key) == XK_m) {
                runFileButtonAction("import.midi");
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && shiftDown && normalizeLetterKey(key) == XK_v) {
                beginTemporalPastePrompt();
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && shiftDown && normalizeLetterKey(key) == XK_p) {
                beginPatternCreatePrompt();
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && altDown && normalizeLetterKey(key) == XK_p) {
                beginPatternClonePrompt();
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && !shiftDown && !altDown && (keyMatches(XK_Delete) || keyMatches(XK_BackSpace))) {
                (void)deleteActivePattern();
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_l) {
                std::ostringstream initial;
                initial.setf(std::ios::fixed);
                initial.precision(2);
                initial << targetSongLengthMinutes;
                beginInlinePrompt(
                    InlinePromptKind::SongLengthMinutes,
                    "Set track length (minutes)",
                    "Example: 4.50",
                    initial.str());
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_i) {
                setSynthWindowVisible(!synthWindowVisible);
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && (keyMatches(XK_Page_Up) || keyMatches(XK_KP_Page_Up))) {
                const AppSessionSnapshot snap = activeSnapshot();
                const int count = static_cast<int>(snap.editor.patterns.size());
                if (count > 0) {
                    const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
                    (void)selectPatternIndex((current + count - 1) % count, true);
                }
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && (keyMatches(XK_Page_Down) || keyMatches(XK_KP_Page_Down))) {
                const AppSessionSnapshot snap = activeSnapshot();
                const int count = static_cast<int>(snap.editor.patterns.size());
                if (count > 0) {
                    const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
                    (void)selectPatternIndex((current + 1) % count, true);
                }
                needsRedraw = true;
                continue;
            }
            if (shiftDown && !ctrlDown && !altDown && normalizeLetterKey(key) == XK_b) {
                buildSongToTargetSeconds(targetSongLengthMinutes * 60.0);
                needsRedraw = true;
                continue;
            }
            if (shiftDown && !ctrlDown && !altDown && normalizeLetterKey(key) == XK_t) {
                trimSongToTargetSeconds(targetSongLengthMinutes * 60.0);
                needsRedraw = true;
                continue;
            }
            if (ctrlDown) {
                const int octaveDigit = digitKeyToInt(key);
                if (octaveDigit >= 0 && octaveDigit <= 8) {
                    setArmedOctave(octaveDigit);
                    if (synthWindowVisible) {
                        synthPreviewMidi = std::clamp((armedOctave * 12) + (synthPreviewMidi % 12), 0, 127);
                        paintNoteMidi = synthPreviewMidi;
                        ensureSynthKeyboardShowsMidi(synthPreviewMidi);
                        synthWindowNeedsRedraw = true;
                    }
                    needsRedraw = true;
                    continue;
                }
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_c) {
                runAction(makeActionRequest("editor.selection.copy"));
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_x) {
                runAction(makeActionRequest("editor.selection.cut"));
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && !shiftDown && normalizeLetterKey(key) == XK_v) {
                runAction(makeActionRequest("editor.selection.paste"));
                needsRedraw = true;
                continue;
            }
            if (ctrlDown && normalizeLetterKey(key) == XK_a) {
                const AppSessionSnapshot snap = activeSnapshot();
                const int rows = std::max(1, activePatternRows);
                const int tracks = std::max(1, snap.editor.activeGrid.trackCount);
                AppActionRequest selectAll;
                selectAll.actionId = "editor.selection.select";
                selectAll.parameters = {
                    {"row", "0"},
                    {"track", "0"},
                    {"rows", std::to_string(rows)},
                    {"tracks", std::to_string(tracks)}};
                runAction(selectAll);
                needsRedraw = true;
                continue;
            }
            if (key == XK_Tab) {
                stepAdvance = !stepAdvance;
                needsRedraw = true;
                continue;
            }
            if (keyMatches(XK_Return) || keyMatches(XK_KP_Enter)) {
                const AppSessionSnapshot snap = activeSnapshot();
                paintNoteAt(snap.editor.status.cursorRow, snap.editor.status.cursorTrack, paintNoteMidi);
                if (stepAdvance) {
                    ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                    (void)runAction(makeActionRequest("editor.navigation.down"));
                } else {
                    refreshSnapshot();
                }
                needsRedraw = true;
                continue;
            }
            if (key == XK_minus || key == XK_KP_Subtract) {
                setArmedOctave(armedOctave - 1);
                needsRedraw = true;
                continue;
            }
            if (key == XK_equal || key == XK_plus || key == XK_KP_Add) {
                setArmedOctave(armedOctave + 1);
                needsRedraw = true;
                continue;
            }
            if (key == XK_comma) {
                defaultVelocity = std::max(0.05f, defaultVelocity - 0.05f);
                needsRedraw = true;
                continue;
            }
            if (key == XK_period) {
                defaultVelocity = std::min(1.0f, defaultVelocity + 0.05f);
                needsRedraw = true;
                continue;
            }
            if (key == XK_bracketleft || key == XK_bracketright) {
                const AppSessionSnapshot snap = activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (count > 0) {
                    if (key == XK_bracketleft) {
                        armedInstrument = (armedInstrument + count - 1) % count;
                    } else {
                        armedInstrument = (armedInstrument + 1) % count;
                    }
                    selectInstrument(armedInstrument);
                }
                needsRedraw = true;
                continue;
            }
            if (key == XK_backslash) {
                auditionArmedInstrument();
                needsRedraw = true;
                continue;
            }
            if (altDown) {
                const int digit = resolvedDigit();
                if (digit >= 0) {
                    selectInstrument(digit);
                    needsRedraw = true;
                    continue;
                }
            }
            if (key == XK_BackSpace || key == XK_Delete) {
                keyboardSelectionActive = false;
                runAction(makeActionRequest("editor.step.clear"));
                if (stepAdvance) {
                    const AppSessionSnapshot snap = activeSnapshot();
                    ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                    (void)runAction(makeActionRequest("editor.navigation.down"));
                }
                needsRedraw = true;
                continue;
            }
            auto applyKeyboardRangeSelection = [&]() {
                const AppSessionSnapshot snap = activeSnapshot();
                const int cursorRow = snap.editor.status.cursorRow;
                const int cursorTrack = snap.editor.status.cursorTrack;
                const int startRow = std::min(keyboardSelectionAnchorRow, cursorRow);
                const int startTrack = std::min(keyboardSelectionAnchorTrack, cursorTrack);
                const int rows = std::abs(cursorRow - keyboardSelectionAnchorRow) + 1;
                const int tracks = std::abs(cursorTrack - keyboardSelectionAnchorTrack) + 1;
                AppActionRequest select;
                select.actionId = "editor.selection.select";
                select.parameters = {
                    {"row", std::to_string(startRow)},
                    {"track", std::to_string(startTrack)},
                    {"rows", std::to_string(rows)},
                    {"tracks", std::to_string(tracks)}};
                runAction(select);
            };
            auto moveByArrow = [&](const std::string& actionId) {
                if (actionId == "editor.navigation.down") {
                    const AppSessionSnapshot snap = activeSnapshot();
                    ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                }
                if (shiftDown) {
                    const AppSessionSnapshot before = activeSnapshot();
                    if (!keyboardSelectionActive) {
                        keyboardSelectionAnchorRow = before.editor.status.cursorRow;
                        keyboardSelectionAnchorTrack = before.editor.status.cursorTrack;
                        keyboardSelectionActive = true;
                    }
                    runAction(makeActionRequest(actionId));
                    applyKeyboardRangeSelection();
                } else {
                    keyboardSelectionActive = false;
                    runAction(makeActionRequest(actionId));
                }
                needsRedraw = true;
            };
            if (keyMatches(XK_Up) || keyMatches(XK_KP_Up)) {
                moveByArrow("editor.navigation.up");
                continue;
            }
            if (keyMatches(XK_Down) || keyMatches(XK_KP_Down)) {
                moveByArrow("editor.navigation.down");
                continue;
            }
            if (keyMatches(XK_Left) || keyMatches(XK_KP_Left)) {
                moveByArrow("editor.navigation.left");
                continue;
            }
            if (keyMatches(XK_Right) || keyMatches(XK_KP_Right)) {
                moveByArrow("editor.navigation.right");
                continue;
            }
            if (key == XK_Home) {
                const AppSessionSnapshot snap = activeSnapshot();
                keyboardSelectionActive = false;
                (void)moveCursor(0, snap.editor.status.cursorTrack);
                viewStartRow = 0;
                needsRedraw = true;
                continue;
            }
            if (key == XK_End) {
                const AppSessionSnapshot snap = activeSnapshot();
                keyboardSelectionActive = false;
                (void)moveCursor(std::max(0, activePatternRows - 1), snap.editor.status.cursorTrack);
                needsRedraw = true;
                continue;
            }
            if (key == XK_Page_Up) {
                lockManualScroll();
                viewStartRow = std::max(0, viewStartRow - requestedRowCount);
                refreshSnapshot();
                needsRedraw = true;
                continue;
            }
            if (key == XK_Page_Down) {
                lockManualScroll();
                viewStartRow = std::max(0, viewStartRow + requestedRowCount);
                refreshSnapshot();
                needsRedraw = true;
                continue;
            }
        }

        {
            const PlaybackSnapshot playback = session.playback().snapshot();
            const bool shouldStreamAudio = playback.state == TransportState::Playing || playback.previewActive;
            bool hasLiveOutput =
#if ARACHNO_HAS_ALSA
                (audioOutputUsesAlsa && audioPcm != nullptr) ||
#endif
                (audioPipe != nullptr);
            if (shouldStreamAudio && (!previousAudioStreamActive || !hasLiveOutput)) {
                if (!hasLiveOutput) {
                    if (!openAudioOutput(playback.sampleRate)) {
                        lastAction.ok = false;
                        lastAction.actionId = "audio.output.open";
                        lastAction.error = "failed to open live audio output (ALSA/aplay)";
                    } else {
                        hasLiveOutput = true;
                    }
                }
            }
            if (!shouldStreamAudio && previousAudioStreamActive) {
                closeAudioOutput();
            }

            if (shouldStreamAudio) {
                tuneRealtimeAudioForLoad(playback.sampleRate);
                const auto nowTick = std::chrono::steady_clock::now();
                const double elapsedSeconds = std::chrono::duration<double>(nowTick - lastPlaybackTick).count();
                int frames = static_cast<int>(std::llround(elapsedSeconds * static_cast<double>(playback.sampleRate)));
                frames = std::clamp(frames, audioFrameMin, audioFrameMax);
                if (audioLeft.size() < static_cast<std::size_t>(frames)) {
                    audioLeft.resize(static_cast<std::size_t>(frames), 0.0f);
                    audioRight.resize(static_cast<std::size_t>(frames), 0.0f);
                }
                std::fill(audioLeft.begin(), audioLeft.begin() + frames, 0.0f);
                std::fill(audioRight.begin(), audioRight.begin() + frames, 0.0f);
                if (session.audioRuntimeHealth().active) {
                    const AudioRuntimeProcessResult processed = session.renderAudioRuntimeBlock(
                        audioLeft.data(),
                        audioRight.data(),
                        frames);
                    if (!processed.ok) {
                        lastAction.ok = false;
                        lastAction.actionId = "audio.runtime.render";
                        lastAction.error = processed.error;
                    }
                } else {
                    session.playback().render(audioLeft.data(), audioRight.data(), frames);
                }
                if (hasLiveOutput && !writeAudioOutput(audioLeft.data(), audioRight.data(), frames)) {
                    lastAction.ok = false;
                    lastAction.actionId = "audio.output.write";
                    lastAction.error = "live audio output stream failed";
                }
                lastPlaybackTick = nowTick;
            } else {
                lastPlaybackTick = std::chrono::steady_clock::now();
            }
            previousAudioStreamActive = shouldStreamAudio;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastRefresh >= std::chrono::milliseconds(80)) {
            refreshSnapshot();
            needsRedraw = true;
            lastRefresh = now;
        }
        if (needsRedraw) {
            draw();
            if (synthWindowVisible) {
                synthWindowNeedsRedraw = true;
            }
            needsRedraw = false;
        }
        if (synthWindowVisible && synthWindowNeedsRedraw) {
            drawSynthWindow();
            synthWindowNeedsRedraw = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    closeAudioOutput();
    releaseTrackerBackbuffer();
    if (uiFont != nullptr) {
        XFreeFont(display, uiFont);
    }
    releaseSynthBackbuffer();
    if (synthGc != nullptr) {
        XFreeGC(display, synthGc);
    }
    if (synthWindow != 0) {
        XDestroyWindow(display, synthWindow);
    }
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}

} // namespace arachno
