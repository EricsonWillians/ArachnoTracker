#pragma once

#include "Effect.h"
#include <juce_dsp/juce_dsp.h>
#include <boost/log/trivial.hpp>
#include <boost/algorithm/string.hpp>

/**
 * @class ReverbEffect
 * @brief Implements a reverb effect using JUCE's DSP module.
 *
 * The ReverbEffect class allows for real-time audio processing using a reverb effect.
 * It inherits from the Effect base class and leverages JUCE's built-in reverb capabilities.
 */
class ReverbEffect : public Effect {
public:
    /**
     * @brief Constructor for ReverbEffect.
     *
     * Initializes the reverb effect with default parameters.
     */
    ReverbEffect() : Effect("Reverb") {
        setDefaultParameters();
    }

    /**
     * @brief Processes the audio buffer with the reverb effect.
     *
     * @param buffer The audio buffer to process.
     * @param numSamples The number of samples to process.
     */
    void process(juce::AudioBuffer<float>& buffer, int numSamples) override {
        juce::dsp::AudioBlock<float> audioBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(audioBlock);
        reverb.process(context);
    }

    /**
     * @brief Sets a parameter for the reverb effect.
     *
     * Overrides the Effect class method to handle specific reverb parameters.
     * 
     * @param paramName The name of the parameter to set (e.g., "wetLevel").
     * @param value The value to set for the parameter.
     */
    void setParameter(const std::string& paramName, float value) override {
        std::string lowerParamName = boost::algorithm::to_lower_copy(paramName);
        juce::dsp::Reverb::Parameters params = reverb.getParameters();

        if (lowerParamName == "roomsize") {
            params.roomSize = juce::jlimit(0.0f, 1.0f, value);
        } else if (lowerParamName == "damping") {
            params.damping = juce::jlimit(0.0f, 1.0f, value);
        } else if (lowerParamName == "wetlevel") {
            params.wetLevel = juce::jlimit(0.0f, 1.0f, value);
        } else if (lowerParamName == "drylevel") {
            params.dryLevel = juce::jlimit(0.0f, 1.0f, value);
        } else if (lowerParamName == "width") {
            params.width = juce::jlimit(0.0f, 1.0f, value);
        } else if (lowerParamName == "freezemode") {
            params.freezeMode = juce::jlimit(0.0f, 1.0f, value);
        } else {
            BOOST_LOG_TRIVIAL(warning) << "Attempted to set unknown parameter: " << paramName;
            return;
        }

        reverb.setParameters(params);
        Effect::setParameter(lowerParamName, value);  // Store the parameter for consistency
    }

private:
    juce::dsp::Reverb reverb;  ///< JUCE Reverb processor instance.

    /**
     * @brief Sets the default parameters for the reverb effect.
     */
    void setDefaultParameters() {
        juce::dsp::Reverb::Parameters params;
        params.roomSize = 0.8f;
        params.damping = 0.5f;
        params.wetLevel = 0.3f;
        params.dryLevel = 0.7f;
        params.width = 1.0f;
        params.freezeMode = 0.0f;
        reverb.setParameters(params);

        // Store default parameters in the base class
        setParameter("roomSize", params.roomSize);
        setParameter("damping", params.damping);
        setParameter("wetLevel", params.wetLevel);
        setParameter("dryLevel", params.dryLevel);
        setParameter("width", params.width);
        setParameter("freezeMode", params.freezeMode);
    }
};
