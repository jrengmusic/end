/**
 * @file LookupTableTests.cpp
 * @brief `jam::LookupTable` canon contract + conversion coverage.
 *
 * Coverage: the canon SSOT LUT pattern. Two
 * layers:
 *   1. `jam::LookupTable` itself — fallback/in-range/out-of-range contract,
 *      exercised directly (jam_core, not jam_terminal-specific).
 *   2. LUT-vs-previous-switch behavior identity for every jam_terminal
 *      conversion covered: SGR attribute set/clear (incl. the
 *      22/23/24 clear codes), OSC 133 subcmd -> `jam::TextLine::Mark`,
 *      UTF-8 lead-byte length classes (2/3/4-byte + invalid-lead-byte
 *      safety), and the Sixel VT340 default palette's first/last slots
 *      (0, 15, 255).
 *
 * @par Four-family DispatchTable normalization (CSI/ESC/OSC/C0)
 * Dispatch-equivalence spot checks below cover the composite-key CSI
 * disambiguation rows (DECRQM, DECSCUSR-vs-DECRQSS, primary-vs-secondary
 * DA), the ESC no-intermediate action LUT (RI, DECSC/DECRC), the OSC
 * command-number action LUT (0, 8; OSC 133 A/B/C is already covered above
 * by the pre-existing "OSC 133 A/B/C..." test — same `setShellIntegration()` call
 * path, re-used rather than duplicated), the C0 action LUT (BEL/BS/HT), and
 * a read-after-DECSET assertion exercising Video's own working-copy mode
 * members (jam_CursorState.h/.cpp, read/written directly) via the fixture's
 * `stateChanged`/`modeChanged` trampolines onto `jam::terminal::Model`.
 */

#include "catch2/catch.hpp"
#include "TestTerm.h"

// ============================================================================
// jam::LookupTable — canon contract (jam_core, standalone)
// ============================================================================

TEST_CASE ("LookupTable returns the row value for an in-range key present in the entry list", "[core][lookuptable]")
{
    static constexpr jam::LookupTable<int, int, 8> lut { -1, { { 2, 20 }, { 5, 50 } } };

    REQUIRE (lut[2] == 20);
    REQUIRE (lut[5] == 50);
}

TEST_CASE ("LookupTable returns fallback for an in-range key absent from the entry list", "[core][lookuptable]")
{
    static constexpr jam::LookupTable<int, int, 8> lut { -1, { { 2, 20 }, { 5, 50 } } };

    REQUIRE (lut[0] == -1);
    REQUIRE (lut[7] == -1);
}

TEST_CASE ("LookupTable returns fallback for an out-of-range key without throwing", "[core][lookuptable]")
{
    static constexpr jam::LookupTable<int, int, 8> lut { -1, { { 2, 20 } } };

    REQUIRE (lut[100] == -1);   // past Capacity
    REQUIRE (lut[-1] == -1);    // negative — casts past Capacity, same positive-check path
}

// ============================================================================
// SGR attribute set/clear — canon LookupTable (jam_VideoSGR.cpp)
// ============================================================================

TEST_CASE ("SGR attribute-set codes 1-3,5-9,53,73,74 map to their Stamp flag bit (4 has its own singleUnderline arm)", "[video][sgr][lookuptable]")
{
    struct Row { const char* seq; uint16_t flag; };
    const Row rows[]
    {
        { "\x1b[1m",  jam::Stamp::bold },
        { "\x1b[2m",  jam::Stamp::dim },
        { "\x1b[3m",  jam::Stamp::italic },
        { "\x1b[5m",  jam::Stamp::blink },
        { "\x1b[6m",  jam::Stamp::blink },        // RAPID_BLINK shares blink's row
        { "\x1b[7m",  jam::Stamp::inverse },
        { "\x1b[9m",  jam::Stamp::strike },
        { "\x1b[53m", jam::Stamp::overline },
        { "\x1b[73m", jam::Stamp::superscript },
        { "\x1b[74m", jam::Stamp::subscript },
    };

    for (const auto& row : rows)
    {
        Test::Term t { 10, 2 };
        t.feed (row.seq);
        t.feed ("x");

        const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
        REQUIRE ((style.flags & row.flag) != 0);
    }
}

TEST_CASE ("SGR 8 (HIDDEN) is an explicit no-op LookupTable row", "[video][sgr][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[8m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE (style.flags == 0);
}

TEST_CASE ("SGR 22 clears bold|dim via the attributeResetLut row", "[video][sgr][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[1;2m");   // bold + dim
    t.feed ("\x1b[22m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & (jam::Stamp::bold | jam::Stamp::dim)) == 0);
}

TEST_CASE ("SGR 21 sets the underline-style field to double, identical to CSI 4:2 m", "[video][sgr][lookuptable]")
{
    // ARCHITECT-ratified: SGR 21 = doubly underlined (ECMA-48 + xterm/kitty/
    // ghostty consensus). Routes through the same underline-style field as
    // CSI 4:2 m.
    Test::Term t { 10, 2 };
    t.feed ("\x1b[21m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::underlineStyleMask) == jam::Stamp::underlineDouble);
}

TEST_CASE ("SGR 24 clears the double-underline style set by SGR 21", "[video][sgr][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[21m");
    t.feed ("\x1b[24m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::underlineStyleMask) == 0);
}

TEST_CASE ("SGR 23 clears italic only, leaving other flags set", "[video][sgr][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[1;3m");   // bold + italic
    t.feed ("\x1b[23m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::italic) == 0);
    REQUIRE ((style.flags & jam::Stamp::bold) != 0);
}

TEST_CASE ("SGR 24 clears the entire underline-style field (any underline style)", "[video][sgr][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[4:3m");   // curly underline
    t.feed ("\x1b[24m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::underlineStyleMask) == 0);
}

TEST_CASE ("SGR 4 after SGR 21 (double) clears the field to single, not curly (Auditor F1 regression)", "[video][sgr][lookuptable]")
{
    // Auditor F1: plain SGR 4 previously OR'd underlineSingle (0x200) into
    // penFlags without clearing the 3-bit underline-style field first. With
    // DOUBLE (0x400) already set by SGR 21, the OR yielded 0x600 = CURLY
    // instead of SINGLE. SgrAction::singleUnderline now clears-then-sets.
    Test::Term t { 10, 2 };
    t.feed ("\x1b[21m");
    t.feed ("\x1b[4m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::underlineStyleMask) == jam::Stamp::underlineSingle);
}

TEST_CASE ("SGR 4 after SGR 4:3 (curly) clears the field to single", "[video][sgr][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[4:3m");
    t.feed ("\x1b[4m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::underlineStyleMask) == jam::Stamp::underlineSingle);
}

TEST_CASE ("CSI 4:3 m sets curly underline without dispatching the ':' sub-parameter as standalone SGR 3 (no spurious italic)", "[video][sgr][lookuptable]")
{
    // Regression: setPen()'s main dispatch loop previously ran every
    // params.values[i] through sgrActionLut regardless of separator type,
    // so the `3` in `4:3` (curly-underline sub-parameter ordinal) was also
    // dispatched as SGR 3 (italic). A sub-parameter is bound to its head
    // parameter and never dispatches standalone (ECMA-48 / xterm consensus).
    Test::Term t { 10, 2 };
    t.feed ("\x1b[4:3m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::underlineStyleMask) == jam::Stamp::underlineCurly);
    REQUIRE ((style.flags & jam::Stamp::italic) == 0);
}

TEST_CASE ("CSI 4:5 m sets dashed underline without dispatching the ':' sub-parameter as standalone SGR 5 (no spurious blink)", "[video][sgr][lookuptable]")
{
    // Same regression as the 4:3/italic case above, with the 4:5/blink pair
    // (Sequence::blink == 5, the same ordinal as Sequence::dashed).
    Test::Term t { 10, 2 };
    t.feed ("\x1b[4:5m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE ((style.flags & jam::Stamp::underlineStyleMask) == jam::Stamp::underlineDashed);
    REQUIRE ((style.flags & jam::Stamp::blink) == 0);
}

TEST_CASE ("CSI 38:2:R:G:B m (colon sub-separator RGB form) sets the foreground colour, unaffected by the sub-parameter dispatch guard", "[video][sgr][lookuptable]")
{
    // The isSubSeparator() guard added for the 4:n regression must not
    // interfere with 38/48/58's own i-advancement — parseExtendedColor()
    // consumes the sub-parameters directly, and the guarded indices are
    // never visited by the main loop at all (i has already jumped past them).
    Test::Term t { 10, 2 };
    t.feed ("\x1b[38:2:255:0:0m");
    t.feed ("x");

    const auto& style { jam::Stamp::getInstance()->get (t.cell (0, 0).styleId()) };
    REQUIRE (style.fg == juce::Colour (255, 0, 0));
}

// ============================================================================
// OSC 133 subcmd -> jam::TextLine::Mark — canon LookupTable (jam_VideoOSCExt.cpp)
// ============================================================================

TEST_CASE ("OSC 133 A/B/C map through the LookupTable to prompt/input/output", "[video][osc133][lookuptable]")
{
    Test::Term t { 20, 4 };

    t.feed ("\x1b]133;A\x07");
    REQUIRE (t.line (0).mark() == jam::TextLine::Mark::prompt);

    t.feed ("\x1b]133;B\x07");
    REQUIRE (t.line (0).mark() == jam::TextLine::Mark::input);

    t.feed ("\x1b]133;C\x07");
    REQUIRE (t.line (0).mark() == jam::TextLine::Mark::output);
}

// ============================================================================
// UTF-8 lead-byte length classes — canon LookupTable (jam_ParserAction.cpp)
// ============================================================================

TEST_CASE ("2-byte UTF-8 lead byte (0xC0-0xDF class) decodes to the source codepoint", "[parser][utf8][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ 0xE9 }));   // U+00E9 'e' with acute accent — 2-byte UTF-8

    REQUIRE (t.cell (0, 0).codepoint() == 0xE9u);
}

TEST_CASE ("3-byte UTF-8 lead byte (0xE0-0xEF class) decodes to the source codepoint", "[parser][utf8][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ 0x2603 }));   // U+2603 SNOWMAN — 3-byte UTF-8, narrow

    REQUIRE (t.cell (0, 0).codepoint() == 0x2603u);
}

TEST_CASE ("4-byte UTF-8 lead byte (0xF0-0xF7 class) decodes to the source codepoint", "[parser][utf8][lookuptable]")
{
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ 0x1F600 }));   // U+1F600 GRINNING FACE — 4-byte UTF-8

    REQUIRE (t.cell (0, 0).codepoint() == 0x1F600u);
}

TEST_CASE ("invalid lead byte (0xF8-0xFF class) never completes a decode — fallback length 1 prevents garbage output", "[parser][utf8][lookuptable]")
{
    Test::Term t { 10, 2 };
    const char invalidSequence[] { static_cast<char> (0xFF), static_cast<char> (0x80) };
    t.feed (invalidSequence, 2);
    t.feed ("a");

    // The invalid lead byte + continuation byte never reach the expected
    // length (fallback 1 < accumulated 2), so accumulateUTF8Byte() never
    // calls printCodepoint() for them — only 'a' lands, at column 0.
    REQUIRE (t.cell (0, 0).codepoint() == uint32_t ('a'));
}

// ============================================================================
// Sixel VT340 default palette — canon LookupTable (jam_SixelDecoder.h)
// ============================================================================

TEST_CASE ("Sixel default palette register 0 decodes to opaque black", "[sixel][lookuptable]")
{
    jam::terminal::SixelDecoder decoder;
    const std::string payload { "#0~" };   // select register 0, draw one full sixel column
    const auto image { decoder.decode (reinterpret_cast<const uint8_t*> (payload.data()), payload.size()) };

    REQUIRE (image.isValid());
    REQUIRE (image.rgba[0] == 0);     // r
    REQUIRE (image.rgba[1] == 0);     // g
    REQUIRE (image.rgba[2] == 0);     // b
    REQUIRE (image.rgba[3] == 255);   // a
}

TEST_CASE ("Sixel default palette register 15 (last explicit VT340 row) decodes to opaque white", "[sixel][lookuptable]")
{
    jam::terminal::SixelDecoder decoder;
    const std::string payload { "#15~" };
    const auto image { decoder.decode (reinterpret_cast<const uint8_t*> (payload.data()), payload.size()) };

    REQUIRE (image.isValid());
    REQUIRE (image.rgba[0] == 255);
    REQUIRE (image.rgba[1] == 255);
    REQUIRE (image.rgba[2] == 255);
    REQUIRE (image.rgba[3] == 255);
}

TEST_CASE ("Sixel default palette register 255 (last slot, fallback row) decodes to opaque black", "[sixel][lookuptable]")
{
    jam::terminal::SixelDecoder decoder;
    const std::string payload { "#255~" };
    const auto image { decoder.decode (reinterpret_cast<const uint8_t*> (payload.data()), payload.size()) };

    REQUIRE (image.isValid());
    REQUIRE (image.rgba[0] == 0);
    REQUIRE (image.rgba[1] == 0);
    REQUIRE (image.rgba[2] == 0);
    REQUIRE (image.rgba[3] == 255);
}

// ============================================================================
// CSI composite-key dispatch — canon DispatchTable (jam_VideoCSI.cpp)
// ============================================================================

TEST_CASE ("DECRQM (CSI ? Pd $ p) reports the queried mode's state via the composite-key CSI dispatch", "[video][csi][lookuptable][step8d]")
{
    Test::Term t { 10, 5 };

    // DECAWM (mode 7) defaults SET — sendModeReport() replies "\x1b[?7;1$y".
    t.feed ("\x1b[?7$p");
    REQUIRE (t.lastResponse() == "\x1b[?7;1$y");
}

TEST_CASE ("DECRQM requires the full wire form CSI ? Pd $ p — bare CSI ? Pd p (no '$') is ignored", "[video][csi][lookuptable][step8d]")
{
    // ARCHITECT ruling: the composite key alone only verifies inter[0] ==
    // '?' (csiInterCode::PRIVATE); the DECRQM executor additionally
    // requires inter[1] == '$' before calling sendModeReport() — see
    // jam_VideoCSI.cpp `Video::sendModeReport()`.
    Test::Term t { 10, 5 };

    // Full wire form — mode 2026 (SYNC_OUTPUT) defaults reset.
    t.feed ("\x1b[?2026$p");
    REQUIRE (t.lastResponse() == "\x1b[?2026;2$y");

    t.clearResponse();

    // Loose form — no '$' intermediate — resolves to the decrqm action via
    // the composite key but is ignored at the executor arm: no response.
    t.feed ("\x1b[?2026p");
    REQUIRE (t.lastResponse().empty());
}

TEST_CASE ("DECSCUSR (CSI Ps SP q) and DECRQSS (CSI Ps $ q) resolve to distinct composite-key rows sharing final 'q'", "[video][csi][lookuptable][step8d]")
{
    Test::Term t { 10, 5 };

    // DECSCUSR (' ' intermediate) only fires the cursorShape event — Test::Term
    // does not subscribe it, so no response bytes are queued.
    t.feed ("\x1b[2 q");
    REQUIRE (t.lastResponse().empty());

    // DECRQSS ('$' intermediate) queries DECAWM (Ps=7) — a genuine response,
    // proving the composite key routed the SAME final byte to a different
    // action based solely on the intermediate.
    t.feed ("\x1b[7$q");
    REQUIRE (t.lastResponse() == "\x1b[7;1$y");
}

TEST_CASE ("Primary DA (CSI c) and secondary DA (CSI > c) resolve to distinct composite-key rows", "[video][csi][lookuptable][step8d]")
{
    Test::Term t { 10, 5 };

    t.feed ("\x1b[c");
    REQUIRE (t.lastResponse() == "\x1b[?62;4c");

    t.clearResponse();
    t.feed ("\x1b[>c");
    REQUIRE (t.lastResponse() == "\x1b[>65;100;0c");
}

// ============================================================================
// ESC no-intermediate action LUT (jam_VideoESC.cpp)
// ============================================================================

TEST_CASE ("ESC M (RI) scrolls the region down through the ESC action LUT when the cursor is at the scroll-region top", "[video][esc][lookuptable][step8d]")
{
    Test::Term t { 5, 3 };

    t.feed ("A");     // row 0, col 0 = 'A'; cursor -> col 1
    t.feed ("\r");     // CR — cursor back to col 0, row 0 (scroll-region top)
    t.feed ("\x1bM");  // ESC M (RI) — reverseIndex action

    REQUIRE (t.cell (0, 0).codepoint() == 0);                 // top row cleared
    REQUIRE (t.cell (1, 0).codepoint() == uint32_t ('A'));    // 'A' shifted down one row
}

TEST_CASE ("ESC 7 / ESC 8 (DECSC/DECRC) save and restore cursor position through the ESC action LUT", "[video][esc][lookuptable][step8d]")
{
    Test::Term t { 10, 5 };

    t.feed ("\x1b[3;3H");   // cursor -> (row 2, col 2), zero-based
    t.feed ("\x1b" "7");    // DECSC — saveCursor action
    t.feed ("\x1b[1;1H");   // move away
    t.feed ("\x1b" "8");    // DECRC — restoreCursor action

    REQUIRE (t.cursorRow() == 2);
    REQUIRE (t.cursorCol() == 2);
}

// ============================================================================
// OSC command-number action LUT (jam_VideoOSC.cpp)
// ============================================================================

TEST_CASE ("OSC 0 fires the title event with the raw payload bytes through the OSC action LUT", "[video][osc][lookuptable][step8d]")
{
    Test::Term t { 20, 2 };

    t.feed ("\x1b]0;hello\x07");
    REQUIRE (t.lastTitle() == "hello");
}

TEST_CASE ("OSC 8 opens a hyperlink through the OSC action LUT's hyperlink row (deeper coverage in HyperlinkTests.cpp)", "[video][osc][lookuptable][step8d]")
{
    Test::Term t { 20, 2 };

    t.feed ("\x1b]8;;https://example.com\x07");
    t.feed ("x");

    REQUIRE (t.cell (0, 0).linkId() != 0);
}

// ============================================================================
// C0 action LUT (jam_CursorState.cpp)
// ============================================================================

TEST_CASE ("C0 HT (0x09) advances the cursor to the next tab stop through the C0 action LUT", "[video][c0][lookuptable][step8d]")
{
    Test::Term t { 10, 2 };

    t.feed ("\x09");
    REQUIRE (t.cursorCol() == 8);   // default 8-column tab stops
}

TEST_CASE ("C0 BS (0x08) moves the cursor one column left through the C0 action LUT", "[video][c0][lookuptable][step8d]")
{
    Test::Term t { 10, 2 };

    t.feed ("AB");
    t.feed ("\x08");
    REQUIRE (t.cursorCol() == 1);
}

TEST_CASE ("C0 BEL (0x07) fires the bell action without moving the cursor or writing a cell", "[video][c0][lookuptable][step8d]")
{
    Test::Term t { 10, 2 };

    t.feed ("X");   // row 0, col 0 = 'X'; cursor -> col 1
    t.feed ("\x07"); // BEL — distinct action from BS/HT, no cursor motion

    REQUIRE (t.cursorCol() == 1);
    REQUIRE (t.cell (0, 0).codepoint() == uint32_t ('X'));
    REQUIRE (t.cell (0, 1).codepoint() == 0);
}

// ============================================================================
// Working-copy mode-flag read-after-DECSET (jam_CursorState.h/.cpp)
// t.mode() reads jam::terminal::Model, kept in sync via the fixture's
// modeChanged trampoline onto Model::setMode().
// ============================================================================

TEST_CASE ("Video's own working-copy member reads correctly after DECRST 2027 disables grapheme clustering", "[video][mode][lookuptable][step8d]")
{
    Test::Term t { 10, 2 };

    REQUIRE (t.mode (Id::graphemeClustering));   // default true

    t.feed ("\x1b[?2027l");   // DECRST 2027
    REQUIRE_FALSE (t.mode (Id::graphemeClustering));

    // printCodepoint() reads the graphemeClustering working-copy member
    // directly on every call — with clustering disabled, the combining
    // codepoint takes the single-codepoint path instead of folding into the
    // previous cell, proving the working-copy read observed the DECRST write.
    t.feed (Test::utf8 ({ 'e', 0x0301 }));   // 'e' + combining acute accent

    REQUIRE (t.cell (0, 0).codepoint() == uint32_t ('e'));
    REQUIRE (t.cell (0, 1).codepoint() == 0x0301u);
}
