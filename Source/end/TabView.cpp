#include "end/TabView.h"
#include "end/MessageOverlay.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "config/ConfigModel.h"
#include "terminal/TerminalModel.h"
#include "Bimap.h"

TabView::TabView (jam::UUID uuid, jam::Model& m, juce::ValueTree sessionState)
    : jam::MatrixComponent (m, sessionState, IDtype::tab, uuid)
{
    setName (IDtype::tab.toString());
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::name, juce::String {});
    addMouseListener (this, true);
}

//==============================================================================
jam::UUID TabView::add()
{
    jam::UUID uuid {};
    auto pane { std::make_unique<TerminalView> (model, state, uuid) };

    MatrixComponent::add (uuid, std::move (pane));

    return uuid;
}

jam::UUID TabView::add (const juce::Identifier& edge, float position)
{
    return split (edge, position);
}

//==============================================================================
std::unique_ptr<jam::OwnedComponent> TabView::createChild (jam::UUID uuid)
{
    return std::make_unique<TerminalView> (model, state, uuid);
}

//==============================================================================
void TabView::remove (jam::UUID uuid)
{
    auto& pane { MatrixComponent::get (uuid) };
    state.removeChild (pane.getValueTree(), nullptr);
    MatrixComponent::remove (uuid);
}

//==============================================================================
TerminalView* TabView::findFocusedPane() const
{
    for (auto& [uuid, child] : getChildren())
        if (child->hasKeyboardFocus (true))
            return static_cast<TerminalView*> (child.get());

    return nullptr;
}

TerminalView*
TabView::findNearestPane (const juce::Identifier& direction, TerminalView* focused) const
{
    return nullptr;
}

//==============================================================================
void TabView::focusPane (const juce::Identifier& direction)
{
    auto* focused { findFocusedPane() };
    auto* nearest { findNearestPane (direction, focused) };

    if (focused != nullptr and nearest != nullptr)
        nearest->toFront (true);
}

//==============================================================================
TerminalView& TabView::get (jam::UUID uuid)
{
    return static_cast<TerminalView&> (MatrixComponent::get (uuid));
}

TerminalView& TabView::get()
{
    jassert (getChildCount() > 0);
    auto& [uuid, child] { *getChildren().begin() };
    return static_cast<TerminalView&> (*child);
}

//==============================================================================
void TabView::mouseDown (const juce::MouseEvent& event)
{
    if (event.eventComponent != this)
    {
        auto* pane { dynamic_cast<jam::PaneComponent*> (event.eventComponent) != nullptr
                         ? dynamic_cast<jam::PaneComponent*> (event.eventComponent)
                         : event.eventComponent->findParentComponentOfClass<jam::PaneComponent>() };

        if (pane != nullptr)
        {
            const auto cursor { event.getEventRelativeTo (pane).getPosition() };
            const auto bounds { pane->getLocalBounds() };

            if ((cursor.getX() < cornerZoneExtent or cursor.getX() > bounds.getWidth() - cornerZoneExtent)
                and (cursor.getY() < cornerZoneExtent or cursor.getY() > bounds.getHeight() - cornerZoneExtent))
            {
                target = jam::UUID { static_cast<int64_t> (pane->getComponentID().getLargeIntValue()) };
                gestureStart = event.getEventRelativeTo (this).getPosition();
            }
        }
    }
}

void TabView::mouseDrag (const juce::MouseEvent& event)
{
    if (target != jam::UUID::none())
    {
        const auto cursor { event.getEventRelativeTo (this).getPosition() };
        const auto delta { cursor - gestureStart };

        if (std::abs (delta.getX()) >= splitDragThreshold or std::abs (delta.getY()) >= splitDragThreshold)
        {
            const auto bounds { get (target).getBounds() };
            const bool horizontal { std::abs (delta.getX()) > std::abs (delta.getY()) };

            const juce::Identifier edge { horizontal
                                              ? (gestureStart.getX() - bounds.getX() < cornerZoneExtent ? jam::ID::left : jam::ID::right)
                                              : (gestureStart.getY() - bounds.getY() < cornerZoneExtent ? jam::ID::top : jam::ID::bottom) };

            const bool inward { edge == jam::ID::left    ? delta.getX() > 0
                                 : edge == jam::ID::right ? delta.getX() < 0
                                 : edge == jam::ID::top   ? delta.getY() > 0
                                                            : delta.getY() < 0 };

            splitVertical = horizontal;
            splitLine = inward ? (horizontal ? cursor.getX() : cursor.getY()) : -1;
            preview = inward ? bounds : juce::Rectangle<int> {};

            repaint();
        }
    }
}

void TabView::mouseUp (const juce::MouseEvent& event)
{
    if (target != jam::UUID::none())
    {
        if (not preview.isEmpty())
        {
            const auto bounds { get (target).getBounds() };
            const juce::Identifier edge { splitVertical
                                              ? (gestureStart.getX() - bounds.getX() < cornerZoneExtent ? jam::ID::left : jam::ID::right)
                                              : (gestureStart.getY() - bounds.getY() < cornerZoneExtent ? jam::ID::top : jam::ID::bottom) };
            const auto position { splitVertical
                                       ? static_cast<float> (splitLine - bounds.getX()) / static_cast<float> (bounds.getWidth())
                                       : static_cast<float> (splitLine - bounds.getY()) / static_cast<float> (bounds.getHeight()) };

            add (edge, position);
            preview = {};
        }

        target = jam::UUID::none();
        splitLine = -1;
        repaint();
    }
}

//==============================================================================
void TabView::paintOverChildren (juce::Graphics& g)
{
    if (not preview.isEmpty())
    {
        const auto metrics { ENDLookAndFeel::getInstance()->getCodeMetrics (TerminalModel::defaultZoom) };

        const auto width1 { splitVertical ? splitLine - preview.getX() : preview.getWidth() };
        const auto height1 { splitVertical ? preview.getHeight() : splitLine - preview.getY() };
        const auto width2 { splitVertical ? preview.getRight() - splitLine : preview.getWidth() };
        const auto height2 { splitVertical ? preview.getHeight() : preview.getBottom() - splitLine };

        const juce::String message { juce::String (static_cast<int> (width1 / metrics.cellWidth)) + " x "
                                     + juce::String (static_cast<int> (height1 / metrics.cellHeight))
                                     + " | "
                                     + juce::String (static_cast<int> (width2 / metrics.cellWidth)) + " x "
                                     + juce::String (static_cast<int> (height2 / metrics.cellHeight)) };

        const auto family { ConfigModel::getInstance()->getValue (jam::IDtype::overlay, ID::fontFamily).toString() };
        const auto size { static_cast<float> (ConfigModel::getInstance()->getValue (jam::IDtype::overlay, ID::fontSize)) };
        const juce::Font font { juce::FontOptions (family, size, juce::Font::plain) };

        const auto background { findColour (juce::Label::backgroundColourId).withAlpha (backgroundAlpha) };
        const auto foreground { findColour (juce::Label::textColourId) };

        const auto lineStyle { OverlayAxisLine::get (
            ConfigModel::getInstance()->getValue (IDtype::pane, ID::splitLine).toString()) };

        drawMessageOverlay (g, preview, message, font, background, foreground, splitLine, splitVertical, lineStyle);
    }
}
