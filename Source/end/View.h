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

    /** @brief Config-driven button routing for this deep mouse listener —
     *  resolved once per graphics.mouse config change by applyMouseConfig()
     *  into @c mouseEnabled/@c orbitButtonConfig/@c resetButtonConfig below
     *  (cached members, not re-read here — same "resolved value cached on
     *  the consumer" contract as end::LookAndFeel's own glyph-rasterization
     *  members, LookAndFeel.h). @c mouseEnabled false disables every branch
     *  below regardless of button. Otherwise: @c orbitButtonConfig held
     *  drives @c background's orbit camera (mouseDrag()'s own doc comment);
     *  @c resetButtonConfig released with no intervening drag of that SAME
     *  button resets it (mouseUp()'s own doc comment) — the two configured
     *  buttons may be the same (click-vs-drag disambiguation below) or
     *  different (independent gestures, no cross-interference); wheel always
     *  drives zoom (mouseWheelMove()'s own doc comment) — fixed, never
     *  button-configurable. iMouse routing is entirely jam::vulkan::
     *  ShaderComponent's own concern (setMouseConfig(), applyMouseConfig()'s
     *  own doc comment), never this deep listener's.
     *
     *  Captures @p e's own position as the starting point for the next
     *  mouseDrag() delta below, and — on a @c resetButtonConfig down —
     *  clears @c resetButtonDragged so mouseUp() can tell a click from a
     *  drag of that button. Registered as a DEEP mouse listener over the
     *  whole View subtree (addMouseListener (this, true), View::View()) —
     *  jam::vulkan::ShaderComponent (@c background) sits beneath every other
     *  child, so a topmost pane/component normally consumes its own mouse
     *  events first, and background's own mouseDown()/mouseDrag() overrides
     *  never fire; this deep listener additionally observes the SAME event
     *  stream (forwarded, not stolen — the topmost component still handles
     *  its own click normally) purely to feed background's orbit camera.
     *  Never disturbs @c lastOrbitDragPosition's own bookkeeping below,
     *  shared by every button.
     *  @param e  The mouse-down event, in its originating component's own coordinates.
     */
    void mouseDown (const juce::MouseEvent& e) override;

    /** @brief Configured orbit button only, gated on @c mouseEnabled and
     *  @c background.hasMesh() (the active background shader's own
     *  Shader::meshPath non-empty — the data-side signal a mesh-backed orbit
     *  camera exists to feed at all; see mouseDown()'s own doc comment for
     *  why this deep listener exists): forwards this drag's delta since the
     *  last mouseDown()/mouseDrag() call to @c background.addOrbitDelta().
     *  Independently, whenever @c e's own button is @c resetButtonConfig
     *  (whether or not it is ALSO the orbit button), marks
     *  @c resetButtonDragged so the matching mouseUp() knows this was a drag
     *  of the reset button, not a click — tracked separately from the orbit
     *  branch above since @c orbitButtonConfig and @c resetButtonConfig may
     *  differ (RATIFIED SCHEMA), and a drag of the reset button alone must
     *  still disqualify mouseUp()'s own click-reset regardless of whether it
     *  also orbited.
     *  @param e  The mouse-drag event, in its originating component's own coordinates.
     */
    void mouseDrag (const juce::MouseEvent& e) override;

    /** @brief Configured reset button only: resolves the click-vs-drag
     *  distinction — when @c resetButtonDragged was never set by mouseDrag()
     *  above (a click of this button, no intervening drag of the SAME
     *  button), resets @c background's orbit camera to its own defaults; a
     *  preceding drag of this same button already orbited the camera via
     *  mouseDrag() above, so no reset fires on that release. @p e's own mods
     *  reflects the button state just BEFORE this release (juce::MouseEvent::
     *  mods's own doc comment, juce_MouseEvent.h: "When used for mouse-up
     *  events, this will indicate the state of the mouse buttons just before
     *  they were released, so that you can tell which button they let go
     *  of."), so jam::map::MouseButton::isDown() here is checking "this
     *  mouseUp is the configured reset button's own release." Gated on
     *  @c mouseEnabled and @c background.hasMesh() (same gate as mouseDown()/
     *  mouseDrag() above).
     *  @param e  The mouse-up event, in its originating component's own coordinates.
     */
    void mouseUp (const juce::MouseEvent& e) override;

    /** @brief Forwards this wheel event's own vertical delta to
     *  @c background.addZoomDelta(), gated on @c mouseEnabled and
     *  @c background.hasMesh() (same gate and forwarding reasoning as
     *  mouseDrag()'s own doc comment) — the SAME deep-listener event stream
     *  mouseDown()/mouseDrag() already observe (this class's own mouseDown()
     *  doc comment): the deep juce::MouseListener registration
     *  (addMouseListener (this, true), View::View()) calls this override for
     *  wheel events over any nested child, not only over View itself,
     *  exactly as it already does for mouseDown()/mouseDrag(). Coexists with
     *  terminal scrollback under the wheel — both fire from the same event,
     *  accepted double-action. Zoom's own trigger (the wheel) is fixed,
     *  never button-configurable — only @c mouseEnabled gates it.
     *  @param e       The mouse-wheel event, in its originating component's own coordinates.
     *  @param details  The wheel movement's own delta/reversed/smooth/inertial state.
     */
    void
    mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& details) override;

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
     *  zoom (zoomIn, zoomOut, zoomReset — resolve the focused pane's
     *  terminal::Session via ID::focusedPane and end::Nexus::getInstance()->
     *  get(), never through the Panes/View tree, and call terminal::Model's
     *  zoomBy()/setZoom(), step read from display.lua's zoom_step), and
     *  directional pane focus (paneLeft, paneRight, paneUp, paneDown).
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
     *  cheap applyPostProcessParams().
     *
     *  graphics.mouse.enabled/imouse/orbit/reset all route to
     *  applyMouseConfig() — no cost split (no recompile involved at all, a
     *  cache-refresh + one background.setMouseConfig() tell-call; see
     *  applyMouseConfig()'s own doc comment). graphics.mouse.zoom carries no
     *  handler here — fixed/documented-only, never read (display.lua's own
     *  mouse.zoom comment). Defined in EventRegistration.cpp.
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

    /** @brief Gathers graphics.mouse config (enabled/imouse/orbit/reset),
     *  resolves the three button fields via jam::map::MouseButton::get(),
     *  caches @c mouseEnabled/@c orbitButtonConfig/@c resetButtonConfig for
     *  this deep listener's own per-event dispatch (mouseDown()/mouseDrag()/
     *  mouseUp()/mouseWheelMove() above — read on every mouse event, so
     *  cached here rather than re-read from config each time, same
     *  cache-on-config-change contract as end::LookAndFeel's own glyph-
     *  rasterization members), and tells @c background its own matching
     *  enabled/imouse/orbit values via @c setMouseConfig() (jam::vulkan::
     *  ShaderComponent's own iMouse-stamping + local orbit-capture gate — a
     *  separate call site from this deep listener's own dispatch, both
     *  reflecting the SAME resolved config). zoom carries no field read here
     *  — fixed/documented-only. Single gather site (SSOT) for every
     *  mouse-identity config change — shared by initial load (callAsync
     *  block, View::View()) and hot-reload. Defined in EventRegistration.cpp.
     */
    void applyMouseConfig();

    /** @brief Reads tab orientation from config.lua and applies it to tabs. */
    // void setTabOrientation();

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

    /** @brief The last mouseDown()/mouseDrag() event position seen by this
     *  deep mouse listener — mirrors jam::vulkan::ShaderComponent's own
     *  lastDragPosition (jam_VulkanShaderComponent.h), tracked separately
     *  here since this listener observes whichever component actually owns
     *  the event, never background itself directly (mouseDown()'s own doc
     *  comment). */
    juce::Point<float> lastOrbitDragPosition { 0.0f, 0.0f };

    /** @brief Whether a drag of @c resetButtonConfig occurred since the last
     *  mouseDown() of that SAME button — cleared there, set by mouseDrag()
     *  on any motion of that button, read by mouseUp() to distinguish a
     *  click of that button (reset the camera) from a drag of it (already
     *  orbited, no reset). Button-agnostic — mirrors the mechanism this
     *  class always used, now scoped to whichever button graphics.mouse.reset
     *  currently names rather than a hardcoded middle button. */
    bool resetButtonDragged { false };

    /** @brief Cached resolved graphics.mouse config — refreshed by
     *  applyMouseConfig() on every mouse.* config change (and once at
     *  startup, View::View()'s own callAsync block), read by mouseDown()/
     *  mouseDrag()/mouseUp()/mouseWheelMove() above on every mouse event
     *  (applyMouseConfig()'s own doc comment: cached rather than re-read
     *  from config per event). @c mouseEnabled false disables every branch
     *  in those four overrides regardless of button. Defaults match today's
     *  pre-config behaviour (enabled, orbit/reset both middle) so a build
     *  that never fires applyMouseConfig() sees unchanged behaviour. */
    bool mouseEnabled { true };

    /** @brief See @c mouseEnabled's own doc comment. Defaults to
     *  jam::map::MouseButton::Type::middle, matching this class's own
     *  pre-config hardcoded convention. */
    jam::map::MouseButton::Type orbitButtonConfig { jam::map::MouseButton::Type::middle };

    /** @brief See @c mouseEnabled's own doc comment. Defaults to
     *  jam::map::MouseButton::Type::middle, matching this class's own
     *  pre-config hardcoded convention. */
    jam::map::MouseButton::Type resetButtonConfig { jam::map::MouseButton::Type::middle };

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
