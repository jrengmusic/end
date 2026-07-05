/**
 * @file MarkTests.cpp
 * @brief V3 conformance — OSC 133 semantic mark stamping + CellFifo header
 *        mark-bit transport round-trip.
 *
 * Coverage: the ratified semantic-mark design (ghostty 4-state vocabulary).
 * Two independent surfaces:
 *   1. Video::handleOsc133() stamping `Row::flags` mark bits (Row::markShift
 *      == 3, since Row::flags also carries flexWrap/collapsed/justify at
 *      bits 0-2) — via `Test::Line::mark()`.
 *   2. `jam::terminal::CellFifo` header flags mark bits (CellFifo::markShift
 *      == 2 — CellFifo's OWN packing, independent of Row's) — exercised
 *      directly against a standalone CellFifo instance, since Video does not
 *      populate CellFifo yet (that wiring is a later consumer's scope).
 */

#include "catch2/catch.hpp"
#include "TestTerm.h"

// ============================================================================
// OSC 133 mark stamping (Video -> Row::flags)
// ============================================================================

TEST_CASE ("OSC 133;A stamps the cursor row prompt", "[video][osc133][v3]")
{
    Test::Term t { 20, 4 };
    t.feed ("\x1b]133;A\x07");

    REQUIRE (t.line (0).mark() == jam::TextLine::Mark::prompt);
}

TEST_CASE ("OSC 133;B stamps the cursor row input", "[video][osc133][v3]")
{
    Test::Term t { 20, 4 };
    t.feed ("\x1b]133;A\x07");
    t.feed ("$ ");
    t.feed ("\x1b]133;B\x07");

    REQUIRE (t.line (0).mark() == jam::TextLine::Mark::input);
}

TEST_CASE ("OSC 133;C stamps output, 133;D closes the block without re-stamping", "[video][osc133][v3]")
{
    Test::Term t { 20, 4 };
    t.feed ("\x1b]133;A\x07");
    t.feed ("\x1b]133;B\x07");
    t.feed ("ls\r\n");
    t.feed ("\x1b]133;C\x07");
    t.feed ("file.txt\r\n");

    // Row that received the 133;C stamp + the printed output carries `output`.
    REQUIRE (t.line (1).mark() == jam::TextLine::Mark::output);

    t.feed ("\x1b]133;D;0\x07");
    t.feed ("next prompt line");   // printed on row2 — proves activeMark was actually cleared

    // 133;D does not re-stamp the row it closes — row1 still carries `output`
    // from C (jam_VideoOSCExt.cpp handleOsc133() doc: "no re-stamp here by
    // design"); the NEXT written row picks up the cleared (none) state.
    REQUIRE (t.line (1).mark() == jam::TextLine::Mark::output);
    REQUIRE (t.line (2).mark() == jam::TextLine::Mark::none);
}

TEST_CASE ("prompt continues as promptContinuation across a line feed", "[video][osc133][v3][promptcontinuation]")
{
    // jam_VideoOps.cpp cursorGoToNextLine(): "a multi-line prompt's
    // continuation rows carry promptContinuation until the next 133;B".
    Test::Term t { 20, 4 };
    t.feed ("\x1b]133;A\x07");
    t.feed ("first prompt line\r\n");
    t.feed ("second prompt line");

    REQUIRE (t.line (0).mark() == jam::TextLine::Mark::prompt);
    REQUIRE (t.line (1).mark() == jam::TextLine::Mark::promptContinuation);
}

TEST_CASE ("prompt continues as promptContinuation across a wide-at-margin wrap", "[video][osc133][v3][promptcontinuation]")
{
    // jam_CursorState.cpp printCodepoint() wide-pair right-margin wrap branch
    // performs the same prompt -> promptContinuation transition as
    // resolveWrapPending() / cursorGoToNextLine().
    Test::Term t { 4, 2 };
    t.feed ("\x1b]133;A\x07");
    t.feed ("abc\xE4\xBD\xA0");   // 'a','b','c', U+4F60 (width 2) — wraps before writing

    REQUIRE (t.line (0).mark() == jam::TextLine::Mark::prompt);
    REQUIRE (t.line (1).mark() == jam::TextLine::Mark::promptContinuation);
}

// ============================================================================
// CellFifo header mark-bit transport (independent of Video/Row)
// ============================================================================

TEST_CASE ("CellFifo pushActive/drainActive round-trips isContinued/isJustified/mark", "[cellfifo][transport][v3]")
{
    jam::terminal::CellFifo fifo (256, 256);

    jam::Char row[3]
    {
        jam::Char::make (uint32_t ('a'), jam::Char::CONTENT_CODEPOINT, jam::Char::NARROW, 0),
        jam::Char::make (uint32_t ('b'), jam::Char::CONTENT_CODEPOINT, jam::Char::NARROW, 0),
        jam::Char::make (uint32_t ('c'), jam::Char::CONTENT_CODEPOINT, jam::Char::NARROW, 0),
    };

    const uint8_t flags
    {
        static_cast<uint8_t> (jam::terminal::CellFifo::isContinuedFlag
                             | jam::terminal::CellFifo::isJustifiedFlag
                             | (static_cast<uint8_t> (jam::TextLine::Mark::input) << jam::terminal::CellFifo::markShift))
    };

    fifo.pushActive (row, 3, flags);

    jam::TextLine line;
    REQUIRE (fifo.drainActive (line));
    REQUIRE (line.cellCount == 3);
    REQUIRE (line.isContinued);
    REQUIRE (line.isJustified);
    REQUIRE (line.mark == jam::TextLine::Mark::input);
    REQUIRE (line.chars[0].codepoint() == uint32_t ('a'));
    REQUIRE (line.chars[2].codepoint() == uint32_t ('c'));
}

TEST_CASE ("CellFifo drainHistory joins isContinued runs into one TextLine", "[cellfifo][transport][v3]")
{
    jam::terminal::CellFifo fifo (256, 256);

    jam::Char rowA[2]
    {
        jam::Char::make (uint32_t ('a'), jam::Char::CONTENT_CODEPOINT, jam::Char::NARROW, 0),
        jam::Char::make (uint32_t ('b'), jam::Char::CONTENT_CODEPOINT, jam::Char::NARROW, 0),
    };
    jam::Char rowB[1]
    {
        jam::Char::make (uint32_t ('c'), jam::Char::CONTENT_CODEPOINT, jam::Char::NARROW, 0),
    };

    const uint8_t markBits { static_cast<uint8_t> (jam::TextLine::Mark::output) << jam::terminal::CellFifo::markShift };

    fifo.pushHistory (rowA, 2, static_cast<uint8_t> (jam::terminal::CellFifo::isContinuedFlag | markBits));
    fifo.pushHistory (rowB, 1, 0);   // terminal (non-continued) row closes the logical line

    jam::TextLine line;
    REQUIRE (fifo.drainHistory (line));
    REQUIRE (line.cellCount == 3);
    REQUIRE_FALSE (line.isContinued);            // joined line is not itself continued
    REQUIRE (line.mark == jam::TextLine::Mark::output);   // mark taken from the FIRST physical row
    REQUIRE (line.chars[0].codepoint() == uint32_t ('a'));
    REQUIRE (line.chars[1].codepoint() == uint32_t ('b'));
    REQUIRE (line.chars[2].codepoint() == uint32_t ('c'));
}

TEST_CASE ("CellFifo drainHistory returns a partial run with isContinued set at drain-end", "[cellfifo][transport][v3]")
{
    jam::terminal::CellFifo fifo (256, 256);

    jam::Char rowA[1] { jam::Char::make (uint32_t ('x'), jam::Char::CONTENT_CODEPOINT, jam::Char::NARROW, 0) };
    fifo.pushHistory (rowA, 1, jam::terminal::CellFifo::isContinuedFlag);   // continued, no terminal row follows

    jam::TextLine line;
    REQUIRE (fifo.drainHistory (line));
    REQUIRE (line.cellCount == 1);
    REQUIRE (line.isContinued);
}
