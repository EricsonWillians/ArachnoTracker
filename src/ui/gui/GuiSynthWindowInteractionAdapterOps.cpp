#include "ui/gui/GuiSynthWindowInteractionAdapterOps.h"

namespace arachno {

bool triggerSynthKeyboardPointerFromWindowState(
    const GuiSynthWindowInteractionAdapterContext& context,
    int mx,
    int my,
    bool allowRetrigger) {
    // End the previously sustained pointer key before starting the next one (glissando).
    if (context.synthLastPointerMidi >= 0) {
        context.session.playback().auditionNoteOff(context.synthLastPointerMidi, -1);
    }
    return triggerSynthKeyboardPointer(
        context.synthKeyboardHits,
        mx,
        my,
        allowRetrigger,
        context.synthLastPointerMidi,
        context.auditionSynthPreviewMidi);
}

bool handleSynthWindowClickFromWindowState(
    const GuiSynthWindowInteractionAdapterContext& context,
    int mx,
    int my) {
    const GuiSynthClickContext synthContext = makeSynthClickContextFromState(
        GuiSynthClickContextFactoryInput {
            context.session,
            mx,
            my,
            context.synthLastPointerMidi,
            context.synthKeyboardBaseOctave,
            context.synthKeyboardVisibleOctaves,
            context.armedOctave,
            context.synthParamPage,
            context.synthParamScroll,
            context.synthParamOscTarget,
            context.synthParamDragStartX,
            context.synthParamDragStartY,
            context.synthParamDragStartValue,
            context.synthParamDragLastValue,
            context.synthParamDragActive,
            context.synthParamDragKnob,
            context.synthParamDragDirty,
            context.synthParamDragName,
            context.synthParamDragRect,
            context.synthPreviewMidi,
            context.synthWindowHits,
            context.clampInstrumentIndex,
            context.setSynthWindowVisible,
            context.selectInstrument,
            context.auditionArmedInstrument,
            context.auditionSynthPreviewMidi,
            context.auditionSynthOscillatorPreview,
            context.runAction,
            context.beginInlinePrompt,
            context.activeSnapshot,
            context.setSynthWaveform,
            context.setSynthParameter,
            context.findSynthParamDef,
            context.getSynthParameterValue});

    if (handleSynthWindowClick(synthContext)) {
        context.synthWindowNeedsRedraw = true;
        context.needsRedraw = true;
        return true;
    }
    return false;
}

bool pollSynthMidiInputFromWindowState(const GuiSynthWindowInteractionAdapterContext& context) {
    return pollSynthMidiPreviewInput(
        context.midiInput,
        context.synthMidiPreviewHeld,
        context.synthWindowVisible,
        context.auditionSynthPreviewMidiVelocity,
        [&session = context.session](int midiNote) {
            session.playback().auditionNoteOff(midiNote, -1);
        },
        context.synthWindowNeedsRedraw);
}

GuiSynthKeyContext makeSynthKeyContextFromWindowState(const GuiSynthWindowInteractionAdapterContext& context) {
    return makeSynthKeyContextFromState(
        GuiSynthKeyContextFactoryInput {
            context.inlinePrompt,
            context.isSynthInlinePromptKind,
            context.inlinePromptUsesFileBrowser,
            context.synthInlinePromptButtonsVisible,
            context.synthInlinePromptAcceptButton,
            context.synthInlinePromptCancelButton,
            context.fileBrowserHits,
            context.fileBrowserListRect,
            context.fileBrowserScroll,
            context.fileBrowserSelected,
            context.fileBrowserEntries,
            context.fileBrowserDirectory,
            context.refreshFileBrowserEntries,
            context.executeInlinePrompt,
            context.cancelInlinePrompt,
            context.armedOctave,
            context.synthPreviewMidi,
            context.paintNoteMidi,
            context.setArmedOctave,
            context.ensureSynthKeyboardShowsMidi,
            context.setSynthWindowVisible,
            context.cycleInstrumentBy,
            context.auditionSynthPreviewMidi,
            context.claimSynthPreviewKey,
            context.releaseSynthPreviewKey});
}

GuiSynthWindowEventContext makeSynthWindowEventContextFromWindowState(
    const GuiSynthWindowInteractionAdapterContext& context,
    const std::function<bool(const XEvent&)>& isAutoRepeatRelease) {
    return makeSynthWindowEventContextFromState(
        GuiSynthWindowEventContextFactoryInput {
            context.display,
            context.synthWindow,
            context.wmDelete,
            context.synthWindowWidth,
            context.synthWindowHeight,
            context.synthWindowMinWidth,
            context.synthWindowMinHeight,
            context.ensureSynthBackbuffer,
            context.setSynthWindowVisible,
            context.synthPointerX,
            context.synthPointerY,
            context.synthPointerDown,
            context.synthLastPointerMidi,
            context.synthParamDragActive,
            context.synthParamDragKnob,
            context.synthParamDragDirty,
            context.synthParamDragName,
            context.synthParamDragRect,
            context.synthParamDragStartX,
            context.synthParamDragStartY,
            context.synthParamDragStartValue,
            context.synthParamDragLastValue,
            context.synthTooltipParam,
            context.synthTooltipHoverSince,
            context.synthParamViewport,
            context.synthParamContentHeight,
            context.synthParamScroll,
            context.synthWindowHits,
            context.inlinePrompt,
            context.isSynthInlinePromptKind,
            context.inlinePromptUsesFileBrowser,
            context.synthInlinePromptButtonsVisible,
            context.synthInlinePromptAcceptButton,
            context.synthInlinePromptCancelButton,
            context.fileBrowserHits,
            context.fileBrowserListRect,
            context.fileBrowserScroll,
            context.fileBrowserSelected,
            context.fileBrowserEntries,
            context.fileBrowserDirectory,
            context.refreshFileBrowserEntries,
            context.executeInlinePrompt,
            context.cancelInlinePrompt,
            [&](int x, int y, bool allowRetrigger) {
                return triggerSynthKeyboardPointerFromWindowState(context, x, y, allowRetrigger);
            },
            [&](int x, int y) { return handleSynthWindowClickFromWindowState(context, x, y); },
            context.refreshSnapshot,
            [&session = context.session](int midiNote) { session.playback().auditionNoteOff(midiNote, -1); },
            context.clampInstrumentIndex,
            context.findSynthParamDef,
            context.setSynthParameterWithRefresh,
            [&]() { return makeSynthKeyContextFromWindowState(context); },
            isAutoRepeatRelease});
}

} // namespace arachno
