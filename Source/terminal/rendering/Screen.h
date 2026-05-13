#pragma once
#include <JuceHeader.h>
#include "../../lua/Engine.h"
#include "../data/State.h"
#include <jam_gui/text_editor/jam_caret_component.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Cell grid renderer — receives row data from Grid via Display
 *        and renders via the Glyph atlas pipeline.
 *
 * Owns a Viewport for scrollback. ContentView (inner component) does the
 * actual painting via Arrangement draw runs + glyph::Graphics.
 */
class Screen : public juce::Component
{
public:
    struct Map : public jam::Map::Instance<Map>
    {
        Map()
        {
            map = {
                { normal,    ID::NORMAL.toString() },
                { alternate, ID::ALTERNATE.toString() }
            };
        }

        enum
        {
            normal = 0,
            alternate
        };

        const juce::String& getDefault() const noexcept override
        {
            return map.at (normal);
        }
    };

    enum ColourIds
    {
        cursorColourId          = 0x3000003,
        selectionColourId       = 0x3000004,
        selectionCursorColourId = 0x3000005,
        hintLabelFgColourId     = 0x3000006,
        hintLabelBgColourId     = 0x3000007,
        ansi0ColourId  = 0x3000010,
        ansi1ColourId  = 0x3000011,
        ansi2ColourId  = 0x3000012,
        ansi3ColourId  = 0x3000013,
        ansi4ColourId  = 0x3000014,
        ansi5ColourId  = 0x3000015,
        ansi6ColourId  = 0x3000016,
        ansi7ColourId  = 0x3000017,
        ansi8ColourId  = 0x3000018,
        ansi9ColourId  = 0x3000019,
        ansi10ColourId = 0x300001A,
        ansi11ColourId = 0x300001B,
        ansi12ColourId = 0x300001C,
        ansi13ColourId = 0x300001D,
        ansi14ColourId = 0x300001E,
        ansi15ColourId = 0x300001F,
    };

    explicit Screen (State& state) noexcept;
    ~Screen() override;

    /** @brief Sets terminal dimensions. Called by Display on resize. */
    void setDimensions (int newCols, int newRows, const jam::Font& font) noexcept;

    /** @brief Appends multiple scroll-off rows to the scrollback region. Called by Display.
     *  Performs a single memmove for all rows (O(n) per frame, not per row). */
    void appendScrollbackRows (const jam::Cell* const* rows, int rowCount, int numCols) noexcept;

    /** @brief Appends a single scroll-off row. Convenience wrapper. */
    void appendScrollbackRow (const jam::Cell* src, int numCols) noexcept;

    /** @brief Updates a visible row from Grid data. Called by Display for dirty rows. */
    void updateVisibleRow (int row, const jam::Cell* src, int numCols) noexcept;

    /** @brief Triggers reshaping and repainting. Called by Display after updating rows. */
    void repaintContent() noexcept;

    /** @brief Sets the active screen buffer. Called by Display on screen change. */
    void setActive (int index) noexcept;

    /** @brief Positions the caret at cell index. Called by Display. */
    void setCaretPosition (int index) noexcept;

    /** @brief Sets the scrollbar thickness in pixels. */
    void setScrollBarThickness (int thickness) noexcept;

    /** @internal */
    void resized() override;

private:
    struct ContentView;

    const lua::Engine& config { *lua::Engine::getContext() };
    State& state;

    std::unique_ptr<ContentView> contentView;
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<jam::CaretComponent> caret;

    jam::Owner<jam::Cells> cells;
    int scrollbackRows { 0 };  ///< Number of scrollback rows stored before visible rows.

    jam::glyph::Arrangement arrangement;
    jam::glyph::Graphics glyphGraphics;

    /** @brief Shapes active cells, resizes content, auto-scrolls, repaints. */
    void shapeAndRepaint (const jam::Font& font) noexcept;

    /** @brief Renders shaped draw runs via glyph::Graphics. Called by ContentView::paint. */
    void drawContent() noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
