#include "ui/gui/GuiInstrumentBrowserOps.h"

#include <algorithm>

#include "GuiInput.h"

namespace arachno {

std::vector<int> filteredInstrumentIndicesForQuery(
    const AppSessionSnapshot& snapshot,
    const std::string& query) {
    std::vector<int> indices;
    const std::string normalizedQuery = lowerCopy(trimCopy(query));
    indices.reserve(snapshot.editor.instruments.size());
    for (const InstrumentSummary& instrument : snapshot.editor.instruments) {
        const std::string indexToken = std::to_string(instrument.index);
        const bool match = normalizedQuery.empty()
            || lowerCopy(instrument.name).find(normalizedQuery) != std::string::npos
            || indexToken.find(normalizedQuery) != std::string::npos;
        if (match) {
            indices.push_back(instrument.index);
        }
    }
    return indices;
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
            context.selectInstrument(indices[static_cast<std::size_t>(row)]);
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
