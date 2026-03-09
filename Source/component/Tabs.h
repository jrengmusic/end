/**
 * @file Tabs.h
 * @brief Tab container that manages multiple Terminal::Component instances.
 *
 * Terminal::Tabs subclasses juce::TabbedComponent to provide a tabbed
 * interface where each tab hosts an independent terminal session. Content
 * components are NOT passed to JUCE's addTab() — instead they are managed
 * as direct children with visibility toggling, so that hidden sessions
 * stay alive and VBlank stops naturally.
 *
 * @par Ownership
 * Terminal::Component instances are owned by `jreng::Owner<Component>`.
 * The TabbedComponent base only knows about tab names and colours.
 *
 * @par Tab bar visibility
 * The tab bar is hidden (depth 0) when only one tab exists, and shown
 * at the configured height when multiple tabs are present.
 *
 * @see Terminal::Component
 * @see Terminal::LookAndFeel
 * @see jreng::Owner
 */

#pragma once
#include <JuceHeader.h>
#include "TerminalComponent.h"
#include "LookAndFeel.h"

namespace Terminal
{

/**
 * @class Tabs
 * @brief Tabbed container managing multiple terminal sessions.
 *
 * Each tab corresponds to a Terminal::Component with its own Session,
 * PTY, Grid, Screen, and OpenGL context. Tabs are added with addNewTab()
 * and removed with closeActiveTab(). The active terminal is the only
 * visible child; all others remain as hidden children.
 *
 * @note MESSAGE THREAD — all methods.
 */
class Tabs : public juce::TabbedComponent
{
public:
    explicit Tabs (juce::TabbedButtonBar::Orientation orientation);
    ~Tabs() override;

    std::function<void()> onRepaintNeeded;

    void addNewTab();
    void closeActiveTab();
    void selectPreviousTab();
    void selectNextTab();

    int getTabCount() const noexcept;

    void copySelection();
    void pasteClipboard();
    void reloadConfig();
    void increaseZoom();
    void decreaseZoom();
    void resetZoom();

    jreng::Owner<jreng::GLComponent>& getComponents() noexcept { return terminals; }

    void resized() override;

private:
    Terminal::Component* getActiveTerminal() const noexcept;

    void currentTabChanged (int newIndex, const juce::String& name) override;
    void updateTabBarVisibility();

    jreng::Owner<jreng::GLComponent> terminals;
    LookAndFeel tabLookAndFeel;

    static constexpr int defaultTabBarHeight { 28 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tabs)
};

}
