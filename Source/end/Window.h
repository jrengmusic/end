/**
 * @file end/Window.h
 * @brief Config-reactive DocumentWindow for END.
 *
 * end::Window extends jam::Window with listener-driven style application.
 * Visual properties (glass: tint colour, blur radius, WindowFX) are applied
 * via lookAndFeelChanged(), which reads from the LAF at theme-change time.
 * Operational properties (always_on_top, buttons) are driven by config::Model
 * listener via styleParameters — a jam::Function::Map keyed on juce::Identifier
 * storing one typed callable per config property.
 *
 * Config tree shape navigated by the dispatcher:
 * @code
 * CONFIG
 *   DISPLAY  (IDtype::display)
 *     WINDOW  (IDtype::window)
 *       always_on_top, buttons
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
 *  @brief Config-reactive window with LAF-driven glass and config-driven
 *  operational properties.
 *
 *  Inherits jam::Window for glassmorphism.  Visual properties (glass) are
 *  applied in lookAndFeelChanged(), which reads the tint colour via
 *  findColour(juce::ResizableWindow::backgroundColourId) and blur/FX from
 *  end::LookAndFeel getters.  Operational properties (always_on_top, buttons)
 *  are mapped via styleParameters and driven by juce::ValueTree::Listener.
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
     *  lookAndFeelChanged() is called after to apply the initial glass state.
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

    /** @brief Applies glass from the LAF when theme properties change.
     *  Reads colour via findColour, blur and FX from end::LookAndFeel getters.
     */
    void lookAndFeelChanged() override;

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
     *  Receives the WINDOW node and forwards it to the registered callable.
     *  The lambda reads its one property from the passed node — no further
     *  navigation in the callable.
     *
     *  Used by both the ValueTree::Listener entry point (valueTreePropertyChanged)
     *  and the init path (setStyle iterates over all registered properties and
     *  calls this for each).
     *
     *  @param property  Identifier of the property to resolve and dispatch.
     */
    void setStyle (const juce::Identifier& property);

    jam::Function::Map<juce::Identifier, void> styleParameters;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
