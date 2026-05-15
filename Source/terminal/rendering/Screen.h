#pragma once
#include <JuceHeader.h>
#include "../data/Identifier.h"

namespace Terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Cell grid renderer — scrollback ring + alternate buffer for the terminal.
 *
 * Owns buffers[0] (normal scrollback ring, inherited from TextEditor) and
 * buffers[1] (alternate screen, added in constructor).
 *
 * Display mediates all communication — Screen has no State or config reference.
 * Visible rows are shaped directly from Grid by Display under lock via shapeContent().
 */
class Screen : public jam::TextEditor
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

    Screen() noexcept;
    ~Screen() override;

    /** @brief Appends scrolled-off rows to the normal ring via appendRow. */
    void append (const jam::Cell* const* rows, int rowCount, int numCols) noexcept;

    /** @brief Copies visible rows from Grid into the live buffer (buffers[2]).
     *         Single calc() after all rows are written. No per-row overhead.
     *  @param rows      Array of row pointers (one per visible row).
     *  @param numRows   Number of rows to copy.
     *  @param numCols   Column count per row. */
    void updateLiveRows (const jam::Cell* const* rows, int numRows, int numCols) noexcept;

    /** @brief Sizes the live buffer (buffers[2]) for terminal dimensions. */
    void setLiveDimensions (int numRows, int numCols) noexcept;

private:

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
