namespace jreng
{
/*____________________________________________________________________________*/

//==============================================================================
/** taken from juce::roundToInt, to reduce dependency with JUCE functions. */

#if JUCE_MSVC
#pragma optimize("t", off)
#ifndef __INTEL_COMPILER
#pragma float_control(precise, on, push)
#endif
#endif

/** Fast floating-point-to-integer conversion.

    This is faster than using the normal c++ cast to convert a float to an int, and
    it will round the value to the nearest integer, rather than rounding it down
    like the normal cast does.

    Note that this routine gets its speed at the expense of some accuracy, and when
    rounding values whose floating point component is exactly 0.5, odd numbers and
    even numbers will be rounded up or down differently.
*/
template <typename FloatType>
int toInt (const FloatType value) noexcept
{
#ifdef __INTEL_COMPILER
#pragma float_control(precise, on, push)
#endif

    union
    {
        int asInt[2];
        double asDouble;
    } n;
    n.asDouble = ((double) value) + 6755399441055744.0;

#if JUCE_BIG_ENDIAN
    return n.asInt[1];
#else
    return n.asInt[0];
#endif
}

inline int toInt (int value) noexcept
{
    return value;
}

#if JUCE_MSVC
#ifndef __INTEL_COMPILER
#pragma float_control(pop)
#endif
#pragma optimize("", on) // resets optimisations to the project defaults
#endif

/** Fast floating-point-to-integer conversion.

    This is a slightly slower and slightly more accurate version of toInt(). It works
    fine for values above zero, but negative numbers are rounded the wrong way.
*/
inline int toIntIntAccurate (double value) noexcept
{
#ifdef __INTEL_COMPILER
#pragma float_control(pop)
#endif

    return toInt (value + 1.5e-8);
}

/**
 * @brief Converts an enum class to its underlying type.
 *
 * This function extracts the raw integral value from an enum class instance
 * by casting it to its underlying type.
 *
 * @tparam EnumType The enum class type.
 * @param e The enum value to convert.
 * @return The underlying integral representation of the enum.
 */
template <typename EnumType>
constexpr auto fromEnumClass(EnumType e) noexcept
{
    return static_cast<std::underlying_type_t<EnumType>>(e);
}

/**
 * @brief Converts a floating-point value to a boolean representation.
 *
 * This function evaluates whether a floating-point value exceeds 0.5.
 * If true, it returns 1 (true), otherwise 0 (false).
 *
 * @tparam FloatType The floating-point type.
 * @param value The value to convert.
 * @return Integer representation (1 or 0) based on the threshold.
 */
template <typename FloatType>
int toBool(const FloatType value) noexcept
{
    return (value > static_cast<FloatType>(0.5));
}

/**
 * @brief Converts an enum value to its underlying integral type.
 *
 * This function extracts the underlying integral type from an enum value.
 *
 * @tparam EnumValueType The enum type.
 * @param e The enum value to convert.
 * @return The integral representation of the enum.
 */
template <typename EnumValueType>
static constexpr typename std::underlying_type<EnumValueType>::type
    to_underlying(EnumValueType e) noexcept
{
    return static_cast<typename std::underlying_type<EnumValueType>::type>(e);
}


/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
