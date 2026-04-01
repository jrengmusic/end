/**
 * @file SelectionType.h
 * @brief Application-level visual selection type enum.
 *
 * Identifies which Vim visual-selection variant is currently active.
 * Stored as a property on the AppState TABS subtree alongside ModalType.
 *
 * @see AppState
 * @see ModalType
 */

#pragma once

/**
 * @enum SelectionType
 * @brief Identifies which Vim visual-selection variant is currently active.
 *
 * 0 = none, 1 = visual, 2 = visualLine, 3 = visualBlock.
 */
enum class SelectionType
{
    none,        ///< No selection in progress.
    visual,      ///< Character-wise selection (Vim `v`).
    visualLine,  ///< Line-wise selection (Vim `V`).
    visualBlock  ///< Block / rectangle selection (Vim `Ctrl+V`).
};
