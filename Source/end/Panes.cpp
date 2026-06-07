#include "end/Panes.h"

namespace end
{
/*____________________________________________________________________________*/

Panes::Panes (jam::UUID uuid, jam::Model& m)
    : jam::Model::Component (m, IDtype::tab)
{
    setName (IDtype::tab.toString());
    setComponentID (uuid.toString());

    auto pane { std::make_unique<PaneView> (uuid, model) };
    addAndMakeVisible (*pane);
    attachments.add (std::make_unique<jam::Model::Attachment> (*pane));
    paneViews.add (std::move (pane));
    paneManager.addLeaf (uuid.toString());
}

void Panes::resized()
{
    paneManager.layout (getLocalBounds(), paneViews, resizerBars);

    for (auto& bar : resizerBars)
        addAndMakeVisible (*bar);
}

void Panes::split (const juce::String& uuid, const juce::String& direction)
{
    jam::UUID newUUID;
    auto pane { std::make_unique<PaneView> (newUUID, model) };
    addAndMakeVisible (*pane);
    attachments.add (std::make_unique<jam::Model::Attachment> (*pane));
    paneViews.add (std::move (pane));
    paneManager.split (uuid, newUUID.toString(), direction);
    resized();
}

void Panes::removePane (const juce::String& uuid)
{
    for (int i { 0 }; i < static_cast<int> (paneViews.size()); ++i)
    {
        if (paneViews.at (static_cast<size_t> (i))->getComponentID() == uuid)
        {
            attachments.remove (i);
            paneViews.remove (i);
            break;
        }
    }

    paneManager.remove (uuid);
    resized();
}

int Panes::getPaneCount() const noexcept { return static_cast<int> (paneViews.size()); }

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
