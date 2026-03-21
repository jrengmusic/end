/**
 * @file ScreenSelection.h
 * @brief Anchor-to-end text selection range with per-cell hit testing.
 *
 * `ScreenSelection` stores the two endpoints of a user text selection as
 * grid coordinates (column, row) and provides `contains()` for per-cell
 * hit testing during snapshot construction.
 *
 * ## Coordinate system
 *
 * Both `anchor` and `end` are in **grid space** — column and row indices
 * within the visible terminal grid, not pixel coordinates.  The anchor is
 * the point where the user started the selection (mouse-down); the end is
 * the current drag position (mouse-drag / mouse-up).
 *
 * ## Normalisation
 *
 * `contains()` normalises the two endpoints into reading order before
 * testing, so the struct is valid regardless of whether the user dragged
 * forwards or backwards.
 *
 * ## Rendering
 *
 * The selection is rendered as a transparent colour overlay on top of the
 * normal cell background.  `Screen::processCellForSnapshot()` calls
 * `contains()` for every cell and emits an additional `Render::Background`
 * quad using `Theme::selectionColour` when the cell is selected.
 *
 * @code
 * ScreenSelection sel;
 * sel.anchor = { 3, 2 };   // column 3, row 2
 * sel.end    = { 7, 4 };   // column 7, row 4
 *
 * bool hit = sel.contains (5, 3);  // true — row 3 is fully selected
 * @endcode
 *
 * @see Screen::setSelection()
 * @see Screen::processCellForSnapshot()
 */

#pragma once
#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct ScreenSelection
 * @brief Anchor + end grid-coordinate pair for terminal text selection.
 *
 * Stores the selection as two `juce::Point<int>` values in grid space.
 * The struct is intentionally minimal — it carries no state beyond the two
 * endpoints and performs no allocation.
 *
 * @par Thread context
 * **MESSAGE THREAD** — created and mutated on the message thread in response
 * to mouse events.  A const pointer is passed to `Screen::setSelection()` and
 * read during `render()` on the same thread.
 *
 * @see Screen::setSelection()
 * @see Screen::processCellForSnapshot()
 */
struct ScreenSelection
{
    /**
     * @enum SelectionType
     * @brief Determines which hit-test algorithm `containsCell()` dispatches to.
     */
    enum class SelectionType
    {
        linear, ///< Character-wise (Vim `v`): wraps across rows like normal text flow.
        line,   ///< Line-wise (Vim `V`): entire rows between anchor and end.
        box     ///< Block / rectangle (Vim `Ctrl+V` or mouse drag): strict column range per row.
    };

    juce::Point<int> anchor; ///< Selection start point (mouse-down position) in grid coordinates (col, row).
    juce::Point<int> end;    ///< Selection end point (current drag position) in grid coordinates (col, row).
    SelectionType    type { SelectionType::box }; ///< Algorithm used by `containsCell()` to test membership.

    /**
     * @brief Tests whether a grid cell falls within the selection.
     *
     * Normalises `anchor` and `end` into reading order (top-left to
     * bottom-right), then applies the following rules:
     *
     * - **Single row**: cell must satisfy `startCol ≤ col ≤ endCol`.
     * - **First row of multi-row**: cell must satisfy `col ≥ startCol`.
     * - **Last row of multi-row**: cell must satisfy `col ≤ endCol`.
     * - **Middle rows**: all columns are selected.
     *
     * @param col  Column index of the cell to test (0-based).
     * @param row  Row index of the cell to test (0-based).
     * @return     `true` if the cell at (@p col, @p row) is within the
     *             selection; `false` otherwise.
     *
     * @note The normalisation is performed on local copies; `anchor` and
     *       `end` are not modified.
     */
    bool contains (int col, int row) const noexcept
    {
        int startRow { anchor.y };
        int startCol { anchor.x };
        int endRow { end.y };
        int endCol { end.x };

        // Normalize to reading order
        if (startRow > endRow or (startRow == endRow and startCol > endCol))
        {
            std::swap (startRow, endRow);
            std::swap (startCol, endCol);
        }

        bool result { false };

        if (row >= startRow and row <= endRow)
        {
            if (startRow == endRow)
            {
                result = col >= startCol and col <= endCol;
            }
            else if (row == startRow)
            {
                result = col >= startCol;
            }
            else if (row == endRow)
            {
                result = col <= endCol;
            }
            else
            {
                result = true;
            }
        }

        return result;
    }

    /**
     * @brief Tests whether a grid cell falls within a box (rectangle) selection.
     *
     * A box selection uses the same column range for every row in the
     * selection — it is a strict rectangle, not a row-wrapped region.
     * Both axes are normalised independently so dragging in any direction
     * produces the correct rectangle.
     *
     * @param col  Column index of the cell to test (0-based).
     * @param row  Row index of the cell to test (0-based).
     * @return     `true` if the cell at (@p col, @p row) is within the
     *             rectangle defined by `anchor` and `end`; `false` otherwise.
     *
     * @note The normalisation is performed on local copies; `anchor` and
     *       `end` are not modified.
     */
    bool containsBox (int col, int row) const noexcept
    {
        const int startRow { std::min (anchor.y, end.y) };
        const int endRow   { std::max (anchor.y, end.y) };
        const int startCol { std::min (anchor.x, end.x) };
        const int endCol   { std::max (anchor.x, end.x) };

        return row >= startRow and row <= endRow
           and col >= startCol and col <= endCol;
    }

    /**
     * @brief Tests whether a grid cell falls within a line-wise selection.
     *
     * Returns `true` for every cell on every row between `anchor.y` and
     * `end.y` (inclusive), regardless of column.  This matches Vim `V`
     * line-wise visual mode, where entire lines are selected.
     *
     * @param col  Column index of the cell to test (0-based). Unused.
     * @param row  Row index of the cell to test (0-based).
     * @return     `true` if @p row is within the selected row range.
     *
     * @note The row range is normalised internally so dragging upward works.
     */
    bool containsLine (int col, int row) const noexcept
    {
        juce::ignoreUnused (col);
        const int startRow { std::min (anchor.y, end.y) };
        const int endRow   { std::max (anchor.y, end.y) };

        return row >= startRow and row <= endRow;
    }

    /**
     * @brief Unified cell membership test — dispatches based on `type`.
     *
     * Calls `contains()` for `SelectionType::linear`, `containsLine()` for
     * `SelectionType::line`, and `containsBox()` for `SelectionType::box`.
     *
     * This is the **only** entry point used by the renderer.  All three
     * selection variants are handled through this single call.
     *
     * @param col  Column index of the cell to test (0-based).
     * @param row  Row index of the cell to test (0-based).
     * @return     `true` if the cell is within the active selection.
     */
    bool containsCell (int col, int row) const noexcept
    {
        bool result { false };

        if (type == SelectionType::linear)
        {
            result = contains (col, row);
        }
        else if (type == SelectionType::line)
        {
            result = containsLine (col, row);
        }
        else
        {
            result = containsBox (col, row);
        }

        return result;
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
