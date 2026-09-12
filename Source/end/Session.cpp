#include "end/Session.h"

Session::Session (jam::UUID newUuid, ENDModel& newModel)
    : model (newModel)
{
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, Id::id, newUuid.value);
}

Session::~Session() = default;

juce::AudioPluginInstance& Session::getPlugin (jam::UUID uuid) { return *plugins.at (uuid); }

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
