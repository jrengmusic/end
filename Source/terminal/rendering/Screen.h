#pragma once
#include <JuceHeader.h>
#include "../data/Identifier.h"

namespace Terminal
{
/*____________________________________________________________________________*/

class State;
class Grid;

/**
 * @brief Cell grid renderer — scrollback ring + alternate buffer for the terminal.
 *
 * Listens to State's ValueTree and re-renders from Grid on every flush.
 * Display mediates all communication — Screen owns its State and Grid references
 * and drives itself on valueTreePropertyChanged.
 */
class Screen : public jam::TextEditor,
               public juce::ValueTree::Listener
{
public:
    struct Map : public jam::Map::Instance<Map>
    {
        Map()
        {
            map = {
                { normal,    ID::NORMAL.toString()    },
                { alternate, ID::ALTERNATE.toString() }
            };
        }

        enum
        {
            normal    = 0,
            alternate = 1
        };

        const juce::String& getDefault() const noexcept override { return map.at (normal); }
    };

    enum ColourIds
    {
        cursorColourId          = 0x3000003,
        selectionColourId       = 0x3000004,
        selectionCursorColourId = 0x3000005,
        hintLabelFgColourId     = 0x3000006,
        hintLabelBgColourId     = 0x3000007,
        ansi0ColourId           = 0x3000010,
        ansi1ColourId           = 0x3000011,
        ansi2ColourId           = 0x3000012,
        ansi3ColourId           = 0x3000013,
        ansi4ColourId           = 0x3000014,
        ansi5ColourId           = 0x3000015,
        ansi6ColourId           = 0x3000016,
        ansi7ColourId           = 0x3000017,
        ansi8ColourId           = 0x3000018,
        ansi9ColourId           = 0x3000019,
        ansi10ColourId          = 0x300001A,
        ansi11ColourId          = 0x300001B,
        ansi12ColourId          = 0x300001C,
        ansi13ColourId          = 0x300001D,
        ansi14ColourId          = 0x300001E,
        ansi15ColourId          = 0x300001F,
    };

    Screen (Terminal::State& state, Terminal::Grid& grid) noexcept;
    ~Screen() override;

    // juce::Component
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    Terminal::State& state;
    Terminal::Grid& grid;
    juce::ValueTree stateTree;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
