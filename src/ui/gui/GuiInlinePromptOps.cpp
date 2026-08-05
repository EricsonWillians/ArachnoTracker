#include "ui/gui/GuiInlinePromptOps.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "PatchIO.h"
#include "ui/gui/GuiFileBrowserOps.h"
#include "GuiInput.h"

namespace arachno {

namespace {

std::string pathLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string sanitizePatternToken(std::string token, const std::string& fallback) {
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
}

} // namespace

void executeInlinePromptOps(const GuiInlinePromptExecutionContext& context) {
    if (!context.inlinePrompt.active) {
        return;
    }
    std::string value = trimCopy(context.inlinePrompt.value);
    if (inlinePromptKindUsesFileBrowser(context.inlinePrompt.kind) && !value.empty()) {
        std::error_code ec;
        const std::filesystem::path candidate(value);
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
            if (context.inlinePrompt.kind == InlinePromptKind::SaveProjectPath) {
                value = (candidate / "project.arachno").string();
            } else if (context.inlinePrompt.kind == InlinePromptKind::ExportMixdownPath) {
                value = (candidate / "mixdown.wav").string();
            } else if (context.inlinePrompt.kind == InlinePromptKind::OpenProjectPath
                || context.inlinePrompt.kind == InlinePromptKind::ImportMidiPath
                || context.inlinePrompt.kind == InlinePromptKind::ImportPatchAsNewPath
                || context.inlinePrompt.kind == InlinePromptKind::ImportPatchReplacePath
                || context.inlinePrompt.kind == InlinePromptKind::ExportPatchPath) {
                context.lastAction.ok = false;
                context.lastAction.error = "select a file, not a folder";
                context.lastAction.actionId = "file.browser.select";
                return;
            } else if (context.inlinePrompt.kind == InlinePromptKind::ImportPatchReplaceAllPath) {
                value = candidate.string();
                context.inlinePrompt.value = value;
            }
            if (context.inlinePrompt.kind != InlinePromptKind::ImportPatchReplaceAllPath) {
                context.inlinePrompt.value = value;
            }
        }
    }
    if (context.inlinePrompt.kind == InlinePromptKind::OpenProjectPath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "project.open";
            context.lastAction.error = "project path is empty";
            context.clearInlinePrompt();
            return;
        }
        AppActionRequest request;
        const std::string ext = pathLower(std::filesystem::path(value).extension().string());
        if (ext == ".mid" || ext == ".midi") {
            request.actionId = "import.midi";
            request.path = value;
            request.parameters = {
                {"path", value},
                {"rows_per_beat", std::to_string(std::clamp(context.midiImportRowsPerBeat, 1, 32))},
                {"pattern_rows", std::to_string(std::clamp(context.midiImportPatternRows, 16, 8192))},
                {"split_by_track", context.midiImportSplitByTrack ? "true" : "false"},
                {"split_by_program", "true"},
                {"preserve_tempo_map", "true"}};
        } else {
            request.actionId = "project.open";
            request.path = value;
            request.parameters = {{"path", value}};
        }
        context.clearInlinePrompt();
        context.runLifecycleAction(request);
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::SaveProjectPath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "project.save_as";
            context.lastAction.error = "save path is empty";
            context.clearInlinePrompt();
            return;
        }
        AppActionRequest saveAs;
        saveAs.actionId = "project.save_as";
        saveAs.path = value;
        saveAs.parameters = {{"path", value}};
        context.clearInlinePrompt();
        context.runAction(saveAs);
        if (context.lastAction.ok) {
            context.handleDeferredPostSave();
        }
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::ExportMixdownPath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "export.mixdown";
            context.lastAction.error = "export path is empty";
            context.clearInlinePrompt();
            return;
        }
        AppActionRequest exportMix;
        const std::string ext = pathLower(std::filesystem::path(value).extension().string());
        exportMix.actionId = (ext == ".mid" || ext == ".midi") ? "export.midi" : "export.mixdown";
        exportMix.path = value;
        exportMix.parameters = {{"path", value}};
        context.clearInlinePrompt();
        context.runAction(exportMix);
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::ImportMidiPath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "import.midi";
            context.lastAction.error = "MIDI path is empty";
            context.clearInlinePrompt();
            return;
        }
        AppActionRequest importMidi;
        importMidi.actionId = "import.midi";
        importMidi.path = value;
        importMidi.parameters = {
            {"path", value},
            {"rows_per_beat", std::to_string(std::clamp(context.midiImportRowsPerBeat, 1, 32))},
            {"pattern_rows", std::to_string(std::clamp(context.midiImportPatternRows, 16, 8192))},
            {"split_by_track", context.midiImportSplitByTrack ? "true" : "false"},
            {"split_by_program", "true"},
            {"preserve_tempo_map", "true"}};
        context.clearInlinePrompt();
        context.runAction(importMidi);
        if (context.lastAction.ok
            && context.lastAction.hasMidiImportReport
            && !context.lastAction.midiImportReport.trackMappings.empty()) {
            context.selectInstrument(context.lastAction.midiImportReport.trackMappings.front().instrumentIndex);
        }
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::ImportPatchAsNewPath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "script.import";
            context.lastAction.error = "patch path is empty";
            context.clearInlinePrompt();
            return;
        }
        AppActionRequest importPatch;
        importPatch.actionId = "script.import";
        importPatch.path = value;
        importPatch.parameters = {{"path", value}};
        context.clearInlinePrompt();
        context.runAction(importPatch);
        if (context.lastAction.ok
            && context.lastAction.hasScriptImportResult
            && context.lastAction.scriptImportResult.importedInstrument >= 0) {
            context.selectInstrument(context.lastAction.scriptImportResult.importedInstrument);
        }
        context.synthWindowNeedsRedraw = true;
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::ImportPatchReplacePath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.instrument.patch_replace";
            context.lastAction.error = "patch path is empty";
            context.clearInlinePrompt();
            return;
        }
        const int instrument = context.inlinePrompt.targetInstrument;
        context.clearInlinePrompt();
        try {
            const SynthPatch patch = loadPatch(value);
            const bool applied = context.applyPatchToInstrument(instrument, patch, false);
            if (applied) {
                context.lastAction.ok = true;
                context.lastAction.actionId = "editor.instrument.patch_replace";
                context.lastAction.message = "Loaded patch into instrument";
                context.lastAction.error.clear();
            } else if (context.lastAction.ok) {
                context.lastAction.ok = false;
                context.lastAction.actionId = "editor.instrument.patch_replace";
                context.lastAction.error = "failed to apply patch";
            }
        } catch (const std::exception& error) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.instrument.patch_replace";
            context.lastAction.error = error.what();
        }
        context.synthWindowNeedsRedraw = true;
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::ImportPatchReplaceAllPath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.instrument.patch_replace_all";
            context.lastAction.error = "patch source path is empty";
            context.clearInlinePrompt();
            return;
        }
        context.clearInlinePrompt();
        try {
            const int instrumentCount = static_cast<int>(context.session.song().instruments.size());
            if (instrumentCount <= 0) {
                throw std::runtime_error("no instruments available in the project");
            }
            std::vector<std::filesystem::path> patchPaths;
            std::error_code ec;
            const std::filesystem::path source(value);
            if (std::filesystem::exists(source, ec) && std::filesystem::is_directory(source, ec)) {
                // Recurse so category subfolders (e.g. patches/factory/bass) are included.
                for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(source, ec)) {
                    if (ec || !entry.is_regular_file(ec)) {
                        continue;
                    }
                    if (pathLower(entry.path().extension().string()) == ".arachnopatch") {
                        patchPaths.push_back(entry.path());
                    }
                }
                std::sort(patchPaths.begin(), patchPaths.end(), [&source](const auto& a, const auto& b) {
                    std::error_code relEc;
                    const std::string relA = std::filesystem::relative(a, source, relEc).string();
                    const std::string relB = std::filesystem::relative(b, source, relEc).string();
                    return relA < relB;
                });
                if (patchPaths.empty()) {
                    throw std::runtime_error("folder has no .arachnopatch files");
                }
            } else {
                patchPaths.push_back(source);
            }

            int replaced = 0;
            int failed = 0;
            if (patchPaths.size() == 1) {
                const SynthPatch patch = loadPatch(patchPaths.front().string());
                for (int instrument = 0; instrument < instrumentCount; ++instrument) {
                    if (context.applyPatchToInstrument(instrument, patch, false)) {
                        ++replaced;
                    } else {
                        ++failed;
                    }
                }
            } else {
                const int applyCount = std::min(instrumentCount, static_cast<int>(patchPaths.size()));
                for (int instrument = 0; instrument < applyCount; ++instrument) {
                    try {
                        const SynthPatch patch =
                            loadPatch(patchPaths[static_cast<std::size_t>(instrument)].string());
                        if (context.applyPatchToInstrument(instrument, patch, false)) {
                            ++replaced;
                        } else {
                            ++failed;
                        }
                    } catch (const std::exception&) {
                        ++failed;
                    }
                }
            }

            context.lastAction.ok = replaced > 0 && failed == 0;
            context.lastAction.actionId = "editor.instrument.patch_replace_all";
            std::ostringstream message;
            if (patchPaths.size() == 1) {
                message << "Loaded one patch into " << replaced << " instrument(s)";
            } else {
                message << "Loaded " << replaced << " patch(es) into current instruments";
                if (static_cast<int>(patchPaths.size()) > instrumentCount) {
                    message << " (extra patches ignored)";
                } else if (static_cast<int>(patchPaths.size()) < instrumentCount) {
                    message << " (remaining instruments unchanged)";
                }
            }
            if (failed > 0) {
                message << ", " << failed << " failed";
                context.lastAction.ok = false;
            }
            context.lastAction.message = message.str();
            context.lastAction.error = context.lastAction.ok ? std::string {} : "some patch loads failed";
        } catch (const std::exception& error) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.instrument.patch_replace_all";
            context.lastAction.error = error.what();
        }
        context.synthWindowNeedsRedraw = true;
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::ExportPatchPath) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.instrument.patch_export";
            context.lastAction.error = "export path is empty";
            context.clearInlinePrompt();
            return;
        }
        const int instrument = context.inlinePrompt.targetInstrument;
        context.clearInlinePrompt();
        try {
            const int count = static_cast<int>(context.session.song().instruments.size());
            if (instrument < 0 || instrument >= count) {
                throw std::out_of_range("instrument index is out of range");
            }
            savePatch(context.session.song().instruments[static_cast<std::size_t>(instrument)].patch, value);
            context.lastAction.ok = true;
            context.lastAction.actionId = "editor.instrument.patch_export";
            context.lastAction.message = "Exported patch";
            context.lastAction.error.clear();
        } catch (const std::exception& error) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.instrument.patch_export";
            context.lastAction.error = error.what();
        }
        context.synthWindowNeedsRedraw = true;
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::RenameInstrument) {
        const int instrument = context.inlinePrompt.targetInstrument;
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.instrument.rename";
            context.lastAction.error = "instrument name cannot be empty";
            context.clearInlinePrompt();
            return;
        }
        AppActionRequest rename;
        rename.actionId = "editor.instrument.rename";
        rename.parameters = {
            {"instrument", std::to_string(instrument)},
            {"name", value}};
        context.clearInlinePrompt();
        context.runAction(rename);
        context.synthWindowNeedsRedraw = true;
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::RenameTrack) {
        const AppSessionSnapshot snap = context.activeSnapshot();
        const int track = std::clamp(
            context.inlinePrompt.targetTrack,
            0,
            std::max(0, static_cast<int>(snap.editor.tracks.size()) - 1));
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.track.rename";
            context.lastAction.error = "track name cannot be empty";
            context.clearInlinePrompt();
            return;
        }
        AppActionRequest rename;
        rename.actionId = "editor.track.rename";
        rename.parameters = {{"track", std::to_string(track)}, {"name", value}};
        context.clearInlinePrompt();
        context.runAction(rename);
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::SongLengthMinutes) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "arrangement.length_target";
            context.lastAction.error = "target minutes cannot be empty";
            context.clearInlinePrompt();
            return;
        }
        std::istringstream in(value);
        double minutes = 0.0;
        in >> minutes;
        if (!in || minutes <= 0.0) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "arrangement.length_target";
            context.lastAction.error = "target minutes must be a positive number";
            context.clearInlinePrompt();
            return;
        }
        context.targetSongLengthMinutes = std::clamp(minutes, 0.1, 180.0);
        context.lastAction.ok = true;
        context.lastAction.actionId = "arrangement.length_target";
        context.lastAction.message = "target length updated";
        context.lastAction.error.clear();
        context.clearInlinePrompt();
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::PatternCreateSpec) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.pattern.new";
            context.lastAction.error = "pattern spec is empty";
            context.clearInlinePrompt();
            return;
        }
        std::istringstream in(value);
        std::string rawName;
        int rows = std::clamp(context.activePatternRows, 8, 8192);
        int tracks = -1;
        if (!(in >> rawName)) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.pattern.new";
            context.lastAction.error = "pattern name is required";
            context.clearInlinePrompt();
            return;
        }
        if (in >> rows) {
            rows = std::clamp(rows, 8, 8192);
        } else {
            in.clear();
        }
        if (in >> tracks) {
            if (tracks <= 0) {
                context.lastAction.ok = false;
                context.lastAction.actionId = "editor.pattern.new";
                context.lastAction.error = "tracks must be greater than zero";
                context.clearInlinePrompt();
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
        context.clearInlinePrompt();
        context.runAction(create);
        if (context.lastAction.ok) {
            context.keyboardSelectionActive = false;
            context.viewStartRow = 0;
        }
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::PatternCloneName) {
        AppActionRequest clone;
        clone.actionId = "editor.pattern.clone";
        const std::string name = sanitizePatternToken(value, "");
        if (!name.empty()) {
            clone.parameters = {{"name", name}};
        }
        context.clearInlinePrompt();
        context.runAction(clone);
        if (context.lastAction.ok) {
            context.keyboardSelectionActive = false;
            context.viewStartRow = 0;
        }
        return;
    }
    if (context.inlinePrompt.kind == InlinePromptKind::TemporalPasteSpec) {
        if (value.empty()) {
            context.lastAction.ok = false;
            context.lastAction.actionId = "editor.selection.paste_temporal";
            context.lastAction.error = "temporal paste spec is empty";
            context.clearInlinePrompt();
            return;
        }
        context.clearInlinePrompt();
        (void)context.executeTemporalPasteSpec(value);
        return;
    }
}

} // namespace arachno
