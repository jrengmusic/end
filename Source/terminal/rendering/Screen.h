#pragma once
#include <JuceHeader.h>
#include "../data/Identifier.h"

namespace Terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Cell grid renderer — thin adapter from Grid's raw Cell* API to
 *        TextEditor's Buffer<Cell> API.
 *
 * All content mutations go through the TextEditor content API:
 *   appendRow / setVisibleRow / setActiveScreen / setLiveBuffer.
 *
 * Display mediates all communication — Screen has no State or config reference.
 *
 * Live buffer:
 *   `live` holds the current visible frame from Grid.
 *   updateVisibleRow writes into `live`, then passes a Block<Cell> view to
 *   TextEditor::setLiveBuffer on each update.
 *   setLiveDimensions sizes `live` when terminal dimensions change.
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

    /** @brief Sizes the Live buffer to match terminal dimensions.
     *         Call from Display after terminal resize. */
    void setLiveDimensions (int numRows, int numCols) noexcept;

    /** @brief Writes one visible row into the Live buffer and pushes a Block<Cell>
     *         view to TextEditor via setLiveBuffer. */
    void updateVisibleRow (int row, const jam::Cell* src, int numCols) noexcept;

    /** @brief Appends scrolled-off rows to the normal ring via appendRow. */
    void append (const jam::Cell* const* rows, int rowCount, int numCols) noexcept;

private:
    jam::Buffer<jam::Cell>     live;          ///< Current visible frame — one row per terminal row.
    jam::Buffer<jam::Grapheme> liveGrapheme;  ///< Lazy grapheme sidecar for live frame. Sized with live.

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
