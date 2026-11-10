/**
 * @file lookAndFeel/ENDLookAndFeel.h
 * @brief END's theme-driven LookAndFeel — SVG Flex tab/bar graphics, live colour mapping.
 */
#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"
#include "config/ConfigModel.h"

/**
 * @class ENDLookAndFeel
 * @brief END's look-and-feel — theme-driven rendering with live ValueTree updates.
 *
 * Inherits jam::StyleCustom for tab bar background, highlight, button,
 * and pane edge rendering. Listens to config as a juce::ValueTree::Listener for live theme changes.
 *
 * Event dispatch uses ConfigModel::getEventKey: the property key takes priority; when no
 * event is registered for it, the lookup falls back to the tree type, then to the parent tree type.
 * This routes the theme rebuild (Id::theme) and the appearance change through one code path.
 */
class ENDLookAndFeel
    : public jam::StyleCustom
    , public jam::Instance<ENDLookAndFeel>
    , public juce::ValueTree::Listener
    , private juce::DarkModeSettingListener
{
public:
    /**
     * @brief Registers the events, and adds itself as a listener of config and of the dark-mode setting.
     *
     * The colours, the font styles and the graphics are not built here. initialiseStyle() builds them,
     * and ENDApplication::initialiseVulkan() calls it after the graphics engine is created.
     */
    ENDLookAndFeel();

    /** @brief Removes itself as a listener of the dark-mode setting and of config. */
    ~ENDLookAndFeel() override;

    /**
     * @brief Event dispatch through the events map.
     *
     * Looks up the key that ConfigModel::getEventKey gives for @p property: the property when it is
     * registered, otherwise the tree type when that is registered, otherwise the parent tree type.
     * The callback under that key runs with @p tree. When no callback is registered under the key, nothing runs.
     *
     * @param tree     The ValueTree whose property changed.
     * @param property The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Renders the tab bar background using SVG Flex segments (Id::tabBar).
     *
     * Applies a rotation AffineTransform for vertical orientations: -90 degrees
     * for left-side bars, +90 degrees for right-side bars.
     *
     * @param g   Graphics context.
     * @param bar The bar background component.
     */
    void drawBarBackground (juce::Graphics& g, juce::Component& bar) override;

    /**
     * @brief Renders the tab highlight indicator using SVG Flex segments (Id::tabHighlight).
     *
     * Applies a rotation AffineTransform for vertical orientations: -90 degrees
     * for left-side bars, +90 degrees for right-side bars.
     *
     * @param g         Graphics context.
     * @param highlight The highlight indicator component.
     */
    void drawBarHighlight (juce::Graphics& g, juce::Component& highlight) override;

    /**
     * @brief Renders a tab button using the per-state SVG from the sparse graphics bank.
     *
     * State is resolved via jam::ButtonSVG::getState and mapped to a
     * map::ButtonState identifier. Paint occurs only when that state slot
     * was authored in the FLEX child (loadGraphics(), sparse bank — missing
     * states are silently skipped). Applies rotation transform for vertical bars.
     *
     * @param g           Graphics context.
     * @param button      The tab button component.
     * @param isMouseOver True when the pointer is over the button.
     * @param isMouseDown True when the primary mouse button is held on the button.
     */
    void drawTabButton (juce::Graphics& g,
                        juce::Button& button,
                        bool isMouseOver,
                        bool isMouseDown) override;

    /** @brief Tab label rendering — uses getTabFont() and getTabText() from theme config.
     *  Fills the background with Label::backgroundColourId. Draws the text centred, in Label::textColourId.
     *  The text is drawn only when the label is not being edited.
     */
    void drawTabLabel (juce::Graphics& g, juce::Label& label) override;

    /**
     * @brief Returns the font of the tab sheet: StyleManager::getFont for Id::tab.
     *
     * getTabBarDepth() measures the tab bar with this font, and drawTabLabel() paints the labels with it.
     */
    juce::FontOptions getTabFont() const override;

    /** @brief Returns the font of the overlay: StyleManager::getFont for Id::overlay. */
    juce::FontOptions getOverlayFont() const;

    /** @brief Returns the popup menu font: StyleManager::getFont for Id::tab, the font of getTabFont(). */
    juce::Font getPopupMenuFont() override;

    /** @brief Returns the padding of the tab label in pixels, from the Id::tabTextPadding metric of the style sheet. */
    int getTabPadding() const override;
    /** @brief Height of the tab bar in pixels: the height of the tab font times the Id::tabDepth metric, rounded.
     *  Zero when the bar holds at most one tab and the Id::alwaysVisible row of the tab table is unset.
     *  @param tabs  The tabbed component whose bar is measured.
     */
    int getTabBarDepth (const jam::TabbedComponent& tabs) const noexcept;

    /** @brief Ordinal of the tab bar side from the tab.position row, resolved through map::Position. */
    int getTabPosition() const noexcept;

    /** @brief Display transform applied to a tab label before it is painted.
     *  Reads the uppercase row of the tab table; returns toUpperCase() when set,
     *  identity otherwise. drawTabLabel() calls it for the text it paints.
     */
    juce::String getTabText (const juce::String& tabName) const override;

    /**
     * @brief Resolves the typeface of a font request from the faces of the Id::fonts config table.
     *
     * Each face gives its typeface through StyleManager::getFont. A face whose name and style both match the
     * request is returned at once. A face whose name matches and whose style is Book is kept as a fallback,
     * and is returned only when no face matches the exact style. Returning that typeface keeps the object that
     * StyleManager holds for the face.
     *
     * @param font The font whose typeface is being resolved.
     * @return The typeface of the matching face, otherwise the base class result.
     */
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    /** @brief Padding of the tab bar, from the Id::tabPaddingTop, Id::tabPaddingLeft, Id::tabPaddingBottom and Id::tabPaddingRight metrics of the style sheet.
     *  The metrics are passed in the order top, left, bottom, right.
     */
    juce::BorderSize<int> getTabBarPadding() const override;

    /** @brief Styles @p window as a jam::Window: its background colour, its blur radius, and its WindowFX ordinal.
     *  The colour is the ResizableWindow::backgroundColourId of this look and feel, the blur is getWindowBlur(),
     *  and the WindowFX is getWindowFX(). Asserts that @p window is a jam::Window.
     *  @param window  The top-level window component to style.
     */
    void prepareWindow (juce::Component& window) override;

    /** @brief Blur radius of the window: StyleManager::getWindowStyle for Id::blur. */
    float getWindowBlur() const noexcept override;

    /**
     * @brief Sets the popup menu window opaque to false, then applies the menu tint, the WindowFX and the blur to it.
     *
     * The tint, the WindowFX and the blur are applied in a juce::MessageManager::callAsync call, guarded by a
     * juce::Component::SafePointer. The tint is the popup menu background colour of the window. The WindowFX is getWindowFX().
     * The blur is the window blur radius when jam::BackgroundBlur is enabled, and 0.0 otherwise.
     *
     * @param newWindow The popup menu's top-level window component.
     */
    void preparePopupMenuWindow (juce::Component& newWindow) override;

    /** @brief Fills the background with the popup menu background colour on Windows when jam::BackgroundBlur is disabled. Does nothing otherwise. */
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&,
                                             int,
                                             int,
                                             const juce::PopupMenu::Options&) override;

    /**
     * @brief Renders a single popup menu row.
     *
     * A separator is drawn as one line in the popup menu text colour at 0.3 alpha. Any other item draws its
     * background, its icon, its submenu arrow, and its text, with the font of getPopupMenuFont().
     *
     * @param g             Graphics context.
     * @param area          The row's bounds.
     * @param isHighlighted True when the row is under the pointer/selection.
     * @param item          The menu item being drawn.
     * @param options       Display options forwarded from the hosting popup.
     */
    void drawPopupMenuItemWithOptions (juce::Graphics& g,
                                       const juce::Rectangle<int>& area,
                                       bool isHighlighted,
                                       const juce::PopupMenu::Item& item,
                                       const juce::PopupMenu::Options& options) override;

    /** @brief Paints the seam of a pane edge with the Id::resizerBar SVG Flex segments.
     *  A seam narrower than it is tall is rotated with the left-side transform of getVerticalTextTransform().
     */
    void drawPaneEdge (juce::Graphics& g, juce::Component& bar) override;

    /** @brief Draws the outline of a pane as a rounded rectangle, inset by the pane padding and by jam::PaneComponent::lineThickness.
     *  The stroke width is jam::PaneComponent::lineThickness. The focused outline colour is used when the pane is focused.
     */
    void drawPaneOutline (juce::Graphics& g, juce::Component& pane) override;

    /** @brief Fills the pane area, inset as in drawPaneOutline(), with its focused or unfocused background colour. */
    void drawPaneBackground (juce::Graphics& g, juce::Component& pane) override;

    /** @brief Pixel thickness of the pane edge seam, from the Id::paneComponentResizeBarThickness metric of the style sheet.
     *  jam::PaneEdge::getSeam() reads it to size the drag-hit region.
     */
    int getPaneEdgeSize() const noexcept override;

    /** @brief Pane padding in pixels, from the Id::paneComponentPadding metric of the style sheet.
     *  The pane outline and the pane fill are inset by this padding and by jam::PaneComponent::lineThickness.
     */
    int getPanePadding() const noexcept override;

    /**
     * @brief Creates the StyleManager from the themes child of config, applies the colours, the font styles and
     *        the graphics, then installs this look and feel as JUCE's default.
     *
     * The StyleManager is created first, because applyColours() and applyFontStyles() read from it.
     * ENDApplication::initialiseVulkan() is the only caller.
     */
    void initialiseStyle();

    /**
     * @brief Sets the font rasterizer backend, gamma, contrast and embolden state of the CPU graphics engine.
     *
     * The backend is the entry of map::FontRasterizerBackend that the rasterizer entry of StyleManager names;
     * an assertion checks that the name is an entry. The embolden state is true when the embolden entry equals
     * Format::fromBoolean (true), ignoring case.
     */
    void applyFontStyles();

private:
    jam::SharedInstance<map::ButtonState> buttonState { std::in_place };
    jam::SharedInstance<map::Position> position { std::in_place };
    jam::SharedInstance<map::WindowFX> windowFX { std::in_place };
    jam::SharedInstance<map::Appearance> appearance { std::in_place };
    jam::SharedInstance<map::FontRasterizerBackend> fontRasterizerBackend { std::in_place };
    jam::SharedInstance<jam::StyleManager> styleManager;

    /** @brief Reapplies the colours when the appearance row of the display table is automatic. */
    void darkModeSettingChanged() override;

    /** @brief Singleton config model reference — source for theme path and top-level config values. */
    ConfigModel& config { *ConfigModel::getInstance() };

    //==============================================================================
    /** @brief Parsed SVG assets keyed by the full name of their FLEX child property.
     *
     *  Each entry is a Svg::Flex::Segments set (9-slice layout, paint-ready)
     *  produced by Svg::Flex::getSegments from the SVG content stored as a
     *  property of the FLEX child of config.state. Rebuilt by loadGraphics(),
     *  which initialiseStyle() calls, and on every Id::theme event.
     *
     *  drawTabButton() skips a button state whose entry is absent.
     */
    jam::HashMap<juce::Identifier, jam::Svg::Flex::Segments> graphics;

    /**
     * @brief Event dispatch map keyed by juce::Identifier (property or tree type).
     *
     * Populated by registerEvents(). A row child of a config table has no entry of its own,
     * so valueTreePropertyChanged() falls back to the type of its parent table. Handles:
     * - jam::Format::toType (Id::theme)      — StyleManager::registerStyle(), applyColours(), applyFontStyles(), loadGraphics()
     * - jam::Format::toType (Id::appearance) — applyColours()
     */
    ConfigModel::Events events;

    /** @brief Rotation transform for a vertical bar side (left or right).
     *  Called by applyBarRotation() and by drawPaneEdge(). The left side rotates by -90 degrees,
     *  the other side by +90 degrees.
     *
     *  @param position Vertical bar side: map::Position::left, or the right-side case otherwise.
     *                  applyBarRotation() passes jam::ButtonBar::getPosition(); drawPaneEdge() passes map::Position::left.
     *  @param width    Bar width before rotation.
     *  @param height   Bar height before rotation.
     */
    juce::AffineTransform
    getVerticalTextTransform (int position, float width, float height) const noexcept;

    /** @brief Applies the vertical rotation to @p g when the parent of @p component is a vertical ButtonBar.
     *  @param g          Graphics context. Receives the rotation when the parent bar is vertical.
     *  @param component  The component to paint. Its local bounds are the source of the result.
     *  @return           The local bounds of @p component as floats. When the parent bar is vertical,
     *                    the bounds that applyRotation() returns for the vertical transform.
     */
    juce::Rectangle<float> applyBarRotation (juce::Graphics& g, juce::Component& component) const;

    /** @brief WindowFX ordinal of the window style string for this platform: Id::mac on macOS, Id::win on Windows.
     *  The string is read through StyleManager::getWindowStyleString(), which reads the window sheet, and it is
     *  resolved through map::WindowFX. On other platforms the string is empty. Zero when the string has no entry.
     *  Consumed by prepareWindow() and preparePopupMenuWindow().
     */
    int16_t getWindowFX() const;

    //==============================================================================
    /**
     * @brief Reads the SVG content of the FLEX child of config.state and parses each property into Svg::Flex::Segments.
     *
     * Iterates the properties of the FLEX child through jam::Model::forEachProperty. Each property name is the key,
     * and its string value is the SVG text. Svg::Flex::getSegments receives the colourScheme member of
     * jam::StyleCustom, which no code in ENDLookAndFeel.cpp assigns. No disk I/O.
     * Defined in ENDLookAndFeel.cpp.
     */
    void loadGraphics();

    /**
     * @brief Applies the colours of the appearance of the display config to this look and feel.
     *
     * Reads the appearance row of the display table and passes it to StyleManager::setAppearance() for the style sheet.
     * Called by initialiseStyle(), by the theme event, by the appearance event, and by darkModeSettingChanged()
     * when the appearance is automatic. The constructor does not call it.
     * Defined in ENDLookAndFeel.cpp.
     */
    void applyColours();

    /**
     * @brief Populates the events map with the callbacks of the theme and of the appearance.
     *
     * Registers two callbacks, in this order:
     * - jam::Format::toType (Id::theme): StyleManager::registerStyle() with the themes child of config, then applyColours(),
     *   applyFontStyles() and loadGraphics().
     * - jam::Format::toType (Id::appearance): applyColours().
     *
     * The events map is read in valueTreePropertyChanged().
     * Defined in ENDLookAndFeel.cpp.
     */
    void registerEvents();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDLookAndFeel)
};
