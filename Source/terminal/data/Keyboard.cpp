/**
 * @file Keyboard.cpp
 * @brief Out-of-line implementation of Keyboard::encodeWin32Input.
 *
 * Contains the Windows-only win32-input-mode encoder.  All other Keyboard
 * methods are inline in Keyboard.h.
 */

#include "Keyboard.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace Terminal
{ /*____________________________________________________________________________*/

#if JUCE_WINDOWS

/**
 * @brief Maps a JUCE key code to a Win32 virtual key code (VK_*).
 *
 * Special keys are looked up in a static table.  For printable ASCII
 * characters the JUCE code equals the uppercase ASCII value, which is also
 * the Win32 VK code (A=0x41 … Z=0x5A, 0=0x30 … 9=0x39).  For everything
 * else 0 is returned (caller treats 0 as "unmapped").
 *
 * @param juceCode  Raw JUCE key code from `KeyPress::getKeyCode()`.
 * @return Win32 virtual key code, or 0 if unmapped.
 */
static int juceToVk (int juceCode) noexcept
{
    // Special-key table — JUCE constant → Win32 VK
    static const std::unordered_map<int, int> specialKeys
    {
        { juce::KeyPress::returnKey,    VK_RETURN  },
        { juce::KeyPress::escapeKey,    VK_ESCAPE  },
        { juce::KeyPress::backspaceKey, VK_BACK    },
        { juce::KeyPress::tabKey,       VK_TAB     },
        { juce::KeyPress::deleteKey,    VK_DELETE  },
        { juce::KeyPress::insertKey,    VK_INSERT  },
        { juce::KeyPress::upKey,        VK_UP      },
        { juce::KeyPress::downKey,      VK_DOWN    },
        { juce::KeyPress::leftKey,      VK_LEFT    },
        { juce::KeyPress::rightKey,     VK_RIGHT   },
        { juce::KeyPress::homeKey,      VK_HOME    },
        { juce::KeyPress::endKey,       VK_END     },
        { juce::KeyPress::pageUpKey,    VK_PRIOR   },
        { juce::KeyPress::pageDownKey,  VK_NEXT    },
        { juce::KeyPress::spaceKey,     VK_SPACE   },
        { juce::KeyPress::F1Key,        VK_F1      },
        { juce::KeyPress::F2Key,        VK_F2      },
        { juce::KeyPress::F3Key,        VK_F3      },
        { juce::KeyPress::F4Key,        VK_F4      },
        { juce::KeyPress::F5Key,        VK_F5      },
        { juce::KeyPress::F6Key,        VK_F6      },
        { juce::KeyPress::F7Key,        VK_F7      },
        { juce::KeyPress::F8Key,        VK_F8      },
        { juce::KeyPress::F9Key,        VK_F9      },
        { juce::KeyPress::F10Key,       VK_F10     },
        { juce::KeyPress::F11Key,       VK_F11     },
        { juce::KeyPress::F12Key,       VK_F12     },
    };

    int vk { 0 };

    const auto it { specialKeys.find (juceCode) };

    if (it != specialKeys.end())
    {
        vk = it->second;
    }
    else if (juceCode >= 'A' and juceCode <= 'Z')
    {
        // JUCE stores letters as uppercase ASCII; Win32 VK_A…VK_Z are identical.
        vk = juceCode;
    }
    else if (juceCode >= '0' and juceCode <= '9')
    {
        // JUCE digit codes equal Win32 VK_0…VK_9.
        vk = juceCode;
    }
    else
    {
        // For all other characters (punctuation, symbols, etc.), ask Win32
        // to map the Unicode codepoint to a virtual key code.
        const SHORT vkScan { VkKeyScanW (static_cast<WCHAR> (juceCode)) };

        if (vkScan != -1)
        {
            vk = vkScan & 0xFF;  // Low byte is the VK code
        }
    }

    return vk;
}

/**
 * @brief Returns true for navigation keys that require ENHANCED_KEY in dwControlKeyState.
 *
 * @param vk  Win32 virtual key code.
 * @return true if the key is in the enhanced (extended) key set.
 */
static bool isEnhancedKey (int vk) noexcept
{
    bool result { false };

    switch (vk)
    {
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_HOME:
        case VK_END:
        case VK_INSERT:
        case VK_DELETE:
        case VK_PRIOR:
        case VK_NEXT:
            result = true;
            break;

        default:
            break;
    }

    return result;
}

/**
 * @brief Encodes a JUCE key press as a Win32-input-mode sequence.
 *
 * Format: `CSI Vk ; Sc ; Uc ; 1 ; Cs ; 1 _`
 *
 * @param key  The JUCE key press event to encode.
 * @return The win32-input-mode escape sequence, or an empty string if the
 *         key code cannot be mapped to a Win32 virtual key.
 */
juce::String Keyboard::encodeWin32Input (const juce::KeyPress& key) noexcept
{
    const int juceCode { key.getKeyCode() };
    const auto mods    { key.getModifiers() };

    const int vk { juceToVk (juceCode) };

    juce::String result;

    if (vk != 0)
    {
        // Scan code via Win32 API; 0 is a safe fallback.
        const UINT sc { MapVirtualKeyW (static_cast<UINT> (vk), MAPVK_VK_TO_VSC) };

        // Unicode character: 0 for Ctrl+letter (ConPTY handles the mapping),
        // otherwise the actual character from the key event.
        int uc { 0 };

        if (not mods.isCtrlDown())
        {
            const juce::juce_wchar ch { key.getTextCharacter() };
            uc = static_cast<int> (ch);
        }

        // dwControlKeyState bitmask
        int cs { 0 };

        if (mods.isShiftDown())
        {
            cs |= SHIFT_PRESSED;       // 0x10
        }

        if (mods.isCtrlDown())
        {
            cs |= LEFT_CTRL_PRESSED;   // 0x08
        }

        if (mods.isAltDown())
        {
            cs |= LEFT_ALT_PRESSED;    // 0x02
        }

        if (GetKeyState (VK_NUMLOCK) & 1)
        {
            cs |= NUMLOCK_ON;          // 0x20
        }

        if (GetKeyState (VK_CAPITAL) & 1)
        {
            cs |= CAPSLOCK_ON;         // 0x80
        }

        if (isEnhancedKey (vk))
        {
            cs |= ENHANCED_KEY;        // 0x100
        }

        // CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
        // Kd = 1 (key-down), Rc = 1 (repeat count)
        result = juce::String ("\x1b[")
               + juce::String (vk)          + ";"
               + juce::String ((int) sc)    + ";"
               + juce::String (uc)          + ";"
               + "1;"
               + juce::String (cs)          + ";"
               + "1_";
    }

    return result;
}

#endif // JUCE_WINDOWS

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
