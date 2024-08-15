#pragma once

#include <string>
#include <unordered_map>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>
#include <juce_dsp/juce_dsp.h>

/**
 * @class Effect
 * @brief Base class for audio effects in the tracker application.
 *
 * The Effect class serves as an abstract base for all audio effects in the tracker application. 
 * It provides an interface for processing audio buffers and managing effect parameters dynamically.
 */
class Effect {
public:
    /**
     * @brief Constructs an Effect with a given name.
     * 
     * This constructor initializes the effect with a specified name, which is stored in lowercase.
     * 
     * @param name The name of the effect.
     */
    explicit Effect(const std::string& name);

    /**
     * @brief Virtual destructor.
     * 
     * Ensures proper cleanup in derived classes.
     */
    virtual ~Effect() = default;

    /**
     * @brief Processes the audio buffer with the effect.
     *
     * This is a pure virtual function that must be implemented by derived classes to apply 
     * the effect's processing logic to the audio buffer.
     *
     * @param buffer The audio buffer to process.
     * @param numSamples The number of samples to process.
     */
    virtual void process(juce::AudioBuffer<float>& buffer, int numSamples) = 0;

    /**
     * @brief Sets a parameter for the effect.
     *
     * This method sets or updates a parameter for the effect. If the parameter already exists, 
     * its value is updated; otherwise, it is added to the parameter map.
     * 
     * @param paramName The name of the parameter.
     * @param value The value to set for the parameter.
     */
    virtual void setParameter(const std::string& paramName, float value);

    /**
     * @brief Gets the value of a parameter.
     *
     * This method retrieves the value of a specified parameter. If the parameter does not exist, 
     * it returns `boost::none`.
     * 
     * @param paramName The name of the parameter to retrieve.
     * @return A boost::optional<float> containing the parameter value, or boost::none if not found.
     */
    boost::optional<float> getParameter(const std::string& paramName) const;

protected:
    /**
     * @brief Gets all parameters of the effect.
     *
     * Provides access to the internal map of parameters for the effect. This is useful 
     * for derived classes that need to iterate over or manipulate multiple parameters.
     * 
     * @return A reference to the unordered_map storing the parameters.
     */
    const std::unordered_map<std::string, float>& getParameters() const;

private:
    std::string name;  ///< The name of the effect, stored in lowercase.
    std::unordered_map<std::string, float> parameters;  ///< A map of parameter names to their values.
};
