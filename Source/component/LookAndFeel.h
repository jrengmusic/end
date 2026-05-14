/**
 * @file LookAndFeel.h
 * @brief Custom LookAndFeel for the Terminal tab bar.
 *
 * Overrides JUCE's default tab drawing to produce a minimal, translucent
 * tab bar that matches the terminal's visual style. Colours are cached
 * via setColours() which reads lua::Engine and sets LookAndFeel colour IDs.
 * Paint methods use findColour() to retrieve colours.
 *
 * @see Terminal::Tabs
 * @see lua::Engine::display.colours.foreground
 * @see lua::Engine::display.colours.cursor
 * @see lua::Engine::display.window.colour
 */

#pragma once
#include <JuceHeader.h>
#include "../lua/Engine.h"

namespace Terminal
{ /*____________________________________________________________________________*/
/**
 * @class LookAndFeel
 * @brief Minimal tab bar appearance for the terminal emulator.
 *
 * Draws flat rectangular tabs with a translucent bar background.
 * Active tab is highlighted with the cursor colour; inactive tabs
 * use the foreground colour at reduced opacity.
 *
 * All colours are cached via LookAndFeel colour IDs on construction
 * and when setColours() is called. The paint methods use findColour()
 * to retrieve colours, enabling JUCE's colour inheritance system.
 *
 * Call setColours() after Config reload to refresh all colour IDs.
 *
 * @note MESSAGE THREAD — all methods called by JUCE painting system.
 */
class LookAndFeel : public juce::LookAndFeel_V4,
                    public jam::TabbedButtonBar::LookAndFeelMethods
{
public:
    /**
     * @brief Custom colour IDs for terminal theme.
     *
     * @note JUCE convention: ColourIds are plain enums (int-based colour ID system).
     *       Converting to enum class would require explicit casts at every setColour/findColour call.
     */
    enum ColourIds
    {
        cursorColourId = 0x2000001,///< Active tab indicator and popup tick colour.
        tabBarBackgroundColourId = 0x2000002,///< Tab bar background fill colour.
        tabLineColourId = 0x2000003,///< Active tab indicator line colour.
        tabActiveColourId = 0x2000004,///< Active tab fill colour (paradiso).
        tabIndicatorColourId = 0x2000005,///< Active tab indicator fill colour.
        paneBarColourId = 0x2000006,///< Pane divider bar colour.
        paneBarHighlightColourId = 0x2000007,///< Pane divider bar colour when dragging or hovering.
        statusBarBackgroundColourId = 0x2000100,     ///< StatusBarOverlay full bar background.
        statusBarLabelBackgroundColourId = 0x2000101,///< StatusBarOverlay mode label background.
        statusBarLabelTextColourId = 0x2000102       ///< StatusBarOverlay mode label text.
    };

    LookAndFeel();

    /**
     * @brief Refreshes all colour IDs from Config.
     *
     * Reads Config once and sets all JUCE colour IDs. Call this after
     * Config reload to update the LookAndFeel colours.
     *
     * @note MESSAGE THREAD.
     */
    void setColours();

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override;

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g) override;

    int getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth) override;

    /**
     * @brief Returns the tab font at the configured point size.
     *
     * Single source of truth for the tab font.  JUCE calls this
     * internally for layout; `drawTabButton` and `getTabButtonBestWidth`
     * also use it.
     *
     * @param button  The tab bar button being queried.
     * @param height  The tab bar depth (height for horizontal bars).
     * @return The tab font at `lua::Engine::display.tab.size` point height.
     * @note MESSAGE THREAD.
     */
    juce::Font getTabButtonFont (juce::TabBarButton& button, float height) override;

    // jam::TabbedButtonBar::LookAndFeelMethods overrides
    // getTabButtonSpaceAroundImage and getTabButtonOverlap have identical signatures
    // in both juce:: and jam:: LookAndFeelMethods — one override satisfies both bases.
    int getTabButtonSpaceAroundImage() override { return 0; }
    int getTabButtonOverlap (int) override { return -1; }
    int getTabButtonBestWidth (jam::TabBarButton& button, int tabDepth) override;
    juce::Rectangle<int> getTabButtonExtraComponentBounds (const jam::TabBarButton& button,
                                                            juce::Rectangle<int>& textArea,
                                                            juce::Component& extraComp) override;
    void drawTabButton (jam::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override;
    juce::Font getTabButtonFont (jam::TabBarButton& button, float height) override;
    void drawTabButtonText (jam::TabBarButton& button, juce::Graphics& g,
                            bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (jam::TabbedButtonBar& bar, juce::Graphics& g) override;
    void drawTabAreaBehindFrontButton (jam::TabbedButtonBar& bar, juce::Graphics& g,
                                       int w, int h) override;
    void createTabButtonShape (jam::TabBarButton& button, juce::Path& path,
                               bool isMouseOver, bool isMouseDown) override;
    void fillTabButtonShape (jam::TabBarButton& button, juce::Graphics& g,
                             const juce::Path& path,
                             bool isMouseOver, bool isMouseDown) override;

    /**
     * @brief Returns the tab font so popup menus match the tab bar text style.
     *
     * @return The tab font at configured point size.
     * @note MESSAGE THREAD.
     */
    juce::Font getPopupMenuFont() override;

    /**
     * @brief Dispatches label fonts via component property inspection.
     *
     * Reads the `font` property from the label's property map.  When the value
     * matches `jam::ID::name`, the action list name font is returned.  When it
     * matches `jam::ID::keyPress`, the action list shortcut font is returned.
     * All other labels fall back to the default LookAndFeel_V4 behaviour.
     *
     * @param label  The label being queried.
     * @return       The resolved font for the given label.
     * @note MESSAGE THREAD.
     */
    juce::Font getLabelFont (juce::Label& label) override;

    /**
     * @brief Makes the popup window transparent and applies native background blur.
     *
     * Sets the popup window to non-opaque and applies background blur via
     * jam::BackgroundBlur. Uses callAsync with a SafePointer to defer
     * blur application until the window has a native peer.
     *
     * @param newWindow  The popup menu window to prepare.
     * @note MESSAGE THREAD.
     */
    void preparePopupMenuWindow (juce::Component& newWindow) override;

    /** @brief Disables popup scaling — font size is already in logical points. */
    bool shouldPopupMenuScaleWithTargetComponent (const juce::PopupMenu::Options&) override { return false; }

    /**
     * @brief No-op — popup background is drawn by the native blur layer.
     *
     * The tint and opacity are applied via BackgroundBlur::enable in
     * preparePopupMenuWindow. This override prevents the base class
     * from filling with an opaque background colour.
     *
     * @note MESSAGE THREAD.
     */
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&, int, int, const juce::PopupMenu::Options&) override {}

    /**
     * @brief Draws a single popup menu item using terminal theme colours.
     *
     * Active items use the foreground colour; highlighted items use the
     * cursor colour background; ticked items show a chevron tick mark.
     * Separators are drawn as thin horizontal lines.
     *
     * @param g                  Graphics context.
     * @param area               Bounding rectangle for the item.
     * @param isSeparator        True if this item is a separator.
     * @param isActive           True if the item is enabled.
     * @param isHighlighted      True if the item is hovered.
     * @param isTicked           True if the item is checked.
     * @param hasSubMenu         True if the item has a submenu.
     * @param text               Item text.
     * @param shortcutKeyText    Optional keyboard shortcut text.
     * @param icon               Optional icon drawable.
     * @param textColourToUse    Optional text colour override.
     * @note MESSAGE THREAD.
     */
    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            const bool isSeparator,
                            const bool isActive,
                            const bool isHighlighted,
                            const bool isTicked,
                            const bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* const textColourToUse) override;

    /**
     * @brief Creates a minimal text button for tab bar overflow.
     *
     * Creates a minimal text button ("...") styled to match the tab bar
     * for the overflow dropdown when tabs don't fit.
     *
     * @return Pointer to a styled overflow button.
     * @note MESSAGE THREAD.
     */
    juce::Button* createTabBarExtrasButton() override;

    /**
     * @brief Dispatches text button fonts via component property inspection.
     *
     * Reads the `font` property from the button's property map.  When the
     * value matches `jam::ID::name`, the action list name font is returned
     * (matching the `name`-role branch of getLabelFont).  All other buttons
     * fall back to the tab font at 60 % of the button height.
     *
     * @param button       The text button being queried.
     * @param buttonHeight The button height in pixels.
     * @return             The resolved font for the given button.
     * @note MESSAGE THREAD.
     */
    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;

    /**
     * @brief Draws the pane resizer bar as a centred 1px line.
     *
     * Uses `paneBarHighlightColourId` when the mouse is over or dragging,
     * `paneBarColourId` otherwise.
     *
     * @param g               Graphics context.
     * @param w               Bar width in pixels.
     * @param h               Bar height in pixels.
     * @param isVerticalBar   True if the bar separates left/right panes.
     * @param isMouseOver     True if the mouse is hovering over the bar.
     * @param isMouseDragging True if the bar is being dragged.
     * @note MESSAGE THREAD.
     */
    void drawStretchableLayoutResizerBar (juce::Graphics& g,
                                          int w,
                                          int h,
                                          bool isVerticalBar,
                                          bool isMouseOver,
                                          bool isMouseDragging) override;

    /**
     * @brief Computes the tab bar height from the configured tab font.
     *
     * Queries the real rendered font height and derives the bar height
     * so the font occupies 50% of the bar.
     *
     * @return Tab bar height in pixels, rounded to nearest integer.
     * @note MESSAGE THREAD.
     */
    static int getTabBarHeight() noexcept;

#if JUCE_WINDOWS
    /**
     * @brief Suppresses the JUCE default title bar on Windows.
     *
     * When END runs without native window buttons (showWindowButtons = false),
     * jam::Window sets title bar height to 0.  This override prevents JUCE from
     * painting any title bar chrome into that zero-height region.
     *
     * @note MESSAGE THREAD.
     */
    void drawDocumentWindowTitleBar (juce::DocumentWindow&,
                                     juce::Graphics&,
                                     int, int, int, int,
                                     const juce::Image*,
                                     bool) override {}
#endif

private:
    static constexpr float tabFontRatio { 0.5f };
    static constexpr float separatorAlpha { 0.3f };
    static constexpr int horizontalPadding { 24 };
    static constexpr float skew { 10.0f };
    static constexpr float strokeWidth { 1.0f };
    static constexpr int minTabChars { 8 };
    static constexpr int maxTabChars { 24 };
    static constexpr int buttonInset { 4 };
    static constexpr int indicatorSize { 22 };
    static constexpr int gap { 4 };
    void loadTabButtonSvg();
    juce::Path svgActiveLeft, svgActiveCenter, svgActiveRight;
    juce::Path svgInactiveLeft, svgInactiveCenter, svgInactiveRight;
    bool hasSvgTabButton { false };
    static juce::Path getTabButtonShape (const juce::Rectangle<float>& area) noexcept;
    static juce::Path getTabButtonIndicator (const juce::Rectangle<float>& area) noexcept;
    static void drawTabButtonCore (LookAndFeel& lf,
                                   juce::Graphics& g,
                                   const juce::Rectangle<float>& area,
                                   bool isActive,
                                   bool isVertical,
                                   bool isLeftOrientation,
                                   bool isMouseOver,
                                   bool isMouseDown);
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal

