#include "State.h"
#include "Layout.h"
#include "../rendering/Screen.h"

namespace Terminal
{

State::State (TextBuffer& tb)
    : jam::ValueTree (ID::SESSION)
    , textBuffer (tb)
{
    auto xml { jam::XML::getFromBinary (jam::IDref::parametersXml) };
    jassert (xml != nullptr);

    Layout::build (*xml, *this, textBuffer);

    keyboardModeStack.allocate (2 * maxKeyboardStackDepth, true);
    keyboardModeStackSize.allocate (2, true);

    startTimerHz (60);

    get().addListener (this);
}

State::~State() = default;

//==========================================================================
// SSOT registration
//==========================================================================

void State::addTextParameter (const juce::Identifier& id, juce::ValueTree& rootNode) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (ID::SESSION) };
    sessionGroup->add<Parameter<const char*>> (id, id, rootNode, id);
}

void State::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent == get())
        registerNodeAtomics (child);
}

void State::registerNodeAtomics (juce::ValueTree& node) noexcept
{
    const juce::Identifier groupId { node.getType() };

    if (not params.contains (groupId))
        params.add<jam::AnyMap> (groupId);

    auto& group { *params.get<jam::AnyMap> (groupId) };

    for (int i { node.getNumProperties() - 1 }; i >= 0; --i)
    {
        const auto propId { node.getPropertyName (i) };
        const auto prop   { node.getProperty (propId) };

        if (not group.contains (propId))
        {
            if (prop.isDouble())
                addParameter<float> (propId, static_cast<float> (prop), group, node);
            else
                addParameter<int> (propId, static_cast<int> (prop), group, node);
        }

        node.removeProperty (propId, nullptr);
    }
}

//==========================================================================
// Reader-thread store helpers
//==========================================================================

void State::storeValue (const juce::Identifier& groupId, const juce::Identifier& paramId, int value) noexcept
{
    params.get<jam::AnyMap> (groupId)->get<Parameter<int>> (paramId)->store (value);
}

int State::loadValue (const juce::Identifier& groupId, const juce::Identifier& paramId) const noexcept
{
    return params.get<jam::AnyMap> (groupId)->get<Parameter<int>> (paramId)->load();
}

void State::storeTextValue (const juce::Identifier& groupId, const juce::Identifier& paramId, const char* ptr) noexcept
{
    params.get<jam::AnyMap> (groupId)->get<Parameter<const char*>> (paramId)->store (ptr);
}

//==========================================================================
// ValueTree access — MESSAGE THREAD
//==========================================================================

bool State::getMode (const juce::Identifier& id) const noexcept
{
    auto modesNode { get().getChildWithName (ID::MODES) };
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (modesNode, id).getValue()) != 0;
}

int State::getActiveScreen() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), ID::activeScreen).getValue());
}

uint32_t State::getKeyboardFlags() const noexcept
{
    const int scr { getActiveScreen() };
    auto screenNode { get().getChildWithName (juce::Identifier { Screen::Map::getContext()->get (scr) }) };
    return static_cast<uint32_t> (
        static_cast<int> (jam::ValueTree::getValueFromChildWithID (screenNode, ID::keyboardFlags).getValue()));
}

static int
getSessionParamInt (const juce::ValueTree& root, const juce::Identifier& paramId, int defaultValue = 0) noexcept
{
    auto param { jam::ValueTree::getChildWithID (root, paramId.toString()) };
    int result { defaultValue };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (Terminal::ID::value));
    }

    return result;
}

static int
getScreenParamInt (const juce::ValueTree& root, int scr, const juce::Identifier& paramId, int defaultValue = 0) noexcept
{
    auto screenNode { root.getChildWithName (juce::Identifier { Terminal::Screen::Map::getContext()->get (scr) }) };
    auto param { jam::ValueTree::getChildWithID (screenNode, paramId.toString()) };
    int result { defaultValue };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (Terminal::ID::value));
    }

    return result;
}

cell State::getCursorRow() const noexcept { return cell (getScreenParamInt (get(), getActiveScreen(), ID::cursorRow)); }

cell State::getCursorCol() const noexcept { return cell (getScreenParamInt (get(), getActiveScreen(), ID::cursorCol)); }

bool State::isCursorVisible() const noexcept
{
    return getScreenParamInt (get(), getActiveScreen(), ID::cursorVisible, 1) != 0;
}

int State::getCursorShape() const noexcept { return getScreenParamInt (get(), getActiveScreen(), ID::cursorShape); }

int State::getCursorColor() const noexcept { return getScreenParamInt (get(), getActiveScreen(), ID::cursorColor, -1); }

int State::getCols() const noexcept { return getSessionParamInt (get(), ID::cols); }

cell State::getVisibleRows() const noexcept { return cell (getSessionParamInt (get(), ID::visibleRows)); }

juce::String State::getTitle() const noexcept { return get().getProperty (ID::title).toString(); }
juce::String State::getCwd() const noexcept { return get().getProperty (ID::cwd).toString(); }
juce::String State::getForegroundProcess() const noexcept
{
    return get().getProperty (ID::foregroundProcess).toString();
}

int State::getScrollbackUsed() const noexcept { return 0; }

//==========================================================================
// Reader-thread setters
//==========================================================================

void State::setId (const juce::String& uuid) { get().setProperty (jam::ID::id, uuid, nullptr); }

void State::setScreen (int s) noexcept { storeValue (ID::SESSION, ID::activeScreen, s); }

void State::setMode (const juce::Identifier& id, bool v) noexcept { storeValue (ID::MODES, id, v ? 1 : 0); }

void State::setCursorRow (int s, cell row) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorRow, row.value);
    setSnapshotDirty();
}

void State::setCursorCol (int s, cell col) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorCol, col.value);
    setSnapshotDirty();
}

void State::setCursorVisible (int s, bool v) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorVisible, v ? 1 : 0);
}

void State::setCursorShape (int s, int shape) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorShape, shape);
}

void State::setCursorColor (int s, juce::Colour colour) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorColor, static_cast<int> (colour.getARGB()));
}

void State::resetCursorColor (int s) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorColor, -1);
}

void State::setTitle (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (ID::title, src, length) };
    storeTextValue (ID::SESSION, ID::title, p);
}

void State::setCwd (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (ID::cwd, src, length) };
    storeTextValue (ID::SESSION, ID::cwd, p);
}

void State::setForegroundProcess (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (ID::foregroundProcess, src, length) };
    storeTextValue (ID::SESSION, ID::foregroundProcess, p);
}

void State::setDimensions (cell cols, cell rows) noexcept
{
    storeValue (ID::SESSION, ID::cols, cols.value);
    storeValue (ID::SESSION, ID::visibleRows, rows.value);
}

//==========================================================================
// Keyboard mode stack
//==========================================================================

void State::pushKeyboardMode (int s, uint32_t flags) noexcept
{
    jassert (s >= 0 and s < 2);
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize[s] };

    if (size >= maxKeyboardStackDepth)
    {
        for (int i { 0 }; i < maxKeyboardStackDepth - 1; ++i)
        {
            jassert (base + i + 1 < 2 * maxKeyboardStackDepth);
            keyboardModeStack[base + i] = keyboardModeStack[base + i + 1];
        }

        --size;
    }

    jassert (base + size < 2 * maxKeyboardStackDepth);
    keyboardModeStack[base + size] = flags;
    ++size;
    const juce::Identifier pushScreenId { Screen::Map::getContext()->get (s) };
    storeValue (pushScreenId, ID::keyboardFlags, static_cast<int> (flags));
}

void State::popKeyboardMode (int s, int count) noexcept
{
    jassert (s >= 0 and s < 2);
    auto& size { keyboardModeStackSize[s] };
    const int toPop { std::min (count, size) };
    size -= toPop;

    const int base { s * maxKeyboardStackDepth };
    jassert (size <= 0 or base + size - 1 < 2 * maxKeyboardStackDepth);
    const uint32_t current { size > 0 ? keyboardModeStack[base + size - 1] : 0u };
    const juce::Identifier popScreenId { Screen::Map::getContext()->get (s) };
    storeValue (popScreenId, ID::keyboardFlags, static_cast<int> (current));
}

void State::setKeyboardMode (int s, uint32_t flags, int mode) noexcept
{
    jassert (s >= 0 and s < 2);
    const int base { s * maxKeyboardStackDepth };
    auto& size { keyboardModeStackSize[s] };

    if (size == 0)
    {
        jassert (base < 2 * maxKeyboardStackDepth);
        keyboardModeStack[base] = 0u;
        size = 1;
    }

    jassert (base + size - 1 < 2 * maxKeyboardStackDepth);
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

    const juce::Identifier setScreenId { Screen::Map::getContext()->get (s) };
    storeValue (setScreenId, ID::keyboardFlags, static_cast<int> (top));
}

void State::resetKeyboardMode (int s) noexcept
{
    jassert (s >= 0 and s < 2);
    keyboardModeStackSize[s] = 0;
    const juce::Identifier resetScreenId { Screen::Map::getContext()->get (s) };
    storeValue (resetScreenId, ID::keyboardFlags, 0);
}

//==========================================================================
// OSC 133 shell integration
//==========================================================================

void State::setOutputBlockStart (cell row) noexcept
{
    storeValue (ID::SESSION, ID::outputBlockTop, row.value);
    storeValue (ID::SESSION, ID::outputBlockBottom, row.value);
    storeValue (ID::SESSION, ID::outputScanActive, 1);
}

void State::setOutputBlockEnd (cell row) noexcept
{
    storeValue (ID::SESSION, ID::outputBlockBottom, row.value);
    storeValue (ID::SESSION, ID::outputScanActive, 0);
}

void State::extendOutputBlock (cell row) noexcept
{
    if (params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::outputScanActive)->load() != 0)
    {
        storeValue (ID::SESSION, ID::outputBlockBottom, row.value);
    }
}

void State::setPromptRow (cell row) noexcept { storeValue (ID::SESSION, ID::promptRow, row.value); }

cell State::getOutputBlockTop() const noexcept { return cell (getSessionParamInt (get(), ID::outputBlockTop, -1)); }

cell State::getOutputBlockBottom() const noexcept { return cell (getSessionParamInt (get(), ID::outputBlockBottom, -1)); }

cell State::getPromptRow() const noexcept { return cell (getSessionParamInt (get(), ID::promptRow, -1)); }

void State::setHistoryRows (int count) noexcept { storeValue (ID::SESSION, ID::historyRows, count); }

int State::getHistoryRows() const noexcept { return getSessionParamInt (get(), ID::historyRows, 0); }

bool State::hasOutputBlock() const noexcept
{
    const cell blockTop { getOutputBlockTop() };
    const cell prompt { getPromptRow() };
    const int screenVal { getActiveScreen() };
    const bool normalScreen { screenVal == Screen::Map::normal };

    return blockTop.value >= 0 and prompt > blockTop and normalScreen;
}

//==========================================================================
// Shell exit signal
//==========================================================================

void State::setShellExited (bool exited) noexcept { storeValue (ID::SESSION, ID::shellExited, exited ? 1 : 0); }

bool State::getShellExited() const noexcept { return getSessionParamInt (get(), ID::shellExited) != 0; }

//==========================================================================
// Snapshot signal
//==========================================================================

void State::setSnapshotDirty() noexcept
{
    if (params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::pasteEchoRemaining)->load() <= 0)
    {
        params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
    }
}

bool State::consumeSnapshotDirty() noexcept
{
    return params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->exchangeAcquire (0) != 0;
}

bool State::isSnapshotDirty() const noexcept { return getSessionParamInt (get(), ID::snapshotDirty) != 0; }

//==========================================================================
// Paste echo gate
//==========================================================================

void State::setPasteEchoGate (int bytes) noexcept
{
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::pasteEchoRemaining)->storeRelease (bytes);
}

void State::consumePasteEcho (int bytes) noexcept
{
    auto* gate { params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::pasteEchoRemaining) };

    if (gate->load() > 0)
    {
        const int remaining { gate->fetchSubAcqRel (bytes) - bytes };

        if (remaining <= 0)
        {
            gate->store (0);
            setSnapshotDirty();
        }
    }
}

void State::clearPasteEchoGate() noexcept
{
    auto* gate { params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::pasteEchoRemaining) };

    if (gate->exchangeAcqRel (0) > 0)
    {
        setSnapshotDirty();
    }
}

//==========================================================================
// Sync output
//==========================================================================

void State::setSyncOutput (bool active) noexcept
{
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::syncOutputActive)->storeRelease (active ? 1 : 0);

    if (not active)
    {
        setSnapshotDirty();
    }
}

bool State::isSyncOutputActive() const noexcept
{
    return params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::syncOutputActive)->load() != 0;
}

void State::requestSyncResize() noexcept
{
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::syncResizePending)->store (1);
}

bool State::consumeSyncResize() noexcept
{
    return params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::syncResizePending)->exchangeRelaxed (0) != 0;
}

//==========================================================================
// Selection
//==========================================================================

void State::setSelectionType (int type) noexcept
{
    AppState::getContext()->setSelectionType (type);
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

int State::getSelectionType() const noexcept { return AppState::getContext()->getSelectionType(); }

void State::setSelectionCursor (cell row, cell col) noexcept
{
    storeValue (ID::SESSION, ID::selectionCursorRow, row.value);
    storeValue (ID::SESSION, ID::selectionCursorCol, col.value);
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

cell State::getSelectionCursorRow() const noexcept { return cell (getSessionParamInt (get(), ID::selectionCursorRow)); }

cell State::getSelectionCursorCol() const noexcept { return cell (getSessionParamInt (get(), ID::selectionCursorCol)); }

void State::setSelectionAnchor (cell row, cell col) noexcept
{
    storeValue (ID::SESSION, ID::selectionAnchorRow, row.value);
    storeValue (ID::SESSION, ID::selectionAnchorCol, col.value);
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

cell State::getSelectionAnchorRow() const noexcept { return cell (getSessionParamInt (get(), ID::selectionAnchorRow)); }

cell State::getSelectionAnchorCol() const noexcept { return cell (getSessionParamInt (get(), ID::selectionAnchorCol)); }

void State::setDragAnchor (cell row, cell col) noexcept
{
    storeValue (ID::SESSION, ID::dragAnchorRow, row.value);
    storeValue (ID::SESSION, ID::dragAnchorCol, col.value);
}

cell State::getDragAnchorRow() const noexcept { return cell (getSessionParamInt (get(), ID::dragAnchorRow)); }

cell State::getDragAnchorCol() const noexcept { return cell (getSessionParamInt (get(), ID::dragAnchorCol)); }

void State::setDragActive (bool active) noexcept
{
    storeValue (ID::SESSION, ID::dragActive, active ? 1 : 0);
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

bool State::isDragActive() const noexcept { return getSessionParamInt (get(), ID::dragActive) != 0; }

//==========================================================================
// Preview
//==========================================================================

bool State::isPreviewActive() const noexcept { return getSessionParamInt (get(), ID::preview) != 0; }

int State::getSplitCol() const noexcept { return getSessionParamInt (get(), ID::splitCol); }

void State::dismissPreview() noexcept
{
    storeValue (ID::SESSION, ID::preview, 0);
    storeValue (ID::SESSION, ID::splitCol, 0);
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

//==========================================================================
// Hints
//==========================================================================

void State::setHintPage (int page) noexcept { storeValue (ID::SESSION, ID::hintPage, page); }

int State::getHintPage() const noexcept { return getSessionParamInt (get(), ID::hintPage); }

void State::setHintTotalPages (int total) noexcept { storeValue (ID::SESSION, ID::hintTotalPages, total); }

int State::getHintTotalPages() const noexcept { return getSessionParamInt (get(), ID::hintTotalPages); }

//==========================================================================
// Modal
//==========================================================================

void State::setModalType (ModalType type) noexcept
{
    AppState::getContext()->setModalType (static_cast<int> (type));
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

ModalType State::getModalType() const noexcept
{
    return static_cast<ModalType> (AppState::getContext()->getModalType());
}

bool State::isModal() const noexcept { return getModalType() != ModalType::none; }

//==========================================================================
// Per-screen atomic loaders — any thread, lock-free
// Called by Processor::ID::screenSwitch handler on the reader thread.
//==========================================================================

int State::loadCursorRow (int s) const noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    return loadValue (screenId, ID::cursorRow);
}

int State::loadCursorCol (int s) const noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    return loadValue (screenId, ID::cursorCol);
}

bool State::loadCursorVisible (int s) const noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    return loadValue (screenId, ID::cursorVisible) != 0;
}

uint32_t State::loadKeyboardFlags (int s) const noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    return static_cast<uint32_t> (loadValue (screenId, ID::keyboardFlags));
}

//==========================================================================
// Dimension atomic loaders — any thread, lock-free
// Called by Processor::process() on the reader thread to detect layout changes.
//==========================================================================

int State::loadCols() const noexcept        { return loadValue (ID::SESSION, ID::cols); }
int State::loadVisibleRows() const noexcept { return loadValue (ID::SESSION, ID::visibleRows); }
int State::loadCellWidth() const noexcept   { return loadValue (ID::DISPLAY, ID::cellWidth); }
int State::loadCellHeight() const noexcept  { return loadValue (ID::DISPLAY, ID::cellHeight); }

}// namespace Terminal
