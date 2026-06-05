/**
 * @file action/Registry.h
 * @brief Action registry — prefix key state machine + key-to-action binding.
 */
#pragma once
#include <JuceHeader.h>
#include "config/Config.h"

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
namespace action
{
/*____________________________________________________________________________*/

/** @class Registry
 *  @brief Maps key presses to actions via direct and modal (prefix) bindings.
 *
 *  Direct bindings (e.g. cmd+t) fire immediately. Modal bindings require
 *  the prefix key first, then the action key within a timeout window.
 *  Actions are registered as void() callables keyed by juce::Identifier.
 */
class Registry
    : private juce::Timer
    , public juce::ValueTree::Listener
{
public:
    Registry();
    ~Registry();

    /** @brief Rebuilds key-to-action maps from the config KEYS section. */
    void buildKeyMap();

    /** @brief Action map — callers register void() callables keyed by Identifier. */
    jam::Function::Map<juce::Identifier, void> actions;

    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Processes a key press — returns true if consumed. */
    bool keyPressed (const juce::KeyPress& key);

    /** @brief Runs the action identified by the given key. */
    void run (const juce::Identifier& action);

    //==============================================================================
private:
    void timerCallback() override;

    std::unordered_map<juce::KeyPress, juce::Identifier> keys;
    std::unordered_map<juce::KeyPress, juce::Identifier> modalKeys;

    juce::ValueTree config { config::Model::get() };
    juce::KeyPress prefixKey;
    int prefixTimeout { 1000 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Registry)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace action
