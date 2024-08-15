#include "Effect.h"

/**
 * @brief Constructs an Effect with a given name.
 * 
 * The constructor initializes the effect with a specified name, which is converted to lowercase 
 * using Boost's string algorithms.
 * 
 * @param name The name of the effect.
 */
Effect::Effect(const std::string& name) : name(boost::algorithm::to_lower_copy(name)) {}

/**
 * @brief Sets a parameter for the effect.
 *
 * This method sets or updates a parameter for the effect. If the parameter already exists, 
 * its value is updated; otherwise, it is added to the parameter map.
 * 
 * @param paramName The name of the parameter.
 * @param value The value to set for the parameter.
 */
void Effect::setParameter(const std::string& paramName, float value) {
    parameters[boost::algorithm::to_lower_copy(paramName)] = value;
}

/**
 * @brief Gets the value of a parameter.
 *
 * This method retrieves the value of a specified parameter. If the parameter does not exist, 
 * it returns `boost::none`.
 * 
 * @param paramName The name of the parameter to retrieve.
 * @return A boost::optional<float> containing the parameter value, or boost::none if not found.
 */
boost::optional<float> Effect::getParameter(const std::string& paramName) const {
    auto it = parameters.find(boost::algorithm::to_lower_copy(paramName));
    if (it != parameters.end()) {
        return it->second;
    }
    return boost::none; // Parameter not found
}

/**
 * @brief Gets all parameters of the effect.
 *
 * Provides access to the internal map of parameters for the effect. This is useful 
 * for derived classes that need to iterate over or manipulate multiple parameters.
 * 
 * @return A reference to the unordered_map storing the parameters.
 */
const std::unordered_map<std::string, float>& Effect::getParameters() const {
    return parameters;
}
