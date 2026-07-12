/**
 * @file TabView.cpp
 * @brief TabView implementation — pane graph mutation, corner drag gesture,
 *        and Area Options menu dispatch.
 */
#include "end/TabView.h"
#include "action/ENDActions.h"


/** @brief Prefix written to state's edge property while a swap-pick target is pending. */
static constexpr const char* swapPickPrefix { "swap:" };

TabView::TabView (jam::UUID uuid, jam::Model& m, juce::ValueTree sessionState)
    : jam::MatrixComponent (m, sessionState, IDtype::tab, uuid)
{
    setName (IDtype::tab.toString());
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::name, juce::String {});
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::edge, juce::String {});
    model.createAndAddParameter<jam::Parameter<float>> (state, jam::ID::position, 0.0f);
    addMouseListener (this, true);
}

//==============================================================================
jam::UUID TabView::add()
{
    jam::UUID uuid {};

    MatrixComponent::add (uuid, createChild (uuid));

    return uuid;
}

//==============================================================================
std::unique_ptr<jam::OwnedComponent> TabView::createChild (jam::UUID uuid)
{
    auto child { std::make_unique<TerminalView> (model, state, uuid) };

    child->setCornerMenuFactory ([this] { return buildAreaOptionsMenu(); });
    child->setCornerMenuAction ([this] (int result) { handleAreaOptionsResult (result); });

    return child;
}

//==============================================================================
void TabView::childRemoved (jam::UUID uuid)
{
    state.removeChild (MatrixComponent::get (uuid).getValueTree(), nullptr);
    MatrixComponent::childRemoved (uuid);
}

//==============================================================================
void TabView::focusPane (const juce::Identifier& direction)
{
    const auto target { getNeighbor (getFocusedChild(), direction) };

    if (target != jam::UUID::none())
        get (target).toFront (true);
}

jam::UUID TabView::join (const juce::Identifier& direction)
{
    return MatrixComponent::join (getFocusedChild(), direction);
}

void TabView::swap (const juce::Identifier& direction)
{
    const auto target { getNeighbor (getFocusedChild(), direction) };

    if (target != jam::UUID::none())
        MatrixComponent::swap (getFocusedChild(), target);
}

//==============================================================================
TerminalView& TabView::get (jam::UUID uuid)
{
    return static_cast<TerminalView&> (MatrixComponent::get (uuid));
}

//==============================================================================
void TabView::mouseDown (const juce::MouseEvent& event)
{
    const auto edge { state.getProperty (jam::ID::edge).toString() };

    if (edge.startsWith (swapPickPrefix))
    {
        if (event.mods.isPopupMenu())
        {
            state.setProperty (jam::ID::edge, juce::String {}, nullptr);
            repaint();
        }
        else if (auto* pane { dynamic_cast<jam::PaneComponent*> (event.originalComponent) })
        {
            const auto source { jam::UUID { juce::var { edge.fromFirstOccurrenceOf (":", false, false) } } };
            const auto target { jam::UUID { pane->getValueTree().getProperty (jam::ID::id) } };

            if (source != jam::UUID::none() and target != source)
                MatrixComponent::swap (source, target);

            state.setProperty (jam::ID::edge, juce::String {}, nullptr);
            layout();
            repaint();
        }
    }
}

void TabView::mouseDrag (const juce::MouseEvent& event)
{
    if (auto* pane { dynamic_cast<jam::PaneComponent*> (event.originalComponent) })
    {
        const auto corner { pane->getCorner (event.getEventRelativeTo (pane).getMouseDownPosition()) };
        const auto delta { event.getOffsetFromDragStart() };

        if (not corner.isEmpty()
            and (std::abs (delta.getX()) >= splitDragThreshold or std::abs (delta.getY()) >= splitDragThreshold))
        {
            const auto cursor { event.getEventRelativeTo (pane).getPosition() };
            const bool horizontal { std::abs (delta.getX()) > std::abs (delta.getY()) };

            const juce::Identifier edge { horizontal ? (corner.getX() == 0 ? jam::ID::left : jam::ID::right)
                                                     : (corner.getY() == 0 ? jam::ID::top : jam::ID::bottom) };

            const bool inward { jam::Position::isLow (jam::Position::get (edge.toString()))
                                == ((horizontal ? delta.getX() : delta.getY()) > 0) };

            state.setProperty (jam::ID::edge, edge.toString(), nullptr);
            state.setProperty (jam::ID::position,
                               inward ? (horizontal ? static_cast<float> (cursor.getX()) / static_cast<float> (pane->getWidth())
                                                    : static_cast<float> (cursor.getY()) / static_cast<float> (pane->getHeight()))
                                      : -1.0f,
                               nullptr);

            repaint();
        }
    }
}

void TabView::mouseUp (const juce::MouseEvent& event)
{
    const auto edge { state.getProperty (jam::ID::edge).toString() };

    if (edge.isNotEmpty())
    {
        const auto position { static_cast<float> (state.getProperty (jam::ID::position)) };

        if (position >= 0.0f)
            split (juce::Identifier { edge }, position);
        else
        {
            const auto& direction { juce::Identifier { edge } };
            const auto& action { direction == jam::ID::left  ? ID::joinLeft
                               : direction == jam::ID::right ? ID::joinRight
                               : direction == jam::ID::top   ? ID::joinUp
                                                             : ID::joinDown };
            ENDActions::getInstance()->run (action);
        }

        state.setProperty (jam::ID::edge, juce::String {}, nullptr);
        repaint();
    }
}

//==============================================================================
void TabView::paintOverChildren (juce::Graphics& g)
{
    const auto edge { state.getProperty (jam::ID::edge).toString() };

    if (edge.isNotEmpty())
    {
        if (edge.startsWith (swapPickPrefix))
        {
            drawMessageOverlay (g, *this, getLocalBounds(), "Click a pane to swap");
        }
        else
        {
            const auto preview { get (getFocusedChild()).getBounds() };
            const auto position { static_cast<float> (state.getProperty (jam::ID::position)) };

            if (position >= 0.0f)
            {
                const bool splitVertical { jam::Position::isVertical (jam::Position::get (edge)) };
                const int splitLine { splitVertical
                                          ? preview.getX() + static_cast<int> (position * static_cast<float> (preview.getWidth()))
                                          : preview.getY() + static_cast<int> (position * static_cast<float> (preview.getHeight())) };
                const auto metrics { ENDLookAndFeel::getInstance()->getCodeMetrics (TerminalModel::defaultZoom) };

                const auto head { splitVertical ? preview.withRight (splitLine) : preview.withBottom (splitLine) };
                const auto tail { splitVertical ? preview.withLeft (splitLine) : preview.withTop (splitLine) };

                const juce::String message { juce::String (static_cast<int> (head.getWidth() / metrics.cellWidth)) + " x "
                                             + juce::String (static_cast<int> (head.getHeight() / metrics.cellHeight))
                                             + " | "
                                             + juce::String (static_cast<int> (tail.getWidth() / metrics.cellWidth)) + " x "
                                             + juce::String (static_cast<int> (tail.getHeight() / metrics.cellHeight)) };

                drawMessageOverlay (g, *this, preview, message, splitLine, splitVertical);
            }
            else
            {
                const auto target { getNeighbor (getFocusedChild(), juce::Identifier { edge }) };

                if (target != jam::UUID::none())
                {
                    const auto targetBounds { get (target).getBounds() };
                    const auto merged { preview.getUnion (targetBounds) };
                    const auto metrics { ENDLookAndFeel::getInstance()->getCodeMetrics (TerminalModel::defaultZoom) };

                    const juce::String message { juce::String (static_cast<int> (merged.getWidth() / metrics.cellWidth)) + " x "
                                                 + juce::String (static_cast<int> (merged.getHeight() / metrics.cellHeight)) };

                    drawMessageOverlay (g, *this, targetBounds, message);
                }
            }
        }
    }
}

//==============================================================================
juce::PopupMenu TabView::buildAreaOptionsMenu()
{
    const auto focused { getFocusedChild() };

    juce::PopupMenu menu;

    menu.addSectionHeader ("Area Options");
    menu.addItem (1, "Vertical Split");
    menu.addItem (2, "Horizontal Split");
    menu.addSeparator();
    menu.addItem (3, "Join Left", getNeighbor (focused, jam::ID::left) != jam::UUID::none());
    menu.addItem (4, "Join Right", getNeighbor (focused, jam::ID::right) != jam::UUID::none());
    menu.addItem (5, "Join Up", getNeighbor (focused, jam::ID::top) != jam::UUID::none());
    menu.addItem (6, "Join Down", getNeighbor (focused, jam::ID::bottom) != jam::UUID::none());
    menu.addSeparator();
    menu.addItem (7, "Swap Areas");

    return menu;
}

void TabView::handleAreaOptionsResult (int result)
{
    switch (result)
    {
        case 1: split (jam::ID::left, 0.5f); break;
        case 2: split (jam::ID::top, 0.5f); break;
        case 3: ENDActions::getInstance()->run (ID::joinLeft); break;
        case 4: ENDActions::getInstance()->run (ID::joinRight); break;
        case 5: ENDActions::getInstance()->run (ID::joinUp); break;
        case 6: ENDActions::getInstance()->run (ID::joinDown); break;

        case 7:
            state.setProperty (jam::ID::edge, juce::String (swapPickPrefix) + getFocusedChild().toString(), nullptr);
            repaint();
            break;
    }
}
