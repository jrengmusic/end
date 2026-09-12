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

    /** @brief Registers a callable under actionId, forwarding to the action map.
     *  @param actionId    Identifier the callable is registered under.
     *  @param newFunction Callable to register.
     */
    template <typename... Args, typename FunctionType>
    void add (const juce::Identifier& actionId, FunctionType&& newFunction)
    {
        actions.add<Args...> (actionId, std::forward<FunctionType> (newFunction));
    }

    /** @brief Invokes the callable registered under actionId, forwarding args to it.
     *  @param actionId Identifier of a previously registered callable.
     *  @param args     Arguments forwarded to the callable.
     */
    template <typename... Args>
    void get (const juce::Identifier& actionId, Args&&... args)
    {
        actions.get (actionId, std::forward<Args> (args)...);
    }

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

    void valueTreePropertyChanged (juce::ValueTree& changedTree, const juce::Identifier&) override
    {
        if (changedTree.hasType (Id::toType (Id::keys)))
            buildKeyMap();
    }

    /** @brief Processes a key press against direct and modal bindings.
     *  @param key Key press to match against the current key maps.
     *  @return True when the key press is consumed — a direct binding ran,
     *          the prefix key armed the modal window, or a modal binding
     *          ran within the timeout.
     */
    bool keyPressed (const juce::KeyPress& key)
    {
        if (jam::Map::contains (keys, key))
            return run (keys.at (key));

        if (key == prefixKey and not isTimerRunning())
        {
            startTimer (prefixTimeout);
            return true;
        }

        if (isTimerRunning())
        {
            stopTimer();

            if (jam::Map::contains (modalKeys, key))
                return run (modalKeys.at (key));
        }

        return false;
    }

    /** @brief Runs the action registered under the given identifier.
     *  @param action Identifier of a previously registered action.
     *  @return True when a matching action was found and run, false otherwise.
     */
    bool run (const juce::Identifier& action)
    {
        if (actions.contains (action))
        {
            actions.get (action);
            return true;
        }

        return false;
    }

    //==============================================================================
private:
    ConfigModel& config { *ConfigModel::getInstance() };

    void timerCallback() override { stopTimer(); }

    /** @brief Action map — callers register void() callables keyed by Identifier. */
    jam::Function::Map<juce::Identifier, void> actions;

    jam::HashMap<juce::KeyPress, juce::Identifier> keys;
    jam::HashMap<juce::KeyPress, juce::Identifier> modalKeys;

    juce::KeyPress prefixKey;
    int prefixTimeout { 0 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDActions)
};
