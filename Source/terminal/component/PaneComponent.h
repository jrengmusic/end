/**
 * @file PaneComponent.h
 * @brief Pure virtual base for pane-hosted components (Terminal, Whelmed).
 *
 * PaneComponent provides the shared interface that Panes uses to manage
 * heterogeneous pane types without type inspection.  Both terminal::Display
 * and whelmed::Component inherit from this.
 *
 * @see terminal::Display
 */

#pragma once
#include <JuceHeader.h>
#include "../../AppIdentifier.h"
#include "../../AppState.h"

/**
 * @class PaneComponent
 * @brief Pure virtual base for renderable pane components.
 *
 * Inherits juce::Component. Shared between Terminal and Whelmed.
 *
 * @note App-level — not in any namespace.
 */
class PaneComponent : public juce::Component
{
public:
    PaneComponent()
    {
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (true);
    }

    ~PaneComponent() override = default;

    void focusGained (FocusChangeType) override
    {
        AppState::getContext()->setModalType (0);
        AppState::getContext()->setSelectionType (0);
        AppState::getContext()->setActivePaneID (getComponentID());
        AppState::getContext()->setActivePaneType (getPaneType());
    }

    /**
     * @brief Returns a string identifier for the pane type ("terminal" or "document").
     *
     * Used by Panes and Tabs to distinguish component types without RTTI.
     *
     * @return The pane type string — see Map::PaneType.
     * @note MESSAGE THREAD.
     */
    virtual juce::String getPaneType() const noexcept = 0;

    /**
     * @brief Returns the pane's root ValueTree for grafting into AppState.
     * @note MESSAGE THREAD.
     */
    virtual juce::ValueTree getValueTree() noexcept = 0;

    /**
     * @brief Applies the given zoom factor to the pane's rendering.
     * @param zoom  Zoom multiplier (1.0 = default).
     * @note MESSAGE THREAD.
     */
    virtual void applyZoom (float zoom) noexcept = 0;

    /** @brief Enters vim-style keyboard selection mode. @note MESSAGE THREAD. */
    virtual void enterSelectionMode() noexcept = 0;

    /** @brief Copies the active selection to the system clipboard and clears selection. @note MESSAGE THREAD. */
    virtual void copySelection() noexcept = 0;

    /** @brief Returns true if there is an active text selection. @note MESSAGE THREAD. */
    virtual bool hasSelection() const noexcept = 0;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneComponent)
};
