#include "action/ActionRegistry.h"

ActionRegistry::ActionRegistry()
{
    config.addListener (this);
    buildKeyMap();
}

ActionRegistry::~ActionRegistry() { config.removeListener (this); }

void ActionRegistry::buildKeyMap()
{
    keys.clear();
    modalKeys.clear();

    auto keysSection { config.getChildWithName (IDtype::keys) };

    prefixKey =
        juce::KeyPress::createFromDescription (keysSection.getProperty (ID::prefix).toString());
    prefixTimeout = keysSection.getProperty (ID::prefixTimeout);

    jam::Model::forEachProperty (
        keysSection,
        [this] (const juce::Identifier& propName, const juce::var& value)
        {
            if (propName != ID::prefix and propName != ID::prefixTimeout)
            {
                auto key { juce::KeyPress::createFromDescription (value.toString()) };

                if (key.getModifiers().isCommandDown() or key.getModifiers().isCtrlDown())
                    keys.emplace (key, propName);
                else
                    modalKeys.emplace (key, propName);
            }
        });
}

bool ActionRegistry::keyPressed (const juce::KeyPress& key)
{
    if (jam::Map::contains (keys, key))
    {
        run (keys.at (key));
        return true;
    }

    if (key == prefixKey and not isTimerRunning())
    {
        startTimer (prefixTimeout);
        return true;
    }

    if (isTimerRunning())
    {
        stopTimer();

        if (jam::Map::contains (modalKeys, key))
        {
            run (modalKeys.at (key));
            return true;
        }
    }

    return false;
}

void ActionRegistry::run (const juce::Identifier& action)
{
    if (actions.contains (action))
        actions.get (action);
}

void ActionRegistry::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    buildKeyMap();
}

void ActionRegistry::timerCallback() { stopTimer(); }
