#pragma once
#include <JuceHeader.h>
#include "../Identifier.h"
#include "../../Map.h"
#include "../State.h"
#include "../Grid.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Cell grid renderer — scrollback ring + alternate buffer for the terminal.
 *
 * Listens to State's ValueTree and re-renders from Grid on every flush.
 * Display mediates all communication — Screen owns its State and Grid references
 * and drives itself on valueTreePropertyChanged.
 */
class Screen : public jam::TextEditor
{
public:
    enum ColourIds
    {
        cursorColourId = 0x3000003,
        selectionColourId = 0x3000004,
        selectionCursorColourId = 0x3000005,
        hintLabelFgColourId = 0x3000006,
        hintLabelBgColourId = 0x3000007,
        ansi0ColourId = 0x3000010,
        ansi1ColourId = 0x3000011,
        ansi2ColourId = 0x3000012,
        ansi3ColourId = 0x3000013,
        ansi4ColourId = 0x3000014,
        ansi5ColourId = 0x3000015,
        ansi6ColourId = 0x3000016,
        ansi7ColourId = 0x3000017,
        ansi8ColourId = 0x3000018,
        ansi9ColourId = 0x3000019,
        ansi10ColourId = 0x300001A,
        ansi11ColourId = 0x300001B,
        ansi12ColourId = 0x300001C,
        ansi13ColourId = 0x300001D,
        ansi14ColourId = 0x300001E,
        ansi15ColourId = 0x300001F,
    };

    Screen (State& state, Grid& grid) noexcept;
    ~Screen() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    State& terminal;
    Grid& grid;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
