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
 * @brief Constructs the pane container with an empty PANES ValueTree.
 *
 * @note MESSAGE THREAD.
 */
Panes::Panes()
    : state (App::ID::PANES)
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
 * @brief Creates a new terminal session in this pane and returns its UUID.
 *
 * Creates a Terminal::Component via Terminal::Component::create(), wires
 * its callbacks, and appends its SESSION ValueTree to the PANES state tree.
 *
 * @return The UUID of the newly created terminal (its componentID).
 * @note MESSAGE THREAD.
 */
juce::String Panes::createTerminal()
{
    auto* term { Terminal::Component::create (*this, getLocalBounds(), terminals) };
    setTerminalCallbacks (term);
    state.appendChild (term->getValueTree(), nullptr);

    if (isShowing())
        term->setVisible (true);

    return term->getComponentID();
}

/**
 * @brief Wires a terminal's repaint callback after creation.
 *
 * @param terminal  The terminal to wire.
 * @note MESSAGE THREAD.
 */
void Panes::setTerminalCallbacks (Terminal::Component* terminal)
{
    if (onRepaintNeeded != nullptr)
    {
        terminal->onRepaintNeeded = onRepaintNeeded;
    }
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
 * @brief Returns the PANES ValueTree for attachment to AppState.
 *
 * @return Reference to the PANES ValueTree.
 * @note MESSAGE THREAD.
 */
juce::ValueTree& Panes::getState() noexcept
{
    return state;
}

/**
 * @brief Splits the pane into vertical columns (side by side).
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitVertical()
{
    if (not hasSplitDirection or isVertical)
    {
        isVertical = true;
        hasSplitDirection = true;
        createTerminal();
        rebuildLayout();
        resized();
    }
}

/**
 * @brief Splits the pane into horizontal rows (stacked).
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitHorizontal()
{
    if (not hasSplitDirection or not isVertical)
    {
        isVertical = false;
        hasSplitDirection = true;
        createTerminal();
        rebuildLayout();
        resized();
    }
}

/**
 * @brief Rebuilds the PaneManager layout and resizer bars for the current terminal count.
 *
 * @note MESSAGE THREAD.
 */
void Panes::rebuildLayout()
{
    for (auto& bar : resizerBars)
    {
        removeChildComponent (bar.get());
    }
    resizerBars.clear();
    paneManager.clearAllItems();

    const int numTerminals { static_cast<int> (terminals.size()) };

    if (numTerminals <= 1) return;

    const double proportion { -1.0 / numTerminals };
    int itemIndex { 0 };

    for (int i { 0 }; i < numTerminals; ++i)
    {
        paneManager.setItemLayout (itemIndex, 50, -1.0, proportion);
        ++itemIndex;

        if (i < numTerminals - 1)
        {
            auto resizer { std::make_unique<jreng::PaneResizerBar> (&paneManager, itemIndex, isVertical) };
            addAndMakeVisible (resizer.get());
            resizerBars.add (std::move (resizer));
            paneManager.setItemLayout (itemIndex, resizerBarSize, resizerBarSize, resizerBarSize);
            ++itemIndex;
        }
    }
}

/**
 * @brief Lays out all visible terminals to fill the component bounds.
 *
 * @note MESSAGE THREAD.
 */
void Panes::resized()
{
    const int numTerminals { static_cast<int> (terminals.size()) };

    if (numTerminals == 1)
    {
        terminals.at (0)->setBounds (getLocalBounds());
    }
    else if (numTerminals > 1)
    {
        auto content { getLocalBounds() };
        const int totalGap { resizerBarSize * (numTerminals - 1) };

        if (isVertical)
        {
            const int termWidth { (content.getWidth() - totalGap) / numTerminals };
            int x { content.getX() };

            for (int i { 0 }; i < numTerminals; ++i)
            {
                const int w { (i == numTerminals - 1) ? content.getRight() - x : termWidth };
                terminals.at (i)->setBounds (x, content.getY(), w, content.getHeight());
                x += w + resizerBarSize;
            }
        }
        else
        {
            const int termHeight { (content.getHeight() - totalGap) / numTerminals };
            int y { content.getY() };

            for (int i { 0 }; i < numTerminals; ++i)
            {
                const int h { (i == numTerminals - 1) ? content.getBottom() - y : termHeight };
                terminals.at (i)->setBounds (content.getX(), y, content.getWidth(), h);
                y += h + resizerBarSize;
            }
        }
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

    for (auto& terminal : terminals)
    {
        terminal->setVisible (visible);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
