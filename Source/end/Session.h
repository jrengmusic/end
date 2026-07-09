#pragma once
#include <JuceHeader.h>
#include "end/ENDModel.h"
#include "terminal/TerminalProcessor.h"
#include "Identifier.h"

class Session : public jam::Model::Listener
{
public:
    Session (jam::UUID newUuid, ENDModel& newModel);

    ~Session();

    TerminalProcessor& get (jam::UUID uuid);

    void newTerminal (jam::UUID uuid);

    void removeTerminal (jam::UUID uuid);

    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

    juce::ValueTree state { jam::IDtype::session };

private:
    jam::UUID uuid;
    ENDModel& model;

    jam::HashMap<jam::UUID, std::unique_ptr<TerminalProcessor>> processors;

    jam::Function::Map<juce::Identifier, void> events;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
