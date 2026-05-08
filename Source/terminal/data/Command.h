/**
 * @file Command.h
 * @brief Semantic terminal operation for the Grid SPSC FIFO transport.
 *
 * Parser emits one Command per terminal action (print, erase, scroll, etc.)
 * on the reader thread.  Display drains the FIFO on the message thread and
 * forwards the batch to Screen for application.
 *
 * Commands carry viewport-relative coordinates from Parser. Screen translates
 * to logical document positions.
 *
 * @see Terminal::Grid   — FIFO transport (AbstractFifo wrapper).
 * @see Terminal::Screen — spatial owner, applies ops to growing Cells document.
 */
#pragma once
#include <JuceHeader.h>

namespace Terminal
{ /*____________________________________________________________________________*/

struct Command
{
    enum class Type : uint8_t
    {
        Print,              ///< Write cell at cursor position.
        LineFeed,           ///< LF / VT / FF — advance cursor row.
        CarriageReturn,     ///< CR — cursor col to 0.
        Tab,                ///< HT — advance to next tab stop.
        Backspace,          ///< BS — cursor col minus 1.
        EraseInLine,        ///< EL — param: mode (0 right / 1 left / 2 entire).
        EraseInDisplay,     ///< ED — param: mode (0 below / 1 above / 2 all / 3 scrollback).
        InsertLines,        ///< IL — param: count.
        DeleteLines,        ///< DL — param: count.
        InsertChars,        ///< ICH — param: count.
        DeleteChars,        ///< DCH — param: count.
        EraseChars,         ///< ECH — param: count.
        SetScreen,          ///< Switch normal / alternate — param: 0 normal, 1 alternate.
        ClearScrollback,    ///< ED mode 3 — clear scrollback buffer.
        SetTabStop,         ///< HTS — param: column.
        ClearTabStop,       ///< TBC — param: column (-1 for all).
        ClearAllTabStops,   ///< TBC mode 3 — clear all tab stops.
    };

    Type         type   { Type::Print };  ///< Operation type.
    jam::Cell    cell   {};               ///< Cell data (valid for Print).
    juce::Colour fillBg {};               ///< Erase fill colour (Pen::bg at emission time).
    int          param  { 0 };            ///< Mode (erase) or count (insert/delete) or column (tab stop).
    int          row    { 0 };            ///< Cursor row at emission time (viewport-relative).
    int          col    { 0 };            ///< Cursor col at emission time.
};

static_assert (std::is_trivially_copyable_v<Command>,
               "Command must be trivially copyable for FIFO memcpy transport");

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
