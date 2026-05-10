#pragma once

#include <JuceHeader.h>
#include "Identifier.h"

namespace Terminal::map
{

struct Screen : public jam::Map::Instance<Screen>
{
    Screen()
    {
        map = {
            { normal,    ID::NORMAL.toString() },
            { alternate, ID::ALTERNATE.toString() }
        };
    }

    enum
    {
        normal = 0,
        alternate
    };

    const juce::String& getDefault() const noexcept override
    {
        return map.at (normal);
    }
};

} // namespace Terminal::map
