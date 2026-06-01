#include "ui/gui/GuiSynthWindowDrawOps.h"

namespace arachno {

void drawSynthWindowContent(const GuiSynthWindowDrawContext& context) {
    context.synthWindowHits.clear();
    context.synthKeyboardHits.clear();
    context.fileBrowserHits.clear();
    context.fileBrowserListRect = UiRect {};
    context.synthParamViewport = UiRect {};
    context.synthParamContentHeight = 0;

    context.drawFilledRect(0, 0, context.synthWindowWidth, context.synthWindowHeight, context.theme.background);
    context.drawFilledRect(8, 8, context.synthWindowWidth - 16, context.synthWindowHeight - 16, context.theme.panel);
    context.drawRect(8, 8, context.synthWindowWidth - 16, context.synthWindowHeight - 16, context.theme.gridLine);
    context.drawFilledRect(9, 9, context.synthWindowWidth - 18, 32, context.theme.gridHeader);
    context.drawText(16, 30, "SYNTH DESIGNER", context.theme.text);
    context.drawText(164, 30, "| PATCH WORKBENCH", context.theme.mutedText);

    const int instrument = context.clampInstrumentIndex();
    if (instrument < 0 || instrument >= static_cast<int>(context.session.song().instruments.size())) {
        context.drawText(16, 52, "No instruments available.", context.theme.mutedText);
        const UiRect addRect {16, 64, 120, 24};
        context.drawButton(addRect, "NEW INSTR", false);
        context.synthWindowHits.push_back({addRect, "inst_new", "", "", "", 0.0});
        context.copyToWindowAndFlush();
        return;
    }

    const SynthPatch& patch = context.session.song().instruments[static_cast<std::size_t>(instrument)].patch;

    const GuiSynthTopSectionResult topSection = drawSynthTopSection(
        GuiSynthTopSectionContext {
            context.synthWindowWidth,
            instrument,
            context.synthParamPage,
            patch,
            context.synthWindowHits,
            context.theme.text,
            [&](int x, int y, const std::string& text, unsigned long color) { context.drawText(x, y, text, color); },
            [&](const UiRect& rect, const std::string& label, bool active) { context.drawButton(rect, label, active); },
            [&](const std::string& text, int maxWidth) { return context.fitText(text, maxWidth); }});
    const int panelLeft = topSection.panelLeft;
    const int panelRight = topSection.panelRight;
    const int oscTop = topSection.oscTop;
    const int oscRowHeight = topSection.oscRowHeight;

    drawSynthComposerSections(
        GuiSynthComposerContext {
            context.session,
            patch,
            instrument,
            panelLeft,
            panelRight,
            oscTop,
            oscRowHeight,
            context.synthPreviewMidi,
            context.defaultVelocity,
            context.synthKeyboardBaseOctave,
            context.synthKeyboardVisibleOctaves,
            context.midiStatusText,
            context.synthKeyboardHits,
            context.synthWindowHits,
            context.synthWindowWidth,
            context.synthWindowHeight,
            context.synthParamPage,
            context.synthParamOscTarget,
            context.synthParamScroll,
            context.synthParamViewport,
            context.synthParamContentHeight,
            context.synthTooltipParam,
            context.synthPointerX,
            context.synthPointerY,
            context.synthTooltipHoverSince,
            context.synthParamDefs,
            context.inlinePrompt,
            context.isSynthInlinePromptKind,
            context.inlinePromptUsesFileBrowser,
            context.refreshFileBrowserEntries,
            context.fileBrowserDirectory,
            context.fileBrowserEntries,
            context.fileBrowserHits,
            context.fileBrowserListRect,
            context.fileBrowserScroll,
            context.fileBrowserSelected,
            context.synthInlinePromptAcceptButton,
            context.synthInlinePromptCancelButton,
            context.synthInlinePromptButtonsVisible,
            context.synthScopeTrackedMidi,
            context.synthScopePhase,
            context.synthScopeLaneSynth,
            context.synthScopeLaneLeft,
            context.synthScopeLaneRight,
            context.synthScopeLaneActive,
            context.synthScopeLaneInstrument,
            context.synthScopeLaneMidi,
            context.synthScopeLaneGateSeconds,
            context.synthScopeRetriggerRequested,
            context.collectSynthPreviewNotes,
            {
                context.theme.panel,
                context.theme.buttonActive,
                context.theme.gridHeader,
                context.theme.gridLine,
                context.theme.text,
                context.theme.mutedText,
                context.theme.background},
            {
                context.scope.tealMain,
                context.scope.tealGlow,
                context.scope.tealDark,
                context.scope.waveSine,
                context.scope.waveSquare,
                context.scope.waveSaw,
                context.scope.waveTriangle,
                context.scope.waveNoise,
                context.scope.waveSupersaw},
            {
                context.theme.button,
                context.theme.gridLine,
                context.theme.pianoWhite,
                context.theme.pianoBlack,
                context.theme.mutedText,
                context.theme.text,
                context.theme.panel,
                context.theme.selection},
            {
                context.theme.background,
                context.theme.panel,
                context.theme.gridLine,
                context.theme.mutedText,
                context.theme.text,
                context.theme.buttonActive},
            {
                context.theme.panel,
                context.theme.gridLine,
                context.theme.text,
                context.theme.mutedText,
                context.theme.background,
                context.theme.selection,
                context.theme.selectionText},
            context.scope.tealMain,
            context.theme.mutedText,
            context.drawFilledRect,
            context.drawRect,
            context.drawText,
            context.drawButton,
            context.drawKnob,
            context.fitText,
            context.textWidth,
            context.setStrokeColor,
            context.drawLine,
            context.drawPoint,
            context.setClipRect,
            context.clearClip});

    context.copyToWindowAndFlush();
}

} // namespace arachno
