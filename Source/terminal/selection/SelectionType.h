/**
 * @file SelectionType.h
 * @brief Vim-style visual selection type enum.
 */

#pragma once

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @enum SelectionType
 * @brief Identifies which Vim visual-selection variant is currently active.
 *
 * Stored in State::selectionType via parameterMap as an integer cast.
 * 0 = none, 1 = visual, 2 = visualLine, 3 = visualBlock.
 */
enum class SelectionType
{
    none,        ///< No selection in progress (selection mode may still be active).
    visual,      ///< Character-wise selection (Vim `v`).
    visualLine,  ///< Line-wise selection (Vim `V`).
    visualBlock  ///< Block / rectangle selection (Vim `Ctrl+V`).
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
