/**
 * @file end/ENDWindow.h
 * @brief Pure jam::Window with LAF-driven style for END.
 *
 * ENDWindow extends jam::Window with no config listener and no styleParameters.
 * Visual properties (tint colour, blur radius, WindowFX, title-bar-button
 * visibility) are applied by ENDLookAndFeel::prepareWindow(), told to style
 * this window from lookAndFeelChanged() at theme-change time. Operational
 * properties (always_on_top, title_bar_buttons) are dispatched by ENDView in
 * a separate step.
 *
 * Constructor calls lookAndFeelChanged() to apply the initial style state.
 * Destructor is default.
 */
#pragma once
#include <JuceHeader.h>
#include "lookAndFeel/ENDLookAndFeel.h"

/** @class ENDWindow
 *  @brief Pure jam::Window with LAF-driven style.
 *
 *  Inherits jam::Window. Style (tint colour, blur radius, WindowFX, traffic-light
 *  visibility) is applied by ENDLookAndFeel::prepareWindow(), told to style this
 *  window from lookAndFeelChanged(). No config listener.
 *
 *  Ownership: constructed and owned by ENDApplication.
 */
class ENDWindow : public jam::Window
{
public:
    /** @brief Constructs the window and calls lookAndFeelChanged() to apply
     *  the initial style state.
     *
     *  Operational properties (alwaysOnTop, windowButtons) default to false
     *  and true respectively. ENDView::initRenderer() corrects both from config
     *  on the first message loop iteration.
     *
     *  @param mainComponent  Content component — ownership transferred to jam::Window.
     *  @param name           Window title string.
     */
    ENDWindow (juce::Component* mainComponent, const juce::String& name);

    /** @brief Tells the LAF to style this window when theme properties change.
     *  Delegates to ENDLookAndFeel::prepareWindow().
     */
    void lookAndFeelChanged() override;

private:
    // /** @brief Singleton LookAndFeel reference — source for window style getters. */
    ENDLookAndFeel& lookAndFeel { *ENDLookAndFeel::getInstance() };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDWindow)
};
