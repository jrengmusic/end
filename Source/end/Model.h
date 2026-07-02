#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

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
