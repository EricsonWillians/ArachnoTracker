#include "ui/gui/GuiPatternPromptOps.h"

#include <algorithm>
#include <sstream>

namespace arachno {

void beginPatternCreatePrompt(const GuiPatternPromptContext& context) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    const int nextNumber = std::max(1, static_cast<int>(snap.editor.patterns.size()) + 1);
    const int defaultRows = std::clamp(context.activePatternRows, 8, 8192);
    const int defaultTracks = std::max(1, snap.editor.activeGrid.trackCount);
    std::ostringstream initial;
    initial << "Pattern" << nextNumber << " " << defaultRows << " " << defaultTracks;
    context.beginInlinePrompt(
        InlinePromptKind::PatternCreateSpec,
        "Create pattern",
        "name rows [tracks] (example: Verse 64 4)",
        initial.str(),
        -1,
        -1);
}

void beginPatternClonePrompt(const GuiPatternPromptContext& context) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    const int count = static_cast<int>(snap.editor.patterns.size());
    std::string initial = "PatternCopy";
    if (count > 0) {
        const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
        initial = context.normalizedPatternNameToken(
            snap.editor.patterns[static_cast<std::size_t>(current)].name + "_copy",
            "PatternCopy");
    }
    context.beginInlinePrompt(
        InlinePromptKind::PatternCloneName,
        "Clone active pattern",
        "optional clone name token (single word)",
        initial,
        -1,
        -1);
}

void beginTemporalPastePrompt(const GuiBeginInlinePromptFn& beginInlinePrompt) {
    beginInlinePrompt(
        InlinePromptKind::TemporalPasteSpec,
        "Rhythmic paste",
        "anchor repeats interval [offset] | anchor: cursor next_row next_beat next_bar next_sel abs:N | interval: beat bar sel rows:N",
        "next_bar 4 bar",
        -1,
        -1);
}

bool deleteActivePattern(const GuiPatternDeleteContext& context) {
    AppActionRequest request;
    request.actionId = "editor.pattern.delete";
    const AppActionResult action = context.runAction(request);
    if (!action.ok) {
        return false;
    }
    context.keyboardSelectionActive = false;
    context.viewStartRow = 0;
    return true;
}

} // namespace arachno
