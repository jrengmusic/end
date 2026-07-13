#pragma once
#include <JuceHeader.h>
#include "end/ENDModel.h"
#include "Identifier.h"

class Session : public jam::Model::Listener
{
public:
    Session (jam::UUID newUuid, ENDModel& newModel);

    ~Session();

    juce::AudioPluginInstance& get (jam::UUID uuid);

    void newPlugin (jam::UUID uuid, const juce::String& pluginId, std::unique_ptr<juce::AudioPluginInstance> instance);

    void removePlugin (jam::UUID uuid);

    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

    juce::ValueTree state { jam::IDtype::session };

private:
    jam::UUID uuid;
    ENDModel& model;

    jam::HashMap<jam::UUID, std::unique_ptr<juce::AudioPluginInstance>> plugins;

    jam::Function::Map<juce::Identifier, void> events;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
