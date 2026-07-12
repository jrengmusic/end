#include "end/TabView.h"
#include "end/MessageOverlay.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "terminal/TerminalModel.h"

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
    return std::make_unique<TerminalView> (model, state, uuid);
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

            state.setProperty (jam::ID::edge, inward ? edge.toString() : juce::String {}, nullptr);
            state.setProperty (jam::ID::position,
                               horizontal ? static_cast<float> (cursor.getX()) / static_cast<float> (pane->getWidth())
                                          : static_cast<float> (cursor.getY()) / static_cast<float> (pane->getHeight()),
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
        split (juce::Identifier { edge }, state.getProperty (jam::ID::position));
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
        const auto preview { get (getFocusedChild()).getBounds() };
        const bool splitVertical { jam::Position::isVertical (jam::Position::get (edge)) };
        const auto position { static_cast<float> (state.getProperty (jam::ID::position)) };
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
}
