#include "ui/TabView.h"
#include "plugins/Nexus.h"

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

void TabView::addPane (const juce::Identifier& edge, const juce::String& directory)
{
    if (edge.isValid())
        split (edge);
    else
        add();

    getFocusedPane().getValueTree().getOrCreateChildWithName (jam::Format::toType (Id::pluginState), nullptr).setProperty (Id::cwd, directory, nullptr);
    loadPlugin();
}

void TabView::loadPlugin()
{
    getFocusedPane().getValueTree().setProperty (Id::plugin, Nexus::getInstance()->getOrCreatePlugin().getFullPathName(), nullptr);
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
        state.getParent().removeChild (state, nullptr);
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

const jam::HashMap<juce::Identifier, juce::CommandID>& TabView::getJoinCommands()
{
    static const jam::HashMap<juce::Identifier, juce::CommandID> joinDirections
    {
        { Id::left, map::CommandId::joinLeft },
        { Id::bottom, map::CommandId::joinDown },
        { Id::top, map::CommandId::joinUp },
        { Id::right, map::CommandId::joinRight }
    };

    return joinDirections;
}

//==============================================================================
void TabView::mouseDrag (const juce::MouseEvent& event)
{
    if (auto* pane { event.originalComponent->findParentComponentOfClass<jam::PaneComponent>() })
    {
        if (getEdge (*pane, event.getEventRelativeTo (pane).getMouseDownPosition(), event.getOffsetFromDragStart()).isValid())
            showPreview();
        else
            preview.setVisible (false);
    }
}

void TabView::mouseUp (const juce::MouseEvent& event)
{
    preview.setVisible (false);

    if (auto* pane { event.originalComponent->findParentComponentOfClass<jam::PaneComponent>() })
    {
        const auto relative { event.getEventRelativeTo (pane) };
        const auto offset { event.getOffsetFromDragStart() };
        const auto edge { getEdge (*pane, relative.getMouseDownPosition(), offset) };

        if (edge.isValid())
        {
            if (isInward (edge, offset))
            {
                commandManager->invokeDirectly (isHorizontal (edge) ? map::CommandId::splitVertical : map::CommandId::splitHorizontal, false);
            }
            else
            {
                commandManager->invokeDirectly (getJoinCommands().at (edge), false);
            }
        }
    }
}

//==============================================================================
TabView::Preview::Preview (TabView& newOwner)
    : owner (newOwner)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

void TabView::Preview::paint (juce::Graphics& g)
{
    owner.paintPreview (g);
}

void TabView::showPreview()
{
    if (auto* peer { getPeer() })
    {
        if (not preview.isOnDesktop())
            preview.addToDesktop (juce::ComponentPeer::windowIgnoresMouseClicks | juce::ComponentPeer::windowIgnoresKeyPresses, peer->getNativeHandle());

        preview.setBounds (peer->getAreaCoveredBy (*this));
        preview.setVisible (true);
        preview.toFront (false);
        preview.repaint();
    }
}

void TabView::paintPreview (juce::Graphics& g)
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
                    paintSplitPreview (g, edge, horizontal ? static_cast<float> (cursor.x) / static_cast<float> (pane->getWidth())
                                                                      : static_cast<float> (cursor.y) / static_cast<float> (pane->getHeight()));
                else
                    paintJoinPreview (g, edge);
            }
        }
    }
}

static const juce::String sizeSeparator { " x " };

void TabView::paintSplitPreview (juce::Graphics& g, const juce::Identifier& edge, float position)
{
    const auto focusedBounds { get (getFocusedChild()).getBounds() };
    const bool splitVertical { isHorizontal (edge) };
    const int splitLine { splitVertical
                              ? focusedBounds.getX() + static_cast<int> (position * static_cast<float> (focusedBounds.getWidth()))
                              : focusedBounds.getY() + static_cast<int> (position * static_cast<float> (focusedBounds.getHeight())) };
    const auto head { splitVertical ? focusedBounds.withRight (splitLine) : focusedBounds.withBottom (splitLine) };
    const auto tail { splitVertical ? focusedBounds.withLeft (splitLine) : focusedBounds.withTop (splitLine) };

    const juce::String message { juce::String (head.getWidth()) + sizeSeparator + juce::String (head.getHeight())
                                 + jam::regionSeparator
                                 + juce::String (tail.getWidth()) + sizeSeparator + juce::String (tail.getHeight()) };

    jam::drawMessageOverlay (g, *this, focusedBounds, message, ENDLookAndFeel::getInstance()->getOverlayFont(), splitLine, splitVertical,
                             ConfigModel::getInstance()->getRowValue (jam::Format::toType (Id::pane), Id::splitLine).toString());
}

void TabView::paintJoinPreview (juce::Graphics& g, const juce::Identifier& edge)
{
    const auto focusedBounds { get (getFocusedChild()).getBounds() };
    const auto target { getNeighbour (getFocusedChild(), edge) };

    if (target != jam::UUID::none())
    {
        const auto targetBounds { get (target).getBounds() };
        const auto merged { focusedBounds.getUnion (targetBounds) };

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
    static const jam::HashMap<int, juce::CommandID> areaOptions
    {
        { static_cast<int> (AreaOption::splitVertical), map::CommandId::splitVertical },
        { static_cast<int> (AreaOption::splitHorizontal), map::CommandId::splitHorizontal },
        { static_cast<int> (AreaOption::joinLeft), map::CommandId::joinLeft },
        { static_cast<int> (AreaOption::joinRight), map::CommandId::joinRight },
        { static_cast<int> (AreaOption::joinUp), map::CommandId::joinUp },
        { static_cast<int> (AreaOption::joinDown), map::CommandId::joinDown },
        { static_cast<int> (AreaOption::swapLeft), map::CommandId::swapLeft },
        { static_cast<int> (AreaOption::swapRight), map::CommandId::swapRight },
        { static_cast<int> (AreaOption::swapUp), map::CommandId::swapUp },
        { static_cast<int> (AreaOption::swapDown), map::CommandId::swapDown }
    };

    const auto found { areaOptions.find (result) };

    if (found != areaOptions.end())
    {
        const auto& [option, action] { *found };
        commandManager->invokeDirectly (action, false);
    }
}
