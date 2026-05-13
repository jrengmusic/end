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

//==========================================================================
// Reader-thread store helpers
//==========================================================================

void State::storeValue (const juce::Identifier& groupId, const juce::Identifier& paramId, int value) noexcept
{
    params.get<jam::AnyMap> (groupId)->get<Parameter<int>> (paramId)->store (value);
}

void State::storeTextValue (const juce::Identifier& groupId, const juce::Identifier& paramId, const char* ptr) noexcept
{
    params.get<jam::AnyMap> (groupId)->get<Parameter<const char*>> (paramId)->store (ptr);
}

void State::storeFloatValue (const juce::Identifier& groupId, const juce::Identifier& paramId, float value) noexcept
{
    params.get<jam::AnyMap> (groupId)->get<Parameter<float>> (paramId)->store (value);
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

int State::getCursorRow() const noexcept { return getScreenParamInt (get(), getActiveScreen(), ID::cursorRow); }

int State::getCursorCol() const noexcept { return getScreenParamInt (get(), getActiveScreen(), ID::cursorCol); }

bool State::isCursorVisible() const noexcept
{
    return getScreenParamInt (get(), getActiveScreen(), ID::cursorVisible, 1) != 0;
}

int State::getCursorShape() const noexcept { return getScreenParamInt (get(), getActiveScreen(), ID::cursorShape); }

int State::getCursorColor() const noexcept { return getScreenParamInt (get(), getActiveScreen(), ID::cursorColor, -1); }

int State::getCols() const noexcept { return getSessionParamInt (get(), ID::cols); }

int State::getVisibleRows() const noexcept { return getSessionParamInt (get(), ID::visibleRows); }

int State::getCellWidth() const noexcept { return getSessionParamInt (get(), ID::cellWidth); }

int State::getCellHeight() const noexcept { return getSessionParamInt (get(), ID::cellHeight); }

int State::getBaseline() const noexcept { return getSessionParamInt (get(), ID::baseline); }

float State::getFontSize() const noexcept
{
    auto param { jam::ValueTree::getChildWithID (get(), ID::fontSize.toString()) };
    float result { 0.0f };

    if (param.isValid())
    {
        result = static_cast<float> (param.getProperty (Terminal::ID::value));
    }

    return result;
}

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

void State::setCursorRow (int s, int row) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorRow, row);
    setSnapshotDirty();
}

void State::setCursorCol (int s, int col) noexcept
{
    const juce::Identifier screenId { Screen::Map::getContext()->get (s) };
    storeValue (screenId, ID::cursorCol, col);
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

void State::setDimensions (int cols, int rows) noexcept
{
    storeValue (ID::SESSION, ID::cols, cols);
    storeValue (ID::SESSION, ID::visibleRows, rows);
}

void State::setCellMetrics (int cellWidth, int cellHeight, int baseline, float fontSize) noexcept
{
    storeValue (ID::SESSION, ID::cellWidth, cellWidth);
    storeValue (ID::SESSION, ID::cellHeight, cellHeight);
    storeValue (ID::SESSION, ID::baseline, baseline);
    storeFloatValue (ID::SESSION, ID::fontSize, fontSize);
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
    const juce::Identifier pushScreenId { Screen::Map::getContext()->get (s) };
    storeValue (pushScreenId, ID::keyboardFlags, static_cast<int> (flags));
}

void State::popKeyboardMode (int s, int count) noexcept
{
    auto& size { keyboardModeStackSize[s] };
    const int toPop { std::min (count, size) };
    size -= toPop;

    const int base { s * maxKeyboardStackDepth };
    const uint32_t current { size > 0 ? keyboardModeStack[base + size - 1] : 0u };
    const juce::Identifier popScreenId { Screen::Map::getContext()->get (s) };
    storeValue (popScreenId, ID::keyboardFlags, static_cast<int> (current));
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

    const juce::Identifier setScreenId { Screen::Map::getContext()->get (s) };
    storeValue (setScreenId, ID::keyboardFlags, static_cast<int> (top));
}

void State::resetKeyboardMode (int s) noexcept
{
    keyboardModeStackSize[s] = 0;
    const juce::Identifier resetScreenId { Screen::Map::getContext()->get (s) };
    storeValue (resetScreenId, ID::keyboardFlags, 0);
}

//==========================================================================
// OSC 133 shell integration
//==========================================================================

void State::setOutputBlockStart (int row) noexcept
{
    storeValue (ID::SESSION, ID::outputBlockTop, row);
    storeValue (ID::SESSION, ID::outputBlockBottom, row);
    storeValue (ID::SESSION, ID::outputScanActive, 1);
}

void State::setOutputBlockEnd (int row) noexcept
{
    storeValue (ID::SESSION, ID::outputBlockBottom, row);
    storeValue (ID::SESSION, ID::outputScanActive, 0);
}

void State::extendOutputBlock (int row) noexcept
{
    if (params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::outputScanActive)->load() != 0)
    {
        storeValue (ID::SESSION, ID::outputBlockBottom, row);
    }
}

void State::setPromptRow (int row) noexcept { storeValue (ID::SESSION, ID::promptRow, row); }

int State::getOutputBlockTop() const noexcept { return getSessionParamInt (get(), ID::outputBlockTop, -1); }

int State::getOutputBlockBottom() const noexcept { return getSessionParamInt (get(), ID::outputBlockBottom, -1); }

int State::getPromptRow() const noexcept { return getSessionParamInt (get(), ID::promptRow, -1); }

bool State::hasOutputBlock() const noexcept
{
    const int blockTop { getOutputBlockTop() };
    const int prompt { getPromptRow() };
    const int screenVal { getActiveScreen() };
    const bool normalScreen { screenVal == Screen::Map::normal };

    return blockTop >= 0 and prompt > blockTop and normalScreen;
}

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

void State::setSelectionCursor (int row, int col) noexcept
{
    storeValue (ID::SESSION, ID::selectionCursorRow, row);
    storeValue (ID::SESSION, ID::selectionCursorCol, col);
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

int State::getSelectionCursorRow() const noexcept { return getSessionParamInt (get(), ID::selectionCursorRow); }

int State::getSelectionCursorCol() const noexcept { return getSessionParamInt (get(), ID::selectionCursorCol); }

void State::setSelectionAnchor (int row, int col) noexcept
{
    storeValue (ID::SESSION, ID::selectionAnchorRow, row);
    storeValue (ID::SESSION, ID::selectionAnchorCol, col);
    params.get<jam::AnyMap> (ID::SESSION)->get<Parameter<int>> (ID::snapshotDirty)->storeRelease (1);
}

int State::getSelectionAnchorRow() const noexcept { return getSessionParamInt (get(), ID::selectionAnchorRow); }

int State::getSelectionAnchorCol() const noexcept { return getSessionParamInt (get(), ID::selectionAnchorCol); }

void State::setDragAnchor (int row, int col) noexcept
{
    storeValue (ID::SESSION, ID::dragAnchorRow, row);
    storeValue (ID::SESSION, ID::dragAnchorCol, col);
}

int State::getDragAnchorRow() const noexcept { return getSessionParamInt (get(), ID::dragAnchorRow); }

int State::getDragAnchorCol() const noexcept { return getSessionParamInt (get(), ID::dragAnchorCol); }

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

}// namespace Terminal
