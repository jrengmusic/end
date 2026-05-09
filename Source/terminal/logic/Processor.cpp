/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the pipeline half that owns the Parser and references
 * Grid and State owned by Terminal::Session.
 * The PTY side (TTY + History) lives in Terminal::Session.
 *
 * ### Thread contexts used in this file
 * - **MESSAGE THREAD** — JUCE message loop; all public methods except `process()`.
 * - **READER THREAD**  — byte source (Terminal::Session onBytes or IPC); only `process()`.
 *
 * @see Processor.h
 */

#include "Processor.h"
#include "../../component/TerminalDisplay.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @brief Constructs the Processor: binds Grid&, State&, then constructs Parser.
 *
 * Grid and State are owned by Terminal::Session and must outlive this Processor.
 * UUID is provided by the caller — no internal generation.  The
 * `parser->writeToHost` callback is initially null; the owner (`Nexus`)
 * wires it to the appropriate sink before bytes start flowing.
 *
 * @param gridRef   Live cell buffer owned by Terminal::Session.
 * @param stateRef  Atomic terminal parameter store owned by Terminal::Session.
 * @param uuid      Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (Grid& gridRef, State& stateRef, const juce::String& uuid)
    : state (stateRef)
    , grid (gridRef)
    , uuid (uuid)
{
    parser = std::make_unique<Parser> (state, grid);

    parser->onClipboardChanged = [this] (const juce::String& c)
    {
        if (onClipboardChanged != nullptr)
            onClipboardChanged (c);
    };

    parser->onBell = [this]
    {
        if (onBell != nullptr)
            onBell();
    };

    parser->onDesktopNotification = [this] (const juce::String& title, const juce::String& body)
    {
        if (onDesktopNotification != nullptr)
            onDesktopNotification (title, body);
    };
}

// =============================================================================

/**
 * @brief Resizes the grid and notifies the parser.
 *
 * Does NOT touch any TTY — SIGWINCH is handled by Terminal::Session.
 * Called from Display when its component resizes.
 *
 * @param cols  New terminal width in character columns.
 * @param rows  New terminal height in character rows.
 * @note MESSAGE THREAD only.
 */
void Processor::resized (int cols, int rows) noexcept
{
    jassert (parser != nullptr);
    grid.setSize (rows, cols, true, true, true);
    state.setDimensions (cols, rows);
    parser->resize (cols, rows);
}

// =============================================================================

/**
 * @brief Encodes a JUCE key press into a VT escape sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodeKeyPress (const juce::KeyPress& key) const noexcept
{
    juce::String seq;

#if JUCE_WINDOWS
    if (state.getMode (ID::win32InputMode))
    {
        seq = Keyboard::encodeWin32Input (key);
    }
    else
#endif
    {
        const bool applicationCursor { state.getMode (ID::applicationCursor) };
        const uint32_t keyboardFlags { state.getKeyboardFlags() };
        seq = Keyboard::map (key, applicationCursor, keyboardFlags);
    }

    return seq;
}

/**
 * @brief Encodes a bracketed-paste text block into a byte sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodePaste (const juce::String& text) const noexcept
{
    juce::String result;

    if (text.isNotEmpty())
    {
        const bool bracketed { state.getMode (ID::bracketedPaste) };

        if (bracketed)
        {
            static constexpr const char open[] { "\x1b[200~" };
            static constexpr const char close[] { "\x1b[201~" };
            const juce::String normalized { text.replace ("\r\n", "\n").replace ("\r", "\n") };
            result = juce::String (open) + normalized + juce::String (close);
        }
        else
        {
            result = text;
        }
    }

    return result;
}

/**
 * @brief Encodes a focus-in or focus-out event into a VT byte sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodeFocusEvent (bool gained) const noexcept
{
    juce::String result;

    if (state.getMode (ID::focusEvents))
    {
        const char seq[4] { '\x1b', '[', gained ? 'I' : 'O', '\0' };
        result = juce::String (seq);
    }

    return result;
}

/**
 * @brief Encodes a mouse event into a VT SGR byte sequence.
 *
 * Const — no side effects, does NOT write to any PTY.
 *
 * @note MESSAGE THREAD only.
 */
juce::String Processor::encodeMouseEvent (int button, int col, int row, bool press) const noexcept
{
    const char finalChar { press ? 'M' : 'm' };
    return juce::String ("\x1b[<") + juce::String (button) + ";" + juce::String (col + 1) + ";" + juce::String (row + 1)
           + finalChar;
}

/**
 * @brief Processes raw bytes through the parser pipeline.
 *
 * Feeds `Parser::process()`.  Display is notified via ValueTree::Listener
 * when State::flush() propagates atomic values to the ValueTree on the timer tick.
 *
 * @note READER THREAD only — called from the byte source (Terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);
    parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
    state.consumePasteEcho (length);
}

State& Processor::getState() noexcept { return state; }
const State& Processor::getState() const noexcept { return state; }

Grid& Processor::getGrid() noexcept { return grid; }
const Grid& Processor::getGrid() const noexcept { return grid; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

/**
 * @brief Returns a const reference to the VT parser.
 * @note Asserts if parser is null (should never be in this new design).
 */
const Parser& Processor::getParser() const noexcept
{
    jassert (parser != nullptr);
    return *parser;
}

Parser& Processor::getParser() noexcept
{
    jassert (parser != nullptr);
    return *parser;
}

/**
 * @brief Wires parser.writeToHost to the given callback.
 *
 * @note MESSAGE THREAD — call before the first process() invocation.
 */
void Processor::setHostWriter (std::function<void (const char*, int)> writer) noexcept
{
    jassert (parser != nullptr);
    parser->writeToHost = std::move (writer);
}

/**
 * @brief Creates and returns a Display for this Processor.
 *
 * @return Unique pointer to the newly created Display.
 * @note MESSAGE THREAD.
 */
std::unique_ptr<Display> Processor::createDisplay()
{
    auto display { std::make_unique<Display> (*this) };
    display->setComponentID (uuid);
    return display;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
