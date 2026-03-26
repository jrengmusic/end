/**
 * @file PaneComponent.h
 * @brief Pure virtual base for pane-hosted components (Terminal, Whelmed).
 *
 * PaneComponent provides the shared interface that Panes uses to manage
 * heterogeneous pane types without type inspection.  Both Terminal::Component
 * and Whelmed::Component inherit from this.
 *
 * @see Terminal::Component
 * @see jreng::GLComponent
 */

#pragma once
#include <JuceHeader.h>
#include "../AppState.h"

/**
 * @class PaneComponent
 * @brief Pure virtual base for renderable pane components.
 *
 * Inherits jreng::GLComponent for GL lifecycle hooks.
 * Subclasses implement switchRenderer and applyConfig.
 *
 * @note App-level — not in any namespace. Shared between Terminal and Whelmed.
 */
class PaneComponent : public jreng::GLComponent
{
public:
    /**
     * @enum RendererType
     * @brief Active rendering backend for pane components.
     */
    enum class RendererType
    {
        gpu,  ///< OpenGL accelerated — GLContext, glassmorphism enabled.
        cpu   ///< Software rendered — GraphicsContext, opaque background.
    };

    PaneComponent()
    {
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (true);
    }

    ~PaneComponent() override = default;

    void focusGained (FocusChangeType) override
    {
        AppState::getContext()->setActivePaneUuid (getComponentID());
        AppState::getContext()->setActivePaneType (getPaneType());
    }

    /**
     * @brief Switches the active rendering backend at runtime.
     * @param type  The desired rendering backend.
     * @note MESSAGE THREAD.
     */
    virtual juce::String getPaneType() const noexcept = 0;

    virtual void switchRenderer (RendererType type) = 0;

    /**
     * @brief Returns the pane's root ValueTree for grafting into AppState.
     * @note MESSAGE THREAD.
     */
    virtual juce::ValueTree getValueTree() noexcept = 0;

    /**
     * @brief Applies the current config to the component.
     * @note MESSAGE THREAD.
     */
    virtual void applyConfig() noexcept = 0;

    /**
     * @brief Callback invoked after rendering to trigger a repaint.
     *
     * Set by Panes/Tabs (which receives it from MainComponent).
     *
     * @note MESSAGE THREAD.
     */
    std::function<void()> onRepaintNeeded;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaneComponent)
};
