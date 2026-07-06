/**
 * @file HyperlinkTests.cpp
 * @brief V5 conformance — OSC 8 hyperlink carrier: Link interning + linkId
 *        stamping on head and SPACER_TAIL, `;;` reset, FLEX_GAP preserves
 *        the merge target's own linkId.
 *
 * Coverage: the ratified OSC 8 hyperlink carrier design and its interning
 * behavior. `jam::Link` is a `SharedResources<Link>` singleton — see TestTerm.h doc for
 * the ownership model (`Test::Term` owns one `jam::Link` instance per fixture).
 *
 * CONFORMANCE FINDING RESOLVED: `Owner<T>::addIfNotAlreadyThere`
 * (jam_core/utilities/jam_IsHashable.h)
 * returns a 0-BASED index — the FIRST interned entry gets index 0.
 * `Video::applyOsc8()` (jam_VideoOSCExt.cpp) stores that index **+1** into
 * `activeLinkId`, exactly matching `jam::Char::linkId()`'s documented
 * contract ("0 = no link — zero-initialized Chars are linkless by
 * construction", jam_Char.h, jam_Link.h class doc) — the FIRST hyperlink
 * ever opened against a fresh `jam::Link` table therefore gets `linkId() ==
 * 1`, never colliding with the 0 sentinel. Every `REQUIRE (... .linkId() !=
 * 0)` below encodes that contract; every table lookup below resolves the
 * offset back via `jam::Link::getInstance()->get (linkId() - 1)`, the
 * consumer-side half of the same contract (jam_Link.h doc).
 */

#include "catch2/catch.hpp"
#include "TestTerm.h"

TEST_CASE ("OSC 8 with URI interns a jam::Link entry and stamps the pen", "[video][osc8][v5]")
{
    Test::Term t { 20, 2 };
    t.feed ("\x1b]8;;https://example.com\x07");
    t.feed ("x");

    const auto cell { t.cell (0, 0) };
    REQUIRE (cell.linkId() != 0);

    // linkId() is the table index + 1 (see file doc) — resolve back via -1.
    const auto& entry { jam::Link::getInstance()->get (cell.linkId() - 1) };
    REQUIRE (entry.uri == juce::String ("https://example.com"));
    REQUIRE (entry.id.isEmpty());
}

TEST_CASE ("OSC 8 with explicit id= dedupes by id, not by uri", "[video][osc8][v5]")
{
    Test::Term t { 20, 2 };

    t.feed ("\x1b]8;id=hover1;https://a.example\x07");
    t.feed ("a");
    t.feed ("\x1b]8;;\x07");                              // close

    t.feed ("\x1b]8;id=hover1;https://b.example\x07");    // same id, different uri
    t.feed ("b");
    t.feed ("\x1b]8;;\x07");

    REQUIRE (t.cell (0, 0).linkId() == t.cell (0, 1).linkId());
}

TEST_CASE ("OSC 8 stamps linkId on both the WIDE head and the SPACER_TAIL", "[video][osc8][v5][width]")
{
    Test::Term t { 20, 2 };
    t.feed ("\x1b]8;;https://example.com\x07");
    t.feed (Test::utf8 ({ 0x4F60 }));   // wide CJK codepoint

    REQUIRE (t.cell (0, 0).wide() == jam::Char::WIDE);
    REQUIRE (t.cell (0, 1).wide() == jam::Char::SPACER_TAIL);
    REQUIRE (t.cell (0, 0).linkId() != 0);
    REQUIRE (t.cell (0, 1).linkId() == t.cell (0, 0).linkId());
}

TEST_CASE ("OSC 8 ';;' (empty URI) resets the pen's linkId to 0", "[video][osc8][v5]")
{
    Test::Term t { 20, 2 };
    t.feed ("\x1b]8;;https://example.com\x07");
    t.feed ("a");
    t.feed ("\x1b]8;;\x07");
    t.feed ("b");

    REQUIRE (t.cell (0, 0).linkId() != 0);
    REQUIRE (t.cell (0, 1).linkId() == 0);
}

TEST_CASE ("malformed OSC 8 (no params/uri separator) clears the pen's linkId", "[video][osc8][v5]")
{
    // applyOSC() (jam_VideoOSC.cpp) strips one leading "8;" (the OSC command
    // header separator) before calling applyOsc8() — the payload it hands
    // applyOsc8() must itself contain zero further ';' to hit its own
    // "Malformed — no separator found" branch (jam_VideoOSCExt.cpp:81-85).
    Test::Term t { 20, 2 };
    t.feed ("\x1b]8;;https://example.com\x07");
    t.feed ("\x1b]8;justauri\x07");   // header ';' present, no second ';' inside applyOsc8's data
    t.feed ("x");

    REQUIRE (t.cell (0, 0).linkId() == 0);
}

TEST_CASE ("FLEX_GAP merge preserves the merge target's own linkId, not the current pen", "[video][osc8][v5][flexgap]")
{
    // jam_CursorState.cpp printCodepoint() doc: "prev, the merge target
    // rewritten by the FLEX_GAP branch, keeps its own already-stamped
    // linkId() rather than being re-stamped with the current pen state".
    Test::Term t { 20, 2 };

    t.feed ("\x1b]8;;https://example.com\x07");
    t.feed (" ");            // first space — written under the active link
    t.feed ("\x1b]8;;\x07"); // close the link — pen linkId back to 0
    t.feed (" ");            // second space — triggers the FLEX_GAP merge

    const auto first  { t.cell (0, 0) };
    const auto second { t.cell (0, 1) };

    REQUIRE (t.line (0).isJustified());
    REQUIRE (first.contentTag()  == jam::Char::FLEX_GAP);
    REQUIRE (second.contentTag() == jam::Char::FLEX_GAP);

    // The merge target (first space) keeps the linkId it already had —
    // stamped while the OSC 8 link was still open.
    REQUIRE (first.linkId() != 0);

    // The newly-written second space reflects the CURRENT pen (link closed).
    REQUIRE (second.linkId() == 0);
}
