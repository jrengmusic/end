#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace end
{
/*____________________________________________________________________________*/

/** @brief Packed window dimensions — int16_t width + height in a uint32_t.
 *  Wraps jam::Union\<int16_t, int16_t\> with typed constructors to avoid
 *  casting at call sites. Round-trips through ValueTree as int.
 */
struct Size : jam::Union<int16_t, int16_t>
{
    /** @brief Pack from component dimensions.
     *  @param width   Component width in pixels.
     *  @param height  Component height in pixels.
     */
    Size (int width, int height) noexcept
        : jam::Union<int16_t, int16_t> { pack (static_cast<int16_t> (width),
                                               static_cast<int16_t> (height)) }
    {
    }

    /** @brief Unpack from ValueTree property (int round-trip).
     *  @param v  var holding the packed int written by toInt().
     */
    explicit Size (const juce::var& v) noexcept
        : jam::Union<int16_t, int16_t> { static_cast<uint32_t> (static_cast<int> (v)) }
    {
    }

    /** @brief Pack to int for ValueTree storage. */
    int toInt() const noexcept { return static_cast<int> (bits); }
};

/** @brief ADL get() for end::Size structured binding. */
template<size_t I>
constexpr auto get (const Size& s) noexcept
{
    return s.template unpack<I>();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end

namespace std
{
/*____________________________________________________________________________*/
/** @brief Structured binding support for end::Size. */
template<>
struct tuple_size<end::Size> : std::integral_constant<size_t, 2>
{
};
template<size_t I>
struct tuple_element<I, end::Size>
{
    using type = int16_t;
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace std

//==============================================================================
namespace end
{
/*____________________________________________________________________________*/

class Model
    : public jam::Model
    , public jam::Instance<Model>
{
public:
    Model();
    ~Model();

    /** @brief Writes a message to the overlay's ParameterText.
     *  Any thread — ParameterText::setValue is lock-free.
     *  @param text  Message text to display.
     */
    void setMessage (const juce::String& text)
    {
        auto* param { getParameter<jam::ParameterText> (IDtype::overlay, ID::message) };

        if (param != nullptr)
            param->setValue (text);
    }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
