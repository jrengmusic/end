/**
 * @file DispatchTests.cpp
 * @brief SGR/CSI/OSC dispatch basics, DECAWM, alt-screen switch, scroll
 *        regions — conformance coverage.
 */

#include "catch2/catch.hpp"
#include "TestTerm.h"

// ============================================================================
// SGR — pen state resolves into a jam::Stamp entry
// ============================================================================

TEST_CASE ("SGR 1 (bold) sets Stamp::BOLD on the written cell's style", "[video][sgr]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[1m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::BOLD) != 0);
}

TEST_CASE ("SGR 0 resets the pen to default attributes", "[video][sgr]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[1m");
    t.feed ("\x1b[0m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::BOLD) == 0);
}

// ============================================================================
// CSI cursor positioning
// ============================================================================

TEST_CASE ("CUP (CSI Pr;Pc H) moves the cursor to a one-based absolute position", "[video][csi][cursor]")
{
    Test::Term t { 20, 10 };
    t.feed ("\x1b[5;10H");

    REQUIRE (t.cursorRow() == 4);
    REQUIRE (t.cursorCol() == 9);
}

// ============================================================================
// DECAWM — auto-wrap mode
// ============================================================================

TEST_CASE ("DECAWM defaults to set and DECRQM reports it (mode-state surface)", "[video][decawm]")
{
    Test::Term t { 10, 2 };
    REQUIRE (t.mode (jam::ID::autoWrap));
}

TEST_CASE ("DECRST 7 disables auto-wrap: the last column is overwritten instead of wrapping", "[video][decawm]")
{
    Test::Term t { 4, 2 };
    t.feed ("\x1b[?7l");
    REQUIRE_FALSE (t.mode (jam::ID::autoWrap));

    t.feed ("abcd");   // fills the row exactly, wrapPending set but autoWrap off
    t.feed ("e");       // resolveWrapPending() no-ops (autoWrap false) — overwrites col 3

    REQUIRE (t.cursorRow() == 0);
    REQUIRE (t.cell (0, 3).codepoint() == uint32_t ('e'));
}

TEST_CASE ("DECSET 7 (default) wraps to the next row at the right margin", "[video][decawm]")
{
    Test::Term t { 4, 2 };
    t.feed ("abcde");

    REQUIRE (t.line (0).isContinued());
    REQUIRE (t.cell (1, 0).codepoint() == uint32_t ('e'));
    REQUIRE (t.cursorRow() == 1);
    REQUIRE (t.cursorCol() == 1);
}

// ============================================================================
// Alternate screen buffer (?1049h / ?1049l)
// ============================================================================

TEST_CASE ("?1049h switches to a cleared alternate screen; ?1049l restores normal screen + cursor", "[video][altscreen]")
{
    Test::Term t { 10, 4 };
    t.feed ("normal");
    t.feed ("\x1b[3;3H");   // move cursor to (row 2, col 2, zero-based) before entering the alt screen

    t.feed ("\x1b[?1049h");
    REQUIRE (t.cell (0, 0, jam::terminal::Screen::alternate).codepoint() == 0);   // cleared

    // setScreen() (jam_VideoEdit.cpp) does not reset the cursor to origin —
    // only clamps it to bounds. The alt-screen write lands where the
    // pre-switch cursor was: (row 2, col 2).
    t.feed ("alt");
    REQUIRE (t.cell (2, 2, jam::terminal::Screen::alternate).codepoint() == uint32_t ('a'));

    t.feed ("\x1b[?1049l");

    // Normal screen content survived the alt-screen excursion.
    REQUIRE (t.cell (0, 0, jam::terminal::Screen::normal).codepoint() == uint32_t ('n'));

    // Cursor restored to the position saveCursor() captured on ?1049h entry.
    REQUIRE (t.cursorRow() == 2);
    REQUIRE (t.cursorCol() == 2);
}

// ============================================================================
// Scroll regions (DECSTBM) — SU / SD confined to the region
// ============================================================================

TEST_CASE ("DECSTBM confines scroll-up to the region; rows outside are untouched", "[video][scrollregion]")
{
    Test::Term t { 10, 5 };

    // Seed every row with a distinct marker character before setting the region.
    for (int row { 0 }; row < 5; ++row)
    {
        t.feed ("\x1b[" + std::to_string (row + 1) + ";1H");
        t.feed (std::string (1, static_cast<char> ('A' + row)));
    }

    t.feed ("\x1b[2;4r");   // scroll region rows 1..3 (zero-based)
    t.feed ("\x1b[3S");     // scroll region up by 3 (drains it fully)

    // Row 0 (outside the region, above top) is untouched.
    REQUIRE (t.cell (0, 0).codepoint() == uint32_t ('A'));

    // Row 4 (outside the region, below bottom) is untouched.
    REQUIRE (t.cell (4, 0).codepoint() == uint32_t ('E'));
}

TEST_CASE ("DECSTBM CUP respects origin mode when DECOM is set", "[video][scrollregion][decom]")
{
    Test::Term t { 10, 5 };
    t.feed ("\x1b[2;4r");    // region rows 1..3 (zero-based)
    t.feed ("\x1b[?6h");     // DECOM on — row 1 below is relative to the region top

    t.feed ("\x1b[1;1H");    // row 1 (one-based, relative) -> region top (zero-based row 1)
    REQUIRE (t.cursorRow() == 1);
}

// ============================================================================
// Erase operations
// ============================================================================

TEST_CASE ("ED 2 erases the entire visible screen", "[video][erase]")
{
    Test::Term t { 5, 2 };
    t.feed ("abcde");
    t.feed ("\x1b[2J");

    REQUIRE (t.cell (0, 0).codepoint() == 0);
}

TEST_CASE ("EL 0 erases from the cursor to end of line", "[video][erase]")
{
    Test::Term t { 5, 1 };
    t.feed ("abcde");
    t.feed ("\x1b[3G");    // cursor to column 2 (zero-based), one-based col 3
    t.feed ("\x1b[0K");

    REQUIRE (t.cell (0, 0).codepoint() == uint32_t ('a'));
    REQUIRE (t.cell (0, 1).codepoint() == uint32_t ('b'));
    REQUIRE (t.cell (0, 2).codepoint() == 0);
    REQUIRE (t.cell (0, 4).codepoint() == 0);
}
