/**
 * @file end/Window.h
 * @brief Pure jam::Window with LAF-driven style for END.
 *
 * end::Window extends jam::Window with no config listener and no styleParameters.
 * Visual properties (tint colour, blur radius, WindowFX) are applied via
 * lookAndFeelChanged(), which inlines the three primitives from end::LookAndFeel
 * at theme-change time. Operational properties (always_on_top, title_bar_buttons)
 * are dispatched by end::View in a separate step.
 *
 * Constructor calls lookAndFeelChanged() to apply the initial style state.
 * Destructor is default.
 */
#pragma once
#include <JuceHeader.h>
#include "lookAndFeel/LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Window
 *  @brief Pure jam::Window with LAF-driven style.
 *
 *  Inherits jam::Window. Style (tint colour, blur radius, WindowFX, traffic-light
 *  visibility) is applied in lookAndFeelChanged(), which reads the four values
 *  from end::LookAndFeel and inlines the primitives (jam::style::window::apply,
 *  jam::BackgroundBlur::enable, jam::style::window::setButtons). No config listener.
 *
 *  Ownership: constructed and owned by end::Application.
 */
class Window : public jam::Window
{
public:
    /** @brief Constructs the window and calls lookAndFeelChanged() to apply
     *  the initial style state.
     *
     *  Operational properties (alwaysOnTop, windowButtons) default to false
     *  and true respectively. View::initRenderer() corrects both from config
     *  on the first message loop iteration.
     *
     *  @param mainComponent  Content component — ownership transferred to jam::Window.
     *  @param name           Window title string.
     */
    Window (juce::Component* mainComponent, const juce::String& name);

    /** @brief Applies window style from the LAF when theme properties change.
     *  Reads colour, blur, FX, and windowButtons from end::LookAndFeel::getWindowStyle()
     *  and inlines jam::style::window::apply, jam::BackgroundBlur::enable, and
     *  jam::style::window::setButtons.
     */
    void lookAndFeelChanged() override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
