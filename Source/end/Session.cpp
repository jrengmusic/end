#include "end/Session.h"

Session::Session (jam::UUID newUuid, ENDModel& newModel)
    : uuid (newUuid)
    , model (newModel)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, jam::ID::id, uuid.value);

    model.addListener (this);
}

Session::~Session() { model.removeListener (this); }

TerminalProcessor& Session::get (jam::UUID uuid) { return *processors.at (uuid); }

void Session::newTerminal (jam::UUID uuid)
{
    const auto [entry, inserted] {
        processors.try_emplace (uuid, std::make_unique<TerminalProcessor> (uuid))
    };
    juce::ignoreUnused (entry);
    jassert (inserted);
}

void Session::removeTerminal (jam::UUID uuid)
{
    processors.erase (uuid);
}

void Session::parameterChanged (const juce::Identifier& id, const juce::var& newValue)
{
    if (events.contains (id))
        events.get (id, newValue);
}
