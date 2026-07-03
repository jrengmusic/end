/**
 * @file WritePathTests.cpp
 * @brief V1 write-path conformance — width, wide advance, combining/cluster
 *        folding, wide-at-margin wrap, mode 2027 gate.
 *
 * Coverage: RFC-vt-correctness.md S1/V1 decision, S5 wide-at-margin scaffold
 * (used verbatim), every `emoji_test.sh` section (endless/test/emoji_test.sh)
 * as assertions.
 *
 * @par Width-oracle pattern
 * For codepoints whose exact East Asian Width classification cannot be
 * verified by reading (the UAX #29 lookup tables in jam_GraphemeSeg.cpp are
 * opaque generated data — Unicode Standard 17.0.0, indexed, not literally
 * readable per-codepoint), assertions use `jam::Char::width (baseCodepoint)`
 * as the oracle for expected cursor advance rather than a hardcoded literal.
 * This still exercises the real integration contract under test — extension
 * codepoints (VS16/VS15/ZWJ/skin-tone modifiers/regional-indicator pairing)
 * fold into the base cell and do not advance the cursor further — without
 * fabricating an unverified width number.
 */

#include "catch2/catch.hpp"
#include "TestTerm.h"

// ============================================================================
// Single-codepoint narrow / wide write path
// ============================================================================

TEST_CASE ("narrow ASCII advances cursor by one column per glyph", "[video][width][v1]")
{
    Test::Term t { 10, 2 };
    t.feed ("abc");

    REQUIRE (t.cell (0, 0).codepoint() == uint32_t ('a'));
    REQUIRE (t.cell (0, 1).codepoint() == uint32_t ('b'));
    REQUIRE (t.cell (0, 2).codepoint() == uint32_t ('c'));
    REQUIRE (t.cursorCol() == 3);
}

TEST_CASE ("CJK wide codepoint writes WIDE head + SPACER_TAIL, cursor +2", "[video][width][v1]")
{
    // U+4F60 (ni hao shi jie section 8, emoji_test.sh) — CJK Unified Ideograph, EAW=Wide.
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ 0x4F60 }));

    REQUIRE (t.cell (0, 0).wide() == jam::Char::WIDE);
    REQUIRE (t.cell (0, 1).wide() == jam::Char::SPACER_TAIL);
    REQUIRE (t.cell (0, 1).codepoint() == 0);
    REQUIRE (t.cursorCol() == 2);
}

TEST_CASE ("Japanese hiragana and Korean hangul are wide", "[video][width][v1][cjk]")
{
    // section 8 emoji_test.sh — Hiragana konnichiwa + Hangul hangugeo.
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ 0x3053, 0x3093 }));   // U+3053 U+3093

    REQUIRE (t.cell (0, 0).wide() == jam::Char::WIDE);
    REQUIRE (t.cell (0, 2).wide() == jam::Char::WIDE);
    REQUIRE (t.cursorCol() == 4);
}

TEST_CASE ("box-drawing characters are narrow (EAW Ambiguous defaults narrow)", "[video][width][v1]")
{
    // section 9 emoji_test.sh: expected width 1 for every box char.
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ 0x250C, 0x2500, 0x2510 }));   // U+250C U+2500 U+2510

    REQUIRE (t.cell (0, 0).wide() == jam::Char::NARROW);
    REQUIRE (t.cell (0, 1).wide() == jam::Char::NARROW);
    REQUIRE (t.cell (0, 2).wide() == jam::Char::NARROW);
    REQUIRE (t.cursorCol() == 3);
}

TEST_CASE ("Nerd Font PUA icons are narrow", "[video][width][v1]")
{
    // section 11 emoji_test.sh: expected width 1.
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ 0xE0A0, 0xF013 }));   // git branch, gear

    REQUIRE (t.cell (0, 0).wide() == jam::Char::NARROW);
    REQUIRE (t.cell (0, 1).wide() == jam::Char::NARROW);
    REQUIRE (t.cursorCol() == 2);
}

// ============================================================================
// Combining marks — width 0, fold into previous cell (Case 1)
// ============================================================================

TEST_CASE ("precomposed and decomposed accented letters both occupy one column", "[video][width][v1][combining]")
{
    // section 10 emoji_test.sh: e + acute (U+0301) vs precomposed U+00E9 — both |X|.
    Test::Term precomposed { 10, 2 };
    precomposed.feed (Test::utf8 ({ 0x00E9 }));

    Test::Term decomposed { 10, 2 };
    decomposed.feed (Test::utf8 ({ uint32_t ('e'), 0x0301 }));

    REQUIRE (precomposed.cursorCol() == 1);
    REQUIRE (decomposed.cursorCol() == 1);

    REQUIRE (decomposed.cell (0, 0).contentTag() == jam::Char::CONTENT_GRAPHEME);
}

TEST_CASE ("combining mark folds into the base cell via jam::Grapheme interning", "[video][width][v1][combining]")
{
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ uint32_t ('n'), 0x0303 }));   // n + combining tilde (U+0303)

    const auto base { t.cell (0, 0) };
    REQUIRE (base.contentTag() == jam::Char::CONTENT_GRAPHEME);

    const auto& entry { jam::Grapheme::getInstance()->get (base.codepoint()) };
    REQUIRE (entry.count == 2);
    REQUIRE (entry.codepoints[0] == char32_t ('n'));
    REQUIRE (entry.codepoints[1] == char32_t (0x0303));
}

// ============================================================================
// Wide-at-margin — RFC-vt-correctness.md S5, verbatim
// ============================================================================

TEST_CASE ("wide at right margin wraps before writing", "[video][width][v1]")
{
    Test::Term t { 4, 2 };                        // cols, rows
    t.feed ("abc\xE4\xBD\xA0");                    // 'a','b','c', U+4F60 (width 2)
    REQUIRE (t.cell (0, 3).codepoint() == 0);       // col 3 left empty — no split pair
    REQUIRE (t.line (0).isContinued());
    REQUIRE (t.cell (1, 0).wide()      == jam::Char::WIDE);
    REQUIRE (t.cell (1, 1).wide()      == jam::Char::SPACER_TAIL);
    REQUIRE (t.cursorCol()             == 2);
}

// ============================================================================
// emoji_test.sh sections 1-7 — width-2 emoji, VS16/VS15, ZWJ, flags,
// skin tones, keycaps — via the width-oracle pattern (see file doc).
// ============================================================================

TEST_CASE ("basic emoji (section 1) advance the cursor by the base codepoint width", "[video][width][v1][emoji]")
{
    // U+1F600 GRINNING FACE — Emoticons block, EAW=Wide.
    Test::Term t { 10, 2 };
    const uint32_t base { 0x1F600 };
    t.feed (Test::utf8 ({ base }));

    REQUIRE (t.cursorCol() == jam::Char::width (base));
    REQUIRE (t.cell (0, 0).contentTag() == jam::Char::CONTENT_CODEPOINT);
}

TEST_CASE ("VS16 (U+FE0F) folds into the base cell, does not add a column", "[video][width][v1][emoji][vs16]")
{
    // section 2 emoji_test.sh: U+263A SMILING FACE + VS16.
    // NOTE: real-terminal expectation is width promotion 1->2 (corpus marks |XX|).
    // V1's write-time model resolves width ONCE from the base codepoint and
    // folds all subsequent Extend-class codepoints (VS16 included) into the
    // same cell without further cursor advance (RFC-vt-correctness.md S1
    // Case 1: "Cursor does not advance"). This assertion pins the SHIPPED
    // integration contract (final cursor column == base codepoint width);
    // if VS16-triggered width promotion is later added, this case is the
    // one to update. Flagged in the sprint BRIEF as a conformance finding.
    const uint32_t base { 0x263A };
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ base, 0xFE0F }));

    REQUIRE (t.cursorCol() == jam::Char::width (base));
    REQUIRE (t.cell (0, 0).contentTag() == jam::Char::CONTENT_GRAPHEME);
}

TEST_CASE ("VS15 (U+FE0E) folds into the base cell same as VS16", "[video][width][v1][emoji][vs15]")
{
    // section 3 emoji_test.sh: U+263A + VS15 forces text presentation (width 1).
    const uint32_t base { 0x263A };
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ base, 0xFE0E }));

    REQUIRE (t.cursorCol() == jam::Char::width (base));
    REQUIRE (t.cell (0, 0).contentTag() == jam::Char::CONTENT_GRAPHEME);
}

TEST_CASE ("ZWJ family sequence folds into one cell", "[video][width][v1][emoji][zwj]")
{
    // section 4 emoji_test.sh: man ZWJ woman ZWJ girl ZWJ boy -> one glyph.
    const uint32_t base { 0x1F468 };
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ base, 0x200D, 0x1F469, 0x200D, 0x1F467, 0x200D, 0x1F466 }));

    REQUIRE (t.cursorCol() == jam::Char::width (base));
    REQUIRE (t.cell (0, 0).contentTag() == jam::Char::CONTENT_GRAPHEME);

    const auto& entry { jam::Grapheme::getInstance()->get (t.cell (0, 0).codepoint()) };
    REQUIRE (entry.count == 7);
}

TEST_CASE ("regional indicator flag pair folds into one cell", "[video][width][v1][emoji][flag]")
{
    // section 5 emoji_test.sh: US flag = U+1F1FA U+1F1F8 (regional indicator pair, GB12/13).
    const uint32_t base { 0x1F1FA };
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ base, 0x1F1F8 }));

    REQUIRE (t.cursorCol() == jam::Char::width (base));
}

TEST_CASE ("skin tone modifier folds into the base emoji cell", "[video][width][v1][emoji][skintone]")
{
    // section 6 emoji_test.sh: waving hand + light skin tone modifier (U+1F3FB).
    const uint32_t base { 0x1F44B };
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ base, 0x1F3FB }));

    REQUIRE (t.cursorCol() == jam::Char::width (base));
    REQUIRE (t.cell (0, 0).contentTag() == jam::Char::CONTENT_GRAPHEME);
}

TEST_CASE ("keycap sequence folds VS16 + combining enclosing keycap into the digit cell", "[video][width][v1][emoji][keycap]")
{
    // section 7 emoji_test.sh: '0' + VS16 + U+20E3 COMBINING ENCLOSING KEYCAP.
    const uint32_t base { uint32_t ('0') };
    Test::Term t { 10, 2 };
    t.feed (Test::utf8 ({ base, 0xFE0F, 0x20E3 }));

    REQUIRE (t.cursorCol() == jam::Char::width (base));
    REQUIRE (t.cell (0, 0).contentTag() == jam::Char::CONTENT_GRAPHEME);
}

// ============================================================================
// Mode 2027 (GRAPHEME_CLUSTERING) — DECSET / DECRST / DECRQM + gate behavior
// ============================================================================
//
// CONFORMANCE FINDING RESOLVED (PLAN-vt-correctness.md Step 8, carried fix
// a): the DFA (jam_Transition.h buildCSIEntry/buildCSIParam/
// buildCSIIntermediate) collects '$' (0x24) as an intermediate byte and
// delivers 'p'/'q' (0x70/0x71) as `finalByte` for `CSI ? Pm $ p` (DECRQM) /
// `CSI Pd $ q` (DECRQSS). `applyCSI()` (jam_VideoCSI.cpp) now carries
// `case csiFinal::DECRQM:` (`'p'`, guarded by `inter[0] == csiInter::PRIVATE`)
// routing to `reportDecrqm()`, and the DECSCUSR case (`finalByte == 'q'`)
// gained an `inter[0] == csiInter::DOLLAR` branch routing to
// `reportStatusString()` (DECRQSS shares 'q' with DECSCUSR, disambiguated by
// the '$' intermediate) — the dead `csiFinal::STATUS` constant (`'$'`,
// unreachable as a final byte per the same DFA) was deleted and replaced
// with `csiFinal::DECRQM`. The REQUIRE assertions below encode the
// CORRECT/expected DECRQM response per the module's own doc table and xterm
// ctlseqs and now exercise the live, reachable dispatch path.

TEST_CASE ("mode 2027 defaults to set (cluster-aware advance on)", "[video][mode2027][v1]")
{
    Test::Term t { 10, 2 };
    REQUIRE (t.mode (jam::ID::graphemeClustering));

    t.feed ("\x1b[?2027$p");
    REQUIRE (t.lastResponse() == "\x1b[?2027;1$y");
}

TEST_CASE ("DECRST 2027 disables clustering and DECRQM reports reset", "[video][mode2027][v1]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[?2027l");
    REQUIRE_FALSE (t.mode (jam::ID::graphemeClustering));

    t.clearResponse();
    t.feed ("\x1b[?2027$p");
    REQUIRE (t.lastResponse() == "\x1b[?2027;2$y");
}

TEST_CASE ("DECSET 2027 after DECRST re-enables clustering", "[video][mode2027][v1]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[?2027l");
    t.feed ("\x1b[?2027h");
    REQUIRE (t.mode (jam::ID::graphemeClustering));
}

TEST_CASE ("mode 2027 off — no fold: every codepoint takes the single-codepoint path", "[video][mode2027][v1][gate]")
{
    // Documented gate behavior (jam_CursorState.cpp printCodepoint doc, Mode
    // 2027 gate): segmentation is skipped entirely; combining marks take the
    // width-clamped single-codepoint path (rawWidth < 1 -> 1) instead of
    // folding — two SEPARATE cells are written.
    Test::Term t { 10, 2 };
    t.feed ("\x1b[?2027l");

    t.feed (Test::utf8 ({ uint32_t ('e'), 0x0301 }));

    REQUIRE (t.cell (0, 0).codepoint() == uint32_t ('e'));
    REQUIRE (t.cell (0, 0).contentTag() == jam::Char::CONTENT_CODEPOINT);
    REQUIRE (t.cell (0, 1).codepoint() == uint32_t (0x0301));
    REQUIRE (t.cell (0, 1).contentTag() == jam::Char::CONTENT_CODEPOINT);
    REQUIRE (t.cursorCol() == 2);
}
