/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the pipeline half that owns State (the APVTS), Video
 * (the terminal state machine), and Parser,
 * and references Buffer<Row> owned by terminal::Session.
 * The PTY-side TTY is owned by Processor and created by startTTY().
 *
 * ### Thread contexts used in this file
 * - **MESSAGE THREAD** — JUCE message loop; all public methods except `process()`.
 * - **READER THREAD**  — byte source (terminal::Session onBytes or IPC); only `process()`.
 *
 * @see Processor.h
 */

#include "Processor.h"

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the Processor: binds State&, activeBlocksRef, and TextBuffer&; constructs Video and Parser.
 *
 * State is owned by terminal::Session and passed by reference.
 * activeBlocksRef is Screen's atomic active-blocks pointer — Video loads it via refreshBlocks().
 * Video is owned by this Processor and receives activeBlocksRef and events& references.
 * Parser is owned by this Processor and receives Video& reference directly.
 * UUID is provided by the caller — no internal generation.
 * Buffer resize is handled entirely by Session::valueChanged (winsize) — Processor does not touch Buffer.
 *
 * @param stateRef        Terminal parameter store owned by terminal::Session.
 * @param activeBlocksRef Reference to Screen's atomic active-blocks pointer.
 * @param textBufferRef   Cross-thread string buffer owned by terminal::Session.
 * @param uuid            Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (State& stateRef,
                      std::atomic<jam::Block<jam::Row>*>& activeBlocksRef,
                      TextBuffer& textBufferRef,
                      const juce::String& uuid)
    : textBuffer (textBufferRef)
    , state (stateRef)
    , video (activeBlocksRef, events)
    , skit (events)
    , uuid (uuid)
{
    registerEvents();
    parser = std::make_unique<Parser> (video);
    state.get().addListener (this);
}

// =============================================================================

/**
 * @brief Destroys the Processor.
 *
 * Closes the TTY first — stops the reader thread before the events map is
 * destroyed by member destruction.  Reader thread joins inside close(), so
 * events.get(id::data) cannot fire after close() returns.
 * No explicit `removeListener()` needed — the ValueTree inside State is
 * destroyed alongside this Processor (member destruction order).
 *
 * @note MESSAGE THREAD.
 */
Processor::~Processor()
{
    if (tty != nullptr)
        tty->close();
}

// =============================================================================
// Keyboard mode stack — per-screen progressive enhancement flags (CSI u protocol).
// Moved from State to Processor so keyboard protocol state lives alongside the
// event handlers that manage it (reader thread).
// =============================================================================

void Processor::pushKeyboardMode (int s, uint32_t flags) noexcept
{
    jassert (s >= 0 and s < 2);
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize.at (static_cast<size_t> (s)) };

    if (size >= maxKeyboardStackDepth)
    {
        for (int i { 0 }; i < maxKeyboardStackDepth - 1; ++i)
        {
            jassert (base + i + 1 < 2 * maxKeyboardStackDepth);
            keyboardModeStack.at (static_cast<size_t> (base + i)) =
                keyboardModeStack.at (static_cast<size_t> (base + i + 1));
        }

        --size;
    }

    jassert (base + size < 2 * maxKeyboardStackDepth);
    keyboardModeStack.at (static_cast<size_t> (base + size)) = flags;
    ++size;
    const juce::Identifier pushScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (pushScreenId, id::keyboardFlags, static_cast<int> (flags));
}

void Processor::popKeyboardMode (int s, int count) noexcept
{
    jassert (s >= 0 and s < 2);
    auto& size { keyboardModeStackSize.at (static_cast<size_t> (s)) };
    const int toPop { std::min (count, size) };
    size -= toPop;

    const int base { s * maxKeyboardStackDepth };
    jassert (size <= 0 or base + size - 1 < 2 * maxKeyboardStackDepth);
    const uint32_t current { size > 0 ? keyboardModeStack.at (static_cast<size_t> (base + size - 1)) : 0u };
    const juce::Identifier popScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (popScreenId, id::keyboardFlags, static_cast<int> (current));
}

void Processor::setKeyboardMode (int s, uint32_t flags, int mode) noexcept
{
    jassert (s >= 0 and s < 2);
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize.at (static_cast<size_t> (s)) };

    if (size == 0)
    {
        jassert (base < 2 * maxKeyboardStackDepth);
        keyboardModeStack.at (static_cast<size_t> (base)) = 0u;
        size = 1;
    }

    jassert (base + size - 1 < 2 * maxKeyboardStackDepth);
    auto& top { keyboardModeStack.at (static_cast<size_t> (base + size - 1)) };

    static constexpr int kbModeSet { 1 };// XTMODKEYS: assign flags verbatim
    static constexpr int kbModeOr { 2 };// XTMODKEYS: enable bits (bitwise OR)
    static constexpr int kbModeAndNot { 3 };// XTMODKEYS: disable bits (bitwise AND NOT)

    if (mode == kbModeSet)
    {
        top = flags;
    }
    else if (mode == kbModeOr)
    {
        top |= flags;
    }
    else if (mode == kbModeAndNot)
    {
        top &= ~flags;
    }

    const juce::Identifier setScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (setScreenId, id::keyboardFlags, static_cast<int> (top));
}

void Processor::resetKeyboardMode (int s) noexcept
{
    jassert (s >= 0 and s < 2);
    keyboardModeStackSize.at (static_cast<size_t> (s)) = 0;
    const juce::Identifier resetScreenId { Map::Screen::getContext()->get (s) };
    state.storeValue (resetScreenId, id::keyboardFlags, 0);
}

// =============================================================================

/**
 * @brief ValueTree::Listener — reacts to top-down property changes from Display.
 *
 * Fires on the message thread when State's ValueTree properties change.
 * Handles cell pixel changes (cellWidth, cellHeight) applied directly to Video,
 * and displayName recomputation from foregroundProcess / cwd.
 * Foreground process query and clear happen on the reader thread in the
 * outputBlockStart and promptRow event handlers (registerEvents).
 *
 * @note MESSAGE THREAD.
 */
void Processor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // PARAM children flush via id::value — check the id property to identify cell pixel params.
    if (property == id::value and tree.getType() == jam::ValueTree::PARAM)
    {
        const juce::Identifier paramId { tree.getProperty (id::id).toString() };

        if (paramId == id::cellWidth or paramId == id::cellHeight)
        {
            const juce::Identifier displayId { id::DISPLAY };
            auto displayNode { state.get().getChildWithName (displayId) };
            const int cellW { static_cast<int> (
                jam::ValueTree::getValueFromChildWithID (displayNode, id::cellWidth).getValue()) };
            const int cellH { static_cast<int> (
                jam::ValueTree::getValueFromChildWithID (displayNode, id::cellHeight).getValue()) };
            video.setCellSize (cellW, cellH);
        }
    }

    // TEXT parameters flush as direct properties on the SESSION root node.
    // When foregroundProcess or cwd change, recompute displayName.
    if (property == id::foregroundProcess or property == id::cwd)
    {
        const auto foreground { tree.getProperty (id::foregroundProcess).toString() };
        const auto cwdPath { tree.getProperty (id::cwd).toString() };
        juce::String name;

        if (foreground.isNotEmpty())
        {
            name = foreground;
        }
        else if (cwdPath.isNotEmpty())
        {
            name = juce::File (cwdPath).getFileName();
        }

        if (name.isNotEmpty())
            state.get().setProperty (app::id::displayName, name, nullptr);
    }
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
    if (state.getMode (id::win32InputMode))
    {
        seq = Keyboard::encodeWin32Input (key);
    }
    else
#endif
    {
        const bool applicationCursor { state.getMode (id::applicationCursor) };
        const int activeScr { state.getActiveScreen() };
        const juce::Identifier kbScreenId { Map::Screen::getContext()->get (activeScr) };
        auto kbScreenNode { state.get().getChildWithName (kbScreenId) };
        const uint32_t keyboardFlags { static_cast<uint32_t> (
            static_cast<int> (jam::ValueTree::getValueFromChildWithID (kbScreenNode, id::keyboardFlags).getValue())) };
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
        const bool bracketed { state.getMode (id::bracketedPaste) };

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

    if (state.getMode (id::focusEvents))
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
juce::String Processor::encodeMouseEvent (int button, cell col, cell row, bool press) const noexcept
{
    const char finalChar { press ? 'M' : 'm' };
    return juce::String ("\x1b[<") + juce::String (button) + ";" + juce::String (col.value + 1) + ";"
           + juce::String (row.value + 1) + finalChar;
}

/**
 * @brief Processes raw bytes through the parser pipeline.
 *
 * Clean pipeline — lock and suspend check are the caller's responsibility
 * (host wrapper pattern, matching the JUCE VST wrapper contract).
 *
 * @note READER THREAD only — called from the byte source (terminal::Session
 *       onBytes callback or IPC dispatch in client mode).
 */
void Processor::process (const char* data, int length) noexcept
{
    jassert (parser != nullptr);
    video.refreshBlocks();

    if (video.getCols() > cell (0) and video.getVisibleRows() > cell (0))
    {
        parser->process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
        video.flush();
        video.flushResponses();
    }

    state.consumePasteEcho (length);
}

/**
 * @brief Suspends or unsuspends processing.
 *
 * Acquires callbackLock before writing the flag — blocks until any in-progress
 * process() call completes.  Same implementation as AudioProcessor::suspendProcessing.
 *
 * @note MESSAGE THREAD — blocks on callbackLock.
 */
void Processor::suspendProcessing (bool shouldBeSuspended) noexcept
{
    const juce::ScopedLock sl (callbackLock);
    suspended = shouldBeSuspended;
}

State& Processor::getState() noexcept { return state; }
const State& Processor::getState() const noexcept { return state; }

const juce::String& Processor::getUuid() const noexcept { return uuid; }

void Processor::flushResponses() noexcept { video.flushResponses(); }

/**
 * @brief Creates the platform TTY, adds shell env vars, wires callbacks, and opens the PTY.
 *
 * Creates UnixTTY or WindowsTTY with the shared events map, adds all shell integration
 * env vars, wires writeInput/writeToHost events for PTY stdin/response routing
 * (id::data is already registered in registerEvents()), then opens the TTY (starts the reader thread).
 *
 * @param shell   Shell program path.
 * @param args    Shell arguments string.  Empty = none.
 * @param cwd     Initial working directory.  Empty = inherit.
 * @param env     Shell integration environment variable pairs.
 * @param cols    Initial terminal width in character columns.
 * @param rows    Initial terminal height in character rows.
 * @note MESSAGE THREAD — called from Session::start().
 */
void Processor::startTTY (const juce::String& shell,
                           const juce::String& args,
                           const juce::String& cwd,
                           const juce::StringPairArray& env,
                           cell cols,
                           cell rows) noexcept
{
#if JUCE_MAC || JUCE_LINUX
    tty = std::make_unique<UnixTTY> (events);
#elif JUCE_WINDOWS
    tty = std::make_unique<WindowsTTY> (events);
#endif

    const juce::StringArray& keys { env.getAllKeys() };

    for (const auto& key : keys)
        tty->addShellEnv (key, env[key]);

    // writeToHost — parser responses (DSR, DA, CPR) → PTY stdin.
    events.add<const char*, int> (id::writeToHost,
                                  [this] (const char* data, int len)
                                  {
                                      if (tty != nullptr)
                                          tty->write (data, len);
                                  });

    // writeInput — keyboard/mouse input from Display → PTY stdin.
    events.add<const char*, int> (id::writeInput,
                                  [this] (const char* data, int len)
                                  {
                                      tty->write (data, len);
                                  });

    tty->open (cols, rows, shell, args, cwd);
}

/**
 * @brief Single resize API — tells Video the new dimensions and sends SIGWINCH to shell.
 *
 * Called from Processor vTPC (viewport change), Session::start() (initial sizing),
 * and the drainComplete event handler (sync-resize path).
 *
 * @param cols        New column count.
 * @param rows        New row count.
 * @param pixelWidth  Total viewport width in physical pixels (0 if unknown).
 * @param pixelHeight Total viewport height in physical pixels (0 if unknown).
 */
void Processor::setWinsize (cell cols, cell rows, int pixelWidth, int pixelHeight) noexcept
{
    video.setWinsize (cols, rows);

    if (tty != nullptr and tty->isThreadRunning())
        tty->setWinsize (cols, rows, pixelWidth, pixelHeight);
}


/**
 * @brief Writes raw input bytes to the PTY via the writeInput event handler.
 *
 * No-op when writeInput is not registered (remote sessions with no TTY before IPC wiring).
 *
 * @note MESSAGE THREAD.
 */
void Processor::writeInput (const char* data, int len) noexcept
{
    if (events.contains (id::writeInput))
        events.get (id::writeInput, data, len);
}

/**
 * @brief Registers the writeInput event handler.
 *
 * Called by Link for remote (IPC-connected) sessions to route input through IPC.
 * startTTY() calls this internally for PTY sessions.
 *
 * @note MESSAGE THREAD.
 */
void Processor::setInputWriter (std::function<void (const char*, int)> handler) noexcept
{
    events.add<const char*, int> (id::writeInput, std::move (handler));
}

/**
 * @brief Registers an IPC byte broadcast observer on the events map.
 *
 * The registered handler fires on the reader thread inside the id::data handler,
 * before callbackLock is acquired.  Used by nexus::Daemon::wireOnBytes() in daemon mode.
 *
 * @note MESSAGE THREAD — call before the first bytes arrive.
 */
void Processor::setBytesObserver (std::function<void (const char*, int)> handler) noexcept
{
    events.add<const char*, int> (id::bytesReceived, std::move (handler));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
