#include "action/Registry.h"

namespace action
{
/*____________________________________________________________________________*/

Registry::Registry()
{
    config.addListener (this);
    buildKeyMap();
}

Registry::~Registry() { config.removeListener (this); }

void Registry::buildKeyMap()
{
    keys.clear();
    modalKeys.clear();

    auto keysSection { config.getChildWithName (IDtype::keys) };

    prefixKey = juce::KeyPress::createFromDescription (keysSection.getProperty (ID::prefix).toString());
    prefixTimeout = keysSection.getProperty (ID::prefixTimeout);

    for (int i { 0 }; i < keysSection.getNumProperties(); ++i)
    {
        auto propName { keysSection.getPropertyName (i) };

        if (propName != ID::prefix and propName != ID::prefixTimeout)
        {
            jam::debug::Log::write ("  bind: " + propName.toString() + " -> " + keysSection.getProperty (propName).toString());

            auto key { juce::KeyPress::createFromDescription (keysSection.getProperty (propName).toString()) };

            if (key.getModifiers().isCommandDown() or key.getModifiers().isCtrlDown())
                keys.emplace (key, propName);
            else
                modalKeys.emplace (key, propName);
        }
    }

    jam::debug::Log::write ("buildKeyMap: keys=" + juce::String (static_cast<int> (keys.size()))
                            + " modalKeys=" + juce::String (static_cast<int> (modalKeys.size())));
}

bool Registry::keyPressed (const juce::KeyPress& key)
{
    jam::debug::Log::write ("keyPressed: " + key.getTextDescription());

    if (jam::Map::contains (keys, key))
    {
        jam::debug::Log::write ("  direct match: " + keys.at (key).toString());
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

void Registry::run (const juce::Identifier& action)
{
    jam::debug::Log::write ("run: " + action.toString() + " found=" + juce::String (static_cast<int> (actions.contains (action))));

    if (actions.contains (action))
        actions.get (action);
}

void Registry::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    buildKeyMap();
}

void Registry::timerCallback() { stopTimer(); }

/**______________________________END OF NAMESPACE______________________________*/
}// namespace action
