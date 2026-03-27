#include "TableBlock.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

//==============================================================================
TableBlock::TableBlock (juce::Font bodyFont)
    : font       (bodyFont),
      headerFont (bodyFont.withStyle (juce::Font::bold))
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
}

//==============================================================================
void TableBlock::setTableMarkdown (const juce::String& markdown)
{
    parseMarkdown (markdown);

    if (getWidth() > 0)
        measureLayout (getWidth());
}

int TableBlock::getPreferredHeight() const noexcept
{
    return layout.totalHeight;
}

int TableBlock::getPreferredHeight (int forWidth) const
{
    int result { 0 };

    if (not table.isEmpty() and forWidth > 0)
    {
        const auto colWidths { distributeColumns (forWidth) };

        int h { measureHeaderHeight (forWidth, colWidths) };

        for (int r { 0 }; r < table.rows.size(); ++r)
            h += measureRowHeight (r, forWidth, colWidths);

        // borders: top + bottom + header-bottom + one per data row
        h += kBorderWidth * (2 + 1 + table.rows.size());

        result = h;
    }

    return result;
}

void TableBlock::setColourScheme (const ColourScheme& scheme)
{
    colours = scheme;
    repaint();
}

//==============================================================================
void TableBlock::paint (juce::Graphics& g)
{
    if (layout.isValid())
    {
        g.fillAll (colours.background);

        paintRows      (g);
        paintHeader    (g);
        paintSelection (g);
        paintBorders   (g);
    }
}

void TableBlock::resized()
{
    const int w { getWidth() };

    if (w != lastMeasuredWidth and w > 0)
    {
        measureLayout (w);
        lastMeasuredWidth = w;
    }
}

//==============================================================================
void TableBlock::mouseDown (const juce::MouseEvent& e)
{
    const auto pos { cellAtPoint (e.getPosition()) };

    if (pos.isValid())
    {
        selection.anchor = pos;
        selection.pivot  = pos;
        isDragging       = true;
    }
    else
    {
        selection = {};
    }

    repaint();
}

void TableBlock::mouseDrag (const juce::MouseEvent& e)
{
    if (isDragging)
    {
        const auto pos { cellAtPoint (e.getPosition()) };

        if (pos.isValid())
        {
            selection.pivot = pos;
            repaint();
        }
    }
}

void TableBlock::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
}

bool TableBlock::keyPressed (const juce::KeyPress& key)
{
    const bool isCopy { key == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)
                     or key == juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0) };

    bool handled { false };

    if (isCopy and selection.isActive())
    {
        copySelectionToClipboard();
        handled = true;
    }

    return handled;
}

//==============================================================================
// Parsing
//==============================================================================
void TableBlock::parseMarkdown (const juce::String& markdown)
{
    table = {};

    const auto lines { juce::StringArray::fromLines (markdown.trim()) };

    if (lines.size() >= 2)
    {
        // Helper: split a markdown row into trimmed cells, strip leading/trailing pipes
        auto splitRow = [] (const juce::String& line) -> juce::StringArray
        {
            auto s { line.trim() };

            if (s.startsWith ("|")) s = s.substring (1);
            if (s.endsWith   ("|")) s = s.dropLastCharacters (1);

            juce::StringArray cells;
            cells.addTokens (s, "|", "");

            for (auto& c : cells)
                c = c.trim();

            return cells;
        };

        // Parse header row
        const auto headerCells { splitRow (lines[0]) };
        table.numCols = headerCells.size();

        for (const auto& h : headerCells)
        {
            Cell c;
            c.text  = h;
            c.align = Align::Left;
            table.headers.add (c);
        }

        // Parse separator row for alignment hints
        juce::Array<Align> alignments;

        for (const auto& sep : splitRow (lines[1]))
        {
            const bool left  { sep.startsWith (":") };
            const bool right { sep.endsWith (":") };

            if (left and right)
                alignments.add (Align::Centre);
            else if (right)
                alignments.add (Align::Right);
            else
                alignments.add (Align::Left);
        }

        // Apply alignments to headers
        for (int i { 0 }; i < juce::jmin (alignments.size(), table.headers.size()); ++i)
            table.headers.getReference (i).align = alignments[i];

        // Parse data rows (skip header + separator)
        for (int lineIdx { 2 }; lineIdx < lines.size(); ++lineIdx)
        {
            const auto rawLine { lines[lineIdx].trim() };

            if (not rawLine.isEmpty())
            {
                const auto cells { splitRow (rawLine) };
                juce::Array<Cell> row;

                for (int c { 0 }; c < table.numCols; ++c)
                {
                    Cell cell;
                    cell.text  = c < cells.size() ? cells[c] : juce::String{};
                    cell.align = c < alignments.size() ? alignments[c] : Align::Left;
                    row.add (cell);
                }

                table.rows.add (row);
            }
        }
    }
}

//==============================================================================
// Layout
//==============================================================================
juce::Array<int> TableBlock::distributeColumns (int forWidth) const
{
    juce::Array<int> result;

    if (table.numCols > 0)
    {
        // Measure natural (single-line) width of each column
        juce::Array<float> weights;
        float totalWeight { 0.0f };

        for (int c { 0 }; c < table.numCols; ++c)
        {
            float maxW { headerFont.getStringWidthFloat (table.headers[c].text)
                       + kCellPaddingH * 2 };

            for (const auto& row : table.rows)
            {
                if (c < row.size())
                {
                    const float w { font.getStringWidthFloat (row[c].text)
                                  + kCellPaddingH * 2 };
                    maxW = juce::jmax (maxW, w);
                }
            }

            maxW = juce::jmax (maxW, (float) kMinColWidth);
            weights.add (maxW);
            totalWeight += maxW;
        }

        // Available width minus borders
        const int available { forWidth - kBorderWidth * (table.numCols + 1) };

        int allocated { 0 };

        for (int c { 0 }; c < table.numCols; ++c)
        {
            const int w { (c < table.numCols - 1)
                        ? juce::roundToInt ((weights[c] / totalWeight) * available)
                        : available - allocated };

            result.add (juce::jmax (w, kMinColWidth));
            allocated += result.getLast();
        }
    }

    return result;
}

juce::TextLayout TableBlock::makeLayout (const Cell& cell, int colWidth,
                                         bool isHeader, int /*maxWidth*/) const
{
    const int innerWidth { juce::jmax (1, colWidth - kCellPaddingH * 2) };

    juce::AttributedString as;
    as.setText (cell.text);
    as.setFont (isHeader ? headerFont : font);
    as.setColour (isHeader ? colours.headerText : colours.cellText);

    switch (cell.align)
    {
        case Align::Centre: as.setJustification (juce::Justification::centredTop);    break;
        case Align::Right:  as.setJustification (juce::Justification::topRight);      break;
        default:            as.setJustification (juce::Justification::topLeft);       break;
    }

    juce::TextLayout tl;
    tl.createLayout (as, (float) innerWidth);

    return tl;
}

int TableBlock::measureHeaderHeight (int /*forWidth*/, const juce::Array<int>& colWidths) const
{
    int maxH { 0 };

    for (int c { 0 }; c < table.headers.size(); ++c)
    {
        const int colW { c < colWidths.size() ? colWidths[c] : kMinColWidth };
        const auto tl  { makeLayout (table.headers[c], colW, true, colW) };
        maxH = juce::jmax (maxH, juce::roundToInt (tl.getHeight()));
    }

    return maxH + kCellPaddingV * 2;
}

int TableBlock::measureRowHeight (int rowIndex, int /*forWidth*/,
                                   const juce::Array<int>& colWidths) const
{
    int result { 0 };

    if (rowIndex < table.rows.size())
    {
        const auto& row { table.rows[rowIndex] };
        int maxH { 0 };

        for (int c { 0 }; c < row.size(); ++c)
        {
            const int colW { c < colWidths.size() ? colWidths[c] : kMinColWidth };
            const auto tl  { makeLayout (row[c], colW, false, colW) };
            maxH = juce::jmax (maxH, juce::roundToInt (tl.getHeight()));
        }

        result = maxH + kCellPaddingV * 2;
    }

    return result;
}

void TableBlock::measureLayout (int forWidth)
{
    layout = {};

    if (not table.isEmpty() and forWidth > 0)
    {
        layout.measuredWidth = forWidth;
        layout.colWidths     = distributeColumns (forWidth);

        // Header
        layout.headerHeight = measureHeaderHeight (forWidth, layout.colWidths);

        for (int c { 0 }; c < table.headers.size(); ++c)
        {
            const int colW { c < layout.colWidths.size() ? layout.colWidths[c] : kMinColWidth };
            auto* tl { new juce::TextLayout (makeLayout (table.headers[c], colW, true, colW)) };
            layout.headerLayouts.add (tl);
        }

        // Data rows
        for (int r { 0 }; r < table.rows.size(); ++r)
        {
            layout.rowHeights.add (measureRowHeight (r, forWidth, layout.colWidths));

            const auto& row { table.rows[r] };

            for (int c { 0 }; c < table.numCols; ++c)
            {
                const int colW { c < layout.colWidths.size() ? layout.colWidths[c] : kMinColWidth };
                const Cell& cell { c < row.size() ? row[c] : Cell{} };
                auto* tl { new juce::TextLayout (makeLayout (cell, colW, false, colW)) };
                layout.cellLayouts.add (tl);
            }
        }

        // Total height: borders + header + all rows
        layout.totalHeight = kBorderWidth;   // top border
        layout.totalHeight += layout.headerHeight;
        layout.totalHeight += kBorderWidth;  // header bottom

        for (int r { 0 }; r < layout.rowHeights.size(); ++r)
        {
            layout.totalHeight += layout.rowHeights[r];
            layout.totalHeight += kBorderWidth;
        }

        // Notify parent the preferred height changed
        setSize (forWidth, layout.totalHeight);
    }
}

//==============================================================================
// Geometry
//==============================================================================
juce::Rectangle<int> TableBlock::getCellBounds (int row, int col) const
{
    juce::Rectangle<int> result;

    if (layout.isValid() and col >= 0 and col < layout.colWidths.size())
    {
        // X: sum of preceding column widths + borders
        int x { kBorderWidth };

        for (int c { 0 }; c < col; ++c)
            x += layout.colWidths[c] + kBorderWidth;

        const int w { layout.colWidths[col] };

        // Y: header or data row
        if (row == -1)
        {
            result = { x, kBorderWidth, w, layout.headerHeight };
        }
        else
        {
            int y { kBorderWidth + layout.headerHeight + kBorderWidth };

            for (int r { 0 }; r < row; ++r)
                y += layout.rowHeights[r] + kBorderWidth;

            result = { x, y, w, layout.rowHeights[row] };
        }
    }

    return result;
}

TableBlock::CellPos TableBlock::cellAtPoint (juce::Point<int> p) const
{
    CellPos result;

    if (layout.isValid())
    {
        for (int col { 0 }; col < table.numCols; ++col)
        {
            // Header
            if (getCellBounds (-1, col).contains (p))
            {
                result = { -1, col };
                break;
            }

            // Data rows
            for (int row { 0 }; row < table.rows.size(); ++row)
            {
                if (getCellBounds (row, col).contains (p))
                {
                    result = { row, col };
                    break;
                }
            }

            if (result.isValid())
                break;
        }
    }

    return result;
}

//==============================================================================
// Paint
//==============================================================================
void TableBlock::paintHeader (juce::Graphics& g) const
{
    for (int c { 0 }; c < table.headers.size(); ++c)
    {
        const auto bounds { getCellBounds (-1, c) };
        g.setColour (colours.headerBackground);
        g.fillRect (bounds);

        if (c < layout.headerLayouts.size())
        {
            const auto inner { bounds.reduced (kCellPaddingH, kCellPaddingV).toFloat() };
            layout.headerLayouts[c]->draw (g, inner);
        }
    }
}

void TableBlock::paintRows (juce::Graphics& g) const
{
    for (int r { 0 }; r < table.rows.size(); ++r)
    {
        const bool isAlt { (r % 2) == 1 };

        for (int c { 0 }; c < table.numCols; ++c)
        {
            const auto bounds { getCellBounds (r, c) };

            g.setColour (isAlt ? colours.rowAlt : colours.background);
            g.fillRect (bounds);

            const int layoutIdx { r * table.numCols + c };

            if (layoutIdx < layout.cellLayouts.size())
            {
                const auto inner { bounds.reduced (kCellPaddingH, kCellPaddingV).toFloat() };
                layout.cellLayouts[layoutIdx]->draw (g, inner);
            }
        }
    }
}

void TableBlock::paintSelection (juce::Graphics& g) const
{
    if (selection.isActive())
    {
        for (int r { selection.minRow() }; r <= selection.maxRow(); ++r)
        {
            for (int c { selection.minCol() }; c <= selection.maxCol(); ++c)
            {
                const auto bounds { getCellBounds (r, c) };

                g.setColour (colours.selectionFill);
                g.fillRect (bounds);

                g.setColour (colours.selectionBorder);
                g.drawRect (bounds, 1);
            }
        }
    }
}

void TableBlock::paintBorders (juce::Graphics& g) const
{
    g.setColour (colours.borderColour);

    const int totalW { getWidth() };
    const int totalH { layout.totalHeight };

    // Outer border
    g.drawRect (0, 0, totalW, totalH, kBorderWidth);

    // Vertical column borders
    int x { kBorderWidth };

    for (int c { 0 }; c < layout.colWidths.size() - 1; ++c)
    {
        x += layout.colWidths[c];
        g.drawVerticalLine (x, 0.0f, (float) totalH);
        x += kBorderWidth;
    }

    // Horizontal header bottom border (slightly brighter)
    const int headerBottom { kBorderWidth + layout.headerHeight };
    g.setColour (colours.borderColour.brighter (0.3f));
    g.drawHorizontalLine (headerBottom, 0.0f, (float) totalW);

    // Horizontal row borders
    g.setColour (colours.borderColour);
    int y { headerBottom + kBorderWidth };

    for (int r { 0 }; r < layout.rowHeights.size() - 1; ++r)
    {
        y += layout.rowHeights[r];
        g.drawHorizontalLine (y, 0.0f, (float) totalW);
        y += kBorderWidth;
    }
}

//==============================================================================
// Clipboard
//==============================================================================
juce::String TableBlock::getCellText (int row, int col) const
{
    juce::String result;

    if (row == -1)
    {
        result = col < table.headers.size() ? table.headers[col].text : juce::String{};
    }
    else if (row < table.rows.size())
    {
        const auto& r { table.rows[row] };
        result = col < r.size() ? r[col].text : juce::String{};
    }

    return result;
}

void TableBlock::copySelectionToClipboard() const
{
    if (selection.isActive())
    {
        juce::String result;

        for (int r { selection.minRow() }; r <= selection.maxRow(); ++r)
        {
            for (int c { selection.minCol() }; c <= selection.maxCol(); ++c)
            {
                result += getCellText (r, c);

                if (c < selection.maxCol())
                    result += "\t";
            }

            result += "\n";
        }

        juce::SystemClipboard::copyTextToClipboard (result);
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
