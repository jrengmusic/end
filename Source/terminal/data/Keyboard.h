/**
 * @file Keyboard.h
 * @brief Translates JUCE KeyPress events into xterm-compatible escape sequences.
 *
 * This header implements the keypress-to-escape-sequence mapping layer used by
 * the terminal emulator. It converts raw JUCE key events into the byte strings
 * that a PTY/shell process expects to receive on stdin.
 *
 * ## xterm Key Sequence Format
 *
 * xterm uses three sequence families:
 *
 * | Family | Format              | Example          | Used for                     |
 * |--------|---------------------|------------------|------------------------------|
 * | SS3    | `ESC O <final>`     | `\x1bOP`         | F1–F4, cursor (app mode)     |
 * | CSI    | `ESC [ <params> <final>` | `\x1b[A`    | Cursor, editing, F5–F12      |
 * | Plain  | single byte         | `\r`, `\x7f`     | Return, Backspace, Tab, Esc  |
 *
 * ### Modifier Encoding
 *
 * When a modifier key (Shift, Alt, Ctrl) is held alongside a special key, xterm
 * encodes the combination as a numeric parameter `N` inserted before the final
 * byte of the sequence:
 *
 * ```
 * N = 1 + shift_bit + alt_bit + ctrl_bit
 * ```
 *
 * | Modifier combination | N  | Example cursor-up         |
 * |----------------------|----|---------------------------|
 * | None                 | 1  | `\x1b[A`  (omitted)       |
 * | Shift                | 2  | `\x1b[1;2A`               |
 * | Alt                  | 3  | `\x1b[1;3A`               |
 * | Shift+Alt            | 4  | `\x1b[1;4A`               |
 * | Ctrl                 | 5  | `\x1b[1;5A`               |
 * | Shift+Ctrl           | 6  | `\x1b[1;6A`               |
 * | Alt+Ctrl             | 7  | `\x1b[1;7A`               |
 * | Shift+Alt+Ctrl       | 8  | `\x1b[1;8A`               |
 *
 * When `N == 1` (no modifiers) the `1;` parameter is omitted entirely.
 *
 * ### Ctrl+Letter Encoding
 *
 * Ctrl+A through Ctrl+Z produce control characters 0x01–0x1A.
 * Ctrl+@ and Ctrl+Space produce NUL (0x00).
 * A small set of punctuation characters have dedicated Ctrl mappings
 * (e.g. Ctrl+[ → ESC, Ctrl+\\ → FS, Ctrl+] → GS).
 *
 * ### Alt Prefix
 *
 * Alt is handled by prepending ESC (`\x1b`) to whatever sequence the key
 * would produce without Alt. If the key produces no special sequence, the
 * text character is prefixed instead (e.g. Alt+a → `\x1ba`).
 */
#pragma once
#include <JuceHeader.h>
#include <array>
#include <unordered_map>

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @struct Keyboard
 * @brief Namespace-struct providing static key-mapping utilities.
 *
 * All members are static; this struct is never instantiated. It acts as a
 * scoped namespace with private implementation details hidden from callers.
 *
 * The single public entry point is `map()`. All other methods are private
 * helpers that handle specific modifier combinations or key families.
 */
struct Keyboard
{
    /**
     * @brief Converts a JUCE KeyPress into an xterm escape sequence string.
     *
     * Dispatches to the appropriate private handler based on which modifier
     * keys are active:
     * - Ctrl-only → `mapCtrl()`
     * - Alt (with or without Shift/Ctrl) → `mapAlt()`
     * - No modifier (or Shift-only) → `mapPlain()`
     *
     * @param key             The JUCE key event to translate.
     * @param applicationCursor  When @c true, cursor keys use SS3 sequences
     *                           (`\x1bOA`) instead of CSI sequences (`\x1b[A`),
     *                           matching xterm's "application cursor keys" mode
     *                           (DECCKM, `\x1b[?1h`).
     * @param keyboardFlags  Kitty keyboard protocol flags from the active screen.
     *                       0 = legacy mode (no enhanced encoding).
     *                       Bit 0 (1): disambiguate escape codes.
     *                       Bit 3 (8): report all keys as escape codes.
     * @return The escape sequence as a juce::String, or an empty string if the
     *         key has no defined mapping (caller should ignore the event).
     */
    static inline juce::String map (const juce::KeyPress& key, bool applicationCursor, uint32_t keyboardFlags = 0)
    {
        const int code { key.getKeyCode() };
        const auto mods { key.getModifiers() };
        const auto ch { key.getTextCharacter() };
        juce::String result;

        if (mods.isCtrlDown() and not mods.isAltDown())
        {
            result = mapCtrl (code, mods, keyboardFlags);
        }
        else if (mods.isAltDown())
        {
            result = mapAlt (code, mods, ch, applicationCursor, keyboardFlags);
        }
        else
        {
            result = mapPlain (code, mods, ch, applicationCursor, keyboardFlags);
        }

        return result;
    }

#if JUCE_WINDOWS
    /**
     * @brief Encodes a JUCE key press as a Win32-input-mode sequence.
     *
     * Produces the ConPTY win32-input-mode encoding:
     * @code
     *   CSI Vk ; Sc ; Uc ; 1 ; Cs ; 1 _
     * @endcode
     *
     * Where:
     * - Vk = Win32 wVirtualKeyCode (decimal)
     * - Sc = Win32 wVirtualScanCode via MapVirtualKeyW (decimal)
     * - Uc = UnicodeChar decimal value (0 for Ctrl+letter)
     * - 1  = bKeyDown (key-down only; JUCE delivers no key-up events)
     * - Cs = dwControlKeyState bitmask
     * - 1  = wRepeatCount (always 1)
     *
     * Navigation keys (arrows, Home, End, Insert, Delete, PgUp, PgDn) have
     * ENHANCED_KEY (0x100) set in the control-key state.
     *
     * @param key  The JUCE key press event to encode.
     * @return The win32-input-mode escape sequence, or an empty string if the
     *         key code cannot be mapped to a Win32 virtual key.
     */
    static juce::String encodeWin32Input (const juce::KeyPress& key) noexcept;
#endif

private:
    /**
     * @brief Handles Ctrl+key combinations (without Alt).
     *
     * Produces control characters and a small set of special sequences:
     * - Ctrl+A…Z → bytes 0x01…0x1A
     * - Ctrl+@ or Ctrl+Space → NUL (0x00)
     * - Ctrl+[ → ESC (0x1B), Ctrl+\\ → FS (0x1C), Ctrl+] → GS (0x1D),
     *   Ctrl+^ → RS (0x1E), Ctrl+_ → US (0x1F)
     *
     * When kitty keyboard flag 1 (disambiguate) or flag 8 (all keys) is
     * active, Ctrl+key combinations produce CSI u sequences instead of
     * legacy control characters, using the lowercase codepoint as the key
     * number (e.g. Ctrl+C → `CSI 99;5u`).
     *
     * @param code  The raw JUCE key code (from `KeyPress::getKeyCode()`).
     * @param mods  Full modifier state (Ctrl is always set; Shift may be).
     * @param keyboardFlags  Kitty keyboard protocol flags.
     * @return The control-character sequence or CSI u sequence, or an
     *         empty string if @p code has no Ctrl mapping.
     */
    static inline juce::String mapCtrl (int code, const juce::ModifierKeys& mods, uint32_t keyboardFlags)
    {
        juce::String result;

        if (keyboardFlags & (kittyDisambiguate | kittyAllKeys))
        {
            if (code >= 'A' and code <= 'Z')
            {
                result = encodeCsiU (code - 'A' + 'a', mods);
            }
            else if (code == '@')
            {
                result = encodeCsiU ('@', mods);
            }
            else if (code == ' ')
            {
                result = encodeCsiU (' ', mods);
            }
            else
            {
                const auto it { ctrlKeys.find (code) };

                if (it != ctrlKeys.end())
                {
                    result = encodeCsiU (code, mods);
                }
            }
        }
        else
        {
            if (code >= 'A' and code <= 'Z')
            {
                result = juce::String::charToString (static_cast<wchar_t> (code - 'A' + ctrlLetterBase));
            }
            else if (code == '@' or code == ' ')
            {
                const char nul { 0 };
                result = juce::String (&nul, 1);
            }
            else
            {
                const auto it { ctrlKeys.find (code) };

                if (it != ctrlKeys.end())
                {
                    result = it->second;
                }
            }
        }

        return result;
    }

    /**
     * @brief Handles Alt+key combinations.
     *
     * Strips Alt from the modifier set, recursively calls `map()` to obtain
     * the base sequence, then prepends ESC (`\x1b`). If the key has no special
     * sequence but carries a printable character, that character is ESC-prefixed
     * instead (standard "meta key" behaviour).
     *
     * Shift and Ctrl are forwarded to the inner `map()` call so that
     * combinations like Alt+Shift+F1 are encoded correctly.
     *
     * @param code             Raw JUCE key code.
     * @param mods             Full modifier state (including Alt).
     * @param ch               Text character associated with the key event.
     * @param applicationCursor Forwarded to the inner `map()` call.
     * @param keyboardFlags  Kitty keyboard protocol flags (forwarded to inner
     *                       `map()` call).
     * @return ESC-prefixed sequence, or an empty string if neither a special
     *         sequence nor a printable character is available.
     */
    static inline juce::String mapAlt (int code, const juce::ModifierKeys& mods, juce::juce_wchar ch, bool applicationCursor, uint32_t keyboardFlags)
    {
        juce::String result;

        if ((keyboardFlags & (kittyDisambiguate | kittyAllKeys)) and isTextKey (code))
        {
            const int codepoint { (code >= 'A' and code <= 'Z') ? (code - 'A' + 'a') : code };
            result = encodeCsiU (codepoint, mods);
        }
        else
        {
            // Arrow, function, and editing keys encode modifiers as a CSI
            // parameter (e.g. \x1b[1;3D for Alt+Left).  The ESC-prefix
            // encoding (\x1b + base sequence) is only used for printable
            // characters and simple keys where no CSI modifier form exists.
            const auto cursorIt { cursorKeys.find (code) };
            const auto editCodeIt { editingCodes.find (code) };
            const auto editCharIt { editingFinalChars.find (code) };
            const bool isFKey { code >= juce::KeyPress::F1Key and code <= juce::KeyPress::F12Key };

            if (cursorIt != cursorKeys.end())
            {
                result = cursorKey (cursorIt->second, mods, applicationCursor);
            }
            else if (isFKey)
            {
                result = functionKey (code, mods);
            }
            else if (editCodeIt != editingCodes.end() or editCharIt != editingFinalChars.end())
            {
                result = editingKey (code, mods);
            }
            else
            {
                int flags { 0 };

                if (mods.isShiftDown())
                {
                    flags |= juce::ModifierKeys::shiftModifier;
                }

                if (mods.isCtrlDown())
                {
                    flags |= juce::ModifierKeys::ctrlModifier;
                }

                const juce::ModifierKeys stripped (flags);
                const juce::KeyPress inner (code, stripped, ch);
                const juce::String seq { map (inner, applicationCursor, keyboardFlags) };

                if (seq.isNotEmpty())
                {
                    result = juce::String (escPrefix) + seq;
                }
                else if (ch > 0)
                {
                    result = juce::String (escPrefix) + juce::String::charToString (ch);
                }
            }
        }

        return result;
    }

    /**
     * @brief Handles unmodified keys (and Shift-only combinations).
     *
     * Dispatches to the appropriate sub-handler in priority order:
     * 1. `simpleKeys` table — Return, Backspace, Tab, Escape.
     * 2. `cursorKeys` table — arrow keys, routed through `cursorKey()`.
     * 3. F1–F12 range — routed through `functionKey()`.
     * 4. Home, End, and editing cluster — routed through `editingKey()`.
     * 5. Printable character fallback — passes the text character through as-is.
     *
     * @param code             Raw JUCE key code.
     * @param mods             Modifier state (Shift may be set; Ctrl/Alt are not).
     * @param ch               Text character associated with the key event.
     * @param applicationCursor Forwarded to `cursorKey()`.
     * @param keyboardFlags  Kitty keyboard protocol flags.
     * @return The escape sequence or character string, or empty if unmapped.
     */
    static inline juce::String mapPlain (int code, const juce::ModifierKeys& mods, juce::juce_wchar ch, bool applicationCursor, uint32_t keyboardFlags)
    {
        juce::String result;
        const auto simpleIt { simpleKeys.find (code) };
        const auto cursorIt { cursorKeys.find (code) };

        if (simpleIt != simpleKeys.end())
        {
            if (shouldUseCsiU (code, mods, keyboardFlags))
            {
                const auto kittyIt { kittyKeyCodes.find (code) };

                if (kittyIt != kittyKeyCodes.end())
                {
                    result = encodeCsiU (kittyIt->second, mods);
                }
            }
            else
            {
                result = simpleIt->second;
            }
        }
        else if (cursorIt != cursorKeys.end())
        {
            result = cursorKey (cursorIt->second, mods, applicationCursor);
        }
        else if (code >= juce::KeyPress::F1Key and code <= juce::KeyPress::F12Key)
        {
            result = functionKey (code, mods);
        }
        else if (code == juce::KeyPress::homeKey
                 or code == juce::KeyPress::endKey
                 or editingCodes.find (code) != editingCodes.end())
        {
            result = editingKey (code, mods);
        }
        else if (ch > 0 and not mods.isCtrlDown())
        {
            if ((keyboardFlags & kittyAllKeys) and isTextKey (code))
            {
                const int codepoint { (code >= 'A' and code <= 'Z') ? (code - 'A' + 'a') : code };
                result = encodeCsiU (codepoint, mods);
            }
            else
            {
                result = juce::String::charToString (ch);
            }
        }

        return result;
    }

    // -------------------------------------------------------------------------
    // xterm escape sequence building blocks
    // -------------------------------------------------------------------------

    /// ESC character — used as a standalone escape or as the Alt prefix.
    static constexpr const char* escPrefix      { "\x1b" };

    /// CSI introducer: ESC [ — begins most special-key sequences.
    static constexpr const char* csiPrefix      { "\x1b[" };

    /**
     * @brief CSI modifier prefix: `ESC [ 1 ;` — used when a modifier is active.
     *
     * The `1` is the parameter position for the key number (always 1 for cursor
     * and F1–F4 keys); the modifier code `N` follows the semicolon.
     */
    static constexpr const char* csiModPrefix   { "\x1b[1;" };

    /// SS3 introducer: ESC O — used for F1–F4 and cursor keys in application mode.
    static constexpr const char* ss3Prefix      { "\x1bO" };

    /// CSI tilde terminator `~` — ends Insert/Delete/PgUp/PgDn/F5–F12 sequences.
    static constexpr const char* csiTerminator  { "~" };

    /// Semicolon separator between CSI parameters.
    static constexpr const char* paramSeparator { ";" };

    // -------------------------------------------------------------------------
    // Single-byte key sequences
    // -------------------------------------------------------------------------

    /// Carriage Return (0x0D) — sent for the Return/Enter key.
    static constexpr const char* carriageReturn { "\r" };

    /// DEL character (0x7F) — sent for the Backspace key (xterm default).
    static constexpr const char* backspaceDel   { "\x7f" };

    /// Horizontal Tab (0x09) — sent for the Tab key.
    static constexpr const char* horizontalTab  { "\t" };

    /// ESC character (0x1B) — sent for the Escape key.
    static constexpr const char* escapeChar     { "\x1b" };

    // -------------------------------------------------------------------------
    // xterm modifier bit weights
    // -------------------------------------------------------------------------

    /**
     * @brief Base value for the modifier parameter when no modifiers are held.
     *
     * The xterm modifier parameter is `1 + shift + alt + ctrl`. When the result
     * equals `noModifier` (1), the parameter is omitted from the sequence.
     */
    static constexpr int noModifier    { 1 };

    /// Shift modifier bit weight added to the xterm modifier parameter.
    static constexpr int shiftBit      { 1 };

    /// Alt modifier bit weight added to the xterm modifier parameter.
    static constexpr int altBit        { 2 };

    /// Ctrl modifier bit weight added to the xterm modifier parameter.
    static constexpr int ctrlBit       { 4 };

    /// Meta (Cmd on macOS) modifier bit weight added to the xterm modifier parameter.
    static constexpr int metaBit       { 8 };

    // -------------------------------------------------------------------------
    // Ctrl key byte offsets
    // -------------------------------------------------------------------------

    /**
     * @brief Byte value of Ctrl+A (0x01).
     *
     * Ctrl+letter sequences are computed as `(letter - 'A') + ctrlLetterBase`,
     * mapping A→1, B→2, …, Z→26.
     */
    static constexpr int ctrlLetterBase { 1 };

    // -------------------------------------------------------------------------
    // Key mapping tables
    // -------------------------------------------------------------------------

    /**
     * @brief Maps simple keys to their fixed single-byte sequences.
     *
     * These keys always produce the same output regardless of modifier state.
     * Handled before cursor/function/editing keys in `mapPlain()`.
     *
     * | JUCE key code              | Sequence  | Notes                  |
     * |----------------------------|-----------|------------------------|
     * | `KeyPress::returnKey`      | `\r`      | Carriage Return        |
     * | `KeyPress::backspaceKey`   | `\x7f`    | DEL (xterm default)    |
     * | `KeyPress::tabKey`         | `\t`      | Horizontal Tab         |
     * | `KeyPress::escapeKey`      | `\x1b`    | Escape                 |
     */
    static inline const std::unordered_map<int, juce::String> simpleKeys
    {
        { juce::KeyPress::returnKey,    carriageReturn },
        { juce::KeyPress::backspaceKey, backspaceDel },
        { juce::KeyPress::tabKey,       horizontalTab },
        { juce::KeyPress::escapeKey,    escapeChar }
    };

    /**
     * @brief Maps Ctrl+punctuation key codes to their control-character sequences.
     *
     * These supplement the Ctrl+A…Z range handled inline in `mapCtrl()`.
     *
     * | Key code | Sequence  | Control character |
     * |----------|-----------|-------------------|
     * | `[`      | `\x1b`    | ESC (0x1B)        |
     * | `\\`     | `\x1c`    | FS  (0x1C)        |
     * | `]`      | `\x1d`    | GS  (0x1D)        |
     * | `^`      | `\x1e`    | RS  (0x1E)        |
     * | `_`      | `\x1f`    | US  (0x1F)        |
     */
    static inline const std::unordered_map<int, juce::String> ctrlKeys
    {
        { '[',  escapeChar },
        { '\\', "\x1c" },
        { ']',  "\x1d" },
        { '^',  "\x1e" },
        { '_',  "\x1f" }
    };

    /**
     * @brief Maps arrow key codes to their CSI/SS3 final byte characters.
     *
     * The final byte is combined with the appropriate prefix in `cursorKey()`.
     *
     * | JUCE key code              | Final byte | Normal seq  | App-cursor seq |
     * |----------------------------|------------|-------------|----------------|
     * | `KeyPress::upKey`          | `A`        | `\x1b[A`    | `\x1bOA`       |
     * | `KeyPress::downKey`        | `B`        | `\x1b[B`    | `\x1bOB`       |
     * | `KeyPress::rightKey`       | `C`        | `\x1b[C`    | `\x1bOC`       |
     * | `KeyPress::leftKey`        | `D`        | `\x1b[D`    | `\x1bOD`       |
     */
    static inline const std::unordered_map<int, char> cursorKeys
    {
        { juce::KeyPress::upKey,    'A' },
        { juce::KeyPress::downKey,  'B' },
        { juce::KeyPress::rightKey, 'C' },
        { juce::KeyPress::leftKey,  'D' }
    };

    /**
     * @brief Maps editing-cluster keys to their xterm CSI numeric codes.
     *
     * These codes appear as the parameter in `CSI <code> ~` sequences.
     * Home and End are handled separately via `editingFinalChars` because they
     * use letter finals (`H`/`F`) rather than tilde sequences.
     *
     * | JUCE key code              | Code | Sequence    |
     * |----------------------------|------|-------------|
     * | `KeyPress::insertKey`      | 2    | `\x1b[2~`   |
     * | `KeyPress::deleteKey`      | 3    | `\x1b[3~`   |
     * | `KeyPress::pageUpKey`      | 5    | `\x1b[5~`   |
     * | `KeyPress::pageDownKey`    | 6    | `\x1b[6~`   |
     */
    static inline const std::unordered_map<int, int> editingCodes
    {
        { juce::KeyPress::insertKey,   2 },
        { juce::KeyPress::deleteKey,   3 },
        { juce::KeyPress::pageUpKey,   5 },
        { juce::KeyPress::pageDownKey, 6 }
    };

    /**
     * @brief Computes the xterm modifier parameter for a given modifier state.
     *
     * The parameter is defined as:
     * @code
     *   N = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0) + (meta ? 8 : 0)
     * @endcode
     *
     * Meta maps to Cmd on macOS (modifier code 9 = meta-only, same as
     * iTerm2/Kitty).  When `N == 1` (no modifiers active), callers omit the
     * modifier parameter from the sequence entirely.
     *
     * @param mods  The modifier key state to encode.
     * @return Integer modifier code in the range [1, 16].
     *
     * @note This function is `noexcept` and performs no allocation.
     */
    static inline int modifierCode (const juce::ModifierKeys& mods) noexcept
    {
        int code { noModifier };

        if (mods.isShiftDown())
        {
            code += shiftBit;
        }

        if (mods.isAltDown())
        {
            code += altBit;
        }

        if (mods.isCtrlDown())
        {
            code += ctrlBit;
        }

        if (mods.isCommandDown())
        {
            code += metaBit;
        }

        return code;
    }

    // -------------------------------------------------------------------------
    // Kitty keyboard protocol — CSI u encoding
    // -------------------------------------------------------------------------

    /// Kitty keyboard flag: disambiguate escape codes (bit 0).
    static constexpr uint32_t kittyDisambiguate { 1 };

    /// Kitty keyboard flag: report all keys as escape codes (bit 3).
    static constexpr uint32_t kittyAllKeys { 8 };

    /**
     * @brief Maps JUCE simple key codes to their kitty CSI u key numbers.
     *
     * | Key       | CSI u code |
     * |-----------|------------|
     * | Enter     | 13         |
     * | Backspace | 127        |
     * | Tab       | 9          |
     * | Escape    | 27         |
     */
    static inline const std::unordered_map<int, int> kittyKeyCodes
    {
        { juce::KeyPress::returnKey,    13 },
        { juce::KeyPress::backspaceKey, 127 },
        { juce::KeyPress::tabKey,       9 },
        { juce::KeyPress::escapeKey,    27 }
    };

    /**
     * @brief Encodes a key event in CSI u format.
     *
     * Produces the kitty keyboard protocol encoding:
     * - `CSI keycode u` when no modifiers are present.
     * - `CSI keycode ; modifiers u` when modifiers are active.
     *
     * @param keycode  The Unicode key number (e.g. 13 for Enter).
     * @param mods     Active modifier keys.
     * @return The CSI u escape sequence string.
     */
    static inline juce::String encodeCsiU (int keycode, const juce::ModifierKeys& mods)
    {
        const int mc { modifierCode (mods) };
        juce::String result;

        if (mc > noModifier)
        {
            result = juce::String (csiPrefix) + juce::String (keycode)
                   + paramSeparator + juce::String (mc) + "u";
        }
        else
        {
            result = juce::String (csiPrefix) + juce::String (keycode) + "u";
        }

        return result;
    }

    /**
     * @brief Determines whether a simple key should use CSI u encoding.
     *
     * Applies the kitty keyboard protocol rules (matching kitty source):
     * - Legacy (flags=0): never CSI u.
     * - Disambiguate (flag 1): Escape always gets CSI u.
     *   Enter/Tab/Backspace get CSI u only when a non-lock modifier
     *   (Shift/Ctrl/Alt) is held; unmodified presses stay legacy so the
     *   user can still type `reset` after a crash.
     * - All keys (flag 8): all simple keys get CSI u unconditionally.
     *
     * @param code           JUCE key code.
     * @param mods           Active modifier keys.
     * @param keyboardFlags  Kitty keyboard protocol flags.
     * @return `true` if the key should be encoded as CSI u.
     */
    static inline bool shouldUseCsiU (int code, const juce::ModifierKeys& mods, uint32_t keyboardFlags) noexcept
    {
        bool result { false };

        if (keyboardFlags & kittyAllKeys)
        {
            result = true;
        }
        else if (keyboardFlags & kittyDisambiguate)
        {
            const bool hasModifiers { mods.isShiftDown() or mods.isCtrlDown() or mods.isAltDown() };

            if (code == juce::KeyPress::escapeKey)
            {
                result = true;
            }
            else if (hasModifiers)
            {
                result = true;
            }
        }

        return result;
    }

    /**
     * @brief Tests whether a key code is an ASCII text key.
     *
     * Text keys are the set of keys that produce printable ASCII characters
     * in the kitty protocol spec: a-z (JUCE codes A-Z), 0-9, and the
     * punctuation keys: ` - = [ ] \\ ; ' , . / and space.
     *
     * @param code  JUCE key code.
     * @return `true` if the key is an ASCII text key.
     */
    static inline bool isTextKey (int code) noexcept
    {
        bool result { false };

        if (code >= 'A' and code <= 'Z')
        {
            result = true;
        }
        else if (code >= '0' and code <= '9')
        {
            result = true;
        }
        else
        {
            static constexpr std::array<int, 12> punctuation
            {
                '`', '-', '=', '[', ']', '\\', ';', '\'', ',', '.', '/', ' '
            };

            for (const auto& p : punctuation)
            {
                if (code == p)
                {
                    result = true;
                }
            }
        }

        return result;
    }

    /**
     * @brief Builds the escape sequence for an arrow key press.
     *
     * Without modifiers:
     * - Normal cursor mode: `CSI <final>`  (e.g. `\x1b[A`)
     * - Application cursor mode: `SS3 <final>` (e.g. `\x1bOA`)
     *
     * With modifiers: always `CSI 1 ; N <final>` regardless of cursor mode
     * (e.g. Shift+Up → `\x1b[1;2A`).
     *
     * @param finalChar       The letter identifying the direction: A/B/C/D.
     * @param mods            Active modifier keys.
     * @param applicationCursor  When @c true and no modifiers are active, use
     *                           SS3 prefix instead of CSI.
     * @return The complete escape sequence string.
     */
    static inline juce::String cursorKey (char finalChar, const juce::ModifierKeys& mods, bool applicationCursor)
    {
        const int mc { modifierCode (mods) };
        juce::String result;

        if (mc > noModifier)
        {
            result = juce::String (csiModPrefix) + juce::String (mc) + finalChar;
        }
        else
        {
            const char* prefix { applicationCursor ? ss3Prefix : csiPrefix };
            result = juce::String (prefix) + finalChar;
        }

        return result;
    }

    /**
     * @brief Builds the escape sequence for a function key press (F1–F12).
     *
     * F1–F4 use SS3 sequences; F5–F12 use CSI tilde sequences. The xterm
     * numeric codes for F5–F12 skip values 16 and 22 (historical gaps):
     *
     * | Key | Code | No-mod sequence | With-mod example (Shift) |
     * |-----|------|-----------------|--------------------------|
     * | F1  | —    | `\x1bOP`        | `\x1b[1;2P`              |
     * | F2  | —    | `\x1bOQ`        | `\x1b[1;2Q`              |
     * | F3  | —    | `\x1bOR`        | `\x1b[1;2R`              |
     * | F4  | —    | `\x1bOS`        | `\x1b[1;2S`              |
     * | F5  | 15   | `\x1b[15~`      | `\x1b[15;2~`             |
     * | F6  | 17   | `\x1b[17~`      | `\x1b[17;2~`             |
     * | F7  | 18   | `\x1b[18~`      | `\x1b[18;2~`             |
     * | F8  | 19   | `\x1b[19~`      | `\x1b[19;2~`             |
     * | F9  | 20   | `\x1b[20~`      | `\x1b[20;2~`             |
     * | F10 | 21   | `\x1b[21~`      | `\x1b[21;2~`             |
     * | F11 | 23   | `\x1b[23~`      | `\x1b[23;2~`             |
     * | F12 | 24   | `\x1b[24~`      | `\x1b[24;2~`             |
     *
     * @param juceKey  JUCE key code in the range [F1Key, F12Key].
     * @param mods     Active modifier keys.
     * @return The complete escape sequence string, or empty if @p juceKey is
     *         outside the F1–F12 range.
     */
    static inline juce::String functionKey (int juceKey, const juce::ModifierKeys& mods)
    {
        juce::String result;

        if (juceKey >= juce::KeyPress::F1Key and juceKey <= juce::KeyPress::F4Key)
        {
            static const std::array<char, 4> ss3Final { 'P', 'Q', 'R', 'S' };
            const int idx { juceKey - juce::KeyPress::F1Key };
            const int mc { modifierCode (mods) };

            if (mc == noModifier)
            {
                result = juce::String (ss3Prefix) + ss3Final.at (idx);
            }
            else
            {
                result = juce::String (csiModPrefix) + juce::String (mc) + ss3Final.at (idx);
            }
        }
        else if (juceKey >= juce::KeyPress::F5Key and juceKey <= juce::KeyPress::F12Key)
        {
            static const std::array<int, 8> csiCode { 15, 17, 18, 19, 20, 21, 23, 24 };
            const int idx { juceKey - juce::KeyPress::F5Key };
            const int nn { csiCode.at (idx) };
            const int mc { modifierCode (mods) };

            if (mc == noModifier)
            {
                result = juce::String (csiPrefix) + juce::String (nn) + csiTerminator;
            }
            else
            {
                result = juce::String (csiPrefix) + juce::String (nn) + paramSeparator + juce::String (mc) + csiTerminator;
            }
        }

        return result;
    }

    /**
     * @brief Maps Home and End to their CSI letter-final sequences.
     *
     * Home and End use letter finals (`H`/`F`) rather than tilde sequences,
     * so they are stored separately from `editingCodes`.
     *
     * | Key  | No-mod seq | With-mod example (Ctrl) |
     * |------|------------|-------------------------|
     * | Home | `\x1b[H`   | `\x1b[1;5H`             |
     * | End  | `\x1b[F`   | `\x1b[1;5F`             |
     */
    static inline const std::unordered_map<int, const char*> editingFinalChars
    {
        { juce::KeyPress::homeKey, "H" },
        { juce::KeyPress::endKey,  "F" }
    };

    /**
     * @brief Builds the escape sequence for an editing-cluster key press.
     *
     * Handles Home, End, Insert, Delete, Page Up, and Page Down.
     *
     * - Home / End: `CSI H` / `CSI F`, or `CSI 1 ; N H` / `CSI 1 ; N F` with mods.
     * - Insert / Delete / PgUp / PgDn: `CSI <code> ~`, or `CSI <code> ; N ~` with mods.
     *
     * @param juceKey  JUCE key code for one of the editing-cluster keys.
     * @param mods     Active modifier keys.
     * @return The complete escape sequence string, or empty if @p juceKey is
     *         not in the editing cluster.
     */
    static inline juce::String editingKey (int juceKey, const juce::ModifierKeys& mods)
    {
        const int mc { modifierCode (mods) };
        juce::String result;

        const auto finalIt { editingFinalChars.find (juceKey) };
        const auto codeIt { editingCodes.find (juceKey) };

        if (finalIt != editingFinalChars.end())
        {
            if (mc == noModifier)
            {
                result = juce::String (csiPrefix) + finalIt->second;
            }
            else
            {
                result = juce::String (csiModPrefix) + juce::String (mc) + finalIt->second;
            }
        }
        else if (codeIt != editingCodes.end())
        {
            if (mc == noModifier)
            {
                result = juce::String (csiPrefix) + juce::String (codeIt->second) + csiTerminator;
            }
            else
            {
                result = juce::String (csiPrefix) + juce::String (codeIt->second) + paramSeparator + juce::String (mc) + csiTerminator;
            }
        }

        return result;
    }
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
