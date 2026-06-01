#include "ui/gui/GuiSynthComposerOps.h"

namespace arachno {

void drawSynthComposerSections(const GuiSynthComposerContext& context) {
    GuiSynthScopeRenderContext scopeContext {
        context.session,
        context.patch,
        context.instrument,
        context.panelLeft,
        context.panelRight,
        context.oscTop,
        context.oscRowHeight,
        context.synthPreviewMidi,
        context.defaultVelocity,
        context.scopeTheme,
        context.scopeTrace,
        {
            context.synthScopeTrackedMidi,
            context.synthScopePhase,
            context.synthScopeLaneSynth,
            context.synthScopeLaneLeft,
            context.synthScopeLaneRight,
            context.synthScopeLaneActive,
            context.synthScopeLaneInstrument,
            context.synthScopeLaneMidi,
            context.synthScopeLaneGateSeconds,
            context.synthScopeRetriggerRequested},
        context.collectSynthPreviewNotes,
        context.drawFilledRect,
        context.drawRect,
        context.drawText,
        context.fitText,
        context.setStrokeColor,
        context.drawLine,
        context.drawPoint};
    const GuiSynthScopeRenderResult scopeResult = drawSynthScopeSection(scopeContext);
    const std::vector<int>& previewNotes = scopeResult.previewNotes;
    const int keyboardTop = scopeResult.keyboardTop;

    GuiSynthKeyboardSectionContext keyboardSectionContext {
        context.panelRight,
        keyboardTop,
        context.synthPreviewMidi,
        context.synthKeyboardBaseOctave,
        context.synthKeyboardVisibleOctaves,
        context.midiStatusText,
        previewNotes,
        context.synthKeyboardHits,
        context.synthWindowHits,
        context.keyboardTheme,
        context.activeKeyColor,
        context.drawFilledRect,
        context.drawRect,
        context.drawText,
        context.drawButton,
        context.fitText};
    const int controlsTop = drawSynthKeyboardSection(keyboardSectionContext);

    GuiSynthParamSectionContext paramSectionContext {
        context.synthWindowWidth,
        context.synthWindowHeight,
        context.panelRight,
        controlsTop,
        context.synthParamPage,
        context.synthParamOscTarget,
        context.synthParamScroll,
        context.synthParamViewport,
        context.synthParamContentHeight,
        context.synthTooltipParam,
        context.synthPointerX,
        context.synthPointerY,
        context.synthTooltipHoverSince,
        context.synthWindowHits,
        context.synthParamDefs,
        context.patch,
        context.paramTheme,
        context.drawFilledRect,
        context.drawRect,
        context.drawText,
        context.drawButton,
        context.drawKnob,
        context.fitText,
        context.textWidth,
        context.setClipRect,
        context.clearClip};
    drawSynthParameterSection(paramSectionContext);

    context.drawText(
        16,
        context.synthWindowHeight - 18,
        "Patch files load/save as .arachnopatch. Imported/custom instruments are persisted with project save.",
        context.footerTextColor);

    if (context.inlinePrompt.active && context.isSynthInlinePromptKind(context.inlinePrompt.kind)) {
        GuiSynthInlinePromptSectionContext inlinePromptContext {
            context.synthWindowWidth,
            context.synthWindowHeight,
            context.inlinePrompt,
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
            context.inlinePromptTheme,
            context.drawFilledRect,
            context.drawRect,
            context.drawText,
            context.drawButton,
            context.fitText};
        drawSynthInlinePromptSection(inlinePromptContext);
    } else {
        context.synthInlinePromptButtonsVisible = false;
    }
}

} // namespace arachno
