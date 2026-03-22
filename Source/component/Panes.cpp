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

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @brief Constructs the pane container.
 *
 * @note MESSAGE THREAD.
 */
Panes::Panes (jreng::Font& font_)
    : font (font_)
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
    auto* term { Terminal::Component::create (font, *this, getLocalBounds(), terminals, workingDirectory) };
    setTerminalCallbacks (term);

    const juce::String uuid { term->getComponentID() };
    paneManager.addLeaf (uuid);

    auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), uuid) };
    jassert (paneNode.isValid());
    paneNode.appendChild (term->getValueTree(), nullptr);

    if (isShowing())
        term->setVisible (true);

    return uuid;
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

    terminal->onShellExited = [this, uuid = terminal->getComponentID()]
    {
        int closedIndex { 0 };

        for (size_t i { 0 }; i < terminals.size(); ++i)
        {
            if (terminals.at (i)->getComponentID() == uuid)
            {
                closedIndex = static_cast<int> (i);
                break;
            }
        }

        closePane (uuid);

        if (not terminals.isEmpty())
        {
            const int nextIndex { juce::jmin (closedIndex, static_cast<int> (terminals.size()) - 1) };
            auto* nearest { terminals.at (static_cast<size_t> (nextIndex)).get() };
            AppState::getContext()->setActiveTerminalUuid (nearest->getComponentID());

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
 * @brief Returns the owned terminal container.
 *
 * @return Reference to the terminal owner container.
 * @note MESSAGE THREAD.
 */
jreng::Owner<Terminal::Component>& Panes::getTerminals() noexcept
{
    return terminals;
}

/**
 * @brief Returns the PANES ValueTree owned by PaneManager.
 *
 * @return Reference to the PANES ValueTree.
 * @note MESSAGE THREAD.
 */
juce::ValueTree& Panes::getState() noexcept
{
    return paneManager.getState();
}

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

    for (auto it { terminals.begin() }; it != terminals.end(); ++it)
    {
        if ((*it)->getComponentID() == uuid)
        {
            removeChildComponent (it->get());
            terminals.erase (it);
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
    resized();
}

/**
 * @brief Splits the active pane into side-by-side columns.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitHorizontal()
{
    splitImpl ("vertical", true);
}

/**
 * @brief Splits the active pane into stacked rows.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitVertical()
{
    splitImpl ("horizontal", false);
}

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
    const juce::String activeUuid { AppState::getContext()->getActiveTerminalUuid() };
    jassert (activeUuid.isNotEmpty());

    auto* term { Terminal::Component::create (font, *this, getLocalBounds(), terminals, AppState::getContext()->getPwd()) };
    setTerminalCallbacks (term);

    const juce::String newUuid { term->getComponentID() };
    paneManager.split (activeUuid, newUuid, direction);

    auto paneNode { jreng::PaneManager::findLeaf (paneManager.getState(), newUuid) };
    jassert (paneNode.isValid());
    paneNode.appendChild (term->getValueTree(), nullptr);

    auto splitNode { paneNode.getParent() };
    jassert (splitNode.isValid());

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
    jreng::PaneManager::layOut (paneManager.getState(), getLocalBounds(), terminals, resizerBars);
}

/**
 * @brief Propagates visibility to child terminals.
 *
 * @note MESSAGE THREAD.
 */
void Panes::visibilityChanged()
{
    const bool visible { isVisible() };

    for (auto& terminal : terminals)
    {
        terminal->setVisible (visible);
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
    const auto activeUuid { AppState::getContext()->getActiveTerminalUuid() };
    Terminal::Component* active { nullptr };

    for (auto& terminal : terminals)
    {
        if (terminal->getComponentID() == activeUuid)
            active = terminal.get();
    }

    if (active != nullptr)
    {
        const auto activeCentre { active->getBounds().getCentre() };

        Terminal::Component* best { nullptr };
        int bestDistance { std::numeric_limits<int>::max() };

        for (auto& terminal : terminals)
        {
            if (terminal.get() != active)
            {
                const auto centre { terminal->getBounds().getCentre() };

                const bool inDirection {
                    (deltaX < 0 and centre.getX() < activeCentre.getX())
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
                        best = terminal.get();
                    }
                }
            }
        }

        if (best != nullptr)
        {
            AppState::getContext()->setActiveTerminalUuid (best->getComponentID());

            if (best->isShowing())
                best->grabKeyboardFocus();
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
