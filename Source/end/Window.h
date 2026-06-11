/**
 * @file end/Window.h
 * @brief Config-reactive DocumentWindow for END.
 *
 * end::Window extends jam::Window with listener-driven style application.
 * It follows the same architectural pattern as ProcessorChain::parameters:
 * a jam::Function::Map keyed on juce::Identifier stores one typed callable
 * per config property, registered as @c add<juce::ValueTree>.  Each callable
 * receives the pre-resolved VT node (WINDOW node or BLUR_STYLE node) and reads
 * its one property directly from it.  Node resolution lives exclusively in
 * setStyle(property), which is the single source of tree navigation.
 * valueTreePropertyChanged() forwards to setStyle(property); setStyle() calls
 * setStyle(property) once per registered property to apply the initial config state.
 *
 * Config tree shape navigated by the dispatcher:
 * @code
 * CONFIG
 *   DISPLAY  (IDtype::display)
 *     WINDOW  (IDtype::window)
 *       colour, blur_radius, always_on_top, buttons, width, height
 *       BLUR_STYLE  (IDtype::blurStyle)
 *         mac, win
 * @endcode
 */
#pragma once
#include <JuceHeader.h>
#include "config/Config.h"
#include "lookAndFeel/LookAndFeel.h"
#include "Identifier.h"
#include "Map.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Window
 *  @brief Config-reactive window that reacts to config tree mutations.
 *
 *  Inherits jam::Window for glassmorphism and juce::ValueTree::Listener
 *  to react to config tree mutations.  Each WINDOW-section config property is
 *  mapped to a typed callable in styleParameters (the analog of
 *  ProcessorChain::parameters).  registerStyleParameters() registers one inline
 *  lambda per property; setStyle() dispatches to the registered callable for each
 *  property to apply the initial state; valueTreePropertyChanged() forwards to
 *  setStyle() on each subsequent change.
 *
 *  Ownership: constructed and owned by end::Application.
 *  Lifecycle: config listener is added in the ctor and removed in the dtor.
 */
class Window
    : public jam::Window
    , public juce::ValueTree::Listener
{
public:
    /** @brief Constructs the window and applies config-driven style.
     *
     *  Builds the styleParameters map inline, then calls setStyle() to apply
     *  the initial config state.  The config listener is added before setStyle()
     *  so that any tree mutations during construction are observed.
     *
     *  @param mainComponent  Content component — ownership transferred to jam::Window.
     *  @param name           Window title string.
     *  @param alwaysOnTop    Initial always-on-top hint (overridden by setStyle).
     *  @param showWindowButtons  Initial chrome hint (overridden by setStyle).
     */
    Window (juce::Component* mainComponent,
            const juce::String& name,
            bool alwaysOnTop,
            bool showWindowButtons);

    /** @brief Removes the config listener. */
    ~Window() override;

    /** @brief Dispatches a single property change to its registered style callable.
     *
     *  If the changed property has a registered callable in styleParameters,
     *  setStyle(property) is invoked to decode the value and dispatch it.
     *  Unregistered properties are silently ignored — this listener observes
     *  the entire config tree, not just WINDOW properties.
     *
     *  @param tree      The ValueTree that changed (unused — dispatcher reads fresh).
     *  @param property  The identifier of the property that changed.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    config::Model& config { *config::Model::getInstance() };

    void registerStyleParameters();

    /** @brief Resolves the config node and dispatches it to the registered
     *  styleParameter callable for the given property.
     *
     *  Two-branch dispatch: @c mac and @c win live one level deeper under
     *  IDtype::blurStyle and receive that child node; all other properties
     *  receive the WINDOW node directly.  The lambda reads its one property
     *  from the passed node — no further navigation in the callable.
     *
     *  Used by both the ValueTree::Listener entry point (valueTreePropertyChanged)
     *  and the init path (setStyle iterates over all registered properties and
     *  calls this for each).
     *
     *  @param property  Identifier of the property to resolve and dispatch.
     */
    void setStyle (const juce::Identifier& property);

    jam::Function::Map<juce::Identifier, void> styleParameters;

    /** @brief Cached tint colour — last value applied via setGlass.
     *  Updated by the @c colour styleParameter, read by every glass-related
     *  setter to keep setGlass arguments consistent.
     */
    juce::Colour tintColour { juce::Colours::black };

    /** @brief Cached blur radius — last value applied via setGlass.
     *  Updated by the @c blurRadius styleParameter, read by every glass-related
     *  setter to keep setGlass arguments consistent.
     */
    float blurRadius { 0.0f };

    /** @brief Cached blur backend — last value applied via setGlass.
     *  Updated by the @c mac / @c win styleParameter, read by every glass-related
     *  setter to keep setGlass arguments consistent.  Platform default matches
     *  jam::button::Dialog::show, jam::GlassComponent::handleAsyncUpdate, and
     *  jam::look_and_feel::Theme::preparePopupMenuWindow.
     */
    jam::BackgroundBlur::Backend glassBackend {
#if JUCE_MAC
        jam::BackgroundBlur::Backend::backgroundBlur
#elif JUCE_WINDOWS
        jam::BackgroundBlur::Backend::blurBehind
#endif
    };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
