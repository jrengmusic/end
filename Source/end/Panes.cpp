#include "end/Panes.h"
#include "config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

Panes::Panes (jam::UUID uuid, jam::Model& m)
    : jam::Model::Component (m, IDtype::tab)
{
    setName (IDtype::tab.toString());
    setComponentID (uuid.toString());
    state.setProperty (jam::ID::name, juce::String {}, nullptr);

    const int thickness { config::Model::getInstance()->getValue (IDtype::pane, ID::resizeBarThickness) };
    paneManager.setResizerBarSize (thickness);

    auto pane { std::make_unique<PaneView> (uuid, model) };
    addAndMakeVisible (*pane);
    attachments.add (std::make_unique<jam::Model::Attachment> (*pane));
    paneViews.add (std::move (pane));
    paneManager.addLeaf (uuid);
}

void Panes::resized()
{
    paneManager.layout (getLocalBounds(), paneViews, resizerBars);

    for (auto& bar : resizerBars)
        addAndMakeVisible (*bar);
}

void Panes::split (jam::UUID uuid, const juce::Identifier& direction)
{
    jam::UUID newUUID;
    auto pane { std::make_unique<PaneView> (newUUID, model) };
    auto* panePtr { pane.get() };
    addAndMakeVisible (*panePtr);
    paneViews.add (std::move (pane));
    paneManager.split (uuid, newUUID, direction);
    resized();
    attachments.add (std::make_unique<jam::Model::Attachment> (*panePtr));
    panePtr->toFront (true);
}

void Panes::removePane (jam::UUID uuid)
{
    const auto uuidString { uuid.toString() };

    for (int i { 0 }; i < static_cast<int> (paneViews.size()); ++i)
    {
        if (paneViews.at (static_cast<size_t> (i))->getComponentID() == uuidString)
        {
            attachments.remove (i);
            paneViews.remove (i);
            break;
        }
    }

    paneManager.remove (uuid);
    resized();
}

void Panes::focusPane (const juce::Identifier& direction)
{
    juce::Component* focused { nullptr };

    for (auto& pane : paneViews)
    {
        if (pane->hasKeyboardFocus (true))
        {
            focused = pane.get();
            break;
        }
    }

    if (focused != nullptr)
    {
        const auto current { focused->getBounds() };
        juce::Component* nearest { nullptr };
        int bestDistance { std::numeric_limits<int>::max() };

        for (auto& pane : paneViews)
        {
            if (pane.get() == focused)
                continue;

            const auto candidate { pane->getBounds() };
            bool isCandidate { false };
            int distance { 0 };

            if (direction == ID::paneLeft)
            {
                isCandidate = candidate.getRight() <= current.getX();
                distance = current.getX() - candidate.getRight();
            }
            else if (direction == ID::paneRight)
            {
                isCandidate = candidate.getX() >= current.getRight();
                distance = candidate.getX() - current.getRight();
            }
            else if (direction == ID::paneUp)
            {
                isCandidate = candidate.getBottom() <= current.getY();
                distance = current.getY() - candidate.getBottom();
            }
            else if (direction == ID::paneDown)
            {
                isCandidate = candidate.getY() >= current.getBottom();
                distance = candidate.getY() - current.getBottom();
            }

            if (isCandidate and distance < bestDistance)
            {
                bestDistance = distance;
                nearest = pane.get();
            }
        }

        if (nearest != nullptr)
            nearest->toFront (true);
    }
}

int Panes::getPaneCount() const noexcept { return static_cast<int> (paneViews.size()); }

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
