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
 * creating a duplicate sibling — the base registers its SESSION group
 * under @c jam::IDtype::session, this class's own @c IDtype::session
 * resolves to the SAME `jam::IDtype` constant, so both sides reference the
 * identical node) — and declares its own Direction B root-level
 * parameters, unchanged from before this session.
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
    /** @brief Unity zoom — the seeded default and setZoom()'s reset target
     *  (end::View's zoomReset action, ActionRegistration.cpp). */
    static constexpr float defaultZoom { 1.0f };

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

    /** @brief Publishes the zoom factor — a font-size multiplier terminal::View
     *  applies in applyFont() before recomputing cell metrics, then re-enters
     *  its own resized() (the sole winsize author, terminal::View.cpp) with
     *  the new metrics. PANE-INSTANCE state: terminal::Model is one-per-
     *  Session (never a process-wide singleton, this class's own doc comment
     *  above), so one zoom value per pane is automatic — no extra bookkeeping
     *  needed. Reuses end::Model's own runtime @c ID::zoom verbatim (same
     *  Identifier, different ValueTree location — the documented reuse
     *  doctrine at Identifier.h:299-301). Processor/reader never sees this
     *  parameter — only the winsize/cellSize it indirectly produces via
     *  terminal::View's own applyFont() -> setCellSize() -> resized() ->
     *  setWinsize() chain.
     *  @param factor  Requested zoom factor — clamped to [zoomMin, zoomMax].
     *  @note Direction B. Any thread — lock-free.
     */
    void setZoom (float factor) noexcept
    {
        auto* parameter { getParameter<jam::Parameter<float>> (state.getType(), ID::zoom) };

        jassert (parameter != nullptr);
        parameter->setValue (juce::jlimit (zoomMin, zoomMax, factor));
    }

    /** @brief Adjusts the zoom factor by @p delta relative to its own current
     *  value — a pure tell: reads its own parameter, adds @p delta, clamps
     *  via the same [zoomMin, zoomMax] bound as setZoom(), and writes
     *  through the same parameter write path. Called by end::View's
     *  zoomIn/zoomOut actions (ActionRegistration.cpp) with +/- ID::zoomStep;
     *  zoomReset calls setZoom (defaultZoom) directly instead. terminal::View
     *  reads the resulting value via its own tree-listener path
     *  (session.getModel().state ValueTree, no getter — Model.h doc's
     *  Direction A passage), not through this class.
     *  @param delta  Signed adjustment added to the current zoom factor —
     *                positive zooms in, negative zooms out.
     *  @note Direction B. Any thread — lock-free.
     */
    void zoomBy (float delta) noexcept
    {
        auto* parameter { getParameter<jam::Parameter<float>> (state.getType(), ID::zoom) };

        jassert (parameter != nullptr);
        parameter->setValue (juce::jlimit (zoomMin, zoomMax, parameter->getValue() + delta));
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
        auto session { getOrCreateChildWithName (jam::IDtype::session) };

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
        createAndAddParameter<jam::Parameter<float>> (state, ID::zoom, defaultZoom);
        createAndAddParameter<jam::Parameter<int>> (state, ID::scrollbackLines, 0);
        createAndAddParameter<jam::Parameter<int>> (state, ID::clearRequested, 0);
    }

    /** @brief setZoom()'s clamp floor — see setZoom()'s own doc comment. */
    static constexpr float zoomMin { 0.25f };

    /** @brief setZoom()'s clamp ceiling — see setZoom()'s own doc comment. */
    static constexpr float zoomMax { 4.0f };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
