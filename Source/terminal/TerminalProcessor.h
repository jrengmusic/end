#pragma once
#include <JuceHeader.h>
#include "terminal/TerminalModel.h"

struct TerminalProcessor : public jam::Model::Listener
{
    /** @brief Constructs the paired TerminalModel with @p uuid — TerminalModel's
     *  own ctor stamps jam::ID::id onto its state BEFORE registering any
     *  parameter (per-instance identity, jam::Model::getGroupId), so the id
     *  must reach it through the member initializer, never set afterward. */
    TerminalProcessor (jam::UUID uuid);

    ~TerminalProcessor() override;

    void setFocus (bool focused);

    void parameterChanged (const juce::Identifier& id, const juce::var& newValue) override;

    TerminalModel model;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TerminalProcessor)
};
