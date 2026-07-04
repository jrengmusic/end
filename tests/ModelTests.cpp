/**
 * @file ModelTests.cpp
 * @brief `terminal::Model` P12 scripted-stub validation (RFC-terminal-editor.md
 *        S7.4 validation gate, PLAN-terminal-editor.md Step 5).
 *
 * Exercises `terminal::Model` (Source/terminal/Model.h, header-only — END's own P12
 * SSOT state machine, distinct from `jam::terminal::Model` which TestTerm.h's
 * fixture uses for plain VT mode state) directly, with no TTY, no Processor,
 * no Video — the "scripted Processor stub" the S7.4 gate calls for is the
 * `StubListener` below, which mimics `terminal::Processor`'s
 * `jam::Model::Listener` shape (Processor.h) without the reader-thread wake.
 *
 * @par Coverage
 * - Schema shape: SESSION/MODES/NORMAL/ALTERNATE/TEXT children in that order
 *   under a TERMINAL root, every group's declared parameter set present with
 *   no extras, Direction B parameters living directly on the root.
 * - Direction A (reader -> message): `getParameter<jam::Parameter<int>>()->
 *   setValue()` is the real reader-thread write path — atomic store + dirty
 *   mark (`ParameterAdapter::flushToTree()` is CAS-gated on `needsUpdate`,
 *   set ONLY by `parameterValueChanged`, which only `Parameter<T>::setValue()`
 *   fires — jam_Model.cpp:78-97). Raw `getRawParameterValue<int>()->store()`
 *   never marks dirty and never reaches the ValueTree — it is read-path-only
 *   for non-message threads. `Model::flush()` (public, MESSAGE THREAD — the
 *   only test-invocable flush path; `timerCallback()` is private) syncs the
 *   dirty parameter to the ValueTree.
 * - Direction B (message -> reader): `setWinsize`/`setCellSize`/
 *   `setScrollbackLines` fire `parameterChanged` on a registered
 *   `jam::Model::Listener`, packed `jam::Size<int16_t>` values round-trip.
 * - `jam::ParameterText` (TEXT/title): setValue/getValue round-trip and the
 *   maxlen-1 truncation contract (`jam_ParameterText.h` setValue doc).
 *
 * @par Identifier scoping
 * `ID`/`IDtype` (Identifier.h) are declared at GLOBAL scope, not inside
 * `namespace terminal` — Model.cpp itself references them unqualified for
 * the same reason. `jam::ID`/`jam::IDtype` require the `jam::` prefix.
 */

#include "catch2/catch.hpp"
#include "terminal/Model.h"

namespace
{
/** @brief Stub `jam::Model::Listener` mimicking `terminal::Processor`'s shape
 *  (Processor.h) — captures the last `parameterChanged()` call for assertion
 *  instead of nudging a TTY poll wake fd (RFC-terminal-editor.md P12
 *  Direction B wake seam). */
struct StubListener : public jam::Model::Listener
{
    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override
    {
        lastId = id;
        lastValue = newValue;
        ++callCount;
    }

    juce::Identifier lastId;
    juce::var lastValue;
    int callCount { 0 };
};

/** @brief Asserts @p tree carries exactly the properties in @p expected — no
 *  fewer, no extras. Local test-only utility, not production code. */
void requireExactProperties (const juce::ValueTree& tree,
                             std::initializer_list<juce::Identifier> expected)
{
    REQUIRE (tree.getNumProperties() == static_cast<int> (expected.size()));

    for (auto& id : expected)
        REQUIRE (tree.hasProperty (id));
}
} // namespace

// ============================================================================
// Schema shape (RFC-terminal-editor.md P12, PLAN-terminal-editor.md Step 5)
// ============================================================================

TEST_CASE ("terminal::Model registers SESSION/MODES/NORMAL/ALTERNATE/TEXT under a TERMINAL root, in that order", "[model][schema]")
{
    terminal::Model model;
    auto& state { model.state };

    REQUIRE (state.getType() == IDtype::terminal);
    REQUIRE (state.getNumChildren() == 5);
    REQUIRE (state.getChild (0).getType() == IDtype::session);
    REQUIRE (state.getChild (1).getType() == jam::IDtype::modes);
    REQUIRE (state.getChild (2).getType() == jam::IDtype::normal);
    REQUIRE (state.getChild (3).getType() == IDtype::alternate);
    REQUIRE (state.getChild (4).getType() == jam::IDtype::text);
}

TEST_CASE ("SESSION carries exactly its 7 declared P12 parameters, no extras", "[model][schema]")
{
    terminal::Model model;
    auto session { model.state.getChildWithName (IDtype::session) };

    requireExactProperties (session,
        {
            ID::gridSize,
            jam::ID::activeScreen,
            jam::ID::syncOutputActive,
            jam::ID::shellExited,
            jam::ID::bell,
            jam::ID::promptRow,
            jam::ID::pasteEchoRemaining,
        });
}

TEST_CASE ("MODES carries exactly its 9 declared P12 parameters, no extras", "[model][schema]")
{
    terminal::Model model;
    auto modes { model.state.getChildWithName (jam::IDtype::modes) };

    requireExactProperties (modes,
        {
            jam::ID::applicationCursor,
            jam::ID::applicationKeypad,
            jam::ID::bracketedPaste,
            jam::ID::mouseTracking,
            jam::ID::mouseMotionTracking,
            jam::ID::mouseAllTracking,
            jam::ID::mouseSgr,
            jam::ID::focusEvents,
            jam::ID::win32InputMode,
        });
}

TEST_CASE ("NORMAL screen carries exactly its 5 declared P12 SCREEN parameters, no extras", "[model][schema]")
{
    terminal::Model model;
    auto normal { model.state.getChildWithName (jam::IDtype::normal) };

    requireExactProperties (normal,
        {
            jam::ID::cursor,
            jam::ID::cursorShape,
            jam::ID::cursorColor,
            jam::ID::keyboardFlags,
            jam::ID::screenDirty,
        });
}

TEST_CASE ("ALTERNATE screen carries exactly its 5 declared P12 SCREEN parameters, no extras", "[model][schema]")
{
    terminal::Model model;
    auto alternate { model.state.getChildWithName (IDtype::alternate) };

    requireExactProperties (alternate,
        {
            jam::ID::cursor,
            jam::ID::cursorShape,
            jam::ID::cursorColor,
            jam::ID::keyboardFlags,
            jam::ID::screenDirty,
        });
}

TEST_CASE ("TEXT carries exactly its 3 declared P12 parameters, no extras", "[model][schema]")
{
    terminal::Model model;
    auto text { model.state.getChildWithName (jam::IDtype::text) };

    requireExactProperties (text,
        {
            jam::ID::title,
            jam::ID::cwd,
            ID::foregroundProcess,
        });
}

TEST_CASE ("Direction B parameters live directly on the TERMINAL root, no wrapping group child", "[model][schema]")
{
    terminal::Model model;

    requireExactProperties (model.state,
        {
            ID::winsize,
            ID::cellSize,
            ID::scrollbackLines,
            ID::clearRequested,
        });
}

// ============================================================================
// Direction A — reader (atomic store) -> message (ValueTree via flush())
// ============================================================================

TEST_CASE ("Direction A: setValue() on a SESSION parameter reaches the ValueTree after flush()", "[model][directiona]")
{
    terminal::Model model;

    auto* gridSize { model.getParameter<jam::Parameter<int>> (IDtype::session, ID::gridSize) };
    REQUIRE (gridSize != nullptr);

    // setValue() is the real Direction A write path (test thread stands in
    // for the reader thread — RFC-terminal-editor.md P12 Direction A):
    // Parameter<int>::setValue() (jam_ParameterBase.h) stores the atomic AND
    // calls sendValueChangedMessageToListeners(), which reaches
    // ParameterAdapter::parameterValueChanged() and sets needsUpdate = true
    // (jam_Model.cpp:78-97). A raw getRawParameterValue<int>()->store() never
    // marks needsUpdate and never reaches the ValueTree.
    gridSize->setValue (42);

    // Model::flush() is the only test-invocable flush path — timerCallback()
    // (jam_Model.h) is private and 10Hz-timer-driven; flush() is the public
    // MESSAGE THREAD entry point it calls internally (jam_Model.cpp).
    // flushToTree() is CAS-gated on needsUpdate and is only ever invoked from
    // flush()/timerCallback(), never from parameterValueChanged() itself —
    // an explicit flush() is still required after setValue().
    model.flush();

    auto session { model.state.getChildWithName (IDtype::session) };
    REQUIRE (static_cast<int> (session.getProperty (ID::gridSize)) == 42);
}

TEST_CASE ("screenDirty counter increments via setValue() on the NORMAL screen are visible on the ValueTree after flush()", "[model][directiona][screendirty]")
{
    terminal::Model model;

    auto* screenDirty { model.getParameter<jam::Parameter<int>> (jam::IDtype::normal, jam::ID::screenDirty) };
    REQUIRE (screenDirty != nullptr);

    // Reader-thread counter bump: load current value, setValue() the
    // increment — setValue() is the write path that marks needsUpdate
    // (jam_ParameterBase.h Parameter<int>::setValue), unlike a raw
    // fetch_add() on the atomic which never reaches the ValueTree.
    screenDirty->setValue (screenDirty->getValue() + 1);
    screenDirty->setValue (screenDirty->getValue() + 1);
    screenDirty->setValue (screenDirty->getValue() + 1);

    model.flush();

    auto normal { model.state.getChildWithName (jam::IDtype::normal) };
    REQUIRE (static_cast<int> (normal.getProperty (jam::ID::screenDirty)) == 3);
}

// ============================================================================
// Direction B — message (setter) -> reader (parameterChanged wake seam)
// ============================================================================

TEST_CASE ("Direction B: setWinsize fires parameterChanged with ID::winsize; the packed value round-trips", "[model][directionb]")
{
    terminal::Model model;
    StubListener listener;
    model.addListener (&listener);

    model.setWinsize (jam::Size<int16_t> (120, 40));

    REQUIRE (listener.callCount == 1);
    REQUIRE (listener.lastId == ID::winsize);

    const jam::Size<int16_t> roundTripped { static_cast<int> (listener.lastValue) };
    const auto [width, height] { roundTripped };

    REQUIRE (width == 120);
    REQUIRE (height == 40);

    model.removeListener (&listener);
}

TEST_CASE ("Direction B: setCellSize fires parameterChanged with ID::cellSize; the packed value round-trips", "[model][directionb]")
{
    terminal::Model model;
    StubListener listener;
    model.addListener (&listener);

    model.setCellSize (jam::Size<int16_t> (9, 18));

    REQUIRE (listener.callCount == 1);
    REQUIRE (listener.lastId == ID::cellSize);

    const jam::Size<int16_t> roundTripped { static_cast<int> (listener.lastValue) };
    const auto [width, height] { roundTripped };

    REQUIRE (width == 9);
    REQUIRE (height == 18);

    model.removeListener (&listener);
}

TEST_CASE ("Direction B: setScrollbackLines fires parameterChanged with ID::scrollbackLines and the raw line count", "[model][directionb]")
{
    terminal::Model model;
    StubListener listener;
    model.addListener (&listener);

    model.setScrollbackLines (5000);

    REQUIRE (listener.callCount == 1);
    REQUIRE (listener.lastId == ID::scrollbackLines);
    REQUIRE (static_cast<int> (listener.lastValue) == 5000);

    model.removeListener (&listener);
}

// ============================================================================
// jam::ParameterText — TEXT/title (jam_ParameterText.h contract)
// ============================================================================

TEST_CASE ("ParameterText: TEXT/title setValue/getValue round-trips under maxlen", "[model][parametertext]")
{
    terminal::Model model;
    auto* title { model.getParameter<jam::ParameterText> (jam::IDtype::text, jam::ID::title) };
    REQUIRE (title != nullptr);

    title->setValue (juce::String ("xterm-256color"));
    REQUIRE (title->getValue() == juce::String ("xterm-256color"));
}

TEST_CASE ("ParameterText: TEXT/title truncates to maxlen - 1 bytes (jam_ParameterText.h setValue contract)", "[model][parametertext]")
{
    terminal::Model model;
    auto* title { model.getParameter<jam::ParameterText> (jam::IDtype::text, jam::ID::title) };
    REQUIRE (title != nullptr);

    // The TEXT/title registration (Model.h, maxlen 256) seeds a 256-byte
    // double buffer; ParameterText::setValue() copies min(length, bufferSize
    // - 1) bytes and null-terminates (jam_ParameterText.h:68-76) — a 300-char
    // ASCII title truncates to 255 chars.
    const juce::String longTitle { juce::String::repeatedString ("a", 300) };
    title->setValue (longTitle);

    const auto truncated { title->getValue() };
    REQUIRE (truncated.length() == 255);
    REQUIRE (truncated == juce::String::repeatedString ("a", 255));
}
