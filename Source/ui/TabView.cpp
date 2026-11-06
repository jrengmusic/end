#include "ui/TabView.h"
#include "plugins/Nexus.h"
#include "ui/SessionView.h"

TabView::TabView (jam::Model& newModel, juce::ValueTree existingState)
    : jam::MatrixComponent (newModel, existingState)
{
    setName (jam::Format::toType (Id::tab).toString());
    model.createAndAddParameter<jam::ParameterText> (state, Id::name, juce::String {});
}

//==============================================================================
void TabView::add()
{
    jam::UUID uuid {};

    MatrixComponent::add (uuid, createChild (uuid));
}

void TabView::addPane (const juce::Identifier& edge)
{
    if (edge.isValid())
        split (edge);
    else
        add();
}

void TabView::loadPlugin (const juce::String& plugin)
{
    getFocusedPane().getValueTree().setProperty (Id::plugin, ConfigDirectory::Config::getPath (Id::plugins).getChildFile (plugin).getFullPathName(), nullptr);
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
void TabView::childRemoved (jam::UUID uuid, const juce::ValueTree& row)
{
    MatrixComponent::childRemoved (uuid, row);
    Nexus::getInstance()->getSession (jam::UUID { state.getParent().getProperty (Id::id) }).removePlugin (uuid);

    if (getNumChildren() == 0)
        findParentComponentOfClass<SessionView>()->remove (jam::UUID { static_cast<int64_t> (state.getProperty (Id::id)) });
}

//==============================================================================
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

EditorView& TabView::getFocusedPane()
{
    return get (getFocusedChild());
}

void TabView::setZoom (juce::CommandID command, float step)
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

bool TabView::isHorizontal (const juce::Identifier& edge) noexcept
{
    return edge == Id::left or edge == Id::right;
}

bool TabView::isInward (const juce::Identifier& edge, juce::Point<int> offset) const
{
    const bool horizontal { isHorizontal (edge) };

    return isLow (map::Position::getInstance()->get (edge.toString())) == ((horizontal ? offset.getX() : offset.getY()) > 0);
}

const std::array<std::pair<juce::CommandID, juce::Identifier>, 4>& TabView::getJoinDirections()
{
    static const std::array<std::pair<juce::CommandID, juce::Identifier>, 4> joinDirections {{
        { map::CommandId::joinLeft, Id::left },
        { map::CommandId::joinDown, Id::bottom },
        { map::CommandId::joinUp, Id::top },
        { map::CommandId::joinRight, Id::right },
    }};

    return joinDirections;
}

juce::CommandID TabView::getJoinAction (const juce::Identifier& edge)
{
    const auto& joinDirections { getJoinDirections() };
    const auto found { std::find_if (joinDirections.begin(), joinDirections.end(), [&edge] (const auto& entry)
                                     {
                                         const auto& [action, direction] { entry };
                                         return direction == edge;
                                     }) };

    if (found != joinDirections.end())
    {
        const auto& [action, direction] { *found };
        return action;
    }

    return {};
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
                const bool horizontal { isHorizontal (edge) };

                split (edge, horizontal ? static_cast<float> (relative.x) / static_cast<float> (pane->getWidth())
                                        : static_cast<float> (relative.y) / static_cast<float> (pane->getHeight()));
                loadPlugin();
            }
            else
            {
                commandManager->invokeDirectly (getJoinAction (edge), false);
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
                const bool horizontal { isHorizontal (edge) };

                if (isInward (edge, cursor - mouseDownPosition))
                    paintSplitPreview (g, edge.toString(), horizontal ? static_cast<float> (cursor.x) / static_cast<float> (pane->getWidth())
                                                                      : static_cast<float> (cursor.y) / static_cast<float> (pane->getHeight()));
                else
                    paintJoinPreview (g, edge.toString());
            }
        }
    }
}

static const juce::String sizeSeparator { " x " };

void TabView::paintSplitPreview (juce::Graphics& g, const juce::String& edge, float position)
{
    const auto preview { get (getFocusedChild()).getBounds() };
    const auto edgeKey { map::Position::getInstance()->get (edge) };
    const bool splitVertical { edgeKey == map::Position::right or edgeKey == map::Position::left };
    const int splitLine { splitVertical
                              ? preview.getX() + static_cast<int> (position * static_cast<float> (preview.getWidth()))
                              : preview.getY() + static_cast<int> (position * static_cast<float> (preview.getHeight())) };
    const auto head { splitVertical ? preview.withRight (splitLine) : preview.withBottom (splitLine) };
    const auto tail { splitVertical ? preview.withLeft (splitLine) : preview.withTop (splitLine) };

    static const juce::String regionSeparator { " | " };

    const juce::String message { juce::String (head.getWidth()) + sizeSeparator + juce::String (head.getHeight())
                                 + regionSeparator
                                 + juce::String (tail.getWidth()) + sizeSeparator + juce::String (tail.getHeight()) };

    jam::drawMessageOverlay (g, *this, preview, message, ENDLookAndFeel::getInstance()->getOverlayFont(), splitLine, splitVertical,
                             ConfigModel::getInstance()->getRowValue (jam::Format::toType (Id::pane), Id::splitLine).toString());
}

void TabView::paintJoinPreview (juce::Graphics& g, const juce::String& edge)
{
    const auto preview { get (getFocusedChild()).getBounds() };
    const auto target { getNeighbour (getFocusedChild(), juce::Identifier { edge }) };

    if (target != jam::UUID::none())
    {
        const auto targetBounds { get (target).getBounds() };
        const auto merged { preview.getUnion (targetBounds) };

        const juce::String message { juce::String (merged.getWidth()) + sizeSeparator + juce::String (merged.getHeight()) };

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
    static const std::array<std::pair<AreaOption, juce::CommandID>, 10> areaOptions {{
        { AreaOption::splitVertical, map::CommandId::splitVertical },
        { AreaOption::splitHorizontal, map::CommandId::splitHorizontal },
        { AreaOption::joinLeft, map::CommandId::joinLeft },
        { AreaOption::joinRight, map::CommandId::joinRight },
        { AreaOption::joinUp, map::CommandId::joinUp },
        { AreaOption::joinDown, map::CommandId::joinDown },
        { AreaOption::swapLeft, map::CommandId::swapLeft },
        { AreaOption::swapRight, map::CommandId::swapRight },
        { AreaOption::swapUp, map::CommandId::swapUp },
        { AreaOption::swapDown, map::CommandId::swapDown },
    }};

    const auto found { std::find_if (areaOptions.begin(), areaOptions.end(), [result] (const auto& entry)
                                     {
                                         const auto& [option, action] { entry };
                                         return static_cast<int> (option) == result;
                                     }) };

    if (found != areaOptions.end())
    {
        const auto& [option, action] { *found };
        commandManager->invokeDirectly (action, false);
    }
}
