/**
 * @file end/View.h
 * @brief Root content component — owns Tabs and Registry, routes keyboard input and ValueTree events.
 */
#pragma once
#include <JuceHeader.h>
#include "end/Tabs.h"
#include "end/MessageOverlay.h"
#include "action/Registry.h"
#include "config/Config.h"
#include "Bimap.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class View
 *  @brief Root content component inside end::Window.
 *
 *  Owns the tab system, action registry, model attachments, and two dispatch
 *  systems wired to ValueTree changes:
 *
 *  - **Action dispatch** — keybinding-triggered callbacks registered in
 *    action::Registry via registerActions(). Key presses are routed through
 *    the Registry's prefix key state machine.
 *
 *  - **Event dispatch** — ValueTree property/type-keyed callbacks stored in the
 *    @c events jam::Function::Map, populated by registerEvents(). Single-key
 *    dispatch in valueTreePropertyChanged(): the changed property is checked
 *    first; if absent from the map, the tree type is used as the fallback key.
 *    The same event handlers drive both initial state (fired via callAsync) and
 *    hot-reload — a single SSOT code path.
 *
 *  Transparent — glass shows through from Window.
 */
class View
    : public juce::Component
    , public jam::Model::Component
    , public juce::ValueTree::Listener
    , public juce::KeyListener
{
public:
    /** @brief Constructs the View and wires all subsystems.
     *
     *  Registers actions and events, seeds the initial packed size property,
     *  creates model attachments for View and Tabs state, adds this as a
     *  listener to config and model trees, applies the initial tab orientation,
     *  and opens the first tab.
     *
     *  @param m  Shared jam::Model that owns the application state tree.
     */
    explicit View (jam::Model& m);

    /** @brief Destructs the View, removing all listeners and the key listener. */
    ~View() override;

    /** @brief Updates view-state dimensions and lays out the background, tabs,
     *         and the message overlay.
     *
     *  Packs current width + height into ID::size via @c setViewState (single atomic
     *  VT write), then bounds @c background, @c tabs, and @c messageOverlay to
     *  @c getLocalBounds().
     */
    void resized() override;

    /** @brief Routes key presses to the action registry.
     *  @param key                    The key press event.
     *  @param originatingComponent   Component that originated the key press (unused).
     *  @return true if consumed by an action binding.
     */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    /** @brief Single-key dispatch through the events map.
     *
     *  Checks whether @p property is a key in @c events; if so, dispatches
     *  with @p property. Otherwise falls back to @p tree.getType() as the
     *  lookup key. Handles config, theme, focus, and window property changes.
     *
     *  @param tree      The ValueTree whose property changed.
     *  @param property  The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Sets the focusedPane state when a new tab's pane is added.
     *
     *  Reads the pane id from @p childWhichHasBeenAdded and writes it to the
     *  view state tree as @c ID::focusedPane.
     *
     *  @param parentTree                The parent tree the child was added to.
     *  @param childWhichHasBeenAdded    The newly added child tree (a tab tree
     *                                   containing an @c IDtype::pane subtree).
     */
    void valueTreeChildAdded (juce::ValueTree& parentTree,
                              juce::ValueTree& childWhichHasBeenAdded) override;

private:
    // /** @brief Singleton config model reference. */
    config::Model& config { *config::Model::getInstance() };

    //==============================================================================
    /** @brief Creates the packed ID::size parameter and grafts View, Tabs, and
     *         MessageOverlay state into the model tree via Attachment.
     *
     *  Reads initial window size from config, packs into jam::Size\<int16_t\>, creates
     *  a Parameter\<int\> on the view state for ID::size, then constructs
     *  Attachments for View, Tabs, and MessageOverlay. Called once from
     *  the constructor after registerEvents().
     */
    void createAndAttachParameters();

    /** @brief Populates action::Registry with keybinding-triggered callbacks.
     *
     *  Registers actions for tab navigation (newTab, closeTab, nextTab, prevTab),
     *  pane splitting (splitHorizontal, splitVertical), pane closing (closePane),
     *  and directional pane focus (paneLeft, paneRight, paneUp, paneDown).
     *  Defined in ActionRegistration.cpp.
     */
    void registerActions();

    /** @brief Populates the events map with ValueTree property/type-keyed callbacks.
     *
     *  Registers handlers for: tabOrientation (applies tab orientation from
     *  config.lua config), focus (updates focusedPane state), theme (propagates
     *  LookAndFeel change), alwaysOnTop and titleBarButtons (dispatch to
     *  jam::Window), gpu (toggles jam::VulkanEngine::getInstance()'s
     *  setGpuEnabled() from config and probe result, the post-process
     *  background-blur shader via jam::BackgroundBlur::setEnabled(), and both
     *  applyBackground()/applyPostProcess() — the VulkanEngine itself
     *  is owned and constructed once by end::Application, never by View, and
     *  is never reset/reconstructed here; see end::Application's vulkanEngine
     *  doc comment, Main.h). background/backgroundOpacity/frameRate/
     *  backgroundResolution route to applyBackground(); postProcessing/
     *  postProcessingOpacity/postProcessingResolution route to
     *  applyPostProcess(); filter (shared by both slots) routes to both.
     *  Message display is handled directly by MessageOverlay via ParameterAttachment.
     *
     *  Font-identity config coverage (fontRasterizer/fontGamma/fontContrast,
     *  the only config.lua values that change a glyph's rasterized bitmap for an
     *  otherwise-unchanged jam::GlyphAtlas::Key) is owned by end::LookAndFeel —
     *  the font owner — not View; see LookAndFeel::registerEvents()'s doc
     *  comment for the full audit of every glyph-identity config value.
     *
     *  Two funnel pairs, each split by cost: background/gpu/filter route to
     *  applyBackground() (full recompile — project identity, GPU toggle, and
     *  filter all change what gets baked into the compiled SPIR-V), while
     *  backgroundOpacity/frameRate/backgroundResolution route to the cheap
     *  applyBackgroundParams() (no recompile, jam::vulkan::ShaderComponent::setParams()).
     *  postProcessing/gpu/filter route to applyPostProcess() (full); filter
     *  is shared by both slots so its handler calls both full funnels; gpu
     *  likewise. postProcessingOpacity/postProcessingResolution route to the
     *  cheap applyPostProcessParams(). Defined in EventRegistration.cpp.
     */
    void registerEvents();

    /** @brief Gathers current config values and the effective GPU state
     *  (config preference ANDed with jam::GpuProbe::probe().isAvailable — the
     *  same effective truth end::Application resolves for the VulkanEngine ctor
     *  and the gpu event handler resolves for setGpuEnabled()), compiles via
     *  @c jam::vulkan::ShaderCompiler when GPU-enabled and a project is configured, and
     *  installs the result on @c background via its @c setShader() tell-API
     *  (@c nullptr on GPU-off/no-project; a failed compile calls nothing,
     *  keeping @c background's last-good shader). Full recompile — routed to
     *  by project/GPU/filter changes only (see registerEvents()'s doc
     *  comment); opacity/resolution/frame-rate-only changes route to the
     *  cheaper applyBackgroundParams() instead. Single gather site (SSOT) for
     *  every background-identity config change — shared by initial load
     *  (transitively, via the gpu handler firing at startup) and hot-reload.
     *  Defined in EventRegistration.cpp.
     */
    void applyBackground();

    /** @brief Cheap parameter-only update — no recompile. Gathers
     *  opacity/resolutionScale/frameRate and forwards them to
     *  @c background via its @c setParams() tell-API. Defined in
     *  EventRegistration.cpp. */
    void applyBackgroundParams();

    /** @brief Gathers current post-processing config values and the effective
     *  GPU state and installs (or clears) jam::VulkanEngine's app-global
     *  post-process chain accordingly. Full recompile — routed to by
     *  project/GPU/filter changes only (see registerEvents()'s doc comment);
     *  opacity/resolution-only changes route to the cheaper
     *  applyPostProcessParams() instead. Single gather site (SSOT) for every
     *  post-process-identity config change — shared by initial load
     *  (transitively, via the gpu handler firing at startup) and hot-reload.
     *  Defined in EventRegistration.cpp.
     */
    void applyPostProcess();

    /** @brief Cheap parameter-only update — no recompile. Gathers
     *  opacity/resolutionScale and forwards them to
     *  jam::VulkanEngine::setPostProcessParams(). Defined in
     *  EventRegistration.cpp. */
    void applyPostProcessParams();

    /** @brief Reads tab orientation from config.lua and applies it to tabs. */
    void setTabOrientation();

    /** @brief Packs width + height into jam::Size<int16_t> and writes as a single int
     *         property (ID::size) on the view state tree.
     *  @param size   Current component width and height in pixels.
     */
    void setViewState (jam::Size<int16_t> size);

    //==============================================================================
    /** @brief Action registry — maps key bindings to action callbacks. */
    action::Registry registry;

    /** @brief Self-managed background shader component — added as the
     *  rearmost child (View::View()) so every other child paints over it.
     */
    jam::vulkan::ShaderComponent background;

    /** @brief Tab system — owns panes and terminal sessions. */
    Tabs tabs;

    /** @brief Transient message display triggered by config load events. */
    MessageOverlay messageOverlay;

    /** @brief Model attachments grafting View and Tabs state into the jam::Model tree. */
    jam::Owner<jam::Model::Attachment> attachments;

    /** @brief Event dispatch map — keyed by juce::Identifier (property or tree type).
     *
     *  Callbacks receive the changed ValueTree. Dispatched via single-key lookup
     *  in valueTreePropertyChanged(): property key takes priority, tree type is
     *  the fallback.
     */
    jam::Function::Map<juce::Identifier, void> events;

//==============================================================================
#if JUCE_DEBUG
    /** @brief ValueTree inspector widget, debug builds only. */
    // jam::debug::Widget widget { this, config.state, false };
#endif
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
