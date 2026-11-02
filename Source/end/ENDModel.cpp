#include "end/ENDModel.h"

ENDModel::ENDModel()
    : jam::Model (Id::toType (Id::end))
{
    static constexpr int maximumMessageLength { 4096 };

    auto overlayRow { getOrCreateChildWithName (Id::toType (Id::overlay)) };
    createAndAddParameter<jam::ParameterText> (overlayRow, Id::message, juce::String {}, maximumMessageLength);
}

void ENDModel::setMessage (const juce::String& text)
{
    auto* param { getParameter<jam::ParameterText> (Id::toType (Id::overlay), Id::message) };
    jassert (param != nullptr);

    if (param != nullptr)
        param->setValue (text);
}
