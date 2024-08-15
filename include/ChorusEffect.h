#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>

class ChorusEffect : public Effect {
public:
    ChorusEffect() : Effect("Chorus") {
        chorus.setRate(1.5f);
        chorus.setDepth(0.5f);
        chorus.setCentreDelay(7.0f);
        chorus.setFeedback(0.9f);
        chorus.setMix(0.5f);
    }

    void process(juce::AudioBuffer<float>& buffer, int numSamples) override {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        chorus.process(context);
    }

private:
    juce::dsp::Chorus<float> chorus;
};
