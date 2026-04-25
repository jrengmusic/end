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
#include "../nexus/Nexus.h"
#include "../terminal/logic/Session.h"
#include "../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @brief Constructs the pane container.
 *
 * @note MESSAGE THREAD.
 */
Panes::Panes (jam::Font& font_, jam::Glyph::Packer& packer_, jam::gl::GlyphAtlas& glAtlas_, jam::GraphicsAtlas& graphicsAtlas_)
    : font (font_)
    , packerRef (packer_)
    , glAtlasRef (glAtlas_)
    , graphicsAtlasRef (graphicsAtlas_)
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

/**
 * @brief Converts a pixel rect into terminal cell dimensions.
 *
 * Subtracts terminal padding from the rect, then divides by the effective
 * cell size derived from font metrics and config multipliers.
 *
 * @param paneRect  Target pixel rect for the pane (chrome already subtracted by caller).
 * @param font_     Font spec carrying resolved typeface; provides cell metrics.
 * @return {cols, rows}. jasserts cols > 0 and rows > 0.
 * @note Pure math — no instance state.
 */
std::pair<int, int> Panes::cellsFromRect (juce::Rectangle<int> paneRect,
                                           jam::Font& font_) noexcept
{
    // Physical-pixel math — matches Screen::calc() exactly (SSOT).
    const auto fm { font_.getResolvedTypeface()->calcMetrics (Config::getContext()->dpiCorrectedFontSize()) };
    const auto* cfg { Config::getContext() };
    const float scale { jam::Typeface::getDisplayScale() };
    const float cellWidthMultiplier  { cfg->getFloat (Config::Key::fontCellWidth) };
    const float lineHeightMultiplier { cfg->getFloat (Config::Key::fontLineHeight) };
    const int physCellW { static_cast<int> (static_cast<float> (fm.physCellW) * cellWidthMultiplier) };
    const int physCellH { static_cast<int> (static_cast<float> (fm.physCellH) * lineHeightMultiplier) };

    jassert (physCellW > 0 and physCellH > 0);

    const int paddingTop    { cfg->getInt (Config::Key::terminalPaddingTop) };
    const int paddingRight  { cfg->getInt (Config::Key::terminalPaddingRight) };
    const int paddingBottom { cfg->getInt (Config::Key::terminalPaddingBottom) };
    const int paddingLeft   { cfg->getInt (Config::Key::terminalPaddingLeft) };

    const int contentW { paneRect.getWidth()  - paddingLeft - paddingRight };
    const int contentH { paneRect.getHeight() - paddingTop  - paddingBottom };

    const int physContentW { static_cast<int> (static_cast<float> (contentW) * scale) };
    const int physContentH { static_cast<int> (static_cast<float> (contentH) * scale) };

    const int cols { (physContentW > 0 and physCellW > 0) ? physContentW / physCellW : 1 };
    const int rows { (physContentH > 0 and physCellH > 0) ? physContentH / physCellH : 1 };

    jassert (cols > 0 and rows > 0);
    return { cols, rows };
}

/**
 * @brief Splits a pixel rect by direction and ratio.
 *
 * Uses jam::PaneManager::resizerBarSize as SSOT — matches layoutNode arithmetic exactly.
 * "vertical" splits width (left/right target/new). "horizontal" splits height (top/bottom).
 *
 * @param parent     Parent pixel rect.
 * @param direction  "vertical" or "horizontal".
 * @param ratio      Split ratio in (0,1).
 * @return {targetRect, newRect}.
 * @note Pure math — no instance state.
 */
std::pair<juce::Rectangle<int>, juce::Rectangle<int>>
    Panes::splitRect (juce::Rectangle<int> parent,
                      const juce::String& direction,
                      double ratio) noexcept
{
    juce::Rectangle<int> targetRect;
    juce::Rectangle<int> newRect;

    if (direction == "vertical")
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

/**
 * @brief Creates a new terminal session as the first (or only) leaf in this pane.
 *
 * Delegates construction to `Nexus::getContext()->create()` for mode-transparent session creation.
 * Wires callbacks, registers with PaneManager via addLeaf, and grafts the session
 * ValueTree into the corresponding PANE node.
 *
 * @param workingDirectory  Initial cwd for the shell. Empty = inherit parent cwd.
 * @param uuid              UUID hint for nexus-mode restoration. Empty = spawn new.
 * @param cols              Terminal column count. Must be > 0.
 * @param rows              Terminal row count. Must be > 0.
 * @return The UUID of the newly created terminal (its componentID).
 * @note MESSAGE THREAD.
 */
juce::String Panes::createTerminal (const juce::String& workingDirectory,
                                     const juce::String& uuid,
                                     int cols,
                                     int rows)
{
    jassert (cols > 0 and rows > 0);

    const juce::String effectiveUuid { uuid.isNotEmpty() ? uuid : juce::Uuid().toString() };
    Terminal::Processor& processor { Nexus::getContext()->create (workingDirectory, effectiveUuid, cols, rows).getProcessor() };

    const juce::String termUuid { processor.getUuid() };

    std::unique_ptr<Terminal::Display> terminal { processor.createDisplay (font, packerRef, glAtlasRef, graphicsAtlasRef) };

    jassert (terminal != nullptr);

    auto* term { terminal.get() };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    paneManager.addLeaf (termUuid);

    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), termUuid) };
    jassert (paneNode.isValid());
    paneNode.appendChild (term->getValueTree(), nullptr);

    if (isShowing())
        term->setVisible (true);

    panes.add (std::move (terminal));

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
    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), activeID) };
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
    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), activeID) };
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
jam::Owner<PaneComponent>& Panes::getPanes() noexcept { return panes; }

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
    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), uuid) };
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

    // Nexus::remove() handles session teardown — Display already erased above.
    Nexus::getContext()->remove (uuid);

    resized();
}

/**
 * @brief Splits the active pane into side-by-side columns.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitHorizontal() { splitActive ("vertical", true, 0.5); }

/**
 * @brief Splits the active pane into stacked rows.
 *
 * @note MESSAGE THREAD.
 */
void Panes::splitVertical() { splitActive ("horizontal", false, 0.5); }

void Panes::splitActiveWithRatio (const juce::String& direction, bool isVertical, double ratio)
{
    splitActive (direction, isVertical, ratio);
}

/**
 * @brief Splits a specific target pane using an explicit new UUID and cwd.
 *
 * Used by the restore walker in MainComponent to reconstruct a saved split tree,
 * and internally by splitActive() for keyboard-shortcut splits.
 *
 * @param targetUuid  UUID of the existing leaf to split.
 * @param newUuid     UUID hint for the new terminal. Empty = generate fresh.
 * @param cwd         Working directory for the new terminal. Empty = inherit.
 * @param direction   "vertical" for left/right divider; "horizontal" for top/bottom.
 * @param isVertical  True when the resizer bar is vertical (direction == "vertical").
 * @param cols        Terminal column count for the new pane. Must be > 0.
 * @param rows        Terminal row count for the new pane. Must be > 0.
 * @note MESSAGE THREAD.
 */
void Panes::splitAt (const juce::String& targetUuid,
                     const juce::String& newUuid,
                     const juce::String& cwd,
                     const juce::String& direction,
                     bool isVertical,
                     int cols,
                     int rows,
                     double ratio)
{
    jassert (targetUuid.isNotEmpty());
    jassert (cols > 0 and rows > 0);

    const juce::String effectiveSplitUuid { newUuid.isNotEmpty() ? newUuid : juce::Uuid().toString() };
    Terminal::Processor& processor { Nexus::getContext()->create (cwd, effectiveSplitUuid, cols, rows).getProcessor() };

    const juce::String splitUuid { processor.getUuid() };

    std::unique_ptr<Terminal::Display> terminal { processor.createDisplay (font, packerRef, glAtlasRef, graphicsAtlasRef) };

    jassert (terminal != nullptr);

    auto* term { terminal.get() };
    term->setBounds (getLocalBounds());
    addChildComponent (term);
    setTerminalCallbacks (term);

    paneManager.split (targetUuid, splitUuid, direction, ratio);

    auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), splitUuid) };
    jassert (paneNode.isValid());
    paneNode.appendChild (term->getValueTree(), nullptr);

    auto splitNode { paneNode.getParent() };
    jassert (splitNode.isValid());

    panes.add (std::move (terminal));

    resizerBars.add (std::make_unique<jam::PaneResizerBar> (&paneManager, splitNode, isVertical));
    addAndMakeVisible (resizerBars.back().get());

    if (isShowing())
        term->setVisible (true);

    resized();
}

/**
 * @brief Shared split implementation for keyboard-shortcut splits.
 *
 * Reads the active pane's live pixel bounds (laid out at runtime), computes
 * the new pane's rect via splitRect at ratio 0.5, then derives cell dims via
 * cellsFromRect before forwarding to splitAt.
 *
 * @param direction  PaneManager direction string: "vertical" = left/right
 *                   divider; "horizontal" = top/bottom divider.
 * @param isVertical True when the resizer bar is vertical (splitHorizontal).
 * @param ratio      Split ratio (0.0–1.0). 0.5 = equal halves.
 * @note MESSAGE THREAD.
 */
void Panes::splitActive (const juce::String& direction, bool isVertical, double ratio)
{
    const juce::String activeID { AppState::getContext()->getActivePaneID() };
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
    const auto [cols, rows] { cellsFromRect (newRect, font) };

    splitAt (activeID, {}, AppState::getContext()->getPwd(), direction, isVertical, cols, rows, ratio);
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
        if (visible and pane->getPaneType() == App::ID::paneTypeTerminal)
        {
            auto paneNode { jam::PaneManager::findLeaf (paneManager.getState(), pane->getComponentID()) };
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
} // namespace Terminal
