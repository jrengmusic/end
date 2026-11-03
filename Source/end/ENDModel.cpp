#include "end/ENDModel.h"

ENDModel::ENDModel()
    : jam::Model (jam::Format::toType (Id::end))
{
    static constexpr int maximumMessageLength { 4096 };

    auto overlayRow { getOrCreateChildWithName (jam::Format::toType (Id::overlay)) };
    createAndAddParameter<jam::ParameterText> (overlayRow, Id::message, juce::String {}, maximumMessageLength);
}

void ENDModel::setMessage (const juce::String& text)
{
    auto* param { getParameter<jam::ParameterText> (jam::Format::toType (Id::overlay), Id::message) };
    jassert (param != nullptr);

    if (param != nullptr)
        param->setValue (text);
}
