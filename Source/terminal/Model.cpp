#include "Model.h"

namespace terminal
{
/*____________________________________________________________________________*/
using jam::Parameter;
using jam::ParameterText;

Model::Model()
    : jam::Model (id::SESSION)
{
    buildLayout (*jam::XML::getFromBinary (jam::IDref::parametersXml));
    startTimerHz (60);
}

Model::~Model() = default;

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

void Model::buildLayout (const juce::XmlElement& xml)
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
                // Explicit registration replacing the removed self-listener path.
                jam::ValueTree::Attachment::registerAtomics (*this, screenNode);
            }
        }
        else if (tag == id::TEXT.toString())
        {
            // TEXT parameter — ParameterText in SESSION group.
            const juce::Identifier textId { child->getStringAttribute (id::id.toString()) };
            const int maxlen              { child->getIntAttribute (id::maxlen.toString()) };

            addTextParameter (textId, rootNode, maxlen);
        }
    }
}

void Model::addTextParameter (const juce::Identifier& id, juce::ValueTree& rootNode, int maxlen) noexcept
{
    auto* sessionGroup { params.get<jam::AnyMap> (id::SESSION) };
    sessionGroup->add<ParameterText> (id, id, rootNode, id, maxlen, juce::String{});
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

//==========================================================================
// Reader-thread setters
//==========================================================================

void Model::setId (const juce::String& uuid) { state.setProperty (jam::ID::id, uuid, nullptr); }

void Model::setScreen (int s) noexcept { storeValue (id::SESSION, id::activeScreen, s); }

void Model::setMode (const juce::Identifier& id, bool v) noexcept { storeValue (id::MODES, id, v ? 1 : 0); }

void Model::setTitle (const char* src, int length) noexcept
{
    storeValue (id::SESSION, id::title, src, length);
}

void Model::setCwd (const char* src, int length) noexcept
{
    storeValue (id::SESSION, id::cwd, src, length);
}

void Model::setForegroundProcess (const char* src, int length) noexcept
{
    storeValue (id::SESSION, id::foregroundProcess, src, length);
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


//==========================================================================
// Preview
//==========================================================================

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

void Model::setHintTotalPages (int total) noexcept { storeValue (id::SESSION, id::hintTotalPages, total); }

//==========================================================================
// Modal
//==========================================================================


/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
