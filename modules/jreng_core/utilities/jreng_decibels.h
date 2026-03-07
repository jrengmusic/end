namespace jreng
{
/*____________________________________________________________________________*/
/**
 * @brief This class contains some helpful static methods for dealing with decibel values.
 *
 * Taken from juce::Decibels, to reduce dependency with JUCE functions.
 */
class Decibels
{
public:
    //==============================================================================
    /**
     * @brief Converts a dBFS value to its equivalent amplification level.
     *
     * A gain of 1.0 = 0 dB, and lower gains map onto negative decibel values. Any
     * decibel value lower than minusInfinityDb will return a gain of 0.
     *
     * @tparam Type The type of the value.
     * @param decibels The decibel value to convert.
     * @param minusInfinityDb The decibel value representing minus infinity.
     * @return The equivalent gain level.
     */
    template<typename Type>
    static Type toAmp (Type decibels,
                       Type minusInfinityDb = Type (defaultMinusInfinitydB))
    {
        return decibels > minusInfinityDb ? std::pow (Type (10.0), decibels * Type (0.05))
                                          : Type();
    }

    /**
     * @brief Converts an amplification level into a dBFS value.
     *
     * A gain of 1.0 = 0 dB, and lower gains map onto negative decibel values.
     * If the gain is 0 (or negative), then the method will return the value
     * provided as minusInfinityDb.
     *
     * @tparam Type The type of the value.
     * @param gain The gain level to convert.
     * @param minusInfinityDb The decibel value representing minus infinity.
     * @return The equivalent dBFS value.
     */
    template<typename Type>
    static Type fromAmp (Type amp,
                         Type minusInfinityDb = Type (defaultMinusInfinitydB))
    {
        return amp > Type() ? std::max (minusInfinityDb, static_cast<Type> (std::log10 (amp)) * Type (20.0))
                             : minusInfinityDb;
    }

    //==============================================================================
    /**
     * @brief Converts a decibel reading to a string.
     *
     * By default, the returned string will have the 'dB' suffix added, but this can be removed by
     * setting the shouldIncludeSuffix argument to false. If a customMinusInfinityString argument
     * is provided, this will be returned if the value is lower than minusInfinityDb, otherwise
     * the return value will be "-INF".
     *
     * @tparam Type The type of the value.
     * @param decibels The decibel value to convert.
     * @param decimalPlaces The number of decimal places to include in the string.
     * @param minusInfinityDb The decibel value representing minus infinity.
     * @param shouldIncludeSuffix Whether to include the 'dB' suffix in the string.
     * @param customMinusInfinityString The custom string to use for minus infinity.
     * @return The decibel value as a string.
     */
    template<typename Type>
    static std::string toString (Type decibels,
                                 int decimalPlaces = 2,
                                 Type minusInfinityDb = Type (defaultMinusInfinitydB),
                                 bool shouldIncludeSuffix = true,
                                 std::string_view customMinusInfinityString = {})
    {
        std::stringstream s;

        if (decibels <= minusInfinityDb)
        {
            if (customMinusInfinityString.empty())
                s << "-INF";
            else
                s << customMinusInfinityString;
        }
        else
        {
            if (decibels >= Type())
                s << '+';

            if (decimalPlaces <= 0)
                s << toInt (decibels);
            else
                s << std::fixed << std::setprecision (decimalPlaces) << decibels;
        }

        if (shouldIncludeSuffix)
            s << " dB";

        return s.str();
    }

private:
    //==============================================================================
    enum
    {
        defaultMinusInfinitydB = -100
    };

    Decibels() = delete;// This class can't be instantiated, it's just a holder for static methods.
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
