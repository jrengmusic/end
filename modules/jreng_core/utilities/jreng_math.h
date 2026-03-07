namespace jreng
{
/*____________________________________________________________________________*/

struct Math
{
  

    //==============================================================================
    /**
     * @brief Mathematical constant for Pi.
     *
     * @tparam FloatType Type of floating point value.
     * @return FloatType Returns Pi value.
     */
    template<typename FloatType>
    static constexpr FloatType pi { static_cast<FloatType> (3.14159265358979323846264338327950288) };

    /**
     * @brief Mathematical constant for Pi/2.
     *
     * @tparam FloatType Type of floating point value.
     * @return FloatType Returns Pi value.
     */
    template<typename FloatType>
    static constexpr FloatType halfPi { static_cast<FloatType> (1.57079632679489661923132169163975144) };

    /**
     * @brief Mathematical constant for sqrt(2).
     *
     * @tparam FloatType Type of floating point value.
     * @return FloatType Returns Pi value.
     */
    template<typename FloatType>
    static constexpr FloatType sqrt2 { static_cast<FloatType> (1.41421356237309504880168872420969808) };

    /**
     * @brief Mathematical constant for sqrt(2).
     *
     * @tparam FloatType Type of floating point value.
     * @return FloatType Returns Pi value.
     */
    template<typename FloatType>
    static constexpr FloatType halfSqrt2 { static_cast<FloatType> (0.70710678118654752440084436210484904) };

    /**
     * @brief Predefined value for Phi (Golden Ratio).
     *
     * @tparam FloatType Type of floating point value.
     * @return FloatType Returns Phi value.
     */
    template<typename FloatType>
    static constexpr FloatType phi { static_cast<FloatType> (1.618033988749894848L) };

    /**
     * @brief The difference between 1 and the smallest floating point number
     *        of type float that is greater than 1.
     *
     * @tparam FloatType Type of floating point value.
     * @return FloatType Returns epsilon value.
     */
    template<typename FloatType>
    static constexpr FloatType flt_epsilon { static_cast<FloatType> (1.19209290E-07F) };

    /**
     * @brief Calculates the square of a given number.
     *
     * @tparam Type Type of the input value.
     * @param x Input value.
     * @return Type Square of the input value.
     */
    template<typename Type>
    static Type square (Type x)
    {
        return (x * x);
    }

    /**
     * @brief Calculates 2 raised to a given exponent (2^n).
     *
     * @tparam Type Type of the result (integer or floating-point).
     * @param n Exponent value.
     * @return Type Result of 2^n.
     *
     * @note For integer exponents, uses bit-shift for efficiency.
     * @note For floating-point exponents, uses std::exp2 for correctness.
     */
    template<typename Type>
    static Type pow2 (Type n)
    {
        if constexpr (std::is_integral_v<Type>)
        {
            // Integer exponent: use bit-shift
            return static_cast<Type>(1) << n;
        }
        else
        {
            // Floating-point exponent: use standard library
            return std::exp2(n);
        }
    }

    /**
     * @brief Calculates the next power of 2 greater than or equal to n.
     *
     * @param n Input value.
     * @return int Next power of 2 >= n.
     */
    static constexpr int nextPowerOf2 (int n)
    {
        int power = 1;
        while (power < n)
            power <<= 1;
        return power;
    }

    /**
     * @brief Determines the sign of a given number.
     *
     * @tparam Type Type of the input value.
     * @param value Input value.
     * @return int Returns -1 for negative numbers, 1 for positive numbers, and 0 for zero.
     */
    template<typename Type>
    static int sign (Type value)
    {
        return (Type (0) < value) - (value < Type (0));
    }

    /**
     * @brief Rounds a number to the nearest Nth integer.
     *
     * @param value Input integer value.
     * @param nth Interval to round to.
     * @return int Rounded value to the nearest Nth.
     */
    static int roundToNearestNthInt (int value, int nth)
    {
        int smaller { (value / nth) * nth };
        int bigger { smaller + nth };
        return (value - smaller >= bigger - value) ? bigger : smaller;
    }

    /**
     * @brief Rounds a floating point number to the nearest Nth.
     *
     * @tparam FloatType Type of floating point value.
     * @param value Input floating point value.
     * @param nth Interval to round to.
     * @return FloatType Rounded value to the nearest Nth.
     */
    template<typename FloatType>
    static FloatType roundToNearestNth (FloatType value, FloatType nth)
    {
        return roundToNearestNthInt (std::round (value), nth);
    }
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
