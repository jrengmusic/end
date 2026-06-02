/**
 * @file PaneView.h
 * @brief Pure virtual base for pane-hosted views (Terminal, Whelmed).
 *
 * PaneView provides the shared interface that Panes uses to manage
 * heterogeneous pane types without type inspection. Both terminal::Display
 * and whelmed::Component inherit from this.
 *
 * @see terminal::Display
 */

#pragma once
#include <JuceHeader.h>
#include "../../AppIdentifier.h"
#include "../../AppModel.h"

/**
 * @class PaneView
 * @brief Pure virtual base for renderable pane views.
 *
 * Inherits juce::Component. Shared between Terminal and Whelmed.
 *
 * @note App-level — not in any namespace.
 */
class PaneView : public juce::Component
{
public:
    PaneView()
    {
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (true);
    }

    ~PaneView() override = default;

    void focusGained (FocusChangeType) override
    {
        AppModel::getContext()->setModalType (0);
        AppModel::getContext()->setSelectionType (0);
        AppModel::getContext()->setActivePaneID (getComponentID());
        AppModel::getContext()->setActivePaneType (getPaneType());
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneView)
};
