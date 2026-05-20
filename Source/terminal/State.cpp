#include "State.h"

namespace terminal
{
/*____________________________________________________________________________*/

State::State (TextBuffer& tb)
    : jam::ValueTree (id::SESSION)
    , textBuffer (tb)
{
    auto xml { jam::XML::getFromBinary (jam::IDref::parametersXml) };
    jassert (xml != nullptr);

    buildLayout (*xml, textBuffer);

    keyboardModeStack.allocate (2 * maxKeyboardStackDepth, true);
    keyboardModeStackSize.allocate (2, true);

    startTimerHz (60);

    get().addListener (this);
}

State::~State() = default;

//==========================================================================
// SSOT registration
//==========================================================================

int State::resolveLayoutDefault (const juce::XmlElement& elem,
                                 const LayoutBoolean& boolMap) noexcept
{
    const auto typeStr    { elem.getStringAttribute (id::type.toString()) };
    const auto defaultStr { elem.getStringAttribute (id::defaultValue.toString()) };
    int result { 0 };

    if (typeStr == id::boolType.toString())
    {
        result = boolMap.get (defaultStr);
    }
    else
    {
        result = elem.getIntAttribute (id::defaultValue.toString());
    }

    return result;
}

void State::buildLayout (const juce::XmlElement& xml, TextBuffer& tb)
{
    LayoutBoolean boolMap;

    // All groups are nested AnyMaps so flush() can iterate uniformly.
    params.add<jam::AnyMap> (id::SESSION);
    params.add<jam::AnyMap> (id::MODES);

    auto* screenCtx { ScreenMap::getContext() };

    for (const auto& [index, screenName] : screenCtx->get())
    {
        params.add<jam::AnyMap> (juce::Identifier { screenName });
    }

    // Root SESSION VT node — already constructed in State (id::SESSION).
    juce::ValueTree rootNode { get() };

    // MODES VT node — appended to SESSION.
    juce::ValueTree modesNode { id::MODES };
    rootNode.appendChild (modesNode, nullptr);

    // Screen VT nodes — appended to SESSION.
    for (const auto& [index, screenName] : screenCtx->get())
    {
        const juce::Identifier screenId { screenName };
        juce::ValueTree screenNode { screenId };
        rootNode.appendChild (screenNode, nullptr);
    }

    // Walk XML, dispatch on tag name.
    for (auto* child : xml.getChildIterator())
    {
        const auto& tag { child->getTagName() };

        if (tag == jam::ValueTree::PARAM.toString())
        {
            // Root-level parameter → SESSION group.
            auto* sessionGroup { params.get<jam::AnyMap> (id::SESSION) };
            const auto typeStr { child->getStringAttribute (id::type.toString()) };

            if (typeStr == "float")
            {
                addParameter<float> (juce::Identifier { child->getStringAttribute (id::id.toString()) },
                                     static_cast<float> (child->getDoubleAttribute (id::defaultValue.toString())),
                                     *sessionGroup,
                                     rootNode);
            }
            else
            {
                addParameter (juce::Identifier { child->getStringAttribute (id::id.toString()) },
                              resolveLayoutDefault (*child, boolMap),
                              *sessionGroup,
                              rootNode);
            }
        }
        else if (tag == id::MODES.toString())
        {
            // Mode parameters → MODES group + modesNode.
            auto* modesGroup { params.get<jam::AnyMap> (id::MODES) };

            for (auto* modeChild : child->getChildIterator())
            {
                addParameter (juce::Identifier { modeChild->getStringAttribute (id::id.toString()) },
                              resolveLayoutDefault (*modeChild, boolMap),
                              *modesGroup,
                              modesNode);
            }
        }
        else if (tag == id::SCREEN.toString())
        {
            // Per-screen parameters — schema declared once, duplicated per screen.
            for (const auto& [index, screenName] : screenCtx->get())
            {
                const juce::Identifier screenId { screenName };
                auto* screenGroup { params.get<jam::AnyMap> (screenId) };
                auto  screenNode  { rootNode.getChildWithName (screenId) };

                for (auto* screenChild : child->getChildIterator())
                {
                    addParameter (juce::Identifier { screenChild->getStringAttribute (id::id.toString()) },
                                  resolveLayoutDefault (*screenChild, boolMap),
                                  *screenGroup,
                                  screenNode);
                }
            }
        }
        else if (tag == id::TEXT.toString())
        {
            // TEXT parameter — Parameter<const char*> in SESSION group, slot in TextBuffer.
            const juce::Identifier textId { child->getStringAttribute (id::id.toString()) };
            const int maxlen              { child->getIntAttribute (id::maxlen.toString()) };

            addTextParameter (textId, rootNode);
            tb.addSlot (textId, maxlen);
        }
    }
}

void State::addTextParameter (const juce::Identifier& id, juce::ValueTree& rootNode) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (id::SESSION) };
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
        const auto prop { node.getProperty (propId) };

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
    auto modesNode { get().getChildWithName (id::MODES) };
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (modesNode, id).getValue()) != 0;
}

int State::getActiveScreen() const noexcept
{
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (get(), id::activeScreen).getValue());
}

uint32_t State::getKeyboardFlags() const noexcept
{
    const int scr { getActiveScreen() };
    auto screenNode { get().getChildWithName (juce::Identifier { ScreenMap::getContext()->get (scr) }) };
    return static_cast<uint32_t> (
        static_cast<int> (jam::ValueTree::getValueFromChildWithID (screenNode, id::keyboardFlags).getValue()));
}

static int
getSessionParamInt (const juce::ValueTree& root, const juce::Identifier& paramId, int defaultValue = 0) noexcept
{
    auto param { jam::ValueTree::getChildWithID (root, paramId.toString()) };
    int result { defaultValue };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (terminal::id::value));
    }

    return result;
}

static int
getScreenParamInt (const juce::ValueTree& root, int scr, const juce::Identifier& paramId, int defaultValue = 0) noexcept
{
    auto screenNode { root.getChildWithName (juce::Identifier { terminal::ScreenMap::getContext()->get (scr) }) };
    auto param { jam::ValueTree::getChildWithID (screenNode, paramId.toString()) };
    int result { defaultValue };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (terminal::id::value));
    }

    return result;
}

cell State::getCursorRow() const noexcept { return cell (getScreenParamInt (get(), getActiveScreen(), id::cursorRow)); }

cell State::getCursorCol() const noexcept { return cell (getScreenParamInt (get(), getActiveScreen(), id::cursorCol)); }

bool State::isCursorVisible() const noexcept
{
    return getScreenParamInt (get(), getActiveScreen(), id::cursorVisible, 1) != 0;
}

int State::getCursorShape() const noexcept { return getScreenParamInt (get(), getActiveScreen(), id::cursorShape); }

int State::getCursorColor() const noexcept { return getScreenParamInt (get(), getActiveScreen(), id::cursorColor, -1); }

cell State::getCols() const noexcept { return cell (getSessionParamInt (get(), id::cols)); }

cell State::getVisibleRows() const noexcept { return cell (getSessionParamInt (get(), id::visibleRows)); }

juce::String State::getTitle() const noexcept { return get().getProperty (id::title).toString(); }
juce::String State::getCwd() const noexcept { return get().getProperty (id::cwd).toString(); }
juce::String State::getForegroundProcess() const noexcept
{
    return get().getProperty (id::foregroundProcess).toString();
}

//==========================================================================
// Viewport scrollback parameters — reader-thread setters, message-thread getters
//==========================================================================

void State::setNumRows (int screen, int value) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (screen) };
    storeValue (screenId, id::numRows, value);
}

void State::setScrollOffset (int screen, int value) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (screen) };
    storeValue (screenId, id::scrollOffset, value);
}

void State::setScreenDirty (int screen) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (screen) };
    const int current { loadValue (screenId, id::screenDirty) };
    storeValue (screenId, id::screenDirty, current + 1);
}

int State::getNumRows (int screen) const noexcept { return getScreenParamInt (get(), screen, id::numRows); }

int State::getScrollOffset (int screen) const noexcept { return getScreenParamInt (get(), screen, id::scrollOffset); }

//==========================================================================
// Reader-thread setters
//==========================================================================

void State::setId (const juce::String& uuid) { get().setProperty (jam::ID::id, uuid, nullptr); }

void State::setScreen (int s) noexcept { storeValue (id::SESSION, id::activeScreen, s); }

void State::setMode (const juce::Identifier& id, bool v) noexcept { storeValue (id::MODES, id, v ? 1 : 0); }

void State::setCursorRow (int s, cell row) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    storeValue (screenId, id::cursorRow, row.value);
    setSnapshotDirty();
}

void State::setCursorCol (int s, cell col) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    storeValue (screenId, id::cursorCol, col.value);
    setSnapshotDirty();
}

void State::setCursorVisible (int s, bool v) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    storeValue (screenId, id::cursorVisible, v ? 1 : 0);
}

void State::setCursorShape (int s, int shape) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    storeValue (screenId, id::cursorShape, shape);
}

void State::setCursorColor (int s, juce::Colour colour) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    storeValue (screenId, id::cursorColor, static_cast<int> (colour.getARGB()));
}

void State::resetCursorColor (int s) noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    storeValue (screenId, id::cursorColor, -1);
}

void State::setTitle (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (id::title, src, length) };
    storeTextValue (id::SESSION, id::title, p);
}

void State::setCwd (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (id::cwd, src, length) };
    storeTextValue (id::SESSION, id::cwd, p);
}

void State::setForegroundProcess (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (id::foregroundProcess, src, length) };
    storeTextValue (id::SESSION, id::foregroundProcess, p);
}

void State::setDimensions (cell cols, cell rows) noexcept
{
    storeValue (id::SESSION, id::cols, cols.value);
    storeValue (id::SESSION, id::visibleRows, rows.value);
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
    const juce::Identifier pushScreenId { ScreenMap::getContext()->get (s) };
    storeValue (pushScreenId, id::keyboardFlags, static_cast<int> (flags));
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
    const juce::Identifier popScreenId { ScreenMap::getContext()->get (s) };
    storeValue (popScreenId, id::keyboardFlags, static_cast<int> (current));
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

    const juce::Identifier setScreenId { ScreenMap::getContext()->get (s) };
    storeValue (setScreenId, id::keyboardFlags, static_cast<int> (top));
}

void State::resetKeyboardMode (int s) noexcept
{
    jassert (s >= 0 and s < 2);
    keyboardModeStackSize[s] = 0;
    const juce::Identifier resetScreenId { ScreenMap::getContext()->get (s) };
    storeValue (resetScreenId, id::keyboardFlags, 0);
}

//==========================================================================
// OSC 133 shell integration
//==========================================================================

void State::setOutputBlockStart (cell row) noexcept
{
    storeValue (id::SESSION, id::outputBlockTop, row.value);
    storeValue (id::SESSION, id::outputBlockBottom, row.value);
    storeValue (id::SESSION, id::outputScanActive, 1);
}

void State::setOutputBlockEnd (cell row) noexcept
{
    storeValue (id::SESSION, id::outputBlockBottom, row.value);
    storeValue (id::SESSION, id::outputScanActive, 0);
}

void State::extendOutputBlock (cell row) noexcept
{
    if (params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::outputScanActive)->load() != 0)
    {
        storeValue (id::SESSION, id::outputBlockBottom, row.value);
    }
}

void State::setPromptRow (cell row) noexcept { storeValue (id::SESSION, id::promptRow, row.value); }

cell State::getOutputBlockTop() const noexcept { return cell (getSessionParamInt (get(), id::outputBlockTop, -1)); }

cell State::getOutputBlockBottom() const noexcept
{
    return cell (getSessionParamInt (get(), id::outputBlockBottom, -1));
}

cell State::getPromptRow() const noexcept { return cell (getSessionParamInt (get(), id::promptRow, -1)); }

bool State::hasOutputBlock() const noexcept
{
    const cell blockTop { getOutputBlockTop() };
    const cell prompt { getPromptRow() };
    const int screenVal { getActiveScreen() };
    const bool normalScreen { screenVal == ScreenMap::normal };

    return blockTop.value >= 0 and prompt > blockTop and normalScreen;
}

//==========================================================================
// Shell exit signal
//==========================================================================

void State::setShellExited (bool exited) noexcept { storeValue (id::SESSION, id::shellExited, exited ? 1 : 0); }

bool State::getShellExited() const noexcept { return getSessionParamInt (get(), id::shellExited) != 0; }

//==========================================================================
// Snapshot signal
//==========================================================================

void State::setSnapshotDirty() noexcept
{
    if (params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::pasteEchoRemaining)->load() <= 0)
    {
        params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->storeRelease (1);
    }
}

bool State::consumeSnapshotDirty() noexcept
{
    return params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->exchangeAcquire (0) != 0;
}

bool State::isSnapshotDirty() const noexcept { return getSessionParamInt (get(), id::snapshotDirty) != 0; }

//==========================================================================
// Clear buffer signal
//==========================================================================

void State::setClearBuffer() noexcept { storeValue (id::SESSION, id::clearBuffer, 1); }

bool State::getClearBuffer() const noexcept { return getSessionParamInt (get(), id::clearBuffer) != 0; }

//==========================================================================
// Paste echo gate
//==========================================================================

void State::setPasteEchoGate (int bytes) noexcept
{
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::pasteEchoRemaining)->storeRelease (bytes);
}

void State::consumePasteEcho (int bytes) noexcept
{
    auto* gate { params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::pasteEchoRemaining) };

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
    auto* gate { params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::pasteEchoRemaining) };

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
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::syncOutputActive)->storeRelease (active ? 1 : 0);

    if (not active)
    {
        setSnapshotDirty();
    }
}

bool State::isSyncOutputActive() const noexcept
{
    return params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::syncOutputActive)->load() != 0;
}

void State::requestSyncResize() noexcept
{
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::syncResizePending)->store (1);
}

bool State::consumeSyncResize() noexcept
{
    return params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::syncResizePending)->exchangeRelaxed (0) != 0;
}

//==========================================================================
// Preview
//==========================================================================

bool State::isPreviewActive() const noexcept { return getSessionParamInt (get(), id::preview) != 0; }

int State::getSplitCol() const noexcept { return getSessionParamInt (get(), id::splitCol); }

void State::dismissPreview() noexcept
{
    storeValue (id::SESSION, id::preview, 0);
    storeValue (id::SESSION, id::splitCol, 0);
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->storeRelease (1);
}

//==========================================================================
// Hints
//==========================================================================

void State::setHintPage (int page) noexcept { storeValue (id::SESSION, id::hintPage, page); }

int State::getHintPage() const noexcept { return getSessionParamInt (get(), id::hintPage); }

void State::setHintTotalPages (int total) noexcept { storeValue (id::SESSION, id::hintTotalPages, total); }

int State::getHintTotalPages() const noexcept { return getSessionParamInt (get(), id::hintTotalPages); }

//==========================================================================
// Modal
//==========================================================================

void State::setModalType (ModalType type) noexcept
{
    AppState::getContext()->setModalType (static_cast<int> (type));
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->storeRelease (1);
}

ModalType State::getModalType() const noexcept
{
    return static_cast<ModalType> (AppState::getContext()->getModalType());
}

bool State::isModal() const noexcept { return getModalType() != ModalType::none; }

//==========================================================================
// Per-screen atomic loaders — any thread, lock-free
// Called by Processor::id::screenSwitch handler on the reader thread.
//==========================================================================

int State::loadCursorRow (int s) const noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    return loadValue (screenId, id::cursorRow);
}

int State::loadCursorCol (int s) const noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    return loadValue (screenId, id::cursorCol);
}

bool State::loadCursorVisible (int s) const noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    return loadValue (screenId, id::cursorVisible) != 0;
}

uint32_t State::loadKeyboardFlags (int s) const noexcept
{
    const juce::Identifier screenId { ScreenMap::getContext()->get (s) };
    return static_cast<uint32_t> (loadValue (screenId, id::keyboardFlags));
}

//==========================================================================
// Dimension atomic loaders — any thread, lock-free
// Called by Processor::process() on the reader thread to detect layout changes.
//==========================================================================

cell State::loadCols() const noexcept { return cell (loadValue (id::SESSION, id::cols)); }
cell State::loadVisibleRows() const noexcept { return cell (loadValue (id::SESSION, id::visibleRows)); }
int State::loadCellWidth() const noexcept { return loadValue (id::DISPLAY, id::cellWidth); }
int State::loadCellHeight() const noexcept { return loadValue (id::DISPLAY, id::cellHeight); }
int State::loadWidth() const noexcept { return loadValue (id::SESSION, jam::ID::width); }
int State::loadHeight() const noexcept { return loadValue (id::SESSION, jam::ID::height); }

//==========================================================================
// Selection anchor adjustment — READER thread, lock-free
//==========================================================================

void State::adjustSelectionAnchors (int, int delta) noexcept
{
    const int selType { loadValue (jam::ID::textEditor, jam::ID::selectionType) };

    if (selType != static_cast<int> (terminal::SelectionType::none))
    {
        const int anchorRow { loadValue (jam::ID::textEditor, jam::ID::selectionAnchorRow) + delta };
        const int cursorRow { loadValue (jam::ID::textEditor, jam::ID::selectionCursorRow) + delta };

        if (anchorRow < 0 or cursorRow < 0)
        {
            storeValue (jam::ID::textEditor, jam::ID::selectionType,
                        static_cast<int> (terminal::SelectionType::none));
        }
        else
        {
            storeValue (jam::ID::textEditor, jam::ID::selectionAnchorRow, anchorRow);
            storeValue (jam::ID::textEditor, jam::ID::selectionCursorRow, cursorRow);
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
