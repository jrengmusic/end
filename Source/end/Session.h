/**
 * @file end/Session.h
 * @brief Session — gui-less engine owner and jam::Model::Listener state
 *        machine (ARCHITECTURE.md "Session Layer — Landed Contract (2026-07-07)").
 */
#pragma once
#include <JuceHeader.h>
#include "end/ENDModel.h"
#include "terminal/TerminalProcessor.h"
#include "Identifier.h"

/** @class Session
 *  @brief Gui-less state of one ENDView's engines — the DAW-host analogy's
 *  "project" (Nexus is the Host; ARCHITECTURE.md's Session Layer — Landed
 *  Contract (2026-07-07)).
 *
 *  Authors a minimal, persistent SESSION node in ENDModel:
 *  @code
 *  SESSION (jam::ID::id = uuid.value)
 *    TABS (ID::focusedTab = 0 — topology authored in a later step)
 *  @endcode
 *  and registers as a @c jam::Model::Listener on @p newModel's root — every
 *  parameter change anywhere on that Model reaches this class's own @c
 *  parameterChanged(), single-key events-map dispatch (constructor):
 *  - @c ID::focusedPane — the SESSIONS node's own canonical copy (never
 *    authored here, see @c Nexus's own ctor) tells every owned Processor
 *    its own @c setFocus() edge (@c uuid == focused). No active-session
 *    filter needed — non-active sessions' processors correctly receive
 *    @c setFocus(false), since their terminals are never the focused one.
 *  The reaction consumes the event's own payload (@c parameterChanged()'s
 *  @c newValue) — this class never re-reads the tree. @c newTerminal()/
 *  @c removeTerminal() (below) are direct verbs, never tree signals — verbs
 *  never live in the state tree.
 *
 *  The SESSION subtree persists across View lifetimes — Session is a daemon
 *  unit (Nexus's own doc comment), never torn down when a View closes.
 *  Nexus::createSession()/removeSession() graft/detach @c state under
 *  SESSIONS directly (bare appendChild/removeChild) — this class carries no
 *  Attachment of its own.
 *
 *  Surface: @c get(uuid) resolves the Processor owning a terminal (the single
 *  orchestrator resolve seam), @c newTerminal(uuid) constructs the paired
 *  Processor and grafts its model.state under the uuid's own PANE node,
 *  @c removeTerminal(uuid) reverses that graft and erases the Processor,
 *  and the public @c state carries identity only.
 */
class Session : public jam::Model::Listener
{
public:
    /** @brief Constructs the minimal SESSION identity node — registers
     *  @c jam::ID::id (seeded from @p newUuid) as a Direction B parameter on
     *  @c state, appends a TABS child carrying @c ID::focusedTab (int64
     *  uuid, seeded 0), populates this class's own events map
     *  (@c ID::focusedPane -> setFocus tell), then registers as a @c
     *  jam::Model::Listener on @p newModel as the LAST statement — no event
     *  can be observed before every prior step has run. Grafting @c state
     *  under SESSIONS is @c Nexus::createSession()'s own responsibility
     *  (bare appendChild, this class's own doc comment).
     *  @param newUuid   Identifier for this session.
     *  @param newModel  App SSOT.
     *  @note MESSAGE THREAD.
     */
    Session (jam::UUID newUuid, ENDModel& newModel);

    /** @brief Deregisters this Session as a @c jam::Model::Listener.
     *  @note MESSAGE THREAD.
     */
    ~Session();

    /** @brief Resolves the Processor owning the terminal at @p uuid.
     *  @param uuid  Terminal identifier.
     *  @return Reference to the owning Processor.
     *  @note MESSAGE THREAD.
     */
    TerminalProcessor& get (jam::UUID uuid);

    /** @brief Constructs the TerminalProcessor paired to @p uuid and
     *  grafts its own @c model.state under the PANE node carrying that same
     *  uuid (resolved via @c jam::Model::getChildWithID against this
     *  Session's own @p model, bare appendChild — the caller's own PANE
     *  leaf, e.g. Panes::add(), must already be placed in the tree before
     *  calling this).
     *  @param uuid  Identifier for the new terminal's paired Processor.
     */
    void newTerminal (jam::UUID uuid);

    /** @brief Detaches the Processor's own model.state from its PANE parent
     *  and erases the Processor — the reverse of newTerminal(), called from
     *  the closePane path.
     *  @param uuid  Identifier of the terminal whose Processor is torn down.
     */
    void removeTerminal (jam::UUID uuid);

    /** @brief Cross-thread parameter-change dispatch — single-key lookup in
     *  @c events (this class's own ctor doc comment above).
     *  @note Fires on the calling thread (jam_Model.h:36-48) — the message
     *  thread for @c ID::focusedPane, written via a direct message-thread
     *  ValueTree property write.
     */
    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

    /** @brief Minimal SESSION node — identity (@c jam::ID::id) only; carries
     *  a TABS child holding @c ID::focusedTab (no PANE/TAB topology yet,
     *  this class's own doc comment above). Public — apvts canon, direct
     *  member access (no getter), matching jam::PaneManager's own
     *  leaf-registration convention. */
    juce::ValueTree state { jam::IDtype::session };

private:
    jam::UUID uuid;
    ENDModel& model;

    /** @brief Every terminal engine this Session owns, keyed by the owning
     *  terminal's jam::UUID::value — O(1) get/create/remove (Nexus's own
     *  uuid->Session keying precedent). */
    jam::HashMap<int64_t, std::unique_ptr<TerminalProcessor>> processors;

    /** @brief Event dispatch map — keyed by juce::Identifier (parameter id).
     *  @c ID::focusedPane -> setFocus tell to every owned Processor.
     *  Dispatched via single-key lookup in @c parameterChanged(). Populated
     *  in the constructor — one entry only, no separate registerEvents(). */
    jam::Function::Map<juce::Identifier, void> events;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
