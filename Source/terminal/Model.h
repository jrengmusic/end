/**
 * @file terminal/Model.h
 * @brief terminal::Model — VT state SSOT.
 */
#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @class Model
 * @brief Bidirectional SSOT state machine for one terminal session.
 *
 * One instance per @c terminal::Session — NOT a process-wide singleton.
 * @c jam::Instance is deliberately NOT mixed in (unlike @c end::Model /
 * @c config::Model): @c jam::Instance<T> asserts a single global slot per
 * type in the standalone branch, which multiple concurrent panes/sessions
 * would violate.
 *
 * @par jam::terminal::Model base — framework owns VT-machine state
 * Extends @c jam::terminal::Model (not @c jam::Model directly): the base
 * class registers every VT-state-machine group @c jam::terminal::Video
 * writes through directly (SESSION/MODES/NORMAL/ALTERNATE/TEXT —
 * `jam_terminal/model/jam_Model.h`). This class's own
 * @c registerParameters() therefore only EXTENDS two of those groups with
 * app/TTY-domain properties that have no VT-machine meaning
 * (@c getOrCreateChildWithName() finds the base-created node rather than
 * creating a duplicate sibling — @c juce::Identifier equality is by
 * interned string content, not C++ symbol identity, so the base's
 * framework-local @c sessionGroupTag/`alternateGroupTag`
 * (`jam_Model.h`) and this class's own @c IDtype::session/`IDtype::alternate`
 * resolve to the SAME physical node) — and declares its own Direction B
 * root-level parameters, unchanged from before this session.
 *
 * @par Direction A — reader to message
 * Video/Processor @c store() atomics lock-free on the reader thread; the
 * inherited 60Hz @c flush() publishes to the ValueTree on the message
 * thread; @c terminal::View reacts via tree listeners. Schema: SESSION,
 * MODES, per-screen NORMAL/ALTERNATE, TEXT groups — a straight-line
 * @c createAndAddParameter call list (registerParameters(), below — the
 * schema SSOT, header-only).
 *
 * @par Direction B — message to reader
 * @c terminal::View / @c Session call the setters below on the message
 * thread. @c terminal::Processor is a @c jam::Model::Listener —
 * @c parameterChanged() fires on the CALLING thread (@c jam_Model.h:36-48),
 * i.e. the message thread, and only wakes the reader — it never executes
 * on the reader thread.
 *
 * @see terminal::Processor
 * @see terminal::View
 * @see terminal::Session
 */
class Model : public jam::terminal::Model
{
public:
    Model()
        : jam::terminal::Model (IDtype::terminal)
    {
        registerParameters();
    }

    ~Model() override = default;

    /** @brief Publishes the width SSOT winsize — terminal
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
     *  under the drain-fully -> resize -> resume protocol.
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
    /** @brief Extends the base-registered SESSION/TEXT groups with app/TTY-
     *  domain properties that have no VT-machine meaning, and declares the
     *  Direction B root-level parameters.
     *  @note MESSAGE THREAD — called once from the constructor.
     */
    void registerParameters()
    {
        // SESSION (Direction A) — jam::terminal::Model::registerSession()
        // (the base ctor, already run) created this node and owns
        // activeScreen/syncOutputActive/bell/promptRow. shellExited/
        // pasteEchoRemaining are jam::ID (jam_IdentifierTerminal.h);
        // gridSize is END-local (the applied-winsize ack has no jam::ID
        // counterpart) — none of the three is VT-machine state, so they
        // stay app-owned, added onto the SAME node the base created.
        auto session { getOrCreateChildWithName (IDtype::session) };

        createAndAddParameter<jam::Parameter<int>> (session, ID::gridSize, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::shellExited, 0);
        createAndAddParameter<jam::Parameter<int>> (session, jam::ID::pasteEchoRemaining, 0);

        // TEXT (Direction A) — jam::terminal::Model::registerText() (the
        // base ctor) created this node and owns title/cwd. foregroundProcess
        // has no jam::ID counterpart and is TTY domain, not VT-machine
        // state — added onto the SAME node the base created.
        auto text { getOrCreateChildWithName (jam::IDtype::text) };

        createAndAddParameter<jam::ParameterText> (text, ID::foregroundProcess, juce::String {}, 256);

        // Direction B (message writes, Processor listens) — root-level
        // parameters, no wrapping group node (Direction B table
        // carries no group column).
        createAndAddParameter<jam::Parameter<int>> (state, ID::winsize, 0);
        createAndAddParameter<jam::Parameter<int>> (state, ID::cellSize, 0);
        createAndAddParameter<jam::Parameter<int>> (state, ID::scrollbackLines, 0);
        createAndAddParameter<jam::Parameter<int>> (state, ID::clearRequested, 0);
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
