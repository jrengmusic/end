#include "Action.h"

namespace Action
{

//==============================================================================
Registry::Registry() = default;

Registry::~Registry()
{
    stopTimer();
}

//==============================================================================
void Registry::registerAction (const juce::String& id,
                              const juce::String& name,
                              const juce::String& description,
                              const juce::String& category,
                              bool isModal,
                              std::function<bool()> callback)
{
    // Debug guard: duplicate IDs are a programming error.
    for (const auto& existing : entries)
        jassert (existing.id != id);

    Entry entry;
    entry.id          = id;
    entry.name        = name;
    entry.description = description;
    entry.category    = category;
    entry.isModal     = isModal;
    entry.execute     = std::move (callback);

    entries.push_back (std::move (entry));
}

//==============================================================================
bool Registry::handleKeyPress (const juce::KeyPress& key)
{
    bool consumed { false };

    if (prefixState == PrefixState::waiting)
    {
        stopTimer();
        prefixState = PrefixState::idle;

        // Modal bindings match by text character, not full KeyPress.
        // This handles shifted characters correctly (e.g. '?' = Shift+/).
        const juce::juce_wchar textChar { key.getTextCharacter() };
        int matchIndex { -1 };

        if (textChar != 0)
        {
            for (const auto& [boundKey, index] : modalBindings)
            {
                if (boundKey.getKeyCode() == static_cast<int> (textChar)
                    or boundKey.getTextCharacter() == textChar)
                {
                    matchIndex = index;
                    break;
                }
            }
        }

        // Fall back to exact KeyPress match for non-character modal keys.
        if (matchIndex == -1)
        {
            const auto it { modalBindings.find (key) };

            if (it != modalBindings.end())
                matchIndex = it->second;
        }

        if (matchIndex >= 0)
        {
            const auto& entry { entries.at (static_cast<std::size_t> (matchIndex)) };

            if (entry.execute != nullptr)
                consumed = entry.execute();
            else
                consumed = true;
        }
    }
    else
    {
        // PrefixState::idle
        if (prefixKey.isValid() and key == prefixKey)
        {
            prefixState = PrefixState::waiting;
            startTimer (prefixTimeoutMs);
            consumed = true;
        }
        else
        {
            const auto it { globalBindings.find (key) };

            if (it != globalBindings.end())
            {
                const auto& entry { entries.at (static_cast<std::size_t> (it->second)) };

                if (entry.execute != nullptr)
                    consumed = entry.execute();
                else
                    consumed = true;
            }
        }
    }

    return consumed;
}

//==============================================================================
void Registry::clear()
{
    stopTimer();
    prefixState = PrefixState::idle;
    entries.clear();
    globalBindings.clear();
    modalBindings.clear();
}

//==============================================================================
const std::vector<Registry::Entry>& Registry::getEntries() const noexcept
{
    return entries;
}

//==============================================================================
void Registry::buildKeyMap (const juce::String& prefixShortcut,
                            int prefixTimeout,
                            const std::vector<Binding>& bindings)
{
    globalBindings.clear();
    modalBindings.clear();

    prefixKey       = parseShortcut (prefixShortcut);
    prefixTimeoutMs = prefixTimeout;

    // Build a lookup from action ID -> index into entries.
    std::unordered_map<juce::String, int> idToIndex;
    idToIndex.reserve (entries.size());

    for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
        idToIndex.insert_or_assign (entries.at (static_cast<std::size_t> (i)).id, i);

    // Walk the provided bindings and populate the appropriate binding map.
    for (const auto& binding : bindings)
    {
        const juce::KeyPress kp { parseShortcut (binding.shortcutString) };

        if (kp.isValid())
        {
            const auto idxIt { idToIndex.find (binding.actionId) };

            if (idxIt != idToIndex.end())
            {
                if (binding.isModal)
                    modalBindings.insert_or_assign (kp, idxIt->second);
                else
                    globalBindings.insert_or_assign (kp, idxIt->second);

                entries.at (static_cast<std::size_t> (idxIt->second)).shortcut = kp;
            }
        }
    }
}

//==============================================================================
juce::KeyPress Registry::parseShortcut (const juce::String& shortcutString)
{
    juce::StringArray tokens;
    tokens.addTokens (shortcutString.toLowerCase().trim(), "+", "");
    tokens.trim();
    tokens.removeEmptyStrings();

    int modifiers { 0 };
    int keyCode   { 0 };

    for (int i { 0 }; i < tokens.size(); ++i)
    {
        // juce::StringArray::operator[] is bounds-safe in debug builds.
        const auto& token { tokens[i] };

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
            static const std::unordered_map<juce::String, int> keyNameTable
            {
                { "pageup",    juce::KeyPress::pageUpKey    },
                { "pagedown",  juce::KeyPress::pageDownKey  },
                { "home",      juce::KeyPress::homeKey      },
                { "end",       juce::KeyPress::endKey       },
                { "delete",    juce::KeyPress::deleteKey    },
                { "insert",    juce::KeyPress::insertKey    },
                { "escape",    juce::KeyPress::escapeKey    },
                { "return",    juce::KeyPress::returnKey    },
                { "tab",       juce::KeyPress::tabKey       },
                { "space",     juce::KeyPress::spaceKey     },
                { "backspace", juce::KeyPress::backspaceKey },
            };

            const auto it { keyNameTable.find (token) };

            if (it != keyNameTable.end())
            {
                keyCode = it->second;
            }
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

//==============================================================================
juce::String Registry::shortcutToString (const juce::KeyPress& key)
{
    juce::String result;

    const auto mods { key.getModifiers() };

#if JUCE_MAC
    if (mods.isCommandDown())
        result += "cmd+";

    if (mods.isCtrlDown())
        result += "ctrl+";
#else
    if (mods.isCtrlDown())
        result += "ctrl+";
#endif

    if (mods.isShiftDown())
        result += "shift+";

    if (mods.isAltDown())
        result += "alt+";

    static const std::unordered_map<int, juce::String> keyCodeTable
    {
        { juce::KeyPress::pageUpKey,    "pageup"    },
        { juce::KeyPress::pageDownKey,  "pagedown"  },
        { juce::KeyPress::homeKey,      "home"      },
        { juce::KeyPress::endKey,       "end"       },
        { juce::KeyPress::deleteKey,    "delete"    },
        { juce::KeyPress::insertKey,    "insert"    },
        { juce::KeyPress::escapeKey,    "escape"    },
        { juce::KeyPress::returnKey,    "return"    },
        { juce::KeyPress::tabKey,       "tab"       },
        { juce::KeyPress::spaceKey,     "space"     },
        { juce::KeyPress::backspaceKey, "backspace" },
    };

    const int code { key.getKeyCode() };
    const auto it { keyCodeTable.find (code) };

    if (it != keyCodeTable.end())
    {
        result += it->second;
    }
    else if (code >= juce::KeyPress::F1Key and code <= juce::KeyPress::F12Key)
    {
        result += "f" + juce::String (code - juce::KeyPress::F1Key + 1);
    }
    else if (code >= 'A' and code <= 'Z')
    {
        result += juce::String::charToString (static_cast<juce::juce_wchar> (code - 'A' + 'a'));
    }
    else if (code > 0)
    {
        result += juce::String::charToString (static_cast<juce::juce_wchar> (code));
    }

    return result;
}

//==============================================================================
void Registry::timerCallback()
{
    stopTimer();
    prefixState = PrefixState::idle;
}

} // namespace Action
