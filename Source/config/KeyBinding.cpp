#include "KeyBinding.h"

static const std::array<juce::String, 11> actionIDs
{
    Config::Key::keysCopy,
    Config::Key::keysPaste,
    Config::Key::keysQuit,
    Config::Key::keysCloseTab,
    Config::Key::keysReload,
    Config::Key::keysZoomIn,
    Config::Key::keysZoomOut,
    Config::Key::keysZoomReset,
    Config::Key::keysNewTab,
    Config::Key::keysPrevTab,
    Config::Key::keysNextTab
};

KeyBinding::KeyBinding (juce::ApplicationCommandManager& commandManager)
    : acm (commandManager)
{
    loadFromConfig();
}

void KeyBinding::reload()
{
    bindings.clear();
    loadFromConfig();
}

juce::KeyPress KeyBinding::getBinding (CommandID cmd) const
{
    const auto& actionID { actionIDForCommand (cmd) };
    auto it { bindings.find (actionID) };

    juce::KeyPress result {};

    if (it != bindings.end())
        result = it->second;

    return result;
}

void KeyBinding::loadFromConfig()
{
    auto* cfg { Config::getContext() };

    static const juce::String keyIDs[]
    {
        Config::Key::keysCopy,
        Config::Key::keysPaste,
        Config::Key::keysQuit,
        Config::Key::keysCloseTab,
        Config::Key::keysReload,
        Config::Key::keysZoomIn,
        Config::Key::keysZoomOut,
        Config::Key::keysZoomReset,
        Config::Key::keysNewTab,
        Config::Key::keysPrevTab,
        Config::Key::keysNextTab
    };

    for (const auto& actionID : keyIDs)
    {
        const juce::String shortcutStr { cfg->getString (actionID) };
        const juce::KeyPress kp { parse (shortcutStr) };

        if (kp.isValid())
            bindings.insert_or_assign (actionID, kp);
    }
}

void KeyBinding::applyMappings()
{
    for (int cmd { static_cast<int> (CommandID::copy) }; cmd <= static_cast<int> (CommandID::nextTab); ++cmd)
    {
        const auto actionID { actionIDForCommand (static_cast<CommandID> (cmd)) };
        auto it { bindings.find (actionID) };
        const juce::KeyPress binding { (it != bindings.end()) ? it->second : juce::KeyPress{} };

        acm.getKeyMappings()->clearAllKeyPresses (static_cast<juce::CommandID> (cmd));

        if (binding.isValid())
            acm.getKeyMappings()->addKeyPress (static_cast<juce::CommandID> (cmd), binding);
    }
}

juce::KeyPress KeyBinding::parse (const juce::String& shortcutString)
{
    juce::StringArray tokens;
    tokens.addTokens (shortcutString.toLowerCase().trim(), "+", "");
    tokens.trim();
    tokens.removeEmptyStrings();

    int modifiers { 0 };
    int keyCode { 0 };

    for (int i { 0 }; i < tokens.size(); ++i)
    {
        const auto& token { tokens[i] }; // juce::StringArray has no .at(); operator[] is bounds-safe in debug

        if (token == "cmd" or token == "ctrl")
        {
#if JUCE_MAC
            modifiers |= juce::ModifierKeys::commandModifier;
#else
            modifiers |= juce::ModifierKeys::ctrlModifier;
#endif
        }
        else if (token == "shift")
        {
            modifiers |= juce::ModifierKeys::shiftModifier;
        }
        else if (token == "alt" or token == "opt")
        {
            modifiers |= juce::ModifierKeys::altModifier;
        }
        else
        {
            if (token == "pageup")        keyCode = juce::KeyPress::pageUpKey;
            else if (token == "pagedown") keyCode = juce::KeyPress::pageDownKey;
            else if (token == "home")     keyCode = juce::KeyPress::homeKey;
            else if (token == "end")      keyCode = juce::KeyPress::endKey;
            else if (token == "delete")   keyCode = juce::KeyPress::deleteKey;
            else if (token == "insert")   keyCode = juce::KeyPress::insertKey;
            else if (token == "escape")   keyCode = juce::KeyPress::escapeKey;
            else if (token == "return")   keyCode = juce::KeyPress::returnKey;
            else if (token == "tab")      keyCode = juce::KeyPress::tabKey;
            else if (token == "space")    keyCode = juce::KeyPress::spaceKey;
            else if (token == "backspace") keyCode = juce::KeyPress::backspaceKey;
            else if (token.length() >= 2 and token[0] == 'f')
            {
                const int fNum { token.substring (1).getIntValue() };

                if (fNum >= 1 and fNum <= 12)
                    keyCode = juce::KeyPress::F1Key + (fNum - 1);
            }
        else if (token.length() == 1)
        {
            const char ch { static_cast<char> (token[0]) };

            if (ch >= 'a' and ch <= 'z')
                keyCode = static_cast<int> (ch - 'a' + 'A');
            else
                keyCode = static_cast<int> (ch);
        }
        }
    }

    return juce::KeyPress (keyCode, juce::ModifierKeys (modifiers), 0);
}

const juce::String& KeyBinding::actionIDForCommand (CommandID cmd)
{
    const int idx { static_cast<int> (cmd) - 1 };
    const int safeIdx { (idx >= 0 and idx < 11) ? idx : 0 };

    return actionIDs.at (safeIdx);
}

KeyBinding::CommandID KeyBinding::commandForActionID (const juce::String& actionID)
{
    for (int i { 0 }; i < 11; ++i)
    {
        if (actionIDs.at (i) == actionID)
            return static_cast<CommandID> (i + 1);
    }

    return CommandID::copy;
}
