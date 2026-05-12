/**
 * @file AppLayout.h
 * @brief XML-driven parameter schema walker for AppState.
 *
 * Walks AppParameters.xml and populates AppState's ValueTree with PARAM
 * children and Parameter<int> adapters. Follows Terminal::Layout pattern with
 * flat AnyMap (no nested groups).
 *
 * @see AppState
 * @see AppParameters.xml
 */

#pragma once

#include <JuceHeader.h>
#include "AppIdentifier.h"

struct AppState;

struct AppLayout
{
    static void build (const juce::XmlElement& xml, AppState& state);

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

    static juce::var resolveDefault (const juce::XmlElement& elem,
                                     const Boolean& boolMap) noexcept;
};
