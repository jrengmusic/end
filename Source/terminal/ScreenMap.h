#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace terminal
{
/*____________________________________________________________________________*/

struct ScreenMap : public jam::Map::Instance<ScreenMap>
{
    ScreenMap()
    {
        map = {
            { normal,    id::NORMAL.toString()    },
            { alternate, id::ALTERNATE.toString() }
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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
