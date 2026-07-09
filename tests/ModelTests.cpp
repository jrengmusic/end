/**
 * @file ModelTests.cpp
 * @brief `TerminalModel` scripted-stub validation.
 *
 * Exercises `TerminalModel` (Source/terminal/TerminalModel.h, header-only —
 * `jam::terminal::Model` subclass, INHERITING every VT-state-machine group
 * (root scalars — former VIDEO/MODES groups, dissolved onto the root
 * ARCHITECT-ratified 2026-07-08 — plus NORMAL/ALTERNATE/TEXT child nodes,
 * `jam_terminal/model/jam_TerminalModel.h`) and extending TEXT with an app/TTY-domain
 * property that has no VT-machine meaning, plus its own app-domain siblings
 * of the former VIDEO scalars directly onto the root) directly, with no TTY,
 * no Processor, no Video — the "scripted Processor stub" this validation
 * calls for is the `StubListener` below, which mimics `TerminalProcessor`'s
 * `jam::Model::Listener` shape (TerminalProcessor.h) without the reader-thread wake.
 *
 * @par Coverage
 * - Schema shape: NORMAL/ALTERNATE/TEXT children in that order under a
 *   TERMINAL root (NORMAL/ALTERNATE/TEXT nodes created by the inherited
 *   `jam::terminal::Model` base constructor; TEXT then extended in place by
 *   `TerminalModel::registerParameters()` — `juce::Identifier` equality is
 *   by interned string content, so `getOrCreateChildWithName()` finds the
 *   SAME node the base created), the root's full declared parameter set
 *   present with no extras (former VIDEO/MODES scalars, END's app-domain
 *   siblings, and Direction B parameters — every root-level parameter
 *   shares one flat tree, no wrapping group child).
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
 * `ID`/`IDtype` (Identifier.h) are declared at GLOBAL scope — TerminalModel
 * itself references them unqualified for the same reason. `jam::ID`/
 * `jam::IDtype` require the `jam::` prefix.
 */

#include "catch2/catch.hpp"
#include "terminal/TerminalModel.h"

namespace
{
/** @brief Stub `jam::Model::Listener` mimicking `TerminalProcessor`'s shape
 *  (TerminalProcessor.h) — captures the last `parameterChanged()` call for assertion
 *  instead of nudging a TTY poll wake fd (the Direction B wake seam). */
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
// Schema shape
// ============================================================================

TEST_CASE ("TerminalModel registers NORMAL/ALTERNATE/TEXT under a TERMINAL root, in that order; root scalars flatten directly onto the root", "[model][schema]")
{
    // ARCHITECT-ratified dissolve (2026-07-08): the former VIDEO/MODES child
    // nodes are gone — jam::terminal::Model::registerParameters() (base
    // ctor) now registers their scalars directly onto `state`, so the root
    // carries 3 children instead of 5.
    TerminalModel model;
    auto& state { model.state };

    REQUIRE (state.getType() == IDtype::terminal);
    REQUIRE (state.getNumChildren() == 3);
    REQUIRE (state.getChild (0).getType() == jam::IDtype::normal);
    REQUIRE (state.getChild (1).getType() == jam::IDtype::alternate);
    REQUIRE (state.getChild (2).getType() == jam::IDtype::text);
}

TEST_CASE ("TERMINAL root carries exactly its 23 declared parameters (former VIDEO/MODES scalars + END app-domain siblings + Direction B), no extras", "[model][schema]")
{
    // Former VIDEO group (4) + former MODES group — 9-entry DecMode bimap
    // (ARCHITECT ruling "conform, multiple bool is wrong": mouse tracking
    // 1000/1002/1003 collapsed onto the single mouseTracking parameter,
    // jam::terminal::MouseTracking-valued, excluded from the bimap the same
    // way insertMode is — jam_DecMode.h's "Mouse tracking excluded" doc) +
    // insertMode + mouseTracking (2) + END's app-domain siblings of the
    // former VIDEO scalars (3) + Direction B (5) = 23. All now register
    // directly onto the same root `state` tree — no wrapping group child
    // (ARCHITECT-ratified dissolve, 2026-07-08).
    TerminalModel model;

    requireExactProperties (model.state,
        {
            // Former VIDEO group
            jam::ID::activeScreen,
            jam::ID::syncOutputActive,
            jam::ID::bell,
            jam::ID::promptRow,
            // Former MODES group — DecMode bimap entries
            jam::ID::applicationCursor,
            jam::ID::reverseVideo,
            jam::ID::autoWrap,
            jam::ID::applicationKeypad,
            jam::ID::focusEvents,
            jam::ID::mouseSgr,
            jam::ID::bracketedPaste,
            jam::ID::win32InputMode,
            jam::ID::graphemeClustering,
            // Former MODES group — outside the bimap
            jam::ID::insertMode,
            jam::ID::mouseTracking,
            // END app-domain siblings (TerminalModel::registerParameters())
            ID::gridSize,
            jam::ID::shellExited,
            jam::ID::pasteEchoRemaining,
            // Direction B
            ID::winsize,
            ID::cellSize,
            ID::zoom,
            ID::scrollbackLines,
            ID::clearRequested,
        });
}

TEST_CASE ("NORMAL screen carries exactly its 14 declared SCREEN parameters "
           "(cursor/cursorShape/cursorColor/keyboardFlags/screenDirty + "
           "8-slot progressive-keyboard-protocol save stack + count), no extras", "[model][schema]")
{
    // Grown from 5 to 14 this sprint: jam::terminal::Model::
    // registerScreenParameters() now also registers the CSI `>`/`<` u
    // progressive keyboard protocol save stack — savedKeyboardFlagsDepth (8)
    // indexed slots plus savedKeyboardFlagsCount — real per-screen schema
    // parameters, not a Video-local stack.
    TerminalModel model;
    auto normal { model.state.getChildWithName (jam::IDtype::normal) };

    requireExactProperties (normal,
        {
            jam::ID::cursor,
            jam::ID::cursorShape,
            jam::ID::cursorColor,
            jam::ID::keyboardFlags,
            jam::ID::screenDirty,
            jam::terminal::Model::savedKeyboardFlagsSlot (0),
            jam::terminal::Model::savedKeyboardFlagsSlot (1),
            jam::terminal::Model::savedKeyboardFlagsSlot (2),
            jam::terminal::Model::savedKeyboardFlagsSlot (3),
            jam::terminal::Model::savedKeyboardFlagsSlot (4),
            jam::terminal::Model::savedKeyboardFlagsSlot (5),
            jam::terminal::Model::savedKeyboardFlagsSlot (6),
            jam::terminal::Model::savedKeyboardFlagsSlot (7),
            jam::ID::savedKeyboardFlagsCount,
        });
}

TEST_CASE ("ALTERNATE screen carries exactly its 14 declared SCREEN parameters "
           "(cursor/cursorShape/cursorColor/keyboardFlags/screenDirty + "
           "8-slot progressive-keyboard-protocol save stack + count), no extras", "[model][schema]")
{
    // Grown from 5 to 14 this sprint — see the NORMAL screen test above.
    TerminalModel model;
    auto alternate { model.state.getChildWithName (jam::IDtype::alternate) };

    requireExactProperties (alternate,
        {
            jam::ID::cursor,
            jam::ID::cursorShape,
            jam::ID::cursorColor,
            jam::ID::keyboardFlags,
            jam::ID::screenDirty,
            jam::terminal::Model::savedKeyboardFlagsSlot (0),
            jam::terminal::Model::savedKeyboardFlagsSlot (1),
            jam::terminal::Model::savedKeyboardFlagsSlot (2),
            jam::terminal::Model::savedKeyboardFlagsSlot (3),
            jam::terminal::Model::savedKeyboardFlagsSlot (4),
            jam::terminal::Model::savedKeyboardFlagsSlot (5),
            jam::terminal::Model::savedKeyboardFlagsSlot (6),
            jam::terminal::Model::savedKeyboardFlagsSlot (7),
            jam::ID::savedKeyboardFlagsCount,
        });
}

TEST_CASE ("TEXT carries exactly its 3 declared parameters, no extras", "[model][schema]")
{
    TerminalModel model;
    auto text { model.state.getChildWithName (jam::IDtype::text) };

    requireExactProperties (text,
        {
            jam::ID::title,
            jam::ID::cwd,
            ID::foregroundProcess,
        });
}

// ============================================================================
// Direction A — reader (atomic store) -> message (ValueTree via flush())
// ============================================================================

TEST_CASE ("Direction A: setValue() on a root scalar reaches the ValueTree after flush()", "[model][directiona]")
{
    TerminalModel model;

    auto* gridSize { model.getParameter<jam::Parameter<int>> (model.getType(), ID::gridSize) };
    REQUIRE (gridSize != nullptr);

    // setValue() is the real Direction A write path (test thread stands in
    // for the reader thread):
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

    REQUIRE (static_cast<int> (model.state.getProperty (ID::gridSize)) == 42);
}

TEST_CASE ("screenDirty counter increments via setValue() on the NORMAL screen are visible on the ValueTree after flush()", "[model][directiona][screendirty]")
{
    TerminalModel model;

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
    TerminalModel model;
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
    TerminalModel model;
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
    TerminalModel model;
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
    TerminalModel model;
    auto* title { model.getParameter<jam::ParameterText> (jam::IDtype::text, jam::ID::title) };
    REQUIRE (title != nullptr);

    title->setValue (juce::String ("xterm-256color"));
    REQUIRE (title->getValue() == juce::String ("xterm-256color"));
}

TEST_CASE ("ParameterText: TEXT/title truncates to maxlen - 1 bytes (jam_ParameterText.h setValue contract)", "[model][parametertext]")
{
    TerminalModel model;
    auto* title { model.getParameter<jam::ParameterText> (jam::IDtype::text, jam::ID::title) };
    REQUIRE (title != nullptr);

    // The TEXT/title registration (TerminalModel.h, maxlen 256) seeds a 256-byte
    // double buffer; ParameterText::setValue() copies min(length, bufferSize
    // - 1) bytes and null-terminates (jam_ParameterText.h:68-76) — a 300-char
    // ASCII title truncates to 255 chars.
    const juce::String longTitle { juce::String::repeatedString ("a", 300) };
    title->setValue (longTitle);

    const auto truncated { title->getValue() };
    REQUIRE (truncated.length() == 255);
    REQUIRE (truncated == juce::String::repeatedString ("a", 255));
}
