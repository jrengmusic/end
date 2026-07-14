/**
 * @file action/ENDActions.h
 * @brief Action registry — prefix key state machine + key-to-action binding.
 */
#pragma once
#include <JuceHeader.h>
#include "config/ConfigModel.h"

namespace std
{
/*____________________________________________________________________________*/
template<>
struct hash<juce::KeyPress>
{
    size_t operator() (const juce::KeyPress& k) const noexcept
    {
        return static_cast<size_t> (k.getKeyCode())
               + (static_cast<size_t> (k.getModifiers().getRawFlags()) << 16);
    }
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace std

//==============================================================================
/** @class ENDActions
 *  @brief Maps key presses to actions via direct and modal (prefix) bindings.
 *
 *  Direct bindings (e.g. cmd+t) fire immediately. Modal bindings require
 *  the prefix key first, then the action key within a timeout window.
 *  Actions are registered as void() callables keyed by juce::Identifier.
 */
class ENDActions
    : public jam::Instance<ENDActions>
    , private juce::Timer
    , public juce::ValueTree::Listener
{
public:
    ENDActions()
    {
        config.addListener (this);
        buildKeyMap();
    }

    ~ENDActions() { config.removeListener (this); }

    /** @brief Rebuilds key-to-action maps from the config KEYS section. */
    void buildKeyMap()
    {
        keys.clear();
        modalKeys.clear();

        auto keysSection { config.getChildWithName (Id::toType (Id::keys)) };

        prefixKey = juce::KeyPress::createFromDescription (
            keysSection.getProperty (Id::prefix).toString());
        prefixTimeout = keysSection.getProperty (Id::prefixTimeout);

        jam::Model::forEachProperty (
            keysSection,
            [this] (const juce::Identifier& propName, const juce::var& value)
            {
                if (propName != Id::prefix and propName != Id::prefixTimeout)
                {
                    auto key { juce::KeyPress::createFromDescription (value.toString()) };

                    if (key.getModifiers().isCommandDown() or key.getModifiers().isCtrlDown())
                        keys.emplace (key, propName);
                    else
                        modalKeys.emplace (key, propName);
                }
            });
    }

    /** @brief Action map — callers register void() callables keyed by Identifier. */
    jam::Function::Map<juce::Identifier, void> actions;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
    {
        buildKeyMap();
    }

    /** @brief Processes a key press — returns true if consumed. */
    bool keyPressed (const juce::KeyPress& key)
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

    /** @brief Runs the action identified by the given key. */
    void run (const juce::Identifier& action)
    {
        if (actions.contains (action))
            actions.get (action);
    }

    //==============================================================================
private:
    ConfigModel& config { *ConfigModel::getInstance() };

    void timerCallback() override { stopTimer(); }

    jam::HashMap<juce::KeyPress, juce::Identifier> keys;
    jam::HashMap<juce::KeyPress, juce::Identifier> modalKeys;

    juce::KeyPress prefixKey;
    int prefixTimeout { 1000 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDActions)
};
