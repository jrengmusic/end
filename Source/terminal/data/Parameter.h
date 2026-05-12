/**
 * @file Parameter.h
 * @brief Brings jam::Parameter and jam::ParameterBase into Terminal namespace.
 *
 * All Parameter infrastructure (ParameterBase, Parameter<int>, Parameter<const char*>) is provided
 * by jam_data_structures (jam_parameter.h). This header imports them into the
 * Terminal namespace for unqualified use by Terminal::State and related code.
 *
 * @see jam::ParameterBase
 * @see jam::Parameter<int>
 * @see jam::Parameter<const char*>
 */

#pragma once

#include <JuceHeader.h>

namespace Terminal
{

using jam::ParameterBase;
using jam::Parameter;

} // namespace Terminal
