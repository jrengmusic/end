#include "terminal/TerminalProcessor.h"

TerminalProcessor::TerminalProcessor (jam::UUID uuid)
    : model (uuid)
{
    model.addListener (this);
}

TerminalProcessor::~TerminalProcessor()
{
    model.removeListener (this);
}

void TerminalProcessor::setFocus (bool focused)
{
    juce::ignoreUnused (focused);
}

void TerminalProcessor::parameterChanged (const juce::Identifier& id, const juce::var& newValue)
{
    juce::ignoreUnused (id, newValue);
}
