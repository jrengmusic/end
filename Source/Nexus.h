#pragma once
#include <JuceHeader.h>
#include "end/Session.h"

struct Nexus : jam::Instance<Nexus>
{
    Nexus()
    {
        model.getOrCreateChildWithName (IDtype::window);

        auto sessionsTree { model.getOrCreateChildWithName (IDtype::sessions) };
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, ID::focusedSession, int64_t { 0 });
        model.createAndAddParameter<jam::Parameter<int64_t>> (
            sessionsTree, ID::focusedPane, int64_t { 0 });

        model.getOrCreateChildWithName (IDtype::overlay);
    }

    Session& createSession()
    {
        jam::UUID sessionUuid;
        auto [entry, inserted] = sessions.try_emplace (
            sessionUuid, std::make_unique<Session> (sessionUuid, model));
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

    Session& getSession (jam::UUID sessionUuid) { return *sessions.at (sessionUuid); }

    void removeSession (jam::UUID sessionUuid)
    {
        auto sessionsTree { model.getChildWithName (IDtype::sessions) };
        sessionsTree.removeChild (sessions.at (sessionUuid)->state, nullptr);
        sessions.erase (sessionUuid);
    }

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
    ENDModel model;

    jam::HashMap<jam::UUID, std::unique_ptr<Session>> sessions;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};
