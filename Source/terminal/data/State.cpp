#include "State.h"
#include "Layout.h"

namespace Terminal
{

State::State (TextBuffer& tb)
    : state (ID::SESSION)
    , textBuffer (tb)
{
    auto xml { jam::XML::getFromBinary (jam::IDref::pluginMetadata) };
    jassert (xml != nullptr);

    Layout::build (*xml, *this, textBuffer);

    keyboardModeStack.allocate (2 * maxKeyboardStackDepth, true);
    keyboardModeStackSize.allocate (2, true);

    startTimerHz (60);
}

State::~State() { stopTimer(); }

//==========================================================================
// SSOT registration
//==========================================================================

std::atomic<int>* State::addParameter (const juce::Identifier& id,
                                       int defaultValue,
                                       jam::AnyMap& targetMap,
                                       juce::ValueTree& parentNode) noexcept
{
    juce::ValueTree param { ID::PARAM };
    param.setProperty (ID::id, id.toString(), nullptr);
    param.setProperty (ID::value, defaultValue, nullptr);
    parentNode.appendChild (param, nullptr);

    auto* atom { targetMap.add<Atom<int>> (id, defaultValue, param) };
    return &atom->raw();
}

void State::addTextParameter (const juce::Identifier& id,
                              juce::ValueTree& rootNode) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (ID::SESSION) };
    sessionGroup->add<Atom<const char*>> (id, rootNode, id);
}

//==========================================================================
// APVTS API
//==========================================================================

std::atomic<int>* State::getRawParameterValue (const juce::Identifier& id) const noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (ID::SESSION) };
    jassert (sessionGroup->contains (id));
    return &sessionGroup->get<Atom<int>> (id)->raw();
}

std::atomic<int>* State::getRawParameterValue (int screen,
                                               const juce::Identifier& id) const noexcept
{
    const juce::Identifier screenId { map::Screen::getContext()->get (screen) };
    auto* screenGroup { params.get<jam::AnyMap> (screenId) };
    jassert (screenGroup->contains (id));
    return &screenGroup->get<Atom<int>> (id)->raw();
}

std::atomic<int>* State::getModeParameterValue (const juce::Identifier& id) const noexcept
{
    auto* modesGroup { params.get<jam::AnyMap> (ID::MODES) };
    jassert (modesGroup->contains (id));
    return &modesGroup->get<Atom<int>> (id)->raw();
}

//==========================================================================
// Private helpers
//==========================================================================

void State::storeAndFlush (std::atomic<int>& atom, int value) noexcept
{
    atom.store (value, std::memory_order_relaxed);
    getRawParameterValue (ID::needsFlush)->store (1, std::memory_order_release);
}

//==========================================================================
// ValueTree access — MESSAGE THREAD
//==========================================================================

juce::ValueTree State::getValueTree() noexcept { return state; }
juce::ValueTree State::getValueTree() const noexcept { return state; }

juce::Value State::getValue (const juce::Identifier& paramId)
{
    return jam::ValueTree::getValueFromChildWithID (state, paramId);
}

bool State::getMode (const juce::Identifier& id) const noexcept
{
    auto modesNode { state.getChildWithName (ID::MODES) };
    auto param { jam::ValueTree::getChildWithID (modesNode, id.toString()) };
    bool result { false };

    if (param.isValid())
    {
        result = static_cast<bool> (param.getProperty (ID::value));
    }

    return result;
}

int State::getActiveScreen() const noexcept
{
    auto param { jam::ValueTree::getChildWithID (state, ID::activeScreen.toString()) };
    int result { map::Screen::normal };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (ID::value));
    }

    return result;
}

uint32_t State::getKeyboardFlags() const noexcept
{
    const int scr { getActiveScreen() };
    auto screenNode { state.getChildWithName (juce::Identifier { map::Screen::getContext()->get (scr) }) };
    auto param { jam::ValueTree::getChildWithID (screenNode, ID::keyboardFlags.toString()) };
    uint32_t result { 0 };

    if (param.isValid())
    {
        result = static_cast<uint32_t> (static_cast<int> (param.getProperty (ID::value)));
    }

    return result;
}

static int getScreenParamInt (const juce::ValueTree& root,
                              int scr,
                              const juce::Identifier& paramId,
                              int defaultValue = 0) noexcept
{
    auto screenNode { root.getChildWithName (juce::Identifier { Terminal::map::Screen::getContext()->get (scr) }) };
    auto param { jam::ValueTree::getChildWithID (screenNode, paramId.toString()) };
    int result { defaultValue };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (Terminal::ID::value));
    }

    return result;
}


int State::getCursorRow() const noexcept
{
    return getScreenParamInt (state, getActiveScreen(), ID::cursorRow);
}

int State::getCursorCol() const noexcept
{
    return getScreenParamInt (state, getActiveScreen(), ID::cursorCol);
}

bool State::isCursorVisible() const noexcept
{
    return getScreenParamInt (state, getActiveScreen(), ID::cursorVisible, 1) != 0;
}

int State::getCursorShape() const noexcept
{
    return getScreenParamInt (state, getActiveScreen(), ID::cursorShape);
}

int State::getCursorColor() const noexcept
{
    return getScreenParamInt (state, getActiveScreen(), ID::cursorColor, -1);
}

int State::getCols() const noexcept
{
    return getRawParameterValue (ID::cols)->load (std::memory_order_relaxed);
}

int State::getVisibleRows() const noexcept
{
    return getRawParameterValue (ID::visibleRows)->load (std::memory_order_relaxed);
}

juce::String State::getTitle() const noexcept { return state.getProperty (ID::title).toString(); }
juce::String State::getCwd() const noexcept   { return state.getProperty (ID::cwd).toString(); }

int State::getScrollbackUsed() const noexcept { return 0; }

//==========================================================================
// Reader-thread setters
//==========================================================================

void State::setId (const juce::String& uuid)
{
    state.setProperty (jam::ID::id, uuid, nullptr);
}

void State::setScreen (int s) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::activeScreen), s);
}

void State::setMode (const juce::Identifier& id, bool v) noexcept
{
    storeAndFlush (*getModeParameterValue (id), v ? 1 : 0);
}

void State::setCursorRow (int s, int row) noexcept
{
    storeAndFlush (*getRawParameterValue (s, ID::cursorRow), row);
    setSnapshotDirty();
}

void State::setCursorCol (int s, int col) noexcept
{
    storeAndFlush (*getRawParameterValue (s, ID::cursorCol), col);
    setSnapshotDirty();
}

void State::setCursorVisible (int s, bool v) noexcept
{
    storeAndFlush (*getRawParameterValue (s, ID::cursorVisible), v ? 1 : 0);
}

void State::setCursorShape (int s, int shape) noexcept
{
    storeAndFlush (*getRawParameterValue (s, ID::cursorShape), shape);
}

void State::setCursorColor (int s, juce::Colour colour) noexcept
{
    storeAndFlush (*getRawParameterValue (s, ID::cursorColor),
                   static_cast<int> (colour.getARGB()));
}

void State::resetCursorColor (int s) noexcept
{
    storeAndFlush (*getRawParameterValue (s, ID::cursorColor), -1);
}

void State::setTitle (const char* src, int length) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (ID::SESSION) };
    auto* p { textBuffer.write (ID::title, src, length) };
    sessionGroup->get<Atom<const char*>> (ID::title)->store (p);
    getRawParameterValue (ID::needsFlush)->store (1, std::memory_order_release);
}

void State::setCwd (const char* src, int length) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (ID::SESSION) };
    auto* p { textBuffer.write (ID::cwd, src, length) };
    sessionGroup->get<Atom<const char*>> (ID::cwd)->store (p);
    getRawParameterValue (ID::needsFlush)->store (1, std::memory_order_release);
}

void State::setForegroundProcess (const char* src, int length) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (ID::SESSION) };
    auto* p { textBuffer.write (ID::foregroundProcess, src, length) };
    sessionGroup->get<Atom<const char*>> (ID::foregroundProcess)->store (p);
    getRawParameterValue (ID::needsFlush)->store (1, std::memory_order_release);
}

void State::setDimensions (int cols, int rows) noexcept
{
    getRawParameterValue (ID::cols)->store (cols, std::memory_order_relaxed);
    getRawParameterValue (ID::visibleRows)->store (rows, std::memory_order_relaxed);
    getRawParameterValue (ID::needsFlush)->store (1, std::memory_order_release);
}

//==========================================================================
// Keyboard mode stack
//==========================================================================

void State::pushKeyboardMode (int s, uint32_t flags) noexcept
{
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize[s] };

    if (size >= maxKeyboardStackDepth)
    {
        for (int i { 0 }; i < maxKeyboardStackDepth - 1; ++i)
        {
            keyboardModeStack[base + i] = keyboardModeStack[base + i + 1];
        }

        --size;
    }

    keyboardModeStack[base + size] = flags;
    ++size;
    storeAndFlush (*getRawParameterValue (s, ID::keyboardFlags),
                   static_cast<int> (flags));
}

void State::popKeyboardMode (int s, int count) noexcept
{
    auto& size { keyboardModeStackSize[s] };
    const int toPop { std::min (count, size) };
    size -= toPop;

    const int base { s * maxKeyboardStackDepth };
    const uint32_t current { size > 0 ? keyboardModeStack[base + size - 1] : 0u };
    storeAndFlush (*getRawParameterValue (s, ID::keyboardFlags),
                   static_cast<int> (current));
}

void State::setKeyboardMode (int s, uint32_t flags, int mode) noexcept
{
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize[s] };

    if (size == 0)
    {
        keyboardModeStack[base] = 0u;
        size = 1;
    }

    auto& top { keyboardModeStack[base + size - 1] };

    if (mode == 1)
    {
        top = flags;
    }
    else if (mode == 2)
    {
        top |= flags;
    }
    else if (mode == 3)
    {
        top &= ~flags;
    }

    storeAndFlush (*getRawParameterValue (s, ID::keyboardFlags),
                   static_cast<int> (top));
}

void State::resetKeyboardMode (int s) noexcept
{
    keyboardModeStackSize[s] = 0;
    storeAndFlush (*getRawParameterValue (s, ID::keyboardFlags), 0);
}

//==========================================================================
// OSC 133 shell integration
//==========================================================================

void State::setOutputBlockStart (int row) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::outputBlockTop), row);
    storeAndFlush (*getRawParameterValue (ID::outputBlockBottom), row);
    storeAndFlush (*getRawParameterValue (ID::outputScanActive), 1);
}

void State::setOutputBlockEnd (int row) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::outputBlockBottom), row);
    storeAndFlush (*getRawParameterValue (ID::outputScanActive), 0);
}

void State::extendOutputBlock (int row) noexcept
{
    if (getRawParameterValue (ID::outputScanActive)->load (std::memory_order_relaxed) != 0)
    {
        storeAndFlush (*getRawParameterValue (ID::outputBlockBottom), row);
    }
}

void State::setPromptRow (int row) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::promptRow), row);
}

int State::getOutputBlockTop() const noexcept
{
    return getRawParameterValue (ID::outputBlockTop)->load (std::memory_order_relaxed);
}

int State::getOutputBlockBottom() const noexcept
{
    return getRawParameterValue (ID::outputBlockBottom)->load (std::memory_order_relaxed);
}

int State::getPromptRow() const noexcept
{
    return getRawParameterValue (ID::promptRow)->load (std::memory_order_relaxed);
}

bool State::hasOutputBlock() const noexcept
{
    const int blockTop { getOutputBlockTop() };
    const int prompt { getPromptRow() };
    const int screenVal { getRawParameterValue (ID::activeScreen)->load (std::memory_order_relaxed) };
    const bool normalScreen { screenVal == map::Screen::normal };

    return blockTop >= 0 and prompt > blockTop and normalScreen;
}

//==========================================================================
// Snapshot signal
//==========================================================================

void State::setSnapshotDirty() noexcept
{
    if (getRawParameterValue (ID::pasteEchoRemaining)->load (std::memory_order_relaxed) <= 0)
    {
        getRawParameterValue (ID::snapshotDirty)->store (1, std::memory_order_release);
    }
}

bool State::consumeSnapshotDirty() noexcept
{
    return getRawParameterValue (ID::snapshotDirty)->exchange (0, std::memory_order_acquire) != 0;
}

bool State::isSnapshotDirty() const noexcept
{
    return getRawParameterValue (ID::snapshotDirty)->load (std::memory_order_relaxed) != 0;
}

//==========================================================================
// Paste echo gate
//==========================================================================

void State::setPasteEchoGate (int bytes) noexcept
{
    getRawParameterValue (ID::pasteEchoRemaining)->store (bytes, std::memory_order_release);
}

void State::consumePasteEcho (int bytes) noexcept
{
    auto& gate { *getRawParameterValue (ID::pasteEchoRemaining) };

    if (gate.load (std::memory_order_relaxed) > 0)
    {
        const int remaining { gate.fetch_sub (bytes, std::memory_order_acq_rel) - bytes };

        if (remaining <= 0)
        {
            gate.store (0, std::memory_order_relaxed);
            setSnapshotDirty();
        }
    }
}

void State::clearPasteEchoGate() noexcept
{
    auto& gate { *getRawParameterValue (ID::pasteEchoRemaining) };

    if (gate.exchange (0, std::memory_order_acq_rel) > 0)
    {
        setSnapshotDirty();
    }
}

//==========================================================================
// Sync output
//==========================================================================

void State::setSyncOutput (bool active) noexcept
{
    getRawParameterValue (ID::syncOutputActive)->store (active ? 1 : 0,
                                                        std::memory_order_release);

    if (not active)
        setSnapshotDirty();
}

bool State::isSyncOutputActive() const noexcept
{
    return getRawParameterValue (ID::syncOutputActive)->load (std::memory_order_relaxed) != 0;
}

void State::requestSyncResize() noexcept
{
    getRawParameterValue (ID::syncResizePending)->store (1, std::memory_order_relaxed);
}

bool State::consumeSyncResize() noexcept
{
    return getRawParameterValue (ID::syncResizePending)->exchange (0, std::memory_order_relaxed) != 0;
}

//==========================================================================
// Selection
//==========================================================================

void State::setSelectionType (int type) noexcept
{
    AppState::getContext()->setSelectionType (type);
    getRawParameterValue (ID::snapshotDirty)->store (1, std::memory_order_release);
}

int State::getSelectionType() const noexcept
{
    return AppState::getContext()->getSelectionType();
}

void State::setSelectionCursor (int row, int col) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::selectionCursorRow), row);
    storeAndFlush (*getRawParameterValue (ID::selectionCursorCol), col);
    getRawParameterValue (ID::snapshotDirty)->store (1, std::memory_order_release);
}

int State::getSelectionCursorRow() const noexcept
{
    return getRawParameterValue (ID::selectionCursorRow)->load (std::memory_order_relaxed);
}

int State::getSelectionCursorCol() const noexcept
{
    return getRawParameterValue (ID::selectionCursorCol)->load (std::memory_order_relaxed);
}

void State::setSelectionAnchor (int row, int col) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::selectionAnchorRow), row);
    storeAndFlush (*getRawParameterValue (ID::selectionAnchorCol), col);
    getRawParameterValue (ID::snapshotDirty)->store (1, std::memory_order_release);
}

int State::getSelectionAnchorRow() const noexcept
{
    return getRawParameterValue (ID::selectionAnchorRow)->load (std::memory_order_relaxed);
}

int State::getSelectionAnchorCol() const noexcept
{
    return getRawParameterValue (ID::selectionAnchorCol)->load (std::memory_order_relaxed);
}

void State::setDragAnchor (int row, int col) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::dragAnchorRow), row);
    storeAndFlush (*getRawParameterValue (ID::dragAnchorCol), col);
}

int State::getDragAnchorRow() const noexcept
{
    return getRawParameterValue (ID::dragAnchorRow)->load (std::memory_order_relaxed);
}

int State::getDragAnchorCol() const noexcept
{
    return getRawParameterValue (ID::dragAnchorCol)->load (std::memory_order_relaxed);
}

void State::setDragActive (bool active) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::dragActive), active ? 1 : 0);
    getRawParameterValue (ID::snapshotDirty)->store (1, std::memory_order_release);
}

bool State::isDragActive() const noexcept
{
    return getRawParameterValue (ID::dragActive)->load (std::memory_order_relaxed) != 0;
}

//==========================================================================
// Preview
//==========================================================================

bool State::isPreviewActive() const noexcept
{
    return getRawParameterValue (ID::preview)->load (std::memory_order_relaxed) != 0;
}

int State::getSplitCol() const noexcept
{
    return getRawParameterValue (ID::splitCol)->load (std::memory_order_relaxed);
}

void State::dismissPreview() noexcept
{
    storeAndFlush (*getRawParameterValue (ID::preview), 0);
    storeAndFlush (*getRawParameterValue (ID::splitCol), 0);
    getRawParameterValue (ID::snapshotDirty)->store (1, std::memory_order_release);
}

//==========================================================================
// Hints
//==========================================================================

void State::setHintPage (int page) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::hintPage), page);
}

int State::getHintPage() const noexcept
{
    return getRawParameterValue (ID::hintPage)->load (std::memory_order_relaxed);
}

void State::setHintTotalPages (int total) noexcept
{
    storeAndFlush (*getRawParameterValue (ID::hintTotalPages), total);
}

int State::getHintTotalPages() const noexcept
{
    return getRawParameterValue (ID::hintTotalPages)->load (std::memory_order_relaxed);
}

//==========================================================================
// Modal
//==========================================================================

void State::setModalType (ModalType type) noexcept
{
    AppState::getContext()->setModalType (static_cast<int> (type));
    getRawParameterValue (ID::snapshotDirty)->store (1, std::memory_order_release);
}

ModalType State::getModalType() const noexcept
{
    return static_cast<ModalType> (AppState::getContext()->getModalType());
}

bool State::isModal() const noexcept { return getModalType() != ModalType::none; }

//==========================================================================
// Timer callback
//==========================================================================

void State::timerCallback()
{
    static constexpr int flushHz { 120 };
    static constexpr int idleHz  { 60 };

    const bool anythingUpdated { flush() };
    const int interval { anythingUpdated ? 1000 / flushHz : 1000 / idleHz };
    startTimer (interval);

    if (onFlush != nullptr)
    {
        onFlush();
    }
}

} // namespace Terminal
