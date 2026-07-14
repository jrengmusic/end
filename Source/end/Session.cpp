#include "end/Session.h"

Session::Session (jam::UUID newUuid, ENDModel& newModel)
    : uuid (newUuid)
    , model (newModel)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, Id::id, uuid.value);

    model.addListener (this);
}

Session::~Session() { model.removeListener (this); }

juce::AudioPluginInstance& Session::get (jam::UUID uuid) { return *plugins.at (uuid); }

bool Session::contains (jam::UUID uuid) const { return plugins.contains (uuid); }

void Session::newPlugin (jam::UUID uuid, const juce::String& pluginId, std::unique_ptr<juce::AudioPluginInstance> instance)
{
    auto paneRow { jam::Model::getChildWithID (state, juce::var (uuid.value)) };
    jassert (paneRow.isValid());

    if (instance != nullptr)
    {
        const auto [entry, inserted] { plugins.try_emplace (uuid, std::move (instance)) };
        jassert (inserted);

        paneRow.setProperty (Id::name, entry->second->getName(), nullptr);
    }

    paneRow.setProperty (Id::pluginId, pluginId, nullptr);
}

void Session::removePlugin (jam::UUID uuid)
{
    plugins.erase (uuid);
}

void Session::parameterChanged (const juce::Identifier& id, const juce::var& newValue)
{
    if (events.contains (id))
        events.get (id, newValue);
}
