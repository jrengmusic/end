/**
 * @file SyncOutputTests.cpp
 * @brief V2 conformance — DEC mode 2026 (SYNC_OUTPUT) auto-reset.
 *
 * Coverage: the ratified SYNC_OUTPUT auto-reset design (loop-top guard, no
 * timer). `Video::clearSyncOutputIfExpired()` is
 * public — this suite exercises it directly (no friendship, no member
 * manipulation).
 *
 * The expiry test drives `Test::SyncOutputDeadlineTerm` (`TestTerm.h`) — a
 * fixture built on `SyncOutputDeadlineVideo`, which writes the protected
 * `Video::syncOutputDeadlineMs` field directly — forcing an already-elapsed
 * SYNC_OUTPUT deadline deterministically instead of a real-time sleep past
 * `Video::syncResetMs`.
 */

#include "catch2/catch.hpp"
#include "TestTerm.h"

TEST_CASE ("mode 2026 defaults to reset", "[video][mode2026][v2]")
{
    // Video::syncOutputActive is deliberately NOT a jam::terminal::map::DecMode
    // bimap entry / jam::terminal::Model MODES parameter (jam_CursorState.h
    // syncOutputActive doc: "kept outside the bimap because that branch also
    // fires the syncOutput event") — Test::Term::mode() cannot observe it.
    // Query via DECRQM instead, the surface Video itself exposes for this mode.
    Test::Term t { 10, 2 };
    t.feed ("\x1b[?2026$p");
    REQUIRE (t.lastResponse() == "\x1b[?2026;2$y");
}

TEST_CASE ("DECSET 2026 sets sync output; DECRQM reports set", "[video][mode2026][v2]")
{
    // See WritePathTests.cpp "Mode 2027" section header — the $p/$q
    // dispatch-routing fix applies identically here.
    Test::Term t { 10, 2 };
    t.feed ("\x1b[?2026h");

    t.clearResponse();
    t.feed ("\x1b[?2026$p");
    REQUIRE (t.lastResponse() == "\x1b[?2026;1$y");
}

TEST_CASE ("DECRST 2026 clears sync output via disableSyncOutput; DECRQM reports reset", "[video][mode2026][v2]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[?2026h");
    t.feed ("\x1b[?2026l");

    t.clearResponse();
    t.feed ("\x1b[?2026$p");
    REQUIRE (t.lastResponse() == "\x1b[?2026;2$y");
}

TEST_CASE ("clearSyncOutputIfExpired is a no-op before the deadline", "[video][mode2026][v2]")
{
    Test::Term t { 10, 2 };
    t.feed ("\x1b[?2026h");

    t.videoRef().clearSyncOutputIfExpired();

    t.clearResponse();
    t.feed ("\x1b[?2026$p");
    REQUIRE (t.lastResponse() == "\x1b[?2026;1$y");
}

TEST_CASE ("clearSyncOutputIfExpired force-clears an elapsed deadline", "[video][mode2026][v2]")
{
    Test::SyncOutputDeadlineTerm t { 10, 2 };
    t.feed ("\x1b[?2026h");

    // Force the deadline into the past — clearSyncOutputIfExpired() compares
    // it against juce::Time::getMillisecondCounterHiRes(), which is already
    // well past 0.0 by the time this test runs. No sleep needed.
    t.videoRef().setSyncOutputDeadlineMs (0.0);

    t.videoRef().clearSyncOutputIfExpired();

    // Same DECRST-equivalent assertions as "DECRST 2026 clears sync output"
    // above — clearSyncOutputIfExpired() shares disableSyncOutput()'s effects.
    t.clearResponse();
    t.feed ("\x1b[?2026$p");
    REQUIRE (t.lastResponse() == "\x1b[?2026;2$y");
}
