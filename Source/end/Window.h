/**
 * @file end/Window.h
 * @brief Pure jam::Window with LAF-driven glass for END.
 *
 * end::Window extends jam::Window with no config listener and no styleParameters.
 * Visual properties (glass: tint colour, blur radius, WindowFX) are applied via
 * lookAndFeelChanged(), which reads from end::LookAndFeel at theme-change time.
 * Operational properties (always_on_top, title_bar_buttons) are dispatched by
 * end::View in a separate step.
 *
 * Constructor calls lookAndFeelChanged() to apply the initial glass state.
 * Destructor is default.
 */
#pragma once
#include <JuceHeader.h>
#include "lookAndFeel/LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

/** @class Window
 *  @brief Pure jam::Window with LAF-driven glass.
 *
 *  Inherits jam::Window for glassmorphism. Visual properties (glass) are
 *  applied in lookAndFeelChanged(), which reads tint colour, blur radius, and
 *  WindowFX from end::LookAndFeel. No config listener. No styleParameters.
 *
 *  Ownership: constructed and owned by end::Application.
 */
class Window : public jam::Window
{
public:
    /** @brief Constructs the window and calls lookAndFeelChanged() to apply
     *  the initial glass state.
     *
     *  @param mainComponent      Content component — ownership transferred to jam::Window.
     *  @param name               Window title string.
     *  @param alwaysOnTop        Initial always-on-top hint.
     *  @param showWindowButtons  Initial chrome hint.
     */
    Window (juce::Component* mainComponent,
            const juce::String& name,
            bool alwaysOnTop,
            bool showWindowButtons);

    /** @brief Applies glass from the LAF when theme properties change.
     *  Reads colour, blur and FX from end::LookAndFeel::getWindowGlass().
     */
    void lookAndFeelChanged() override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
