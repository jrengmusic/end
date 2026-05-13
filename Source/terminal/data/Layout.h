#pragma once

#include <JuceHeader.h>
#include "Identifier.h"
#include "TextBuffer.h"

namespace Terminal
{

struct State;

struct Layout
{
    static void build (const juce::XmlElement& xml, State& state, TextBuffer& textBuffer);

private:
    struct Boolean : public jam::Map::Instance<Boolean>
    {
        Boolean()
        {
            map = {
                { no,  "false" },
                { yes, "true"  }
            };
        }

        enum { no = 0, yes = 1 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (no);
        }
    };

    static int resolveDefault (const juce::XmlElement& elem,
                               const Boolean& boolMap) noexcept;
};

} // namespace Terminal
