#pragma once
#include <JuceHeader.h>
#include "terminal/Identifier.h"

/*____________________________________________________________________________*/

struct Map
{
    struct Bool : public jam::Map::Instance<Bool>
    {
        Bool()
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

    struct Screen : public jam::Map::Instance<Screen>
    {
        Screen()
        {
            map = {
                { normal,    terminal::id::NORMAL.toString()    },
                { alternate, terminal::id::ALTERNATE.toString() }
            };
        }

        enum
        {
            normal    = 0,
            alternate = 1
        };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (normal);
        }
    };

    struct Gpu : public jam::Map::Instance<Gpu>
    {
        Gpu()
        {
            map = {
                { off,       "false" },
                { on,        "true"  },
                { automatic, "auto"  }
            };
        }

        enum { off = 0, on = 1, automatic = 2 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (automatic);
        }
    };
};
