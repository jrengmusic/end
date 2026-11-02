#include "ui/TabView.h"
#include "action/ENDActions.h"

TabView::TabView (jam::UUID uuid, jam::Model& newModel, juce::ValueTree sessionState)
    : jam::MatrixComponent (newModel, sessionState, Id::toType (Id::tab), uuid)
{
    setName (Id::toType (Id::tab).toString());
    model.createAndAddParameter<jam::ParameterText> (state, Id::name, juce::String {});
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
    auto child { std::make_unique<EditorView> (model, state, uuid) };

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
    const auto target { getNeighbour (getFocusedChild(), direction) };

    if (target != jam::UUID::none())
        setFocusedChild (target);
}

jam::UUID TabView::join (const juce::Identifier& direction)
{
    return MatrixComponent::join (getFocusedChild(), direction);
}

void TabView::swap (const juce::Identifier& direction)
{
    const auto target { getNeighbour (getFocusedChild(), direction) };

    if (target != jam::UUID::none())
        MatrixComponent::swap (getFocusedChild(), target);
}

//==============================================================================
EditorView& TabView::get (jam::UUID uuid)
{
    return static_cast<EditorView&> (MatrixComponent::get (uuid));
}

void TabView::setZoom (const juce::Identifier& command, float step)
{
    for (const auto& [uuid, child] : getChildren())
        static_cast<EditorView&> (*child).setZoom (command, step);
}

juce::Identifier TabView::getEdge (const jam::PaneComponent& pane, juce::Point<int> mouseDownPosition, juce::Point<int> offset) const
{
    const auto corner { pane.getCorner (mouseDownPosition) };
    const bool horizontal { std::abs (offset.getX()) > std::abs (offset.getY()) };

    if (not corner.isEmpty()
        and (std::abs (offset.getX()) >= splitDragThreshold or std::abs (offset.getY()) >= splitDragThreshold))
        return horizontal ? (corner.getX() == 0 ? Id::left : Id::right)
                          : (corner.getY() == 0 ? Id::top : Id::bottom);

    return {};
}

bool TabView::isInward (const juce::Identifier& edge, juce::Point<int> offset) const
{
    const bool horizontal { edge == Id::left or edge == Id::right };

    return isLow (map::Position::getInstance()->get (edge.toString())) == ((horizontal ? offset.getX() : offset.getY()) > 0);
}

//==============================================================================
void TabView::mouseDrag (const juce::MouseEvent& event)
{
    if (auto* pane { event.originalComponent->findParentComponentOfClass<jam::PaneComponent>() })
    {
        if (getEdge (*pane, event.getEventRelativeTo (pane).getMouseDownPosition(), event.getOffsetFromDragStart()).isValid())
            repaint();
    }
}

void TabView::mouseUp (const juce::MouseEvent& event)
{
    if (auto* pane { event.originalComponent->findParentComponentOfClass<jam::PaneComponent>() })
    {
        const auto relative { event.getEventRelativeTo (pane) };
        const auto offset { event.getOffsetFromDragStart() };
        const auto edge { getEdge (*pane, relative.getMouseDownPosition(), offset) };

        if (edge.isValid())
        {
            if (isInward (edge, offset))
            {
                const bool horizontal { edge == Id::left or edge == Id::right };

                split (edge, horizontal ? static_cast<float> (relative.x) / static_cast<float> (pane->getWidth())
                                        : static_cast<float> (relative.y) / static_cast<float> (pane->getHeight()));
            }
            else
            {
                static const auto joinActions {
                    []
                    {
                        jam::HashMap<int, juce::Identifier> actions;
                        actions.try_emplace (map::Position::left, Id::joinLeft);
                        actions.try_emplace (map::Position::right, Id::joinRight);
                        actions.try_emplace (map::Position::top, Id::joinUp);
                        actions.try_emplace (map::Position::bottom, Id::joinDown);
                        return actions;
                    }()
                };

                ENDActions::getInstance()->run (joinActions.at (map::Position::getInstance()->get (edge.toString())));
            }

            repaint();
        }
    }
}

//==============================================================================
void TabView::paintOverChildren (juce::Graphics& g)
{
    const auto& source { juce::Desktop::getInstance().getMainMouseSource() };

    if (source.isDragging() and source.getComponentUnderMouse() != nullptr)
    {
        if (auto* pane { source.getComponentUnderMouse()->findParentComponentOfClass<jam::PaneComponent>() })
        {
            const auto mouseDownPosition { pane->getLocalPoint (nullptr, source.getLastMouseDownPosition()).toInt() };
            const auto cursor { pane->getLocalPoint (nullptr, source.getScreenPosition()).toInt() };
            const auto edge { getEdge (*pane, mouseDownPosition, cursor - mouseDownPosition) };

            if (edge.isValid())
            {
                const bool horizontal { edge == Id::left or edge == Id::right };

                if (isInward (edge, cursor - mouseDownPosition))
                    paintSplitPreview (g, edge.toString(), horizontal ? static_cast<float> (cursor.x) / static_cast<float> (pane->getWidth())
                                                                      : static_cast<float> (cursor.y) / static_cast<float> (pane->getHeight()));
                else
                    paintJoinPreview (g, edge.toString());
            }
        }
    }
}

void TabView::paintSplitPreview (juce::Graphics& g, const juce::String& edge, float position)
{
    const auto preview { get (getFocusedChild()).getBounds() };
    const auto edgeKey { map::Position::getInstance()->get (edge) };
    const bool splitVertical { edgeKey == map::Position::right or edgeKey == map::Position::left };
    const int splitLine { splitVertical
                              ? preview.getX() + static_cast<int> (position * static_cast<float> (preview.getWidth()))
                              : preview.getY() + static_cast<int> (position * static_cast<float> (preview.getHeight())) };
    const auto metrics { ENDLookAndFeel::getInstance()->getCodeMetrics() };

    const auto head { splitVertical ? preview.withRight (splitLine) : preview.withBottom (splitLine) };
    const auto tail { splitVertical ? preview.withLeft (splitLine) : preview.withTop (splitLine) };

    const juce::String message { juce::String (static_cast<int> (head.getWidth() / metrics.cellWidth)) + " x "
                                 + juce::String (static_cast<int> (head.getHeight() / metrics.cellHeight))
                                 + " | "
                                 + juce::String (static_cast<int> (tail.getWidth() / metrics.cellWidth)) + " x "
                                 + juce::String (static_cast<int> (tail.getHeight() / metrics.cellHeight)) };

    jam::drawMessageOverlay (g, *this, preview, message, ENDLookAndFeel::getInstance()->getOverlayFont(), splitLine, splitVertical,
                             ConfigModel::getInstance()->getRowValue (Id::toType (Id::pane), Id::splitLine).toString());
}

void TabView::paintJoinPreview (juce::Graphics& g, const juce::String& edge)
{
    const auto preview { get (getFocusedChild()).getBounds() };
    const auto target { getNeighbour (getFocusedChild(), juce::Identifier { edge }) };

    if (target != jam::UUID::none())
    {
        const auto targetBounds { get (target).getBounds() };
        const auto merged { preview.getUnion (targetBounds) };
        const auto metrics { ENDLookAndFeel::getInstance()->getCodeMetrics() };

        const juce::String message { juce::String (static_cast<int> (merged.getWidth() / metrics.cellWidth)) + " x "
                                     + juce::String (static_cast<int> (merged.getHeight() / metrics.cellHeight)) };

        jam::drawMessageOverlay (g, *this, targetBounds, message, ENDLookAndFeel::getInstance()->getOverlayFont());
    }
}

//==============================================================================
juce::PopupMenu TabView::buildAreaOptionsMenu()
{
    const auto focused { getFocusedChild() };

    juce::PopupMenu menu;

    menu.addSectionHeader ("Area Options");
    menu.addItem (static_cast<int> (AreaOption::splitVertical), "Vertical Split");
    menu.addItem (static_cast<int> (AreaOption::splitHorizontal), "Horizontal Split");
    menu.addSeparator();
    menu.addItem (static_cast<int> (AreaOption::joinLeft), "Join Left", getNeighbour (focused, Id::left) != jam::UUID::none());
    menu.addItem (static_cast<int> (AreaOption::joinRight), "Join Right", getNeighbour (focused, Id::right) != jam::UUID::none());
    menu.addItem (static_cast<int> (AreaOption::joinUp), "Join Up", getNeighbour (focused, Id::top) != jam::UUID::none());
    menu.addItem (static_cast<int> (AreaOption::joinDown), "Join Down", getNeighbour (focused, Id::bottom) != jam::UUID::none());
    menu.addSeparator();
    menu.addItem (static_cast<int> (AreaOption::swapLeft), "Swap Left", getNeighbour (focused, Id::left) != jam::UUID::none());
    menu.addItem (static_cast<int> (AreaOption::swapRight), "Swap Right", getNeighbour (focused, Id::right) != jam::UUID::none());
    menu.addItem (static_cast<int> (AreaOption::swapUp), "Swap Up", getNeighbour (focused, Id::top) != jam::UUID::none());
    menu.addItem (static_cast<int> (AreaOption::swapDown), "Swap Down", getNeighbour (focused, Id::bottom) != jam::UUID::none());

    return menu;
}

void TabView::handleAreaOptionsResult (int result)
{
    static const auto areaOptions {
        []
        {
            jam::Function::Map<AreaOption, void> options;
            options.add<TabView&> (AreaOption::splitVertical, [] (TabView&) { ENDActions::getInstance()->run (Id::splitVertical); });
            options.add<TabView&> (AreaOption::splitHorizontal, [] (TabView&) { ENDActions::getInstance()->run (Id::splitHorizontal); });
            options.add<TabView&> (AreaOption::joinLeft, [] (TabView&) { ENDActions::getInstance()->run (Id::joinLeft); });
            options.add<TabView&> (AreaOption::joinRight, [] (TabView&) { ENDActions::getInstance()->run (Id::joinRight); });
            options.add<TabView&> (AreaOption::joinUp, [] (TabView&) { ENDActions::getInstance()->run (Id::joinUp); });
            options.add<TabView&> (AreaOption::joinDown, [] (TabView&) { ENDActions::getInstance()->run (Id::joinDown); });
            options.add<TabView&> (AreaOption::swapLeft, [] (TabView&) { ENDActions::getInstance()->run (Id::swapLeft); });
            options.add<TabView&> (AreaOption::swapRight, [] (TabView&) { ENDActions::getInstance()->run (Id::swapRight); });
            options.add<TabView&> (AreaOption::swapUp, [] (TabView&) { ENDActions::getInstance()->run (Id::swapUp); });
            options.add<TabView&> (AreaOption::swapDown, [] (TabView&) { ENDActions::getInstance()->run (Id::swapDown); });
            return options;
        }()
    };

    areaOptions.get (static_cast<AreaOption> (result), *this);
}
