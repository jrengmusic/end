#include "end/ENDModel.h"

ENDModel::ENDModel()
    : jam::Model (Id::toType (Id::end))
{
    auto overlayRow { getOrCreateChildWithName (Id::toType (Id::overlay)) };
    createAndAddParameter<jam::ParameterText> (overlayRow, Id::message, juce::String {}, 4096);
}
