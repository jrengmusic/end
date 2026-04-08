/**
 * @file Panes.cpp
 * @brief Terminal::Panes implementation — pane lifecycle and layout management.
 *
 * @see Panes.h
 * @see Terminal::Display
 * @see Terminal::Tabs
 */

#include "Panes.h"
#include "../AppState.h"
#include "../terminal/data/Identifier.h"
#include "../whelmed/Component.h"
#include "../nexus/Session.h"
#include "../nexus/Log.h"

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
 * @brief Constructs a Processor for a new terminal via the unified Session API.
 *
 * Delegates to `Nexus::Session::getContext()->create()` which routes internally
 * through local, daemon, or client mode.  Returns the UUID and a non-owning
 * pointer into the Session (or Client) pool.
 *
 * @param shell      Shell program path.  Empty = use Config default.
 * @param args       Shell arguments.  Empty = use Config default.
 * @param cwd        Initial working directory.  Empty = inherit.
 * @param uuidHint   UUID hint (nexus restore or split).  Empty = generate new.
 * @return CreatedTerminal { uuid, processor* }.
 * @note MESSAGE THREAD.
 */
Panes::CreatedTerminal Panes::buildTerminal (const juce::String& shell,
                                              const juce::String& args,
                                              const juce::String& cwd,
                                              const juce::String& uuidHint)
{
    CreatedTerminal result;

    Terminal::Processor& processor { uuidHint.isNotEmpty()
        ? Nexus::Session::getContext()->create (shell, args, cwd, uuidHint)
        : Nexus::Session::getContext()->create (shell, args, cwd) };

    result.uuid = processor.uuid;
    result.processor = &processor;

    return result;
}

/**
 * @brief Tears down a terminal via the unified Session API.
 *
 * Delegates to `Nexus::Session::getContext()->remove()` which routes internally
 * through local, daemon, or client mode.  Display must be erased from panes
 * before this call so no dangling reference exists at destruction time.
 *
 * @param uuid  The UUID of the terminal to tear down.
 * @note MESSAGE THREAD.
 */
void Panes::teardownTerminal (const juce::String& uuid)
{
    Nexus::Session::getContext()->remove (uuid);
}

// =============================================================================

/**
 * @brief Creates a new terminal session as the first (or only) leaf in this pane.
 *
 * Delegates construction to buildTerminal() which routes through Session::create().
 * Wires callbacks, registers with PaneManager via addLeaf, and grafts the session
 * ValueTree into the corresponding PANE node.
 *
 * @param workingDirectory  Initial cwd for the shell. Empty = inherit parent cwd.
 * @param uuid              UUID hint for nexus-mode restoration. Empty = spawn new.
 * @return The UUID of the newly created terminal (its componentID).
 * @note MESSAGE THREAD.
 */
juce::String Panes::createTerminal (const juce::String& workingDirectory,
                                     const juce::String& uuid)
{
    auto created { buildTerminal ({}, {}, workingDirectory, uuid) };

    jassert (created.processor != nullptr);

    std::unique_ptr<Terminal::Display> terminal { created.processor->createDisplay (font) };

    jassert (terminal != nullptr);

    auto* term { terminal.get() };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    paneManager.addLeaf (created.uuid);

    auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), created.uuid) };
    jassert (paneNode.isValid());
    paneNode.appendChild (term->getValueTree(), nullptr);

    if (isShowing())
        term->setVisible (true);

    panes.add (std::move (terminal));

    return created.uuid;
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
        if (pane->getComponentID() == activeID and pane->getPaneType() == App::ID::paneTypeTerminal)
            activeTerminal = pane.get();
    }

    jassert (activeTerminal != nullptr);

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
    AppState::getContext()->setActivePaneType (App::ID::paneTypeDocument);
    resized();

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
            if (panes.at (i)->getPaneType() == App::ID::paneTypeDocument)
            {
                whelmedPane = panes.at (i).get();
                whelmedIndex = i;
            }

            if (panes.at (i)->getPaneType() == App::ID::paneTypeTerminal)
                terminalPane = panes.at (i).get();
        }
    }

    jassert (whelmedPane != nullptr);
    jassert (terminalPane != nullptr);

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
    AppState::getContext()->setActivePaneType (App::ID::paneTypeTerminal);
}

/**
 * @brief Wires a terminal's repaint callback after creation.
 *
 * @param terminal  The terminal to wire.
 * @note MESSAGE THREAD.
 */
void Panes::setTerminalCallbacks (Terminal::Display* terminal)
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
            AppState::getContext()->setModalType (0);
            AppState::getContext()->setSelectionType (0);
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

    // Erase the Display first — ~Display() unwires all Session callbacks.
    // Session is removed after the Display is destroyed.
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

    // teardownTerminal routes through Session::remove(), which handles local and client modes.
    // Display is already erased above so no dangling reference exists when the Processor is destroyed.
    teardownTerminal (uuid);

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
 * @brief Splits a specific target pane using an explicit new UUID and cwd.
 *
 * Used by the restore walker in MainComponent to reconstruct a saved split tree,
 * and internally by splitImpl() for keyboard-shortcut splits.
 *
 * @param targetUuid  UUID of the existing leaf to split.
 * @param newUuid     UUID hint for the new terminal. Empty = generate fresh.
 * @param cwd         Working directory for the new terminal. Empty = inherit.
 * @param direction   "vertical" for left/right divider; "horizontal" for top/bottom.
 * @param isVertical  True when the resizer bar is vertical (direction == "vertical").
 * @note MESSAGE THREAD.
 */
void Panes::splitAt (const juce::String& targetUuid,
                     const juce::String& newUuid,
                     const juce::String& cwd,
                     const juce::String& direction,
                     bool isVertical)
{
    jassert (targetUuid.isNotEmpty());

    auto created { buildTerminal ({}, {}, cwd, newUuid) };

    jassert (created.processor != nullptr);

    std::unique_ptr<Terminal::Display> terminal { created.processor->createDisplay (font) };

    jassert (terminal != nullptr);

    auto* term { terminal.get() };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    paneManager.split (targetUuid, created.uuid, direction);

    auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), created.uuid) };
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
 * @brief Shared split implementation for keyboard-shortcut splits.
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

    splitAt (activeID, {}, AppState::getContext()->getPwd(), direction, isVertical);
}

/**
 * @brief Lays out all terminals and resizer bars via PaneManager.
 *
 * @note MESSAGE THREAD.
 */
void Panes::resized()
{
    jreng::PaneManager::layout (paneManager.getState(), getLocalBounds(), panes, resizerBars);
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
        if (visible and pane->getPaneType() == App::ID::paneTypeTerminal)
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
            AppState::getContext()->setModalType (0);
            AppState::getContext()->setSelectionType (0);
            AppState::getContext()->setActivePaneID (best->getComponentID());

            if (best->isShowing())
                best->grabKeyboardFocus();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
