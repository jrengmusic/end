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
    juce::Point<int> anchor; ///< Selection start point (mouse-down position) in grid coordinates (col, row).
    juce::Point<int> end;    ///< Selection end point (current drag position) in grid coordinates (col, row).

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
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
