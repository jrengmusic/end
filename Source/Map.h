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

    struct Cursor : public jam::Map::Instance<Cursor>
    {
        Cursor()
        {
            map = {
                { block,     "block"     },
                { underline, "underline" },
                { bar,       "bar"       }
            };
        }

        enum { block = 1, underline = 3, bar = 5 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (block);
        }
    };

    struct TabPosition : public jam::Map::Instance<TabPosition>
    {
        TabPosition()
        {
            map = {
                { top,    "top"    },
                { bottom, "bottom" },
                { left,   "left"   },
                { right,  "right"  }
            };
        }

        // Values match jam::TabbedButtonBar::Orientation: TabsAtTop=0, TabsAtBottom=1, TabsAtLeft=2, TabsAtRight=3.
        enum { top = 0, bottom = 1, left = 2, right = 3 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (left);
        }
    };

    struct Renderer : public jam::Map::Instance<Renderer>
    {
        Renderer()
        {
            map = {
                { gpu, "gpu" },
                { cpu, "cpu" }
            };
        }

        enum { gpu = 0, cpu = 1 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (gpu);
        }
    };

    struct PaneType : public jam::Map::Instance<PaneType>
    {
        PaneType()
        {
            map = {
                { terminal, "terminal" },
                { document, "document" }
            };
        }

        enum { terminal = 0, document = 1 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (terminal);
        }
    };

    struct Direction : public jam::Map::Instance<Direction>
    {
        Direction()
        {
            map = {
                { vertical,   "vertical"   },
                { horizontal, "horizontal" }
            };
        }

        enum { vertical = 0, horizontal = 1 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (vertical);
        }
    };

    struct Position : public jam::Map::Instance<Position>
    {
        Position()
        {
            map = {
                { top,    "top"    },
                { center, "center" },
                { bottom, "bottom" }
            };
        }

        enum { top = 0, center = 1, bottom = 2 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (top);
        }
    };

    struct LinkHandler : public jam::Map::Instance<LinkHandler>
    {
        LinkHandler()
        {
            map = {
                { whelmed, "whelmed" },
                { image,   "image"   }
            };
        }

        enum { whelmed = 0, image = 1 };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (whelmed);
        }
    };
};
