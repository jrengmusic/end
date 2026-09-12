#include "end/TabView.h"
#include "action/ENDActions.h"

// Prefix written to state's edge property while a swap-pick target is pending.
static constexpr const char* swapPickPrefix { "swap:" };

static constexpr float joinPreviewSentinel { -1.0f };

TabView::TabView (jam::UUID uuid, jam::Model& m, juce::ValueTree sessionState)
    : jam::MatrixComponent (m, sessionState, Id::toType (Id::tab), uuid)
{
    setName (Id::toType (Id::tab).toString());
    model.createAndAddParameter<jam::ParameterText> (state, Id::name, juce::String {});
    model.createAndAddParameter<jam::ParameterText> (state, Id::edge, juce::String {});
    model.createAndAddParameter<jam::Parameter<float>> (state, Id::position, 0.0f);
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
        get (target).toFront (true);
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

//==============================================================================
void TabView::mouseDown (const juce::MouseEvent& event)
{
    const auto edge { state.getProperty (Id::edge).toString() };

    if (edge.startsWith (swapPickPrefix))
    {
        if (event.mods.isPopupMenu())
        {
            state.setProperty (Id::edge, juce::String {}, nullptr);
            repaint();
        }
        else if (auto* pane { dynamic_cast<jam::PaneComponent*> (event.originalComponent) })
        {
            const auto source { jam::UUID { juce::var { edge.fromFirstOccurrenceOf (swapPickPrefix, false, false) } } };
            const auto target { jam::UUID { pane->getValueTree().getProperty (Id::id) } };

            if (source != jam::UUID::none() and target != source)
                MatrixComponent::swap (source, target);

            state.setProperty (Id::edge, juce::String {}, nullptr);
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

            const juce::Identifier edge { horizontal ? (corner.getX() == 0 ? Id::left : Id::right)
                                                     : (corner.getY() == 0 ? Id::top : Id::bottom) };

            const bool inward { isLow (map::Position::getInstance()->get (edge.toString()))
                                == ((horizontal ? delta.getX() : delta.getY()) > 0) };

            state.setProperty (Id::edge, edge.toString(), nullptr);
            state.setProperty (Id::position,
                               inward ? (horizontal ? static_cast<float> (cursor.getX()) / static_cast<float> (pane->getWidth())
                                                    : static_cast<float> (cursor.getY()) / static_cast<float> (pane->getHeight()))
                                      : joinPreviewSentinel,
                               nullptr);

            repaint();
        }
    }
}

void TabView::mouseUp (const juce::MouseEvent& event)
{
    const auto edge { state.getProperty (Id::edge).toString() };

    if (edge.isNotEmpty())
    {
        const auto position { static_cast<float> (state.getProperty (Id::position)) };

        if (position != joinPreviewSentinel)
            split (juce::Identifier { edge }, position);
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

            ENDActions::getInstance()->run (joinActions.at (map::Position::getInstance()->get (edge)));
        }

        state.setProperty (Id::edge, juce::String {}, nullptr);
        repaint();
    }
}

//==============================================================================
void TabView::paintOverChildren (juce::Graphics& g)
{
    const auto edge { state.getProperty (Id::edge).toString() };

    if (edge.isNotEmpty())
    {
        if (edge.startsWith (swapPickPrefix))
        {
            drawMessageOverlay (g, *this, getLocalBounds(), "Click a pane to swap");
        }
        else
        {
            const auto position { static_cast<float> (state.getProperty (Id::position)) };

            if (position != joinPreviewSentinel)
                paintSplitPreview (g, edge, position);
            else
                paintJoinPreview (g, edge);
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
    const auto metrics { ENDLookAndFeel::getInstance()->getCodeMetrics (EditorView::defaultZoom) };

    const auto head { splitVertical ? preview.withRight (splitLine) : preview.withBottom (splitLine) };
    const auto tail { splitVertical ? preview.withLeft (splitLine) : preview.withTop (splitLine) };

    const juce::String message { juce::String (static_cast<int> (head.getWidth() / metrics.cellWidth)) + " x "
                                 + juce::String (static_cast<int> (head.getHeight() / metrics.cellHeight))
                                 + " | "
                                 + juce::String (static_cast<int> (tail.getWidth() / metrics.cellWidth)) + " x "
                                 + juce::String (static_cast<int> (tail.getHeight() / metrics.cellHeight)) };

    drawMessageOverlay (g, *this, preview, message, splitLine, splitVertical);
}

void TabView::paintJoinPreview (juce::Graphics& g, const juce::String& edge)
{
    const auto preview { get (getFocusedChild()).getBounds() };
    const auto target { getNeighbour (getFocusedChild(), juce::Identifier { edge }) };

    if (target != jam::UUID::none())
    {
        const auto targetBounds { get (target).getBounds() };
        const auto merged { preview.getUnion (targetBounds) };
        const auto metrics { ENDLookAndFeel::getInstance()->getCodeMetrics (EditorView::defaultZoom) };

        const juce::String message { juce::String (static_cast<int> (merged.getWidth() / metrics.cellWidth)) + " x "
                                     + juce::String (static_cast<int> (merged.getHeight() / metrics.cellHeight)) };

        drawMessageOverlay (g, *this, targetBounds, message);
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
    menu.addItem (static_cast<int> (AreaOption::swapPick), "Swap Areas");

    return menu;
}

void TabView::handleAreaOptionsResult (int result)
{
    static const auto areaOptions {
        []
        {
            jam::Function::Map<AreaOption, void> options;
            options.add<TabView&> (AreaOption::splitVertical, [] (TabView& tab) { tab.split (Id::left, 0.5f); });
            options.add<TabView&> (AreaOption::splitHorizontal, [] (TabView& tab) { tab.split (Id::top, 0.5f); });
            options.add<TabView&> (AreaOption::joinLeft, [] (TabView&) { ENDActions::getInstance()->run (Id::joinLeft); });
            options.add<TabView&> (AreaOption::joinRight, [] (TabView&) { ENDActions::getInstance()->run (Id::joinRight); });
            options.add<TabView&> (AreaOption::joinUp, [] (TabView&) { ENDActions::getInstance()->run (Id::joinUp); });
            options.add<TabView&> (AreaOption::joinDown, [] (TabView&) { ENDActions::getInstance()->run (Id::joinDown); });
            options.add<TabView&> (AreaOption::swapPick, [] (TabView& tab)
            {
                tab.state.setProperty (Id::edge, juce::String (swapPickPrefix) + tab.getFocusedChild().toString(), nullptr);
                tab.repaint();
            });
            return options;
        }()
    };

    areaOptions.get (static_cast<AreaOption> (result), *this);
}
