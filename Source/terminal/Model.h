/**
 * @file terminal/Model.h
 * @brief terminal::Model — VT state SSOT (RFC-terminal-editor.md P12).
 */
#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @class Model
 * @brief Bidirectional SSOT state machine for one terminal session (P12).
 *
 * One instance per @c terminal::Session — NOT a process-wide singleton.
 * @c jam::Instance is deliberately NOT mixed in (unlike @c end::Model /
 * @c config::Model): @c jam::Instance<T> asserts a single global slot per
 * type in the standalone branch, which multiple concurrent panes/sessions
 * would violate.
 *
 * @par Direction A — reader to message (RFC P12)
 * Video/Processor @c store() atomics lock-free on the reader thread; the
 * inherited 60Hz @c flush() publishes to the ValueTree on the message
 * thread; @c terminal::View reacts via tree listeners. Schema: SESSION,
 * MODES, per-screen NORMAL/ALTERNATE, TEXT groups — a straight-line
 * @c createAndAddParameter call list (registerParameters(), below — the
 * schema SSOT, header-only).
 *
 * @par Direction B — message to reader (RFC P12)
 * @c terminal::View / @c Session call the setters below on the message
 * thread. @c terminal::Processor is a @c jam::Model::Listener —
 * @c parameterChanged() fires on the CALLING thread (@c jam_Model.h:36-48),
 * i.e. the message thread, and only wakes the reader (completed Step 6) —
 * it never executes on the reader thread.
 *
 * @see terminal::Processor
 * @see terminal::View
 * @see terminal::Session
 */
class Model : public jam::Model
{
public:
    Model()
        : jam::Model (IDtype::terminal)
    {
        registerParameters();
    }

    ~Model() override = default;

    /** @brief Publishes the width SSOT winsize (RFC P5/S4) — terminal
     *  columns/rows packed as one atomic parameter.
     *  @param size  Terminal viewport in cell columns (width) and rows (height).
     *  @note Direction B. Any thread — lock-free.
     */
    void setWinsize (jam::Size<int16_t> size) noexcept
    {
        auto* parameter { getParameter<jam::Parameter<int>> (state.getType(), ID::winsize) };

        jassert (parameter != nullptr);
        parameter->setValue (size.toInt());
    }

    /** @brief Publishes the cell pixel metrics — consumed by the reader for
     *  CSI 14t / XTWINOPS pixel size reports.
     *  @param size  Cell width (x) and height (y) in physical pixels.
     *  @note Direction B. Any thread — lock-free.
     */
    void setCellSize (jam::Size<int16_t> size) noexcept
    {
        auto* parameter { getParameter<jam::Parameter<int>> (state.getType(), ID::cellSize) };

        jassert (parameter != nullptr);
        parameter->setValue (size.toInt());
    }

    /** @brief Publishes the scrollback capacity — reader resizes its rings
     *  under the drain-fully -> resize -> resume protocol (RFC P6).
     *  @param lines  Maximum scrollback line count (config::Model terminal.scrollback_lines).
     *  @note Direction B. Any thread — lock-free.
     */
    void setScrollbackLines (int lines) noexcept
    {
        auto* parameter { getParameter<jam::Parameter<int>> (state.getType(), ID::scrollbackLines) };

        jassert (parameter != nullptr);
        parameter->setValue (lines);
    }

private:
    /** @brief Declares the full P12 schema as a straight-line
     *  @c createAndAddParameter call list, in P12 tree order — SESSION,
     *  MODES, per-screen NORMAL/ALTERNATE (registerScreenParameters()),
     *  TEXT, then the Direction B root parameters.
     *  @note MESSAGE THREAD — called once from the constructor.
     */
    void registerParameters()
    {
        juce::ValueTree session { IDtype::session };

        // SESSION (RFC P12 Direction A) — activeScreen/syncOutputActive/
        // shellExited/bell/promptRow/pasteEchoRemaining are jam::ID
        // (jam_IdentifierTerminal.h); gridSize is END-local (the
        // applied-winsize ack has no jam::ID counterpart).
        createAndAddParameter<jam::Parameter<int>> (session, ID::gridSize, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::activeScreen, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::syncOutputActive, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::shellExited, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::bell, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::promptRow, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::pasteEchoRemaining, 0);
        state.appendChild (session, nullptr);

        juce::ValueTree modes { jam::IDtype::modes };

        // MODES (RFC P12 Direction A) — bool parameters transported as
        // jam::Parameter<int> (0/1); no separate bool specialization exists
        // (jam_ParameterBase.h).
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::applicationCursor, 0);
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::applicationKeypad, 0);
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::bracketedPaste, 0);
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::mouseTracking, 0);
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::mouseMotionTracking, 0);
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::mouseAllTracking, 0);
        // mouseSgr is jam::Video's live DECSET 1006 wire identifier — reused
        // for the P12 "mouseSgrEncoding" schema slot rather than introducing
        // a duplicate identifier.
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::mouseSgr, 0);
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::focusEvents, 0);
        createAndAddParameter<jam::Parameter<int>> (modes, jam::ID::win32InputMode, 0);
        state.appendChild (modes, nullptr);

        // jam::IDtype::normal is jam-owned (jam_IdentifierButton.h, reused
        // here); IDtype::alternate is END-owned (Identifier.h) — no
        // jam-side counterpart.
        registerScreenParameters (jam::IDtype::normal);
        registerScreenParameters (IDtype::alternate);

        // jam::IDtype::text is the SVG-born "text" tag (jam_IdentifierSVG.h),
        // deliberately reused here as the terminal TEXT group tag — the SVG
        // and terminal trees are disjoint, so no runtime collision occurs.
        juce::ValueTree text { jam::IDtype::text };

        // TEXT (RFC P12 Direction A) — title/cwd are jam::ID;
        // foregroundProcess has no jam::ID counterpart.
        createAndAddParameter<jam::ParameterText> (text, jam::ID::title, juce::String {}, 256);
        createAndAddParameter<jam::ParameterText> (text, jam::ID::cwd, juce::String {}, 4096);
        createAndAddParameter<jam::ParameterText> (text, ID::foregroundProcess, juce::String {}, 256);
        state.appendChild (text, nullptr);

        // Direction B (message writes, Processor listens) — root-level
        // parameters, no wrapping group node (RFC P12 Direction B table
        // carries no group column).
        createAndAddParameter<jam::Parameter<int>> (state, ID::winsize, 0);
        createAndAddParameter<jam::Parameter<int>> (state, ID::cellSize, 0);
        createAndAddParameter<jam::Parameter<int>> (state, ID::scrollbackLines, 0);
        createAndAddParameter<jam::Parameter<int>> (state, ID::clearRequested, 0);
    }

    /** @brief Creates one SCREEN node (5 declared P12 parameters), appends
     *  it to @c state. Called once per screen type from registerParameters().
     *  @param screenType  Node type — jam::IDtype::normal or IDtype::alternate.
     *  @note MESSAGE THREAD — called once per screen from the constructor.
     */
    void registerScreenParameters (const juce::Identifier& screenType)
    {
        juce::ValueTree screen { screenType };

        createAndAddParameter<jam::Parameter<int>> (screen, jam::ID::cursor, 0);
        createAndAddParameter<jam::Parameter<int>> (screen, jam::ID::cursorShape, 0);
        createAndAddParameter<jam::Parameter<int>> (screen, jam::ID::cursorColor, 0);
        createAndAddParameter<jam::Parameter<int>> (screen, jam::ID::keyboardFlags, 0);
        createAndAddParameter<jam::Parameter<int>> (screen, jam::ID::screenDirty, 0);
        state.appendChild (screen, nullptr);
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
