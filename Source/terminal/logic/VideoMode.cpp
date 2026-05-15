/**
 * @file VideoMode.cpp
 * @brief DEC private mode, ANSI mode, cursor style, and keyboard protocol handlers.
 *
 * This translation unit implements the mode-related Video methods split from
 * VideoCSI.cpp to keep each file under 300 non-doxygen lines:
 *
 * - `handleCursorStyle()`  — DECSCUSR (CSI Ps SP q) cursor shape.
 * - `handleKeyboardMode()` — Progressive Keyboard Protocol (CSI … u).
 * - `handlePrivateMode()`  — DECSET / DECRST (CSI ? Pm h/l).
 * - `handleMode()`         — ANSI SM / RM (CSI Pm h/l).
 * - `applyPrivateModeTable()` (file-local) — table-driven private mode lookup.
 * - `privateModeTable` (file-local) — DECSET mode-number → State ID mapping.
 *
 * @par Thread model
 * All functions in this file run exclusively on the **READER THREAD**.
 *
 * @see Video.h      — class declaration and full API documentation
 * @see VideoCSI.cpp — CSI sequence dispatch, cursor, scroll, and report handlers
 * @see VideoESC.cpp — ESC sequence dispatch
 * @see CSI          — parameter accumulator passed to every handler
 */

#include "Video.h"

namespace Terminal
{ /*____________________________________________________________________________*/

void Video::handleCursorStyle (const CSI& params) noexcept
{
    const int ps { static_cast<int> (params.param (0, 0)) };

    if (ps >= 0 and ps <= 6)
    {
        if (events.contains (ID::cursorShape)) events.get (ID::cursorShape, static_cast<int> (activeScreen), int (ps));
    }
}

// ============================================================================
// CSI Handler — Progressive Keyboard Protocol (CSI u)
// ============================================================================

/**
 * @brief Handles progressive keyboard protocol sequences (`CSI … u`).
 *
 * Dispatches based on the intermediate byte collected before the params:
 *
 * | Intermediate | Sequence              | Action                          |
 * |--------------|-----------------------|---------------------------------|
 * | `>`          | `CSI > flags u`       | Push flags onto stack           |
 * | `<`          | `CSI < count u`       | Pop count entries from stack    |
 * | `?`          | `CSI ? u`             | Query — respond `CSI ? flags u` |
 * | `=`          | `CSI = flags ; mode u`| Set / OR / AND-NOT flags        |
 * | (none)       | `CSI … u`             | No-op (future: key event input) |
 *
 * @param params      CSI parameter accumulator.
 * @param inter       Intermediate byte buffer.
 * @param interCount  Number of intermediate bytes collected.
 * @note READER THREAD only.
 */
void Video::handleKeyboardMode (const CSI& params, const uint8_t* inter, uint8_t interCount) noexcept
{
    if (interCount > 0)
    {
        const auto scr { activeScreen };

        if (inter[0] == '>')
        {
            const auto flags { static_cast<uint32_t> (params.param (0, 0)) };
            keyboardFlags = flags;
            if (events.contains (ID::pushKeyboardMode))
                events.get (ID::pushKeyboardMode, static_cast<int> (scr), uint32_t (flags));
        }
        else if (inter[0] == '<')
        {
            const auto count { static_cast<int> (params.param (0, 1)) };
            if (events.contains (ID::popKeyboardMode))
                events.get (ID::popKeyboardMode, static_cast<int> (scr), int (count));
        }
        else if (inter[0] == '?')
        {
            const auto flags { keyboardFlags };
            char buf[32];
            std::snprintf (buf, sizeof (buf), "\x1b[?%uu", flags);
            sendResponse (buf);
        }
        else if (inter[0] == '=')
        {
            const auto flags { static_cast<uint32_t> (params.param (0, 0)) };
            keyboardFlags = flags;
        }
    }
}

// ============================================================================
// VT Handler: Private Mode (DECSET / DECRST)
// ============================================================================

/**
 * @brief Lookup-table entry mapping a DECSET/DECRST mode number to a State ID.
 *
 * Used by `applyPrivateModeTable()` to resolve the most common private
 * mode numbers without a large switch statement.
 *
 * @see privateModeTable
 * @see applyPrivateModeTable()
 */
struct PrivateModeEntry { uint16_t modeValue; juce::Identifier id; };

/**
 * @brief Table of DECSET/DECRST mode numbers and their corresponding State IDs.
 *
 * Covers the private modes that map directly to a single `Video::setMode()`
 * call.  Modes that require additional side effects are handled separately in
 * `handlePrivateMode()`.
 */
static const PrivateModeEntry privateModeTable[]
{
    {    1, ID::applicationCursor    },
    {    5, ID::reverseVideo         },
    {    7, ID::autoWrap             },
    {   66, ID::applicationKeypad   },
    { 1000, ID::mouseTracking        },
    { 1002, ID::mouseMotionTracking  },
    { 1003, ID::mouseAllTracking     },
    { 1004, ID::focusEvents          },
    { 1006, ID::mouseSgr             },
    { 2004, ID::bracketedPaste       },
    { 9001, ID::win32InputMode       },
};

/**
 * @brief Applies a private mode number via the lookup table.
 *
 * Searches `privateModeTable` for an entry matching `modeValue`.  If found,
 * calls `Video::setMode()` with the corresponding ID and `enable`.
 *
 * @param video      Video instance to update.
 * @param modeValue  The DECSET/DECRST mode number.
 * @param enable     `true` to set the mode, `false` to reset it.
 *
 * @return `true` if the mode was found in the table and applied;
 *         `false` if the caller must handle it separately.
 *
 * @note READER THREAD only.
 */
static bool applyPrivateModeTable (Video& video, uint16_t modeValue, bool enable) noexcept
{
    bool found { false };

    for (const auto& entry : privateModeTable)
    {
        if (entry.modeValue == modeValue and not found)
        {
            video.setMode (entry.id, enable);
            found = true;
        }
    }

    return found;
}

/**
 * @brief Handles `CSI ? Pm h` / `CSI ? Pm l` — DEC Private Mode Set/Reset (DECSET/DECRST).
 *
 * Iterates over all parameters in `params` and enables or disables the
 * corresponding private mode.  Most modes are resolved via `applyPrivateModeTable()`.
 *
 * @param params  CSI parameters containing the mode numbers.
 * @param enable  `true` to set the mode (h), `false` to reset it (l).
 *
 * @note READER THREAD only.
 *
 * @see applyPrivateModeTable()
 * @see handleMode()
 * @see setScreen()
 */
void Video::handlePrivateMode (const CSI& params, bool enable) noexcept
{
    // ?1049h/?1049l re-read activeScreen inline to capture pre-switch / post-switch values.
    const auto scr { activeScreen };

    for (uint8_t i { 0 }; i < params.count; ++i)
    {
        const auto modeValue { params.values.at (i) };

        if (not applyPrivateModeTable (*this, modeValue, enable))
        {
            if (modeValue == 6)
            {
                originMode = enable;
                if (enable)
                    cursorSetPosition (0, 0, cols, visibleRows);
            }
            else if (modeValue == 25)
            {
                cursorVisible = enable;
            }
            else if (modeValue == 47 or modeValue == 1047)
            {
                setScreen (enable);
            }
            else if (modeValue == 1049)
            {
                if (enable)
                {
                    saveCursor (activeScreen);
                    setScreen (true);
                }
                else
                {
                    setScreen (false);
                    restoreCursor (activeScreen);
                }
            }
            else if (modeValue == 2026)
            {
                if (events.contains (ID::syncOutput)) events.get (ID::syncOutput, bool (enable));

                if (enable)
                    if (events.contains (ID::requestSyncResize))
                        events.get (ID::requestSyncResize);
            }
        }
    }
}

// ============================================================================
// VT Handler: Mode (SM / RM)
// ============================================================================

/**
 * @brief Handles `CSI Pm h` / `CSI Pm l` — ANSI Mode Set/Reset (SM / RM).
 *
 * Iterates over all parameters in `params` and enables or disables the
 * corresponding ANSI standard mode.
 *
 * @par Supported modes
 * | Mode | Name | Effect                                                    |
 * |------|------|-----------------------------------------------------------|
 * | 4    | IRM  | Insert mode — characters shift right on input             |
 *
 * @param params  CSI parameters containing the mode numbers.
 * @param enable  `true` to set the mode (h), `false` to reset it (l).
 *
 * @note READER THREAD only.
 *
 * @see handlePrivateMode()
 */
void Video::handleMode (const CSI& params, bool enable) noexcept
{
    for (uint8_t i { 0 }; i < params.count; ++i)
    {
        const auto modeValue { params.values.at (i) };

        switch (modeValue)
        {
            case 4:
                insertMode = enable;
                break;

            default:
                break;
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
