/**
 * @file Nexus.h
 * @brief Session host — gui-less, daemon-capable. Owns ENDModel (the app
 *        SSOT) and every Session.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Session.h"

/** @brief Session host — DAW-host analog. Owns ENDModel and every
 *  Session, keyed by jam::UUID.
 *
 *  Singleton via jam::Instance. Bootstraps ENDModel's WINDOW node bare —
 *  zero children authored here (jam::PaneManager's flat model: geometry IS
 *  the state, no PANES container, no ratios, no position-as-id). ENDView
 *  adopts the SAME WINDOW tree afterward, authoring its own center PANE
 *  leaf (jam::UUID-keyed) and owning the long-lived jam::PaneManager
 *  (layout()/split()/remove()) for that tree. Also bootstraps SESSIONS
 *  (IDtype::sessions), registering
 *  ID::focusedSession (int64 uuid value, 0 = none) and ID::focusedPane
 *  (int64 uuid value, 0 = none) on the SESSIONS node — both app-level SSOT,
 *  owned here rather than by any View. Creates zero Sessions here. Session
 *  creation is ENDApplication's own bootstrap responsibility (Main.cpp's
 *  initialise(), after ConfigModel exists — a Session's Processor binds a
 *  ConfigModel& at construction, so none may exist before config does): the
 *  first createSession() call sets ID::focusedSession since it starts at 0,
 *  and getActiveSession() asserts fail-fast if called before that first
 *  Session exists.
 *
 *  Phase 3/15: standalone mode only; daemon mode (IPC, session persistence)
 *  is unimplemented.
 */
struct Nexus : jam::Instance<Nexus>
{
    /** @brief Bootstraps ENDModel's WINDOW/SESSIONS nodes — WINDOW is
     *  created bare (zero children; ENDView authors its own center PANE
     *  leaf once it adopts this tree) — no Session is created here (see
     *  class doc). @c model (declared first) is fully constructed and
     *  self-registered (jam::Instance<ENDModel>) before this body runs,
     *  which is what lets ConfigModel's own appModel member
     *  (Source/config/ConfigModel.h) resolve *ENDModel::getInstance() safely
     *  once config constructs after this.
     *  @note MESSAGE THREAD.
     */
    Nexus()
    {
        model.getOrCreateChildWithName (IDtype::window);

        auto sessionsTree { model.getOrCreateChildWithName (IDtype::sessions) };
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, ID::focusedSession, int64_t { 0 });
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, ID::focusedPane, int64_t { 0 });

        // Bootstraps the OVERLAY node viewless, so the state exists before
        // ENDView's own MessageOverlay ever constructs (MessageOverlay
        // adopts this SAME tree — its own adopt ctor).
        model.getOrCreateChildWithName (IDtype::overlay);
    }

    /** @brief Creates a new Session with a fresh uuid, grafts its state
     *  under SESSIONS bare (verb-direct — Session carries no Attachment of
     *  its own, this class's own doc comment), and — if no session is
     *  active yet (@c ID::focusedSession still 0) — makes it the active
     *  one.
     *  @return Reference to the created Session.
     *  @note MESSAGE THREAD.
     */
    Session& createSession()
    {
        jam::UUID sessionUuid;
        auto [entry, inserted] = sessions.try_emplace (
            sessionUuid.value, std::make_unique<Session> (sessionUuid, model));
        jassert (inserted);
        auto& [key, session] = *entry;

        auto sessionsTree { model.getChildWithName (IDtype::sessions) };
        sessionsTree.appendChild (session->state, nullptr);

        auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
            IDtype::sessions, ID::focusedSession) };
        jassert (focusedSessionParameter != nullptr);

        if (focusedSessionParameter->getValue() == 0)
            focusedSessionParameter->setValue (sessionUuid.value);

        return *session;
    }

    /** @brief Returns the Session for the given identifier.
     *  @param sessionUuid  Session identifier.
     *  @return Reference to the Session.
     *  @note MESSAGE THREAD.
     */
    Session& getSession (jam::UUID sessionUuid) { return *sessions.at (sessionUuid.value); }

    /** @brief Detaches the Session's state from SESSIONS bare (verb-direct)
     *  and destroys the Session at @p sessionUuid.
     *  @param sessionUuid  Session identifier.
     *  @note MESSAGE THREAD.
     */
    void removeSession (jam::UUID sessionUuid)
    {
        auto sessionsTree { model.getChildWithName (IDtype::sessions) };
        sessionsTree.removeChild (sessions.at (sessionUuid.value)->state, nullptr);
        sessions.erase (sessionUuid.value);
    }

    /** @brief Resolves @c ID::focusedSession to its Session — the surface
     *  every GUI action (newTab/closeTab/split/closePane/zoom/focus,
     *  ActionRegistration.cpp) and GUI projection (Tabs/Panes/TerminalView)
     *  route through.
     *  @pre At least one Session must already exist (ENDApplication's own
     *       bootstrap createSession() call, Main.cpp's initialise()) —
     *       @c ID::focusedSession is still 0 otherwise.
     *  @return Reference to the active Session.
     *  @note MESSAGE THREAD.
     */
    Session& getActiveSession()
    {
        auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
            IDtype::sessions, ID::focusedSession) };
        jassert (focusedSessionParameter != nullptr);

        // Threat: called before ENDApplication's bootstrap createSession() —
        // ID::focusedSession is still its 0 (no-session) seed value.
        jassert (focusedSessionParameter->getValue() != 0);

        return getSession (jam::UUID (focusedSessionParameter->getValue()));
    }

private:
    /** @brief App SSOT — declared first so it is fully constructed (and
     *  self-registered as jam::Instance<ENDModel>) before any Session is
     *  created below. Moved here from ENDApplication (Main.h) — Instance
     *  access for consumers is unchanged. */
    ENDModel model;

    /** @brief Every Session this host owns, keyed by uuid.value — O(1)
     *  get/create/remove. jam::Owner<Session> was considered but
     *  offers no keyed lookup (its own O(1) path requires a
     *  content-hash/operator== on the owned type, not an external key), so
     *  a HashMap is the honest choice here, mirroring the map this class
     *  held before Step 4 (now one level up: sessions instead of
     *  processors). */
    jam::HashMap<int64_t, std::unique_ptr<Session>> sessions;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
