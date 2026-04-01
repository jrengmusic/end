/**
 * @file Panes.cpp
 * @brief Terminal::Panes implementation — pane lifecycle and layout management.
 *
 * @see Panes.h
 * @see Terminal::Component
 * @see Terminal::Tabs
 */

#include "Panes.h"
#include "../AppState.h"
#include "../terminal/data/Identifier.h"
#include "../whelmed/Component.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @brief Constructs the pane container.
 *
 * @note MESSAGE THREAD.
 */
Panes::Panes (jreng::Typeface& font_, jreng::Typeface& whelmedBodyFont_, jreng::Typeface& whelmedCodeFont_)
    : font (font_)
    , whelmedBodyFont (whelmedBodyFont_)
    , whelmedCodeFont (whelmedCodeFont_)
{
    setOpaque (false);
}

/**
 * @brief Destructor.
 *
 * @note MESSAGE THREAD.
 */
Panes::~Panes() = default;

/**
 * @brief Creates a new terminal session as the first (or only) leaf in this pane.
 *
 * Creates a Terminal::Component via Terminal::Component::create(), wires its
 * callbacks, registers it with PaneManager via addLeaf, and grafts its SESSION
 * ValueTree into the corresponding PANE node.
 *
 * @param workingDirectory  Initial cwd for the shell. Empty = inherit parent cwd.
 * @return The UUID of the newly created terminal (its componentID).
 * @note MESSAGE THREAD.
 */
juce::String Panes::createTerminal (const juce::String& workingDirectory)
{
    auto terminal { Terminal::Component::create (font, workingDirectory) };
    auto* term { terminal.get() };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    const juce::String uuid { term->getComponentID() };
    paneManager.addLeaf (uuid);

    auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), uuid) };
    jassert (paneNode.isValid());
    paneNode.appendChild (term->getValueTree(), nullptr);

    if (isShowing())
        term->setVisible (true);

    panes.add (std::move (terminal));
    return uuid;
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
    const juce::String activeID { AppState::getContext()->getActivePaneID() };
    jassert (activeID.isNotEmpty());

    // Find the active terminal
    PaneComponent* activeTerminal { nullptr };

    for (auto& pane : panes)
    {
        if (pane->getComponentID() == activeID and pane->getPaneType() == "terminal")
            activeTerminal = pane.get();
    }

    jassert (activeTerminal != nullptr);

    if (activeTerminal != nullptr)
    {
        activeTerminal->setVisible (false);

        auto component { std::make_unique<Whelmed::Component>() };
        component->setComponentID (activeID);
        component->setBounds (activeTerminal->getBounds());
        component->openFile (file);
        component->onRepaintNeeded = onRepaintNeeded;

        // Graft DOCUMENT alongside SESSION in the PANE node
        auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), activeID) };
        jassert (paneNode.isValid());

        auto valueTree { component->getValueTree() };
        jassert (valueTree.isValid());
        paneNode.appendChild (valueTree, nullptr);

        if (isShowing())
        {
            addAndMakeVisible (component.get());
            component->grabKeyboardFocus();
        }

        panes.add (std::move (component));

        AppState::getContext()->setModalType (0);
        AppState::getContext()->setSelectionType (0);
        AppState::getContext()->setActivePaneType ("document");
        resized();
    }

    return activeID;
}

void Panes::closeWhelmed()
{
    const juce::String activeID { AppState::getContext()->getActivePaneID() };
    jassert (activeID.isNotEmpty());

    // Find whelmed and terminal with matching UUID
    PaneComponent* whelmedPane { nullptr };
    PaneComponent* terminalPane { nullptr };
    size_t whelmedIndex { 0 };

    for (size_t i { 0 }; i < panes.size(); ++i)
    {
        if (panes.at (i)->getComponentID() == activeID)
        {
            if (panes.at (i)->getPaneType() == "document")
            {
                whelmedPane = panes.at (i).get();
                whelmedIndex = i;
            }

            if (panes.at (i)->getPaneType() == "terminal")
                terminalPane = panes.at (i).get();
        }
    }

    jassert (whelmedPane != nullptr);
    jassert (terminalPane != nullptr);

    if (whelmedPane != nullptr and terminalPane != nullptr)
    {
        // Remove DOCUMENT from PANE node
        auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), activeID) };
        jassert (paneNode.isValid());

        auto documentTree { whelmedPane->getValueTree() };

        if (documentTree.isValid())
            paneNode.removeChild (documentTree, nullptr);

        removeChildComponent (whelmedPane);
        panes.erase (panes.begin() + static_cast<int> (whelmedIndex));

        terminalPane->setVisible (true);

        if (isShowing())
            terminalPane->grabKeyboardFocus();

        AppState::getContext()->setModalType (0);
        AppState::getContext()->setSelectionType (0);
        AppState::getContext()->setActivePaneType ("terminal");
    }
}

/**
 * @brief Wires a terminal's repaint callback after creation.
 *
 * @param terminal  The terminal to wire.
 * @note MESSAGE THREAD.
 */
void Panes::setTerminalCallbacks (Terminal::Component* terminal)
{
    jassert (onRepaintNeeded != nullptr);
    terminal->onRepaintNeeded = onRepaintNeeded;

    terminal->onOpenMarkdown = [this] (const juce::File& file)
    {
        if (onOpenMarkdown != nullptr)
            onOpenMarkdown (file);
    };

    terminal->onShellExited = [this, uuid = terminal->getComponentID()]
    {
        int closedIndex { 0 };

        for (size_t i { 0 }; i < panes.size(); ++i)
        {
            if (panes.at (i)->getComponentID() == uuid)
            {
                closedIndex = static_cast<int> (i);
                break;
            }
        }

        closePane (uuid);

        if (not panes.isEmpty())
        {
            const int nextIndex { juce::jmin (closedIndex, static_cast<int> (panes.size()) - 1) };
            auto* nearest { panes.at (static_cast<size_t> (nextIndex)).get() };
            AppState::getContext()->setActivePaneID (nearest->getComponentID());

            if (nearest->isShowing())
                nearest->grabKeyboardFocus();
        }
        else
        {
            if (onLastPaneClosed != nullptr)
                onLastPaneClosed();
        }
    };
}

/**
 * @brief Returns the owned pane container.
 *
 * @return Reference to the pane owner container.
 * @note MESSAGE THREAD.
 */
jreng::Owner<PaneComponent>& Panes::getPanes() noexcept { return panes; }

/**
 * @brief Returns the PANES ValueTree owned by PaneManager.
 *
 * @return Reference to the PANES ValueTree.
 * @note MESSAGE THREAD.
 */
juce::ValueTree& Panes::getState() noexcept { return paneManager.getState(); }

/**
 * @brief Closes the pane with the given uuid.
 *
 * @param uuid The componentID of the terminal to close.
 * @note MESSAGE THREAD.
 */
void Panes::closePane (const juce::String& uuid)
{
    auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), uuid) };
    jassert (paneNode.isValid());

    auto splitNode { paneNode.getParent() };
    paneNode.removeAllChildren (nullptr);

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

    paneManager.remove (uuid);

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

    resized();
}

/**
 * @brief Splits the active pane into side-by-side columns.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitHorizontal() { splitImpl ("vertical", true); }

/**
 * @brief Splits the active pane into stacked rows.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitVertical() { splitImpl ("horizontal", false); }

/**
 * @brief Shared split implementation.
 *
 * @param direction  PaneManager direction string: "vertical" = left/right
 *                   divider; "horizontal" = top/bottom divider.
 * @param isVertical True when the resizer bar is vertical (splitHorizontal).
 * @note MESSAGE THREAD.
 */
void Panes::splitImpl (const juce::String& direction, bool isVertical)
{
    const juce::String activeID { AppState::getContext()->getActivePaneID() };
    jassert (activeID.isNotEmpty());

    auto terminal { Terminal::Component::create (font, AppState::getContext()->getPwd()) };
    auto* term { terminal.get() };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    const juce::String newID { term->getComponentID() };
    paneManager.split (activeID, newID, direction);

    auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), newID) };
    jassert (paneNode.isValid());
    paneNode.appendChild (term->getValueTree(), nullptr);

    auto splitNode { paneNode.getParent() };
    jassert (splitNode.isValid());

    panes.add (std::move (terminal));

    resizerBars.add (std::make_unique<jreng::PaneResizerBar> (&paneManager, splitNode, isVertical));
    addAndMakeVisible (resizerBars.back().get());

    if (isShowing())
        term->setVisible (true);

    resized();
}

/**
 * @brief Lays out all terminals and resizer bars via PaneManager.
 *
 * @note MESSAGE THREAD.
 */
void Panes::resized()
{
    jreng::PaneManager::layout (paneManager.getState(), getLocalBounds(), panes, resizerBars);

    for (auto& pane : panes)
    {
        pane->resized();
    }
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
        if (visible and pane->getPaneType() == "terminal")
        {
            auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), pane->getComponentID()) };
            const bool hasDocument { paneNode.isValid() and paneNode.getChildWithName (App::ID::DOCUMENT).isValid() };

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
 * @brief Focus the nearest pane in the given direction.
 *
 * @param deltaX  -1 for left, +1 for right, 0 for vertical.
 * @param deltaY  -1 for up, +1 for down, 0 for horizontal.
 * @note MESSAGE THREAD.
 */
void Panes::focusPane (int deltaX, int deltaY)
{
    const auto activeID { AppState::getContext()->getActivePaneID() };
    PaneComponent* active { nullptr };

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

        PaneComponent* best { nullptr };
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
            AppState::getContext()->setActivePaneID (best->getComponentID());

            if (best->isShowing())
                best->grabKeyboardFocus();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
