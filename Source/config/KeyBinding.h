#pragma once

#include <JuceHeader.h>
#include "Config.h"

namespace std
{
template<>
struct hash<juce::KeyPress>
{
    size_t operator() (const juce::KeyPress& k) const noexcept
    {
        auto h1 { std::hash<int>{} (k.getKeyCode()) };
        auto h2 { std::hash<int>{} (k.getModifiers().getRawFlags()) };
        return h1 ^ (h2 << 16);
    }
};
}

struct KeyBinding
{
    enum class CommandID : int
    {
        copy = 1,
        paste,
        quit,
        closeTab,
        reload,
        zoomIn,
        zoomOut,
        zoomReset,
        newTab,
        prevTab,
        nextTab
    };

    explicit KeyBinding (juce::ApplicationCommandManager& commandManager);

    void reload();

    juce::KeyPress getBinding (CommandID cmd) const;

    static juce::KeyPress parse (const juce::String& shortcutString);

    static bool isCommandKeyPress (const juce::KeyPress& key);

    static const juce::String& actionIDForCommand (CommandID cmd);

    static CommandID commandForActionID (const juce::String& actionID);

    void applyMappings();

private:
    juce::ApplicationCommandManager& acm;
    std::unordered_map<juce::String, juce::KeyPress> bindings;

    void loadFromConfig();
};
