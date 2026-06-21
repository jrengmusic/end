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
 *  @brief Stub pane component with ValueTree identity.
 *
 *  Each PaneView owns a PANE tree (via jam::Model::Component) identified by a UUID.
 *  Seeds focus=0 and stores the UUID as both componentID (for PaneManager matching)
 *  and as an ID::id VT property (for model tree inspection).
 *  Empty in Phase 3 — terminal::View subclasses this in Phase 4.
 */
class PaneView
    : public juce::Component
    , public jam::Model::Component
{
public:
    /** @brief Constructs a pane with the given UUID identity.
     *  @param uuid   Unique identifier — stored as componentID and ID::id VT property.
     *  @param model  Model reference — forwarded to Component.
     */
    PaneView (jam::UUID uuid, jam::Model& model)
        : jam::Model::Component (model, IDtype::pane)
    {
        setName (IDtype::pane.toString());
        state.setProperty (jam::ID::id, uuid.value, nullptr);
        setComponentID (uuid.toString());
        setOpaque (false);
        setWantsKeyboardFocus (true);
        model.createAndAddParameter<jam::Parameter<int>> (state, ID::focus, 0);
    }

    void resized() override {}
    void paint (juce::Graphics&) override {}

    void visibilityChanged() override
    {
        if (isShowing())
            toFront (true);
    }

    /** @brief Sets focus=1 on this pane's state tree when keyboard focus is gained. */
    void focusGained (FocusChangeType) override
    {
        state.setProperty (ID::focus, 1, nullptr);
    }

    /** @brief Sets focus=0 on this pane's state tree when keyboard focus is lost. */
    void focusLost (FocusChangeType) override
    {
        state.setProperty (ID::focus, 0, nullptr);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneView)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
