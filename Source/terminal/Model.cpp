#include "Model.h"

namespace terminal
{
/*____________________________________________________________________________*/

Model::Model (TextBuffer& tb)
    : jam::Model (id::SESSION)
    , textBuffer (tb)
{
    auto xml { jam::XML::getFromBinary (jam::IDref::parametersXml) };
    jassert (xml != nullptr);

    buildLayout (*xml, textBuffer);

    startTimerHz (60);

    listenedTree = state;
    listenedTree.addListener (this);
}

Model::~Model()
{
    listenedTree.removeListener (this);
}

//==========================================================================
// SSOT registration
//==========================================================================

int Model::resolveLayoutDefault (const juce::XmlElement& elem) noexcept
{
    const auto typeStr    { elem.getStringAttribute (id::type.toString()) };
    const auto defaultStr { elem.getStringAttribute (id::defaultValue.toString()) };
    int result { 0 };

    if (typeStr == id::boolType.toString())
    {
        result = Map::Bool::getContext()->get (defaultStr);
    }
    else
    {
        result = elem.getIntAttribute (id::defaultValue.toString());
    }

    return result;
}

void Model::buildLayout (const juce::XmlElement& xml, TextBuffer& tb)
{
    // All groups are nested AnyMaps so flush() can iterate uniformly.
    params.add<jam::AnyMap> (id::SESSION);
    params.add<jam::AnyMap> (id::MODES);

    // Root SESSION VT node — already constructed in Model (id::SESSION).
    juce::ValueTree rootNode { state };

    // MODES VT node — appended to SESSION.
    juce::ValueTree modesNode { id::MODES };
    rootNode.appendChild (modesNode, nullptr);

    // Walk XML, dispatch on tag name.
    for (auto* child : xml.getChildIterator())
    {
        const auto& tag { child->getTagName() };

        if (tag == jam::Model::PARAM.toString())
        {
            // Root-level parameter → SESSION group.
            auto* sessionGroup { params.get<jam::AnyMap> (id::SESSION) };
            const auto typeStr { child->getStringAttribute (id::type.toString()) };

            if (typeStr == app::id::floatType.toString())
            {
                addParameter<float> (juce::Identifier { child->getStringAttribute (id::id.toString()) },
                                     static_cast<float> (child->getDoubleAttribute (id::defaultValue.toString())),
                                     *sessionGroup,
                                     rootNode);
            }
            else
            {
                addParameter (juce::Identifier { child->getStringAttribute (id::id.toString()) },
                              resolveLayoutDefault (*child),
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
                              resolveLayoutDefault (*modeChild),
                              *modesGroup,
                              modesNode);
            }
        }
        else if (tag == id::SCREEN.toString())
        {
            // Per-screen parameters — create NORMAL and ALTERNATE nodes with identical params.
            for (int screenIndex { 0 }; screenIndex < Map::Screen::count; ++screenIndex)
            {
                const juce::Identifier screenId { Map::Screen::getContext()->get (screenIndex) };
                juce::ValueTree screenNode { screenId };

                params.add<jam::AnyMap> (screenId);
                auto* screenGroup { params.get<jam::AnyMap> (screenId) };

                for (auto* screenChild : child->getChildIterator())
                {
                    addParameter (juce::Identifier { screenChild->getStringAttribute (id::id.toString()) },
                                  resolveLayoutDefault (*screenChild),
                                  *screenGroup,
                                  screenNode);
                }

                rootNode.appendChild (screenNode, nullptr);
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

void Model::addTextParameter (const juce::Identifier& id, juce::ValueTree& rootNode) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (id::SESSION) };
    sessionGroup->add<Parameter<const char*>> (id, id, rootNode, id);
}

void Model::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent == state)
        registerNodeAtomics (child);
}

void Model::registerNodeAtomics (juce::ValueTree& node) noexcept
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

void Model::storeValue (const juce::Identifier& groupId, const juce::Identifier& paramId, int value) noexcept
{
    params.get<jam::AnyMap> (groupId)->get<Parameter<int>> (paramId)->store (value);
}

int Model::loadValue (const juce::Identifier& groupId, const juce::Identifier& paramId) const noexcept
{
    return params.get<jam::AnyMap> (groupId)->get<Parameter<int>> (paramId)->load();
}

//==========================================================================
// Per-row flush-dirty flags
//==========================================================================

void Model::setRowDirty (int row) noexcept
{
    jassert (row >= 0 and row < rowDirtyCount);

    if (row >= 0 and row < rowDirtyCount)
        rowDirtyFlags[static_cast<size_t> (row)].store (1, std::memory_order_relaxed);
}

bool Model::consumeRowDirty (int row) noexcept
{
    jassert (row >= 0 and row < rowDirtyCount);

    if (row >= 0 and row < rowDirtyCount)
        return rowDirtyFlags[static_cast<size_t> (row)].exchange (0, std::memory_order_relaxed) != 0;

    return false;
}

void Model::rebuildRowDirtyFlags (int newVisibleRows) noexcept
{
    rowDirtyFlags = std::make_unique<std::atomic<int>[]> (static_cast<size_t> (newVisibleRows));
    rowDirtyCount = newVisibleRows;
}

void Model::storeTextValue (const juce::Identifier& groupId, const juce::Identifier& paramId, const char* ptr) noexcept
{
    params.get<jam::AnyMap> (groupId)->get<Parameter<const char*>> (paramId)->store (ptr);
}

//==========================================================================
// ValueTree access — MESSAGE THREAD
//==========================================================================

bool Model::getMode (const juce::Identifier& id) const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    auto modesNode { state.getChildWithName (id::MODES) };
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (modesNode, id).getValue()) != 0;
}

int Model::getActiveScreen() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return static_cast<int> (jam::ValueTree::getValueFromChildWithID (state, id::activeScreen).getValue());
}

static int
getSessionParamInt (const juce::ValueTree& root, const juce::Identifier& paramId, int defaultValue = 0) noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    auto param { jam::ValueTree::getChildWithID (root, paramId.toString()) };
    int result { defaultValue };

    if (param.isValid())
    {
        result = static_cast<int> (param.getProperty (terminal::id::value));
    }

    return result;
}

cell Model::getCols() const noexcept
{
    const auto teNode { state.getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };
    const int64_t packed { static_cast<int64_t> (teNode.getProperty (jam::CodeView::properties.at (jam::CodeView::viewportId), 0)) };
    return jam::Cell::Rectangle::unpack (packed).getWidth();
}

cell Model::getVisibleRows() const noexcept
{
    const auto teNode { state.getChildWithName (jam::CodeView::properties.at (jam::CodeView::codeViewId)) };
    const int64_t packed { static_cast<int64_t> (teNode.getProperty (jam::CodeView::properties.at (jam::CodeView::viewportId), 0)) };
    return jam::Cell::Rectangle::unpack (packed).getHeight();
}

juce::String Model::getTitle() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return state.getProperty (id::title).toString();
}

juce::String Model::getCwd() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return state.getProperty (id::cwd).toString();
}

juce::String Model::getForegroundProcess() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return state.getProperty (id::foregroundProcess).toString();
}

//==========================================================================
// Reader-thread setters
//==========================================================================

void Model::setId (const juce::String& uuid) { state.setProperty (jam::ID::id, uuid, nullptr); }

void Model::setScreen (int s) noexcept { storeValue (id::SESSION, id::activeScreen, s); }

void Model::setMode (const juce::Identifier& id, bool v) noexcept { storeValue (id::MODES, id, v ? 1 : 0); }

void Model::setTitle (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (id::title, src, length) };
    storeTextValue (id::SESSION, id::title, p);
}

void Model::setCwd (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (id::cwd, src, length) };
    storeTextValue (id::SESSION, id::cwd, p);
}

void Model::setForegroundProcess (const char* src, int length) noexcept
{
    auto* p { textBuffer.write (id::foregroundProcess, src, length) };
    storeTextValue (id::SESSION, id::foregroundProcess, p);
}

//==========================================================================
// OSC 133 shell integration
//==========================================================================

void Model::setOutputBlockStart (cell row) noexcept
{
    storeValue (id::SESSION, id::outputBlockTop, row.value);
    storeValue (id::SESSION, id::outputBlockBottom, row.value);
    storeValue (id::SESSION, id::outputScanActive, 1);
}

void Model::setOutputBlockEnd (cell row) noexcept
{
    storeValue (id::SESSION, id::outputBlockBottom, row.value);
    storeValue (id::SESSION, id::outputScanActive, 0);
}

void Model::extendOutputBlock (cell row) noexcept
{
    if (params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::outputScanActive)->load() != 0)
    {
        storeValue (id::SESSION, id::outputBlockBottom, row.value);
    }
}

void Model::setPromptRow (cell row) noexcept { storeValue (id::SESSION, id::promptRow, row.value); }

cell Model::getOutputBlockTop() const noexcept { return cell (getSessionParamInt (state, id::outputBlockTop, -1)); }

cell Model::getOutputBlockBottom() const noexcept
{
    return cell (getSessionParamInt (state, id::outputBlockBottom, -1));
}

cell Model::getPromptRow() const noexcept { return cell (getSessionParamInt (state, id::promptRow, -1)); }

bool Model::hasOutputBlock() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    const cell blockTop { getOutputBlockTop() };
    const cell prompt { getPromptRow() };
    const int screenVal { getActiveScreen() };
    const bool normalScreen { screenVal == Map::Screen::normal };

    return blockTop.value >= 0 and prompt > blockTop and normalScreen;
}

//==========================================================================
// Shell exit signal
//==========================================================================

void Model::setShellExited (bool exited) noexcept { storeValue (id::SESSION, id::shellExited, exited ? 1 : 0); }

//==========================================================================
// Snapshot signal
//==========================================================================

void Model::setSnapshotDirty() noexcept
{
    if (params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::pasteEchoRemaining)->load() <= 0)
    {
        params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->storeRelease (1);
    }
}

bool Model::consumeSnapshotDirty() noexcept
{
    return params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->exchangeAcquire (0) != 0;
}

//==========================================================================
// Paste echo gate
//==========================================================================

void Model::setPasteEchoGate (int bytes) noexcept
{
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::pasteEchoRemaining)->storeRelease (bytes);
}

void Model::consumePasteEcho (int bytes) noexcept
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

void Model::clearPasteEchoGate() noexcept
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

void Model::setSyncOutput (bool active) noexcept
{
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::syncOutputActive)->storeRelease (active ? 1 : 0);

    if (not active)
    {
        setSnapshotDirty();
    }
}

bool Model::isSyncOutputActive() const noexcept
{
    return params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::syncOutputActive)->load() != 0;
}

//==========================================================================
// Preview
//==========================================================================

bool Model::isPreviewActive() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return getSessionParamInt (state, id::preview) != 0;
}

int Model::getSplitCol() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return getSessionParamInt (state, id::splitCol);
}

void Model::dismissPreview() noexcept
{
    storeValue (id::SESSION, id::preview, 0);
    storeValue (id::SESSION, id::splitCol, 0);
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->storeRelease (1);
}

//==========================================================================
// Hints
//==========================================================================

void Model::setHintPage (int page) noexcept { storeValue (id::SESSION, id::hintPage, page); }

int Model::getHintPage() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return getSessionParamInt (state, id::hintPage);
}

void Model::setHintTotalPages (int total) noexcept { storeValue (id::SESSION, id::hintTotalPages, total); }

int Model::getHintTotalPages() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return getSessionParamInt (state, id::hintTotalPages);
}

//==========================================================================
// Modal
//==========================================================================

void Model::setModalType (ModalType type) noexcept
{
    AppModel::getContext()->setModalType (static_cast<int> (type));
    params.get<jam::AnyMap> (id::SESSION)->get<Parameter<int>> (id::snapshotDirty)->storeRelease (1);
}

ModalType Model::getModalType() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return static_cast<ModalType> (AppModel::getContext()->getModalType());
}

bool Model::isModal() const noexcept
{
    JUCE_ASSERT_MESSAGE_THREAD
    return getModalType() != ModalType::none;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
