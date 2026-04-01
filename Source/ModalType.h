/**
 * @file ModalType.h
 * @brief Application-level modal input type enum.
 *
 * Identifies which modal input mode is currently active across the application.
 * Stored as a property on the AppState TABS subtree.  Both Terminal and Whelmed
 * pane types share these modal types.
 *
 * @see AppState
 * @see SelectionType
 */

#pragma once

#include <cstdint>

/**
 * @enum ModalType
 * @brief Identifies which modal input mode is currently active.
 *
 * - `none`      — normal input; keys flow to Action system and PTY/navigation.
 * - `selection` — vim-style keyboard selection mode.
 * - `openFile`  — open file / hyperlink hint label mode (Terminal only).
 * - `uriAction` — URI picker overlay (reserved).
 *
 * @note Stored as int in ValueTree.  Compared as uint8_t for compactness.
 */
enum class ModalType : uint8_t
{
    none,       ///< No modal active — normal input routing.
    selection,  ///< Vim-style keyboard selection mode.
    openFile,   ///< Open file / hyperlink hint label mode.
    uriAction   ///< URI picker overlay (reserved).
};
