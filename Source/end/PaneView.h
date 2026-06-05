/**
 * @file end/PaneView.h
 * @brief Terminal pane stub — base for terminal::View in Phase 4.
 */
#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class PaneView
 *  @brief Stub pane component with ValueTree node identity.
 *
 *  Each PaneView owns a PANE node (via ComponentWithID) identified by a UUID.
 *  PaneManager matches leaves to PaneViews by componentID.
 *  Empty in Phase 3 — terminal::View subclasses this in Phase 4.
 */
class PaneView
    : public juce::Component
    , public jam::ValueTree::ComponentWithID<PaneView>
{
public:
    /** @brief Constructs a pane with the given UUID identity.
     *  @param uuid  Unique identifier — stored as componentID for PaneManager matching.
     */
    explicit PaneView (const juce::String& uuid)
        : jam::ValueTree::ComponentWithID<PaneView> (IDtype::pane, uuid)
    {
        setOpaque (false);
    }

    void resized() override {}
    void paint (juce::Graphics&) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneView)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
