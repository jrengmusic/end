/**
 * @file Panes.cpp
 * @brief terminal::Panes implementation — pane lifecycle and layout management.
 *
 * @see Panes.h
 * @see terminal::Display
 * @see terminal::Tabs
 */

#include "Panes.h"

namespace terminal
{
/*____________________________________________________________________________*/
/**
 * @brief Constructs the pane container.
 *
 * @note MESSAGE THREAD.
 */
Panes::Panes()
{
    setOpaque (false);
}

/**
 * @brief Destructor.
 *
 * @note MESSAGE THREAD.
 */
Panes::~Panes() = default;

// =============================================================================

std::pair<cell, cell> Panes::cellsFromRect (juce::Rectangle<int> paneRect) noexcept
{
    // Physical-pixel math — matches Screen::calc() exactly (SSOT).
    const auto* appState { AppModel::getContext() };
    const float scale { jam::Typeface::getDisplayScale() };
    const float fontSize { appState->dpiCorrectedFontSize() };
    const float cellWidth  { appState->getValue<float> (app::id::DISPLAY_LUA, app::id::cellWidth) };
    const float lineHeight { appState->getValue<float> (app::id::DISPLAY_LUA, app::id::lineHeight) };
    const jam::Font font { appState->getValue<juce::String> (app::id::DISPLAY_LUA, app::id::fontFamily), fontSize, cellWidth, lineHeight };
    jassert (font.cellWidth > 0 and font.cellHeight > 0);

    const int physCellW { jam::toInt (static_cast<float> (font.cellWidth) * scale, true) };
    const int physCellH { jam::toInt (static_cast<float> (font.cellHeight) * scale, true) };

    jassert (physCellW > 0 and physCellH > 0);

    const int paddingTop    { appState->getValue<int> (app::id::NEXUS_LUA, app::id::paddingTop) };
    const int paddingRight  { appState->getValue<int> (app::id::NEXUS_LUA, app::id::paddingRight) };
    const int paddingBottom { appState->getValue<int> (app::id::NEXUS_LUA, app::id::paddingBottom) };
    const int paddingLeft   { appState->getValue<int> (app::id::NEXUS_LUA, app::id::paddingLeft) };

    const int contentW { paneRect.getWidth()  - paddingLeft - paddingRight };
    const int contentH { paneRect.getHeight() - paddingTop  - paddingBottom };

    const int physContentW { jam::toInt (static_cast<float> (contentW) * scale, true) };
    const int physContentH { jam::toInt (static_cast<float> (contentH) * scale, true) };

    const auto gridRect { jam::Cell::Rectangle::fromPixel (juce::Rectangle<int> { 0, 0, physContentW, physContentH }, physCellW, physCellH) };
    const cell cols { (physContentW > 0 and physCellW > 0) ? gridRect.getWidth().value  : 1 };
    const cell rows { (physContentH > 0 and physCellH > 0) ? gridRect.getHeight().value : 1 };

    jassert (cols.value > 0 and rows.value > 0);
    return { cols, rows };
}

/**
 * @brief Splits a pixel rect by direction and ratio.
 */
std::pair<juce::Rectangle<int>, juce::Rectangle<int>>
    Panes::splitRect (juce::Rectangle<int> parent,
                      const juce::String& direction,
                      double ratio) noexcept
{
    juce::Rectangle<int> targetRect;
    juce::Rectangle<int> newRect;

    if (direction == Map::Direction::getContext()->get (Map::Direction::vertical))
    {
        const int totalWidth  { parent.getWidth() };
        const int firstWidth  { juce::roundToInt (static_cast<double> (totalWidth - jam::PaneManager::resizerBarSize) * ratio) };
        const int secondWidth { totalWidth - firstWidth - jam::PaneManager::resizerBarSize };

        targetRect = parent.withWidth (firstWidth);
        newRect    = parent.withX (parent.getX() + firstWidth + jam::PaneManager::resizerBarSize).withWidth (secondWidth);
    }
    else
    {
        const int totalHeight  { parent.getHeight() };
        const int firstHeight  { juce::roundToInt (static_cast<double> (totalHeight - jam::PaneManager::resizerBarSize) * ratio) };
        const int secondHeight { totalHeight - firstHeight - jam::PaneManager::resizerBarSize };

        targetRect = parent.withHeight (firstHeight);
        newRect    = parent.withY (parent.getY() + firstHeight + jam::PaneManager::resizerBarSize).withHeight (secondHeight);
    }

    return { targetRect, newRect };
}

// =============================================================================

juce::String Panes::createTerminal (const juce::String& workingDirectory,
                                     const juce::String& uuid,
                                     jam::Cell::Rectangle dims)
{
    jassert (dims.getWidth().value > 0 and dims.getHeight().value > 0);

    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    terminal::Session& termSession { Nexus::getContext()->create (workingDirectory, effectiveUuid, dims) };

    const juce::String termUuid { termSession.getProcessor().getUuid() };

    auto pane { std::make_unique<terminal::Display> (termSession) };
    pane->setComponentID (termSession.getProcessor().getUuid());

    jassert (pane != nullptr);

    auto* term { static_cast<terminal::Display*> (pane.get()) };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    paneManager.addLeaf (termUuid);

    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), termUuid) };
    jassert (paneNode.isValid());
    // Graft SESSION tree into PANE node — Session owns the Attachment (RAII ungraft on destroy).
    termSession.graftInto (paneNode);

    // Open the TTY after Display/Screen are constructed and the session ValueTree
    // is grafted, so all screen node atomics exist before the reader thread fires.
    termSession.start();

    if (isShowing())
        term->setVisible (true);

    panes.add (std::move (pane));

    return termUuid;
}

/**
 * @brief Creates a new Whelmed markdown viewer pane and opens the given file.
 *
 * @param file  The .md file to open.
 * @return The UUID of the newly created Whelmed pane (its componentID).
 * @note MESSAGE THREAD.
 */
juce::String Panes::createWhelmed (const juce::File& file)
{
    const juce::String activeID { AppModel::getContext()->getActivePaneID() };
    jassert (activeID.isNotEmpty());

    // Find the active terminal
    PaneView* activeTerminal { nullptr };

    for (auto& pane : panes)
    {
        if (pane->getComponentID() == activeID and pane->getPaneType() == Map::PaneType::getContext()->get (Map::PaneType::terminal))
            activeTerminal = pane.get();
    }

    jassert (activeTerminal != nullptr);

    activeTerminal->setVisible (false);

    auto component { std::make_unique<whelmed::Component>() };
    component->setComponentID (activeID);
    component->setBounds (activeTerminal->getBounds());
    component->openFile (file);

    // Graft DOCUMENT alongside SESSION in the PANE node — Component owns Attachment (RAII ungraft on destroy).
    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), activeID) };
    jassert (paneNode.isValid());
    component->graftDocumentInto (paneNode);

    if (isShowing())
    {
        addAndMakeVisible (component.get());
        component->grabKeyboardFocus();
    }

    panes.add (std::move (component));

    AppModel::getContext()->setModalType (0);
    AppModel::getContext()->setSelectionType (0);
    AppModel::getContext()->setActivePaneType (Map::PaneType::getContext()->get (Map::PaneType::document));
    resized();

    return activeID;
}

void Panes::closeWhelmed()
{
    const juce::String activeID { AppModel::getContext()->getActivePaneID() };
    jassert (activeID.isNotEmpty());

    // Find whelmed and terminal with matching UUID
    PaneView* whelmedPane { nullptr };
    PaneView* terminalPane { nullptr };
    size_t whelmedIndex { 0 };

    for (size_t i { 0 }; i < panes.size(); ++i)
    {
        if (panes.at (i)->getComponentID() == activeID)
        {
            if (panes.at (i)->getPaneType() == Map::PaneType::getContext()->get (Map::PaneType::document))
            {
                whelmedPane = panes.at (i).get();
                whelmedIndex = i;
            }

            if (panes.at (i)->getPaneType() == Map::PaneType::getContext()->get (Map::PaneType::terminal))
                terminalPane = panes.at (i).get();
        }
    }

    jassert (whelmedPane != nullptr);
    jassert (terminalPane != nullptr);

    // DOCUMENT is ungrafted automatically when whelmedPane is destroyed via panes.erase below
    // (whelmed::Component owns the documentAttachment — RAII ungraft on destruction).
    removeChildComponent (whelmedPane);
    panes.erase (panes.begin() + static_cast<int> (whelmedIndex));

    terminalPane->setVisible (true);

    if (isShowing())
        terminalPane->grabKeyboardFocus();

    AppModel::getContext()->setModalType (0);
    AppModel::getContext()->setSelectionType (0);
    AppModel::getContext()->setActivePaneType (Map::PaneType::getContext()->get (Map::PaneType::terminal));
}

/**
 * @brief Wires a terminal's repaint callback after creation.
 *
 * @param terminal  The terminal to wire.
 * @note MESSAGE THREAD.
 */
void Panes::setTerminalCallbacks (terminal::Display* terminal)
{
    const juce::String terminalUuid { terminal->getComponentID() };
    sessionStateTrees[terminalUuid] = terminal->getProcessor().getState().getRootTree();
    sessionStateTrees[terminalUuid].addListener (this);
}

/**
 * @brief Returns true when all pane components have been closed.
 *
 * @return true when the pane owner is empty.
 * @note MESSAGE THREAD.
 */
bool Panes::isEmpty() const noexcept { return panes.isEmpty(); }

/**
 * @brief Returns the owned pane container.
 *
 * @return Reference to the pane owner container.
 * @note MESSAGE THREAD.
 */
jam::Owner<PaneView>& Panes::getPanes() noexcept { return panes; }

/**
 * @brief Returns the PANES ValueTree owned by PaneManager.
 *
 * @return Reference to the PANES ValueTree.
 * @note MESSAGE THREAD.
 */
juce::ValueTree& Panes::getState() noexcept { return paneManager.getState(); }

void Panes::closePane (const juce::String& uuid)
{
    if (sessionStateTrees.count (uuid) > 0)
    {
        sessionStateTrees[uuid].removeListener (this);
        sessionStateTrees.erase (uuid);
    }

    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), uuid) };
    jassert (paneNode.isValid());

    auto splitNode { paneNode.getParent() };

    // Erase the Display first — ~Display() unwires all Session callbacks.
    // Session is removed after the Display is destroyed.
    // SESSION tree is ungrafted automatically when Session is destroyed via Nexus::remove (RAII Attachment).
    for (auto it { panes.begin() }; it != panes.end(); ++it)
    {
        if ((*it)->getComponentID() == uuid)
        {
            removeChildComponent (it->get());
            panes.erase (it);
            break;
        }
    }

    for (auto it { resizerBars.begin() }; it != resizerBars.end(); ++it)
    {
        if ((*it)->getSplitNode() == splitNode)
        {
            removeChildComponent (it->get());
            resizerBars.erase (it);
            break;
        }
    }

    if (not panes.isEmpty())
    {
        paneManager.remove (uuid);
    }
    else
    {
        // Last pane — remove the leaf node directly from the tree.
        // PaneManager::remove() asserts on last-leaf (no sibling to promote).
        // This child removal fires valueTreeChildRemoved on Tabs, which closes the tab.
        auto leaf { jam::PaneManager::findLeaf (paneManager.getState(), uuid) };

        if (leaf.isValid())
            paneManager.getState().removeChild (leaf, nullptr);
    }

    for (auto it { resizerBars.begin() }; it != resizerBars.end();)
    {
        if (not (*it)->getSplitNode().getParent().isValid())
        {
            removeChildComponent (it->get());
            it = resizerBars.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // Nexus::remove() handles session teardown — Display already erased above.
    Nexus::getContext()->remove (uuid);

    if (not panes.isEmpty())
        resized();
}

/**
 * @brief Splits the active pane into side-by-side columns.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitHorizontal() { splitActive (Map::Direction::getContext()->get (Map::Direction::vertical),   true,  0.5); }

/**
 * @brief Splits the active pane into stacked rows.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitVertical() { splitActive (Map::Direction::getContext()->get (Map::Direction::horizontal), false, 0.5); }

void Panes::splitActiveWithRatio (const juce::String& direction, bool isVertical, double ratio)
{
    splitActive (direction, isVertical, ratio);
}

void Panes::splitAt (const juce::String& targetUuid,
                     const juce::String& newUuid,
                     const juce::String& cwd,
                     const juce::String& direction,
                     bool isVertical,
                     cell cols,
                     cell rows,
                     double ratio)
{
    jassert (targetUuid.isNotEmpty());
    jassert (cols.value > 0 and rows.value > 0);

    const juce::String effectiveSplitUuid { newUuid.isNotEmpty() ? newUuid : juce::Uuid().toString() };
    terminal::Session& splitSession { Nexus::getContext()->create (cwd, effectiveSplitUuid, jam::Cell::Rectangle (cols, rows)) };

    const juce::String splitUuid { splitSession.getProcessor().getUuid() };

    auto pane { std::make_unique<terminal::Display> (splitSession) };
    pane->setComponentID (splitSession.getProcessor().getUuid());

    jassert (pane != nullptr);

    auto* term { static_cast<terminal::Display*> (pane.get()) };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    paneManager.split (targetUuid, splitUuid, direction, ratio);

    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), splitUuid) };
    jassert (paneNode.isValid());
    // Graft SESSION tree into PANE node — Session owns the Attachment (RAII ungraft on destroy).
    splitSession.graftInto (paneNode);

    // Open the TTY after Display/Screen are constructed and the session ValueTree
    // is grafted, so all screen node atomics exist before the reader thread fires.
    splitSession.start();

    auto splitNode { paneNode.getParent() };
    jassert (splitNode.isValid());

    panes.add (std::move (pane));

    resizerBars.add (std::make_unique<jam::PaneResizerBar> (&paneManager, splitNode, isVertical));
    addAndMakeVisible (resizerBars.back().get());

    if (isShowing())
        term->setVisible (true);

    resized();
}

void Panes::splitActive (const juce::String& direction, bool isVertical, double ratio)
{
    const juce::String activeID { AppModel::getContext()->getActivePaneID() };
    jassert (activeID.isNotEmpty());

    // Find the active pane's current pixel bounds. At runtime the Panes instance
    // is fully laid out, so the pane component has valid bounds.
    // Initialise to full Panes bounds as fallback (used if the active pane is not found,
    // which should not happen in normal operation).
    juce::Rectangle<int> activeBounds { getLocalBounds() };

    for (const auto& pane : panes)
    {
        if (pane->getComponentID() == activeID and pane->isVisible())
            activeBounds = pane->getBounds();
    }

    const auto [targetRect, newRect] { splitRect (activeBounds, direction, ratio) };
    const auto [newCols, newRows] { cellsFromRect (newRect) };

    splitAt (activeID, {}, AppModel::getContext()->getPwd(), direction, isVertical, newCols, newRows, ratio);
}

/**
 * @brief Lays out all terminals and resizer bars via PaneManager.
 *
 * @note MESSAGE THREAD.
 */
void Panes::resized()
{
    jam::PaneManager::layout (paneManager.getState(), getLocalBounds(), panes, resizerBars);
}

/**
 * @brief Propagates visibility to child terminals.
 *
 * @note MESSAGE THREAD.
 */
void Panes::visibilityChanged()
{
    const bool visible { isVisible() };

    for (auto& pane : panes)
    {
        if (visible and pane->getPaneType() == Map::PaneType::getContext()->get (Map::PaneType::terminal))
        {
            auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), pane->getComponentID()) };
            const bool hasDocument { paneNode.isValid() and paneNode.getChildWithName (app::id::DOCUMENT).isValid() };

            if (not hasDocument)
                pane->setVisible (true);
        }
        else
        {
            pane->setVisible (visible);
        }
    }
}

/**
 * @brief Reacts to State ValueTree property changes on any registered terminal.
 *
 * Registered on each terminal's State ValueTree in setTerminalCallbacks().
 * Detects shellExited by checking for a PARAM node whose id property equals
 * terminal::id::shellExited with value 1.  Defers pane closure via callAsync
 * to avoid destroying a component from within its own listener callback chain
 * (the VT callback fires from flush() which fires from the timer-driven flush on the message thread).
 *
 * @note MESSAGE THREAD.
 */
void Panes::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == terminal::id::value
        and tree.getType() == jam::Model::PARAM
        and tree.getProperty (terminal::id::id).toString() == terminal::id::shellExited.toString()
        and static_cast<int> (tree.getProperty (terminal::id::value)) == 1)
    {
        const juce::ValueTree sessionRoot { tree.getParent() };

        juce::String exitedUuid;

        for (const auto& [uuid, tree] : sessionStateTrees)
        {
            if (tree == sessionRoot)
            {
                exitedUuid = uuid;
                break;
            }
        }

        if (exitedUuid.isNotEmpty())
        {
            juce::MessageManager::callAsync ([this, exitedUuid]
            {
                int closedIndex { 0 };

                for (size_t i { 0 }; i < panes.size(); ++i)
                {
                    if (panes.at (i)->getComponentID() == exitedUuid)
                    {
                        closedIndex = static_cast<int> (i);
                        break;
                    }
                }

                closePane (exitedUuid);

                if (not panes.isEmpty())
                {
                    const int nextIndex { juce::jmin (closedIndex, static_cast<int> (panes.size()) - 1) };
                    auto* nearest { panes.at (static_cast<size_t> (nextIndex)).get() };
                    AppModel::getContext()->setModalType (0);
                    AppModel::getContext()->setSelectionType (0);
                    AppModel::getContext()->setActivePaneID (nearest->getComponentID());

                    if (nearest->isShowing())
                        nearest->grabKeyboardFocus();
                }
                // When all panes in this tab are closed, Tabs detects the empty
                // PANES VT via valueTreeChildRemoved and closes the active tab.
            });
        }
    }
}

void Panes::focusPane (int deltaX, int deltaY)
{
    const auto activeID { AppModel::getContext()->getActivePaneID() };
    PaneView* active { nullptr };

    for (auto& pane : panes)
    {
        if (pane->getComponentID() == activeID and pane->isVisible())
        {
            active = pane.get();
        }
    }

    if (active != nullptr)
    {
        const auto activeCentre { active->getBounds().getCentre() };

        PaneView* best { nullptr };
        int bestDistance { std::numeric_limits<int>::max() };

        for (auto& pane : panes)
        {
            if (pane.get() != active and pane->isVisible())
            {
                const auto centre { pane->getBounds().getCentre() };

                const bool inDirection { (deltaX < 0 and centre.getX() < activeCentre.getX())
                                         or (deltaX > 0 and centre.getX() > activeCentre.getX())
                                         or (deltaY < 0 and centre.getY() < activeCentre.getY())
                                         or (deltaY > 0 and centre.getY() > activeCentre.getY()) };

                if (inDirection)
                {
                    const int dx { centre.getX() - activeCentre.getX() };
                    const int dy { centre.getY() - activeCentre.getY() };
                    const int distance { dx * dx + dy * dy };

                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        best = pane.get();
                    }
                }
            }
        }

        if (best != nullptr)
        {
            AppModel::getContext()->setModalType (0);
            AppModel::getContext()->setSelectionType (0);
            AppModel::getContext()->setActivePaneID (best->getComponentID());

            if (best->isShowing())
                best->grabKeyboardFocus();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
