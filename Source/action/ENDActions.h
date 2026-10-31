/**
 * @file action/ENDActions.h
 * @brief Action registry — callables keyed by action name.
 */
#pragma once
#include <JuceHeader.h>

//==============================================================================
/** @class ENDActions
 *  @brief Registry of void() callables keyed by juce::Identifier.
 */
class ENDActions
    : public jam::Instance<ENDActions>
{
public:
    ENDActions() = default;

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
    /** @brief Action map — callers register void() callables keyed by Identifier. */
    jam::Function::Map<juce::Identifier, void> actions;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDActions)
};
