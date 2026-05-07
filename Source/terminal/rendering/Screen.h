#pragma once
#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

class Screen : public jam::TextEditor
{
public:
    enum ColourIds
    {
        defaultForegroundColourId = 0x3000001,
        defaultBackgroundColourId = 0x3000002,
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

    Screen() noexcept;

    ~Screen() = default;

    /** Renders the preview content at the given cell metrics.
     *  Called by Display after computing its authoritative cell dimensions.
     *  Screen owns the preview pen data; Display owns the metrics.
     */
    void setText (jam::Bounds cellSize, int bl) noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
