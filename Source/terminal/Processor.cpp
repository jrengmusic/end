/**
 * @file Processor.cpp
 * @brief Implementation of the terminal pipeline orchestrator.
 *
 * Implements Processor — the Controller that owns Video (terminal state machine),
 * CellFifo (reader→message bridge), Parser, and Model&.
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
 * @brief Constructs the Processor: binds Model& and TextBuffer&; constructs Video and Parser.
 *
 * Model is owned by terminal::Session and passed by reference.
 * Video is owned by this Processor and receives the initial terminal dimensions and the events map.
 * Parser is owned by this Processor and receives Video& reference directly.
 * CellFifo is sized for scrollbackLines × cols worst-case Char slots.
 * UUID is provided by the caller — no internal generation.
 * Buffer resize is handled by prepare() called from Session's Resizer stop trigger.
 *
 * @param stateRef      Terminal parameter store owned by terminal::Session.
 * @param dims          Terminal dimensions in cells.
 * @param textBufferRef Cross-thread string buffer owned by terminal::Session.
 * @param uuid          Stable UUID for this Processor — generated once by the caller.
 *
 * @note MESSAGE THREAD — must be constructed on the message thread.
 */
Processor::Processor (Model& stateRef,
                      jam::Cell::Rectangle dims,
                      TextBuffer& textBufferRef,
                      const juce::String& uuid)
    : textBuffer (textBufferRef)
    , state (stateRef)
    , video (dims, events)
    , skit (events)
    , uuid (uuid)
{
    const int scrollbackLines { AppModel::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
    cellFifo.setSize (scrollbackLines * dims.getWidth().value, dims.getHeight().value * dims.getWidth().value);

    state.rebuildRowDirtyFlags (dims.getHeight().value);

    registerEvents();

    parameters.add (id::cellWidth,  [this] { setCellSize(); });
    parameters.add (id::cellHeight, [this] { setCellSize(); });

    parser = std::make_unique<Parser> (video);
    state.addListener (this);
}

// =============================================================================

/**
 * @brief Destroys the Processor.
 *
 * Removes the ValueTree listener first — ComponentAttachment ungraft on
 * CodeView destruction fires VT events; Processor must not be in the
 * listener list at that point.  Then closes the TTY — stops the reader
 * thread before the events map is destroyed by member destruction.
 * Reader thread joins inside close(), so events.get(id::data) cannot
 * fire after close() returns.
 *
 * @note MESSAGE THREAD.
 */
Processor::~Processor()
{
    state.removeListener (this);

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

    static constexpr int kbModeSet { 1 }; // XTMODKEYS: assign flags verbatim
    static constexpr int kbModeOr { 2 }; // XTMODKEYS: enable bits (bitwise OR)
    static constexpr int kbModeAndNot { 3 }; // XTMODKEYS: disable bits (bitwise AND NOT)

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
 * Fires on the message thread when Model's ValueTree properties change.
 * Handles cell pixel changes (cellWidth, cellHeight) applied directly to Video,
 * and displayName recomputation from foregroundProcess / cwd.
 * Handles screenDirty — drains CellFifo history rows, overwrites live content from Video via Value::map projection.
 * Foreground process query and clear happen on the reader thread in the
 * outputBlockStart and promptRow event handlers (registerEvents).
 *
 * @note MESSAGE THREAD.
 */
void Processor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // PARAM children flush via id::value — dispatch on paramId via parameters map.
    if (property == id::value and tree.getType() == jam::Model::PARAM)
    {
        const juce::Identifier paramId { tree.getProperty (id::id).toString() };

        if (parameters.contains (paramId))
            parameters.get (paramId);
    }

    // TEXT parameters flush as direct properties on the SESSION root node.
    // When foregroundProcess or cwd change, recompute displayName.
    if (property == id::foregroundProcess or property == id::cwd)
        setDisplayName (tree);
}

// =============================================================================

void Processor::setCellSize() noexcept
{
    auto displayNode { state.getChildWithName (id::DISPLAY) };
    const int cellW { static_cast<int> (
        jam::Model::getValueFromChildWithID (displayNode, id::cellWidth).getValue()) };
    const int cellH { static_cast<int> (
        jam::Model::getValueFromChildWithID (displayNode, id::cellHeight).getValue()) };
    video.setCellSize (cellW, cellH);
}

// =============================================================================

/**
 * @brief Drains one logical history line from the history ring. Message thread.
 *
 * Delegates to CellFifo::drainHistory — Processor does not apply the entry to any editor.
 * The View (Display) is the sole CodeModel owner and applies drained entries directly.
 *
 * @param outLine  Receives the joined jam::CodeLine built from Char data and flags.
 * @return true if a complete (or partial tail) entry was produced; false when the history ring is empty.
 * @note MESSAGE THREAD only.
 */
bool Processor::drainHistory (jam::CodeLine& outLine) noexcept
{
    return cellFifo.drainHistory (outLine);
}

/**
 * @brief Drains one viewport row from the active ring. Message thread.
 *
 * Delegates to CellFifo::drainActive — Processor does not apply the entry to any editor.
 * The View (Display) is the sole CodeModel owner and applies drained entries directly.
 *
 * @param outLine  Receives the viewport row jam::CodeLine built from Char data and flags.
 * @return true if an entry was produced; false when the active ring is empty.
 * @note MESSAGE THREAD only.
 */
bool Processor::drainActive (jam::CodeLine& outLine) noexcept
{
    return cellFifo.drainActive (outLine);
}

void Processor::setDisplayName (const juce::ValueTree& tree) noexcept
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
        state.setTreeProperty (app::id::displayName, name, nullptr);
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
        auto kbScreenNode { state.getChildWithName (kbScreenId) };
        const uint32_t keyboardFlags { static_cast<uint32_t> (
            static_cast<int> (jam::Model::getValueFromChildWithID (kbScreenNode, id::keyboardFlags).getValue())) };
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

Model& Processor::getState() noexcept { return state; }
const Model& Processor::getState() const noexcept { return state; }

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
 * @param dims    Terminal dimensions in cells.
 * @note MESSAGE THREAD — called from Session::start().
 */
void Processor::startTTY (const juce::String& shell,
                           const juce::String& args,
                           const juce::String& cwd,
                           const juce::StringPairArray& env,
                           jam::Cell::Rectangle dims) noexcept
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

    tty->open (dims, shell, args, cwd);
}

/**
 * @brief Prepares Processor for new terminal dimensions.
 *
 * Resizes Video's grid only — CellFifo and CodeView are untouched (I3).
 * History is preserved; in-flight departures drain normally.
 * Live zone refreshes on the next screenDirty tick when the shell redraws
 * after SIGWINCH.
 *
 * @param dims  Terminal dimensions in cells.
 * @note MESSAGE THREAD — safe only while processing is suspended.
 */
void Processor::prepare (jam::Cell::Rectangle dims) noexcept
{
    video.setWinsize (dims);
    state.rebuildRowDirtyFlags (dims.getHeight().value);

    if (tty != nullptr and tty->isThreadRunning())
        tty->setWinsize (terminal::Winsize { dims.getWidth().value, dims.getHeight().value, 0, 0 });
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


/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
