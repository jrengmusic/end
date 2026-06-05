#include "end/Panes.h"

namespace end
{
/*____________________________________________________________________________*/

Panes::Panes (const juce::String& firstPaneUUID)
{
    auto pane { std::make_unique<PaneView> (firstPaneUUID) };
    addAndMakeVisible (*pane);
    paneViews.add (std::move (pane));
    paneManager.addLeaf (firstPaneUUID);
}

void Panes::resized()
{
    paneManager.layout (getLocalBounds(), paneViews, resizerBars);

    for (auto& bar : resizerBars)
        addAndMakeVisible (*bar);
}

void Panes::split (const juce::String& uuid, const juce::String& direction)
{
    auto newUUID { juce::Uuid().toString() };
    auto pane { std::make_unique<PaneView> (newUUID) };
    addAndMakeVisible (*pane);
    paneViews.add (std::move (pane));
    paneManager.split (uuid, newUUID, direction);
    resized();
}

void Panes::removePane (const juce::String& uuid)
{
    for (int i { 0 }; i < static_cast<int> (paneViews.size()); ++i)
    {
        if (paneViews.at (static_cast<size_t> (i))->getComponentID() == uuid)
        {
            paneViews.remove (i);
            break;
        }
    }

    paneManager.remove (uuid);
    resized();
}

int Panes::getPaneCount() const noexcept
{
    return static_cast<int> (paneViews.size());
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
