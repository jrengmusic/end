/**
 * @file AppLayout.h
 * @brief Populates an application-level ValueTree from an XML parameter schema.
 *
 * AppLayout walks the AppParameters.xml document and sets properties directly
 * on ValueTree nodes — no AnyMap, no Atom, no State reference required.
 * Properties are typed (int, float, bool, string) and resolved to juce::var
 * before being stored.
 *
 * @see AppState
 * @see AppIdentifier.h
 */

#pragma once

#include <JuceHeader.h>

struct AppState;

/**
 * @struct AppLayout
 * @brief Builds the application ValueTree and Atom parameter table from an XML schema.
 *
 * Walks AppParameters.xml: int/bool PARAMs are registered as Atom parameters via
 * AppState::addParameter(); float/string PARAMs are set as direct ValueTree properties.
 * Group elements (WINDOW, TABS) become child ValueTree nodes holding their own PARAMs.
 *
 * @see AppState
 * @see AppIdentifier.h
 */
struct AppLayout
{
    /**
     * @brief Walks @p xml and populates @p appState with all declared parameters.
     *
     * Top-level PARAM elements of type int/bool are added to the END AnyMap group
     * and appended to the root ValueTree node.  float/string PARAMs are set as
     * direct VT properties on the root node.
     *
     * Any other top-level element (WINDOW, TABS) creates an AnyMap group and a
     * child ValueTree node whose PARAM children are registered or set accordingly.
     *
     * @param xml       Root XmlElement of the parameter schema document.
     * @param appState  AppState to populate (AnyMap groups and ValueTree modified in-place).
     */
    static void build (const juce::XmlElement& xml, AppState& appState);

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

    static int resolveIntDefault (const juce::XmlElement& elem,
                                  const Boolean& boolMap) noexcept;
};
