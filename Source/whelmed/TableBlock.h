#pragma once

#include <JuceHeader.h>
#include "Block.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

//==============================================================================
/**
    Renders a single markdown table block inside a document stack.

    Contract:
      - Caller feeds raw table markdown (header + separator + data rows only).
      - Caller calls layout (width) before querying getPreferredHeight().
      - paint() is a pure draw from cache; all coordinates are relative to area origin.
 */
class TableBlock : public Block
{
public:
    //==========================================================================
    struct ColourScheme
    {
        juce::Colour background       { 0xFF1E1E2E };
        juce::Colour headerBackground { 0xFF2A2A3E };
        juce::Colour rowAlt           { 0xFF252535 };
        juce::Colour borderColour     { 0xFF3A3A5C };
        juce::Colour headerText       { 0xFFCDD6F4 };
        juce::Colour cellText         { 0xFFBAC2DE };
    };

    //==========================================================================
    explicit TableBlock (juce::Font bodyFont = juce::Font (juce::FontOptions().withPointHeight (14.0f)));
    ~TableBlock() override = default;

    //==========================================================================
    /** Feed raw markdown table string. Triggers parse; layout deferred until layout() is called. */
    void setTableMarkdown (const juce::String& markdown);

    void setColourScheme (const ColourScheme& scheme);

    /** Recomputes layout for a given width. Call before getPreferredHeight(). */
    void layout (int width);

    int  getPreferredHeight (int width) const noexcept override;
    void paint (juce::Graphics& g, juce::Rectangle<int> area) const override;

private:
    //==========================================================================
    enum class Align { Left, Centre, Right };

    struct Cell
    {
        juce::String text;
        Align        align { Align::Left };
    };

    struct ParsedTable
    {
        juce::Array<Cell>              headers;
        juce::Array<juce::Array<Cell>> rows;
        int                            numCols { 0 };

        bool isEmpty() const noexcept { return numCols == 0; }
    };

    //==========================================================================
    // Layout cache — rebuilt on every measureLayout() call
    struct LayoutCache
    {
        juce::Array<int> colWidths;
        juce::Array<int> rowHeights;
        int              headerHeight  { 0 };
        int              totalHeight   { 0 };
        int              measuredWidth { 0 };

        juce::OwnedArray<juce::TextLayout> headerLayouts;
        juce::OwnedArray<juce::TextLayout> cellLayouts;   // row-major

        bool isValid() const noexcept { return measuredWidth > 0; }
    };

    //==========================================================================
    // Core pipeline
    void             parseMarkdown  (const juce::String& markdown);
    void             measureLayout  (int forWidth);

    // Stateless measure helpers
    int              measureHeaderHeight (int forWidth, const juce::Array<int>& colWidths) const;
    int              measureRowHeight    (int rowIndex, int forWidth, const juce::Array<int>& colWidths) const;
    juce::Array<int> distributeColumns  (int forWidth) const;

    // TextLayout factory
    juce::TextLayout makeLayout (const Cell& cell, int colWidth, bool isHeader, int maxWidth) const;

    // Paint helpers
    void paintHeader  (juce::Graphics& g) const;
    void paintRows    (juce::Graphics& g) const;
    void paintBorders (juce::Graphics& g) const;

    // Geometry helper
    juce::Rectangle<int> getCellBounds (int row, int col) const;   // row = -1 for header

    //==========================================================================
    static constexpr int kCellPaddingH { 10 };
    static constexpr int kCellPaddingV { 6 };
    static constexpr int kBorderWidth  { 1 };
    static constexpr int kMinColWidth  { 48 };

    //==========================================================================
    juce::Font   font;
    juce::Font   headerFont;

    ParsedTable  table;
    LayoutCache  layoutCache;
    ColourScheme colours;

    int lastMeasuredWidth { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TableBlock)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
