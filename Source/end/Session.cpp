#include "end/Session.h"

Session::Session (jam::UUID newUuid, ENDModel& newModel)
    : uuid (newUuid)
    , model (newModel)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, jam::ID::id, uuid.value);

    juce::ValueTree tabsState { IDtype::tabs };

    // Per-instance identity (jam::Model::getGroupId) — stamped BEFORE any
    // createAndAddParameter() call on this tree, so every Session's own
    // TABS node (same IDtype::tabs type, one per Session) groups under its
    // own uuid instead of colliding on the bare type.
    tabsState.setProperty (jam::ID::id, uuid.value, nullptr);

    model.createAndAddParameter<jam::Parameter<int64_t>> (tabsState, ID::focusedTab, int64_t { 0 });
    state.appendChild (tabsState, nullptr);

    model.addListener (this);
}

Session::~Session() { model.removeListener (this); }

TerminalProcessor& Session::get (jam::UUID uuid) { return *processors.at (uuid.value); }

void Session::newTerminal (jam::UUID uuid)
{
    const auto [entry, inserted] {
        processors.try_emplace (uuid.value, std::make_unique<TerminalProcessor> (uuid))
    };
    juce::ignoreUnused (entry);
    jassert (inserted);
}

void Session::removeTerminal (jam::UUID uuid)
{
    processors.erase (uuid.value);
}

void Session::parameterChanged (const juce::Identifier& id, const juce::var& newValue)
{
    if (events.contains (id))
        events.get (id, newValue);
}
