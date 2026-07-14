#pragma once
#include <JuceHeader.h>
#include "generated/Lexicon.h"

class ENDModel
    : public jam::Model
    , public jam::Instance<ENDModel>
{
public:
    ENDModel();
    ~ENDModel();

    /** @brief Writes a message to the overlay's ParameterText.
     *  Any thread — ParameterText::setValue is lock-free.
     *  @param text  Message text to display.
     */
    void setMessage (const juce::String& text)
    {
        auto* param { getParameter<jam::ParameterText> (Id::toType (Id::overlay), Id::message) };

        if (param != nullptr)
            param->setValue (text);
    }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDModel)
};
