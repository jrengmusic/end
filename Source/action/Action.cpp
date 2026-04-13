#include "Action.h"

// ---------------------------------------------------------------------------
// Config key → action ID mapping table
// Each row maps a Config::Key constant to the action ID string it configures.
// Used by buildKeyMap() to resolve shortcuts without hard-coding key strings.
// ---------------------------------------------------------------------------
namespace
{

struct ActionKeyEntry
{
    const juce::String& configKey;
    const char* actionId;
    bool isModal;
};

// clang-format off
static const ActionKeyEntry actionKeyTable[]
{
    { Config::Key::keysCopy,             "copy",             false },
    { Config::Key::keysPaste,            "paste",            false },
    { Config::Key::keysQuit,             "quit",             false },
    { Config::Key::keysCloseTab,         "close_tab",        false },
    { Config::Key::keysReload,           "reload_config",    false },
    { Config::Key::keysZoomIn,           "zoom_in",          false },
    { Config::Key::keysZoomOut,          "zoom_out",         false },
    { Config::Key::keysZoomReset,        "zoom_reset",       false },
    { Config::Key::keysNewWindow,        "new_window",       false },
    { Config::Key::keysNewTab,           "new_tab",          false },
    { Config::Key::keysPrevTab,          "prev_tab",         false },
    { Config::Key::keysNextTab,          "next_tab",         false },
    { Config::Key::keysSplitHorizontal,  "split_horizontal", true  },
    { Config::Key::keysSplitVertical,    "split_vertical",   true  },
    { Config::Key::keysPaneLeft,         "pane_left",        true  },
    { Config::Key::keysPaneDown,         "pane_down",        true  },
    { Config::Key::keysPaneUp,           "pane_up",          true  },
    { Config::Key::keysPaneRight,        "pane_right",       true  },
    { Config::Key::keysNewline,          "newline",          false },
    { Config::Key::keysActionList,       "action_list",      true  },
    { Config::Key::keysEnterSelection,   "enter_selection",  true  },
    { Config::Key::keysEnterOpenFile,    "enter_open_file",  true  },
};
// clang-format on

} // anonymous namespace

namespace Action
{

//==============================================================================
juce::String Registry::configKeyForAction (const juce::String& actionId)
{
    juce::String result;

    for (const auto& row : actionKeyTable)
    {
        if (juce::String (row.actionId) == actionId and result.isEmpty())
            result = row.configKey;
    }

    return result;
}

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
void Registry::reload()
{
    stopTimer();
    prefixState = PrefixState::idle;
    buildKeyMap();
}

//==============================================================================
const std::vector<Registry::Entry>& Registry::getEntries() const noexcept
{
    return entries;
}

//==============================================================================
void Registry::buildKeyMap()
{
    globalBindings.clear();
    modalBindings.clear();

    auto* cfg { Config::getContext() };
    jassert (cfg != nullptr);

    // Resolve prefix key and timeout.
    prefixKey      = parseShortcut (cfg->getString (Config::Key::keysPrefix));
    prefixTimeoutMs = cfg->getInt (Config::Key::keysPrefixTimeout);

    // Build a lookup from action ID → index into entries for fast resolution.
    std::unordered_map<juce::String, int> idToIndex;
    idToIndex.reserve (entries.size());

    for (int i { 0 }; i < static_cast<int> (entries.size()); ++i)
        idToIndex.insert_or_assign (entries.at (static_cast<std::size_t> (i)).id, i);

    // Walk the config key table and populate the appropriate binding map.
    for (const auto& row : actionKeyTable)
    {
        const juce::String shortcutStr { cfg->getString (row.configKey) };
        const juce::KeyPress kp { parseShortcut (shortcutStr) };

        if (kp.isValid())
        {
            const auto idxIt { idToIndex.find (juce::String (row.actionId)) };

            if (idxIt != idToIndex.end())
            {
                if (row.isModal)
                    modalBindings.insert_or_assign (kp, idxIt->second);
                else
                    globalBindings.insert_or_assign (kp, idxIt->second);

                entries.at (static_cast<std::size_t> (idxIt->second)).shortcut = kp;
            }
        }
    }

    // Walk popup entries and populate modal/global bindings.
    for (const auto& [name, entry] : cfg->getPopups())
    {
        if (entry.modal.isNotEmpty())
        {
            const juce::KeyPress kp { parseShortcut (entry.modal) };

            if (kp.isValid())
            {
                const auto idxIt { idToIndex.find ("popup:" + name) };

                if (idxIt != idToIndex.end())
                {
                    modalBindings.insert_or_assign (kp, idxIt->second);
                    entries.at (static_cast<std::size_t> (idxIt->second)).shortcut = kp;
                }
            }
        }

        if (entry.global.isNotEmpty())
        {
            const juce::KeyPress kp { parseShortcut (entry.global) };

            if (kp.isValid())
            {
                const auto idxIt { idToIndex.find ("popup_global:" + name) };

                if (idxIt != idToIndex.end())
                {
                    globalBindings.insert_or_assign (kp, idxIt->second);
                    entries.at (static_cast<std::size_t> (idxIt->second)).shortcut = kp;
                }
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
