#include "ui/gui/GuiInstrumentBrowserOps.h"

#include <array>
#include <algorithm>
#include <string>
#include <utility>

#include <cctype>

#include "GuiInput.h"

namespace arachno {

namespace {

constexpr int kStandardMidiVirtualBase = -1000;

struct StandardMidiEntry {
    int virtualIndex;
    const char* name;
    const char* category;
};

const std::array<StandardMidiEntry, 16> kStandardMidiEntries {
    StandardMidiEntry {kStandardMidiVirtualBase + 0, "Acoustic Grand Piano", "Standard MIDI / Piano"},
    {kStandardMidiVirtualBase + 1, "Electric Piano 1", "Standard MIDI / Piano"},
    {kStandardMidiVirtualBase + 2, "Electric Guitar (jazz)", "Standard MIDI / Guitar"},
    {kStandardMidiVirtualBase + 3, "Acoustic Bass", "Standard MIDI / Bass"},
    {kStandardMidiVirtualBase + 4, "Synth Bass 1", "Standard MIDI / Bass"},
    {kStandardMidiVirtualBase + 5, "Violin", "Standard MIDI / Strings"},
    {kStandardMidiVirtualBase + 6, "String Ensemble 1", "Standard MIDI / Strings"},
    {kStandardMidiVirtualBase + 7, "Trumpet", "Standard MIDI / Brass"},
    {kStandardMidiVirtualBase + 8, "Soprano Sax", "Standard MIDI / Brass"},
    {kStandardMidiVirtualBase + 9, "Flute", "Standard MIDI / Reeds"},
    {kStandardMidiVirtualBase + 10, "Lead 1 (square)", "Standard MIDI / Leads"},
    {kStandardMidiVirtualBase + 11, "Pad 1 (new age)", "Standard MIDI / Pads"},
    {kStandardMidiVirtualBase + 12, "FX 1 (rain)", "Standard MIDI / Effects"},
    {kStandardMidiVirtualBase + 13, "Taiko Drum", "Standard MIDI / Drums"},
    {kStandardMidiVirtualBase + 14, "Reverse Cymbal", "Standard MIDI / Drums"},
    {kStandardMidiVirtualBase + 15, "Choir Aahs", "Standard MIDI / Vocals"}};

std::string canonicalBrowserName(const std::string& name) {
    const std::string lowered = lowerCopy(trimCopy(name));
    std::string value;
    value.reserve(lowered.size());
    for (char ch : lowered) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || std::isspace(static_cast<unsigned char>(ch))) {
            value.push_back(ch);
        }
    }
    return value;
}

std::string canonicalBrowserNameCompact(const std::string& name) {
    std::string canonical = canonicalBrowserName(name);
    canonical.erase(std::remove_if(canonical.begin(), canonical.end(), [](unsigned char ch) {
        return std::isspace(ch);
    }),
    canonical.end());
    return canonical;
}

int matchedProjectInstrumentIndex(const AppSessionSnapshot& snapshot, const std::string& standardName) {
    const std::string normalized = canonicalBrowserName(standardName);
    const std::string normalizedCompact = canonicalBrowserNameCompact(standardName);
    if (normalized.empty()) {
        return -1;
    }
    for (std::size_t index = 0; index < snapshot.editor.instruments.size(); ++index) {
        const InstrumentSummary& instrument = snapshot.editor.instruments[index];
        const std::string canonicalName = canonicalBrowserName(instrument.name);
        if (canonicalName == normalized) {
            return static_cast<int>(index);
        }
        if (canonicalBrowserNameCompact(canonicalName) == normalizedCompact) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

struct IndexedMatch {
    std::string sortKey;
    int instrumentIndex;
};

} // namespace

std::string instrumentCategoryFromName(const std::string& name) {
    const std::string lowered = lowerCopy(name);
    auto token = [&](const std::string& value) {
        return lowered.find(value) != std::string::npos;
    };
    if (token("kick") || token("snare") || token("hat") || token("tom") || token("crash") || token("ride")) {
        return "Percussion";
    }
    if (token("bass") || token("sub") || token("ebm") || token("reese")) {
        return "Bass";
    }
    if (token("lead") || token("solo") || token("arp") || token("pluck")) {
        return "Lead";
    }
    if (token("pad") || token("drone") || token("stabs") || token("soundscape") || token("choir") || token("bell")) {
        return "Pads";
    }
    if (token("clap") || token("shaker") || token("noise")) {
        return "FX";
    }
    if (token("guitar")) {
        return "Guitar";
    }
    if (token("string") || token("violin") || token("viola") || token("cello") || token("orchestra")) {
        return "Strings";
    }
    if (token("brass") || token("trumpet") || token("sax") || token("horn")) {
        return "Brass";
    }
    return "MIDI";
}

std::vector<int> filteredInstrumentIndicesForQuery(
    const AppSessionSnapshot& snapshot,
    const std::string& query) {
    std::vector<IndexedMatch> entries;
    const std::string normalizedQuery = lowerCopy(trimCopy(query));
    const std::size_t expectedSize = snapshot.editor.instruments.size() + kStandardMidiEntries.size();
    entries.reserve(expectedSize);
    for (std::size_t index = 0; index < snapshot.editor.instruments.size(); ++index) {
        const InstrumentSummary& instrument = snapshot.editor.instruments[index];
        const std::string indexToken = std::to_string(static_cast<int>(index));
        const bool match = normalizedQuery.empty()
            || lowerCopy(instrument.name).find(normalizedQuery) != std::string::npos
            || indexToken.find(normalizedQuery) != std::string::npos;
        if (match) {
            entries.push_back({"project|"
                               + instrumentCategoryFromName(instrument.name) + "|" + lowerCopy(instrument.name),
                               static_cast<int>(index)});
        }
    }
    for (const StandardMidiEntry& entry : kStandardMidiEntries) {
        const std::string lowerName = lowerCopy(entry.name);
        if (matchedProjectInstrumentIndex(snapshot, entry.name) >= 0) {
            continue;
        }
        const std::string category = lowerCopy(entry.category);
        const bool match = normalizedQuery.empty()
            || lowerName.find(normalizedQuery) != std::string::npos
            || category.find(normalizedQuery) != std::string::npos;
        if (match) {
            entries.push_back({"standard|" + category + "|" + lowerName, entry.virtualIndex});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const IndexedMatch& left, const IndexedMatch& right) {
        return left.sortKey < right.sortKey;
    });
    std::vector<int> indices;
    indices.reserve(entries.size());
    for (const IndexedMatch& entry : entries) {
        indices.push_back(entry.instrumentIndex);
    }
    return indices;
}

bool isStandardMidiBrowserIndex(int instrumentIndex) {
    return instrumentIndex >= kStandardMidiVirtualBase
        && instrumentIndex < static_cast<int>(kStandardMidiVirtualBase + kStandardMidiEntries.size());
}

std::string standardMidiBrowserName(int instrumentIndex) {
    const int offset = instrumentIndex - kStandardMidiVirtualBase;
    if (offset < 0 || offset >= static_cast<int>(kStandardMidiEntries.size())) {
        return "";
    }
    return kStandardMidiEntries[static_cast<std::size_t>(offset)].name;
}

std::string standardMidiBrowserCategory(int instrumentIndex) {
    const int offset = instrumentIndex - kStandardMidiVirtualBase;
    if (offset < 0 || offset >= static_cast<int>(kStandardMidiEntries.size())) {
        return "";
    }
    return kStandardMidiEntries[static_cast<std::size_t>(offset)].category;
}

int standardMidiBrowserCount() {
    return static_cast<int>(kStandardMidiEntries.size());
}

void openInstrumentBrowser(
    const GuiInstrumentBrowserOpenContext& context,
    const AppSessionSnapshot& snapshot) {
    context.audioTuningDialogActive = false;
    context.instrumentBrowserActive = true;
    context.instrumentBrowserQuery.clear();
    context.instrumentBrowserScroll = 0;
    context.instrumentBrowserSelected = 0;
    const std::vector<int> indices = context.filteredInstrumentIndices(snapshot);
    for (int row = 0; row < static_cast<int>(indices.size()); ++row) {
        if (indices[static_cast<std::size_t>(row)] == context.armedInstrument) {
            context.instrumentBrowserSelected = row;
            break;
        }
    }
}

void closeInstrumentBrowser(const GuiInstrumentBrowserCloseContext& context, bool applySelection) {
    if (applySelection) {
        const AppSessionSnapshot snap = context.activeSnapshot();
        const std::vector<int> indices = context.filteredInstrumentIndices(snap);
        if (!indices.empty()) {
            const int row = std::clamp(
                context.instrumentBrowserSelected,
                0,
                static_cast<int>(indices.size()) - 1);
            const int instrumentIndex = indices[static_cast<std::size_t>(row)];
            context.selectInstrument(instrumentIndex);
        }
    }
    context.instrumentBrowserActive = false;
    context.instrumentBrowserQuery.clear();
    context.instrumentBrowserScroll = 0;
    context.instrumentBrowserSelected = 0;
    context.instrumentBrowserHitTargets.clear();
    context.instrumentBrowserListRect = UiRect {};
    context.instrumentBrowserAcceptButton = UiRect {};
    context.instrumentBrowserCancelButton = UiRect {};
}

void cycleInstrumentBy(
    int delta,
    int armedInstrument,
    const AppSessionSnapshot& snapshot,
    const std::function<void(int)>& selectInstrument) {
    const int count = static_cast<int>(snapshot.editor.instruments.size());
    if (count <= 0) {
        return;
    }
    const int current = std::clamp(armedInstrument, 0, count - 1);
    const int next = (current + count + (delta % count)) % count;
    selectInstrument(next);
}

void clampInstrumentListWindow(
    const AppSessionSnapshot& snapshot,
    int instrumentListVisibleRows,
    int& instrumentListStart) {
    const int count = static_cast<int>(snapshot.editor.instruments.size());
    const int maxStart = std::max(0, count - std::max(1, instrumentListVisibleRows));
    instrumentListStart = std::clamp(instrumentListStart, 0, maxStart);
}

void scrollInstrumentList(
    int delta,
    const AppSessionSnapshot& snapshot,
    int instrumentListVisibleRows,
    int& instrumentListStart) {
    instrumentListStart = std::max(0, instrumentListStart + delta);
    clampInstrumentListWindow(snapshot, instrumentListVisibleRows, instrumentListStart);
}

} // namespace arachno
