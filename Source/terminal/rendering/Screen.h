#pragma once
#include <JuceHeader.h>
#include "../../lua/Engine.h"
#include "../data/Command.h"
#include "../data/State.h"
#include <jam_gui/text_editor/jam_caret_component.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Cell grid renderer — applies Commands to a growing Cells document
 *        and renders via the Glyph atlas pipeline.
 *
 * Owns a Viewport for scrollback. ContentView (inner component) does the
 * actual painting via ShapedText draw runs + Glyph::Graphics.
 */
class Screen : public juce::Component
{
public:
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
    void setDimensions (int newCols, int newRows) noexcept;

    /** @brief Applies Commands to the active Cells buffer, shapes, and repaints. */
    void makeLayout (const Command* commands, int count) noexcept;

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

    std::unique_ptr<juce::Viewport> viewport;
    ContentView* contentView { nullptr };
    std::unique_ptr<jam::CaretComponent> caret;

    jam::Owner<jam::Cells> cells;
    int activeScreen { 0 };

    jam::Glyph::ShapedText shapedText;
    jam::Glyph::Graphics glyphGraphics;

    int cols        { 0 };
    int visibleRows { 0 };

    int cellW    { 0 };     ///< Logical cell width in pixels.
    int cellH    { 0 };     ///< Logical cell height in pixels.
    int baseline { 0 };     ///< Baseline offset in pixels.
    float fontSize { 0.0f };

    /** @brief Computes cell metrics from the configured font. */
    void computeCellMetrics() noexcept;

    /** @brief Shapes active cells, resizes content, auto-scrolls, repaints. */
    void shapeAndRepaint() noexcept;

    /** @brief Renders shaped draw runs via Glyph::Graphics. Called by ContentView::paint. */
    void drawContent() noexcept;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
