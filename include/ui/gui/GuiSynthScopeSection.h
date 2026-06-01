#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "ApplicationSession.h"
#include "Instrument.h"
#include "Synthesizer.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

constexpr int kSynthScopeTraceSamples = 192;

struct GuiSynthScopeThemeColors {
    unsigned long panel = 0;
    unsigned long buttonActive = 0;
    unsigned long gridHeader = 0;
    unsigned long gridLine = 0;
    unsigned long text = 0;
    unsigned long mutedText = 0;
    unsigned long background = 0;
};

struct GuiSynthScopeTraceColors {
    unsigned long tealMain = 0;
    unsigned long tealGlow = 0;
    unsigned long tealDark = 0;
    unsigned long waveSine = 0;
    unsigned long waveSquare = 0;
    unsigned long waveSaw = 0;
    unsigned long waveTriangle = 0;
    unsigned long waveNoise = 0;
    unsigned long waveSupersaw = 0;
};

struct GuiSynthScopeRuntime {
    int& trackedMidi;
    std::array<double, 4>& phase;
    std::array<Synthesizer, 4>& laneSynth;
    std::array<std::array<float, kSynthScopeTraceSamples>, 4>& laneLeft;
    std::array<std::array<float, kSynthScopeTraceSamples>, 4>& laneRight;
    bool& laneActive;
    int& laneInstrument;
    int& laneMidi;
    double& laneGateSeconds;
    bool& retriggerRequested;
};

struct GuiSynthScopeRenderContext {
    ApplicationSession& session;
    const SynthPatch& patch;
    int instrument = 0;
    int panelLeft = 0;
    int panelRight = 0;
    int oscTop = 0;
    int oscRowHeight = 0;
    int synthPreviewMidi = 60;
    float defaultVelocity = 0.8f;
    GuiSynthScopeThemeColors theme;
    GuiSynthScopeTraceColors traceColors;
    GuiSynthScopeRuntime runtime;

    std::function<std::vector<int>()> collectSynthPreviewNotes;
    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<std::string(const std::string&, int)> fitText;
    std::function<void(unsigned long)> setStrokeColor;
    std::function<void(int, int, int, int)> drawLine;
    std::function<void(int, int)> drawPoint;
};

struct GuiSynthScopeRenderResult {
    std::vector<int> previewNotes;
    int keyboardTop = 0;
};

GuiSynthScopeRenderResult drawSynthScopeSection(const GuiSynthScopeRenderContext& context);

} // namespace arachno
