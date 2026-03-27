#pragma once

#include <JuceHeader.h>
#include "Block.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

//==============================================================================
/**
    Renders a single markdown table block inside a Viewport-hosted document stack.

    Contract:
      - Caller feeds raw table markdown (header + separator + data rows only).
      - Caller sets width via setBounds / setSize.
      - Caller queries getPreferredHeight() after setBounds, or getPreferredHeight (int forWidth) before committing bounds.
      - resized() re-runs the full measure pass; paint() is a pure draw from cache.
      - Text selection (rectangular cell range) + Cmd/Ctrl+C copy as TSV.
 */
class TableBlock : public Block
{
public:
    //==========================================================================
    struct ColourScheme
    {
        juce::Colour background        { 0xFF1E1E2E };
        juce::Colour headerBackground  { 0xFF2A2A3E };
        juce::Colour rowAlt            { 0xFF252535 };
        juce::Colour borderColour      { 0xFF3A3A5C };
        juce::Colour headerText        { 0xFFCDD6F4 };
        juce::Colour cellText          { 0xFFBAC2DE };
        juce::Colour selectionFill     { 0x3389B4FA };
        juce::Colour selectionBorder   { 0xFF89B4FA };
    };

    //==========================================================================
    explicit TableBlock (juce::Font bodyFont = juce::Font (juce::FontOptions().withPointHeight (14.0f)));
    ~TableBlock() override = default;

    //==========================================================================
    /** Feed raw markdown table string. Triggers parse + layout if width is known. */
    void setTableMarkdown (const juce::String& markdown);

    /** Returns the cached total height from the last measureLayout() call. */
    int getPreferredHeight() const noexcept override;

    /** Pure measure — does not mutate layout cache. Safe to call before setBounds. */
    int getPreferredHeight (int forWidth) const;

    void setColourScheme (const ColourScheme& scheme);

    //==========================================================================
    // juce::Component
    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;
    void mouseUp    (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    //==========================================================================
    enum class Align { Left, Centre, Right };

    struct Cell
    {
        juce::String  text;
        Align         align { Align::Left };
    };

    struct ParsedTable
    {
        juce::Array<Cell>               headers;
        juce::Array<juce::Array<Cell>>  rows;
        int                             numCols { 0 };

        bool isEmpty() const noexcept { return numCols == 0; }
    };

    //==========================================================================
    // Layout cache — rebuilt on every measureLayout() call
    struct LayoutCache
    {
        juce::Array<int>  colWidths;    // pixel width per column
        juce::Array<int>  rowHeights;   // pixel height per data row
        int               headerHeight  { 0 };
        int               totalHeight   { 0 };
        int               measuredWidth { 0 };

        // Prebuilt TextLayouts for paint — avoids re-wrapping in paint()
        // Index: [row][-1 = header | 0..N = data rows][col]
        // Stored flat: headers first, then row0col0, row0col1 ...
        juce::OwnedArray<juce::TextLayout> headerLayouts;
        juce::OwnedArray<juce::TextLayout> cellLayouts;   // row-major

        bool isValid() const noexcept { return measuredWidth > 0; }
    };

    //==========================================================================
    // Selection state
    struct CellPos
    {
        int row { -1 };  // -1 = header
        int col { -1 };

        bool isValid() const noexcept { return col >= 0; }
        bool operator== (const CellPos& o) const noexcept { return row == o.row and col == o.col; }
    };

    struct Selection
    {
        CellPos anchor;
        CellPos pivot;

        bool isActive() const noexcept { return anchor.isValid() and pivot.isValid(); }

        int minRow() const noexcept { return juce::jmin (anchor.row, pivot.row); }
        int maxRow() const noexcept { return juce::jmax (anchor.row, pivot.row); }
        int minCol() const noexcept { return juce::jmin (anchor.col, pivot.col); }
        int maxCol() const noexcept { return juce::jmax (anchor.col, pivot.col); }

        bool contains (int row, int col) const noexcept
        {
            return isActive()
                and row >= minRow() and row <= maxRow()
                and col >= minCol() and col <= maxCol();
        }
    };

    //==========================================================================
    // Core pipeline
    void         parseMarkdown (const juce::String& markdown);
    void         measureLayout (int forWidth);

    // Stateless measure helpers
    int          measureHeaderHeight   (int forWidth, const juce::Array<int>& colWidths) const;
    int          measureRowHeight      (int rowIndex, int forWidth,
                                        const juce::Array<int>& colWidths) const;
    juce::Array<int> distributeColumns (int forWidth) const;

    // TextLayout factory
    juce::TextLayout makeLayout (const Cell& cell, int colWidth,
                                 bool isHeader, int maxWidth) const;

    // Paint helpers
    void paintHeader    (juce::Graphics& g) const;
    void paintRows      (juce::Graphics& g) const;
    void paintSelection (juce::Graphics& g) const;
    void paintBorders   (juce::Graphics& g) const;

    // Geometry helpers
    juce::Rectangle<int> getCellBounds (int row, int col) const;   // row = -1 for header
    CellPos              cellAtPoint   (juce::Point<int> p) const;

    // Selection / clipboard
    void copySelectionToClipboard() const;
    juce::String getCellText (int row, int col) const;

    //==========================================================================
    static constexpr int kCellPaddingH { 10 };
    static constexpr int kCellPaddingV { 6 };
    static constexpr int kBorderWidth  { 1 };
    static constexpr int kMinColWidth  { 48 };

    //==========================================================================
    juce::Font   font;
    juce::Font   headerFont;

    ParsedTable  table;
    LayoutCache  layout;
    ColourScheme colours;

    Selection    selection;
    bool         isDragging { false };

    int          lastMeasuredWidth { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TableBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
