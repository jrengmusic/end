#include "end/Panes.h"
#include "terminal/View.h"
#include "config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

Panes::Panes (jam::UUID uuid, jam::Model& m)
    : jam::Model::Component (m, IDtype::tab)
{
    setName (IDtype::tab.toString());
    setComponentID (uuid.toString());
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::name, juce::String {});

    // const int thickness { config::Model::getInstance()->getValue (IDtype::pane, ID::resizeBarThickness) };
    const int thickness { 4 };
    paneManager.setResizerBarSize (thickness);
}

void Panes::createPane (jam::UUID uuid)
{
    const jam::Font font {
        config::Model::getInstance()->getValue (IDtype::code, ID::fontFamily).toString(),
        static_cast<float> (config::Model::getInstance()->getValue (IDtype::code, ID::fontSize)) };
    auto& session { end::Nexus::getInstance()->create (uuid, font) };
    auto pane { std::make_unique<terminal::View> (uuid, model, session) };
    auto* panePtr { pane.get() };
    addChildComponent (*pane);
    attachments.add (std::make_unique<jam::Model::Attachment> (*pane));
    paneViews.add (std::move (pane));
    paneManager.addLeaf (uuid);
    panePtr->setVisible (isShowing());
    resized();
}

void Panes::resized()
{
    paneManager.layout (getLocalBounds(), paneViews, resizerBars);

    for (auto& bar : resizerBars)
        addAndMakeVisible (*bar);
}

void Panes::visibilityChanged()
{
    if (isShowing())
    {
        for (auto& pane : paneViews)
            pane->setVisible (true);

        resized();
    }
}

void Panes::split (jam::UUID uuid, const juce::Identifier& direction)
{
    jam::UUID newUUID;
    const jam::Font font {
        config::Model::getInstance()->getValue (IDtype::code, ID::fontFamily).toString(),
        static_cast<float> (config::Model::getInstance()->getValue (IDtype::code, ID::fontSize)) };
    auto& session { end::Nexus::getInstance()->create (newUUID, font) };
    auto pane { std::make_unique<terminal::View> (newUUID, model, session) };
    auto* panePtr { pane.get() };
    addChildComponent (*panePtr);
    paneViews.add (std::move (pane));
    paneManager.split (uuid, newUUID, direction);
    resized();
    attachments.add (std::make_unique<jam::Model::Attachment> (*panePtr));
    panePtr->setVisible (isShowing());
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

    end::Nexus::getInstance()->remove (uuid);
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

jam::UUID Panes::getFirstPaneUUID() const noexcept
{
    if (paneViews.size() > 0)
        return jam::UUID (paneViews.at (0)->getComponentID().getLargeIntValue());

    return jam::UUID (0);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
