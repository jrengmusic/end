#include "end/Panes.h"

namespace end
{
/*____________________________________________________________________________*/

Panes::Panes (jam::UUID uuid, jam::Model& m)
    : jam::Model::Component (m, IDtype::tab)
{
    setName (IDtype::tab.toString());
    setComponentID (uuid.toString());
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::name, juce::String {});

    lookAndFeelChanged();
    registerEvents();
}

terminal::View& Panes::addPaneView (jam::UUID uuid)
{
    // Session needs no font — terminal::View reads font identity from config
    // and applies it to the jam::CodeView it parents.
    auto& session { end::Nexus::getInstance()->create (uuid) };
    auto pane { std::make_unique<terminal::View> (uuid, model, session) };
    auto& view { *pane };

    addChildComponent (*pane);
    attachments.add (std::make_unique<jam::Model::Attachment> (*pane));
    pane->setVisible (isShowing());
    panes.add (std::move (pane));

    return view;
}

void Panes::createPane (jam::UUID uuid)
{
    addPaneView (uuid);
    paneManager.addLeaf (uuid);
    resized();
}

void Panes::resized()
{
    paneManager.layout (getLocalBounds(), panes, resizerBars);

    for (auto& bar : resizerBars)
        addAndMakeVisible (*bar);
}

void Panes::visibilityChanged()
{
    if (isShowing())
    {
        for (auto& pane : panes)
            pane->setVisible (true);

        resized();
    }
}

void Panes::lookAndFeelChanged()
{
    auto& laf { static_cast<end::LookAndFeel&> (getLookAndFeel()) };
    paneManager.setResizerBarSize (laf.getPaneResizerBarSize());
}

void Panes::split (jam::UUID uuid, const juce::Identifier& direction)
{
    jam::UUID newUUID;
    auto& view { addPaneView (newUUID) };
    paneManager.split (uuid, newUUID, direction);
    resized();
    view.toFront (true);
}

void Panes::removePane (jam::UUID uuid)
{
    const auto uuidString { uuid.toString() };

    end::Nexus::getInstance()->remove (uuid);
    paneManager.remove (uuid);
    for (std::size_t i { 0 }; i < panes.size(); ++i)
    {
        if (panes.at (i)->getComponentID() == uuidString)
        {
            attachments.remove (i);
            panes.remove (i);
            break;
        }
    }

    resized();
}

void Panes::registerEvents()
{
    events.add<const juce::Rectangle<int>&, const juce::Rectangle<int>&> (
        ID::paneLeft,
        [] (juce::Rectangle<int> current, juce::Rectangle<int> candidate) -> std::pair<bool, int>
        {
            return { candidate.getRight() <= current.getX(),
                     current.getX() - candidate.getRight() };
        });

    events.add<const juce::Rectangle<int>&, const juce::Rectangle<int>&> (
        ID::paneRight,
        [] (juce::Rectangle<int> current, juce::Rectangle<int> candidate) -> std::pair<bool, int>
        {
            return { candidate.getX() >= current.getRight(),
                     candidate.getX() - current.getRight() };
        });

    events.add<const juce::Rectangle<int>&, const juce::Rectangle<int>&> (
        ID::paneUp,
        [] (juce::Rectangle<int> current, juce::Rectangle<int> candidate) -> std::pair<bool, int>
        {
            return { candidate.getBottom() <= current.getY(),
                     current.getY() - candidate.getBottom() };
        });

    events.add<const juce::Rectangle<int>&, const juce::Rectangle<int>&> (
        ID::paneDown,
        [] (juce::Rectangle<int> current, juce::Rectangle<int> candidate) -> std::pair<bool, int>
        {
            return { candidate.getY() >= current.getBottom(),
                     candidate.getY() - current.getBottom() };
        });
}

PaneView* Panes::findFocusedPane() const
{
    PaneView* focused { nullptr };

    for (auto& pane : panes)
    {
        if (pane->hasKeyboardFocus (true))
        {
            focused = pane.get();
            break;
        }
    }

    return focused;
}

PaneView* Panes::findNearestPane (const juce::Identifier& direction, PaneView* focused) const
{
    const auto current { focused->getBounds() };
    PaneView* nearest { nullptr };
    int bestDistance { std::numeric_limits<int>::max() };

    for (auto& pane : panes)
    {
        if (pane.get() != focused)
        {
            const auto candidate { pane->getBounds() };
            auto [isCandidate, distance] = events.get (direction, current, candidate);

            if (isCandidate and distance < bestDistance)
            {
                bestDistance = distance;
                nearest = pane.get();
            }
        }
    }

    return nearest;
}

void Panes::focusPane (const juce::Identifier& direction)
{
    if (auto* focused { findFocusedPane() })
    {
        if (auto* nearest { findNearestPane (direction, focused) })
            nearest->toFront (true);
    }
}

int Panes::getPaneCount() const noexcept { return static_cast<int> (panes.size()); }

jam::UUID Panes::getFirstPaneUUID() const noexcept
{
    if (panes.size() > 0)
        return jam::UUID (
            static_cast<int64_t> (panes.at (0)->getValueTree().getProperty (jam::ID::id)));

    return jam::UUID (0);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
