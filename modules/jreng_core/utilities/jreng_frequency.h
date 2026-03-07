#include <complex>
namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @brief This struct contains methods for dealing with frequency values.
 *
 * Provides functionality to convert between normalized and logarithmic frequency scales,
 * and to compute the magnitude of a frequency response.
 */
struct Frequency
{
    //==============================================================================
    /**
     * The mathematical formula for converting between a (minFreq) 20- (maxFreq) 20000.
     * A decade is the jump from 20 to 200, or 200 to 2000, or 2000 to 20000 (count them, 3 decades).
     *
     * 3 decade logarithmic scale with normalized input 0-1 linear proportion is as follows.
     */

    /**
     * @brief Convert a normalized value to a frequency.
     *
     * This function converts a normalized value (0-1) to a frequency value on a 3 decade logarithmic scale.
     *
     * @tparam FloatType The type of the value.
     * @param normal The normalized value to convert.
     * @param minFreq The minimum frequency (default is 20).
     * @param maxFreq The maximum frequency (default is 20000).
     * @return The frequency value.
     */
    template<typename FloatType>
    static FloatType fromNormal (FloatType normal,
                                 FloatType minFreq = static_cast<FloatType> (20),
                                 FloatType maxFreq = static_cast<FloatType> (20000))
    {
        return minFreq + ((maxFreq - minFreq) * (std::pow ((maxFreq / minFreq), normal) - static_cast<FloatType> (1)) / ((maxFreq / minFreq) - static_cast<FloatType> (1)));
    }

    /**
     * @brief Convert a frequency to a normalized value.
     *
     * This function converts a frequency value to a normalized value (0-1) on a 3 decade logarithmic scale.
     *
     * @tparam FloatType The type of the value.
     * @param freq The frequency value to convert.
     * @param minFreq The minimum frequency (default is 20).
     * @param maxFreq The maximum frequency (default is 20000).
     * @return The normalized value.
     */
    template<typename FloatType>
    static FloatType toNormal (FloatType freq,
                               FloatType minFreq = static_cast<FloatType> (20),
                               FloatType maxFreq = static_cast<FloatType> (20000))
    {
        return std::log ((((freq - minFreq) / (maxFreq - minFreq)) * ((maxFreq / minFreq) - static_cast<FloatType> (1))) + static_cast<FloatType> (1)) / (std::log (maxFreq / minFreq));
    }

    /**
     * @brief Get the magnitude of a frequency response.
     *
     * This function calculates the magnitude of a frequency response given the frequency and coefficients.
     *
     * @tparam FloatType The type of the value.
     * @tparam C The type of the coefficients.
     * @param frequency The frequency to calculate the magnitude for.
     * @param coef The coefficients for the frequency response calculation.
     * @param srate The sample rate (default is 44100.0).
     * @return The magnitude of the frequency response.
     */
    template<typename FloatType, typename C>
    FloatType getMagnitude (FloatType frequency, C coef, double srate = 44100.0)
    {
        FloatType w { static_cast<FloatType> (2) * Math::pi<FloatType> * frequency / srate };
        std::complex<FloatType> z { std::cos (w), std::sin (w) };
        std::complex<FloatType> h { (coef.b0 * z * z + coef.b1 * z + coef.b2) / (z * z + coef.a1 * z + coef.a2) };
        return std::abs (h);
    }
    
    /** return the magnitude response of given frequency, samplerate for biquad coefficient */

    //template <typename T>
    //T magnitude (T f, const double& srate, const array<double, 5>& coef)
    //{
    //  T const w = 2.0 * M_PI * f / srate;
    //  T const num = sqrt (square (coef[0]*square (cos (w)) - coef[0]*square (sin (w)) + coef[1]*cos (w) + coef[2]) + square (2.0 * coef[0]*cos (w)*sin (w) + coef[1]*(sin (w))));
    //  T const den = sqrt (square (square (cos (w)) - square (sin (w)) + coef[3]*cos (w) + coef[4]) + square (2.0 * cos (w)*sin (w) + coef[3]*(sin (w))));
    //  return num/den;
    //}

    //template <typename T>
    //T mag (T f, const double& srate, const array<double, 5>& coef)
    //{
    //  T const w = 2.0 * M_PI * f / srate;
    //  complex<double> z (cos (w), sin (w));
    //  complex<double> h = (coef[0] * z * z + coef[1] * z + coef[2]) / (1.0 * z * z + coef[3] * z + coef[4]);
    //  return std::abs (h);
    //}
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
