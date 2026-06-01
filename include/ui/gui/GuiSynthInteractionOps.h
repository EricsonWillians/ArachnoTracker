#pragma once

#include <functional>
#include <string>
#include <vector>

#include "AppActions.h"
#include "Synthesizer.h"
#include "ui/gui/SynthUiData.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

bool triggerSynthKeyboardPointer(
    const std::vector<PianoKeyHit>& synthKeyboardHits,
    int mx,
    int my,
    bool allowRetrigger,
    int& synthLastPointerMidi,
    const std::function<void(int)>& auditionSynthPreviewMidi);

struct GuiSynthClickContext {
    ApplicationSession& session;
    int mx = 0;
    int my = 0;
    int& synthLastPointerMidi;
    int& synthKeyboardBaseOctave;
    int synthKeyboardVisibleOctaves = 4;
    int armedOctave = 4;
    int& synthParamPage;
    int& synthParamScroll;
    int& synthParamOscTarget;
    int& synthParamDragStartX;
    int& synthParamDragStartY;
    double& synthParamDragStartValue;
    double& synthParamDragLastValue;
    bool& synthParamDragActive;
    bool& synthParamDragKnob;
    bool& synthParamDragDirty;
    std::string& synthParamDragName;
    UiRect& synthParamDragRect;
    int synthPreviewMidi = 60;
    std::vector<SynthWindowHit> synthWindowHits;

    std::function<int()> clampInstrumentIndex;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<void(int)> selectInstrument;
    std::function<void()> auditionArmedInstrument;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<void(const SynthPatch&, int, int)> auditionSynthOscillatorPreview;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<bool(int, const std::string&, const std::string&)> setSynthWaveform;
    std::function<bool(int, const std::string&, double)> setSynthParameter;
    std::function<const SynthParamDef*(const std::string&)> findSynthParamDef;
    std::function<double(const SynthPatch&, const std::string&)> getSynthParameterValue;
};

bool handleSynthWindowClick(const GuiSynthClickContext& context);

} // namespace arachno
