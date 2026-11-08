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
 * and pane edge rendering. Listens to the config root ValueTree, which reports the
 * property changes of its subtrees, for live theme changes.
 *
 * Event dispatch uses ConfigModel::getEventKey: the property key takes priority; when no
 * event is registered for it, the lookup falls back to the tree type, then to the parent tree type.
 * This routes theme rebuild (Id::theme) and per-component colour refresh
 * (scrollbar, button, overlay, pane, appearance, menu) through one code path.
 */
class ENDLookAndFeel
    : public jam::StyleCustom
    , public jam::Instance<ENDLookAndFeel>
    , public juce::ValueTree::Listener
    , private juce::DarkModeSettingListener
{
public:
    /**
     * @brief Applies the colours from config, loads SVG graphics from FLEX
     *        child properties, registers events, adds itself as a listener of
     *        config and of the dark-mode setting, and installs itself as JUCE's
     *        default LookAndFeel.
     *        Typeface registration happens later — see registerTypeface()'s
     *        doc comment for why it cannot run here.
     */
    ENDLookAndFeel();

    /** @brief Removes itself as a listener of the dark-mode setting and of config. */
    ~ENDLookAndFeel() override;

    /**
     * @brief Registers END's six embedded typefaces.
     *
     * For each of the six embedded fonts, creates a Typeface::Ptr with juce::Typeface::createSystemTypefaceFor,
     * stores it in @ref typefaces under its {name, style} key from getTypefaceKey(), and registers the same
     * Ptr and its bytes with jam::Typeface.
     */
    void registerTypeface();

    /**
     * @brief Event dispatch through the events map.
     *
     * Looks up the key that ConfigModel::getEventKey gives for @p property: the property when it is
     * registered, otherwise the tree type when that is registered, otherwise the parent tree type.
     * Routes theme rebuild (Id::theme) and per-component colour refresh to their respective callbacks.
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
     * @brief Returns tab font constructed from theme config (family, size, kerning).
     *
     * Reads the jam::Format::toType (Id::tab) rows Id::textFontFamily, Id::textFontSize and Id::kerningFactor.
     */
    juce::FontOptions getTabFont() const override;

    /** @brief Returns the font of the overlay table: its textFontFamily and textFontSize rows. */
    juce::FontOptions getOverlayFont() const;

    /** @brief Returns the popup menu font from the tab table: its textFontFamily and textFontSize rows.
     *  The kerning factor is not applied.
     */
    juce::Font getPopupMenuFont() override;

    /** @brief Returns horizontal padding in pixels per side of the tab label.
     *  Reads the textPadding row of the tab table.
     */
    int getTabPadding() const override;
    /** @brief Height of the tab bar in pixels: the tab font height times the depth row of the tab table.
     *  Zero when the bar holds at most one tab and the alwaysVisible row of the tab table is unset.
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
     * @brief Resolves the exact Typeface::Ptr registered in @ref typefaces for
     *        one of END's six embedded fonts, bypassing JUCE's TypefaceCache
     *        platform lookup by name.
     *
     * TypefaceCache resolves juce::Font name lookups through a platform-specific
     * path and would otherwise hand back a *different* Typeface object than the
     * Ptr registerTypeface() interned in jam::Typeface.
     * Returning the same object identity here guarantees it
     * flows unchanged from name-based Font construction through shaping into the
     * interned typeface index, so jam::GlyphRasterizer reaches the FreeType face of
     * that entry for END's embedded fonts instead of opening a second face for every glyph.
     *
     * Two probes against @ref typefaces before falling back to the base class:
     * the exact {name, style} key first, then {name, "Book"} — "Book" is the
     * regular weight of both embedded families (Display, DisplayMono), and
     * juce::Font requests style "Regular" by default, which none of the six
     * embedded faces carry.
     *
     * @param font The font whose typeface is being resolved.
     * @return The registered Ptr when @p font names one of the six embedded
     *         fonts (exact style or the family's Book weight); otherwise the
     *         base class result.
     */
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    /** @brief Component-level padding for the tab bar from display config.
     *  Reads tab.padding { top, right, bottom, left } (CSS convention).
     */
    juce::BorderSize<int> getTabBarPadding() const override;

    /** @brief Applies the window background colour, the blur radius, and the WindowFX ordinal to @p window.
     *  The colour comes from the window table, the WindowFX ordinal from getWindowFX(). Asserts that
     *  @p window is a jam::Window.
     *  @param window  The top-level window component to style.
     */
    void prepareWindow (juce::Component& window) override;

    /** @brief Blur radius from the active theme (jam::Format::toType (Id::window), Id::blurRadius). */
    float getWindowBlur() const noexcept override;

    /**
     * @brief Sets the popup menu window opaque to false, then applies the tint, the blur and the menu opacity to it.
     *
     * The tint and the blur are applied in a juce::MessageManager::callAsync call, guarded by a SafePointer.
     * The opacity is the menu opacity row when jam::BackgroundBlur is enabled, and 1.0 otherwise.
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

    /** @brief Paints the seam of a pane edge with the resizerBar SVG Flex segments.
     *  A seam narrower than it is tall is rotated with the vertical transform of getVerticalTextTransform().
     */
    void drawPaneEdge (juce::Graphics& g, juce::Component& bar) override;

    /** @brief Draws the outline of a pane as a rounded rectangle, the pane area inset by the pane padding.
     *  The stroke width is jam::PaneComponent::lineThickness. The focused outline colour is used when the pane is focused.
     */
    void drawPaneOutline (juce::Graphics& g, juce::Component& pane) override;

    /** @brief Fills the pane with its focused or unfocused background colour. */
    void drawPaneBackground (juce::Graphics& g, juce::Component& pane) override;

    /** @brief Pane EDGE seam thickness in pixels from pane config.
     *  Reads pane.resize_bar_thickness (user-configurable). Consumed by
     *  jam::PaneEdge::getSeam() to size the drag-hit region.
     */
    int getPaneEdgeSize() const noexcept override;

    /** @brief Pane padding in pixels from the display config.
     *  Reads the padding row of the pane table. The space lies between the pane bounds and the pane outline.
     */
    int getPanePadding() const noexcept override;

private:
    jam::SharedInstance<map::ButtonState> buttonState { std::in_place };
    jam::SharedInstance<map::Position> position { std::in_place };
    jam::SharedInstance<map::WindowFX> windowFX { std::in_place };
    jam::SharedInstance<map::Appearance> appearance { std::in_place };

    /** @brief Reapplies the colours when the appearance row of the display table is automatic. */
    void darkModeSettingChanged() override;

    /** @brief Singleton config model reference — source for theme path and top-level config values. */
    ConfigModel& config { *ConfigModel::getInstance() };

    //==============================================================================
    /** @brief JUCE embedded font ownership — Ptrs kept alive so font names resolve
     *  via juce::Font name lookup without requiring system-installed fonts.
     *
     *  Keyed by the {name, style} composite built by getTypefaceKey(), NOT by name
     *  alone — the two embedded families each register three weights sharing
     *  one juce::Typeface name; a name-only key collides across weights (see
     *  registerTypeface()'s doc comment).
     *  @see registerTypeface
     *  @see getTypefaceKey
     */
    jam::HashMap<juce::String, juce::Typeface::Ptr> typefaces;

    /** @brief Parsed SVG assets keyed by their FLEX child property name or
     *  button state identifier.
     *
     *  Each entry is a Svg::Flex::Segments set (9-slice layout, paint-ready)
     *  produced by Svg::Flex::getSegments from the SVG content stored as a
     *  property of the FLEX child of config.state. Rebuilt by loadGraphics()
     *  on construction and on every Id::theme rebuild event.
     *
     *  Keys are resolved from the property name: the suffix after the last '_'
     *  is used when it matches a map::ButtonState entry; otherwise the full
     *  property name is the key. Svg::Flex::getSegments looks up each named SVG
     *  element as a top-level int key of the colourScheme member of jam::StyleCustom
     *  (jam_SvgAttributes.cpp). END leaves colourScheme empty, so every element
     *  keeps its own SVG style.
     */
    jam::HashMap<juce::Identifier, jam::Svg::Flex::Segments> graphics;

    /**
     * @brief Event dispatch map keyed by juce::Identifier (property or tree type).
     *
     * Populated by registerEvents(). A row child of a config table has no entry of its own,
     * so valueTreePropertyChanged() looks up the type of its parent table. Handles:
     * - Id::theme         — full theme rebuild via applyColours() + loadGraphics()
     * - jam::Format::toType (Id::scrollbar), jam::Format::toType (Id::button),
     *   jam::Format::toType (Id::overlay), jam::Format::toType (Id::pane)
     *                     — per-component colour refresh via setColours()
     * - jam::Format::toType (Id::appearance)
     *                     — colour refresh via applyColours()
     * - jam::Format::toType (Id::menu)
     *                     — popup menu colour refresh via setPopupMenuColours()
     */
    jam::Function::Map<juce::Identifier, void> events;

    /**
     * @brief Builds the composite key @ref typefaces is keyed by: name and
     *        style joined by "/".
     *
     * Both registerTypeface() (ENDLookAndFeel.cpp) and getTypefaceForFont()
     * use this single definition so the key building never drifts between
     * insertion and lookup. See @ref typefaces and registerTypeface() for why
     * a name-only key is unsafe.
     *
     * @param name  juce::Typeface::getName() / juce::Font::getTypefaceName().
     * @param style juce::Typeface::getStyle() / juce::Font::getTypefaceStyle().
     * @return The composite key, e.g. "Display/Book".
     */
    static juce::String getTypefaceKey (const juce::String& name, const juce::String& style);

    /** @brief Rotation transform for a vertical ButtonBar side (left/right).
     *  Called by applyBarRotation().
     *
     *  @param position Vertical bar side (map::Position::left or the right-side case),
     *                  as returned by jam::ButtonBar::getPosition().
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

    /** @brief WindowFX ordinal from the active theme's platform style name
     *  (jam::Format::toType (Id::style), Id::mac on macOS / Id::win on Windows),
     *  resolved through map::WindowFX. Zero when the style name has no entry.
     *  Consumed by prepareWindow() and preparePopupMenuWindow().
     */
    int16_t getWindowFX() const;

    //==============================================================================
    /**
     * @brief Reads SVG content from the FLEX child of config.state and
     *        parses each property into Svg::Flex::Segments.
     *
     * Iterates properties of the FLEX child via jam::Model::forEachProperty.
     * For each string property: the key is the suffix after the last '_' when it
     * matches a map::ButtonState entry, or the full property name otherwise.
     * Svg::Flex::getSegments looks up each named SVG element as a top-level int key
     * of the colourScheme member of jam::StyleCustom (jam_SvgAttributes.cpp). END
     * leaves colourScheme empty, so every element keeps its own SVG style. No disk I/O.
     * Defined in ENDLookAndFeel.cpp.
     */
    void loadGraphics();

    /**
     * @brief Applies all component colours, then the popup menu colours.
     *
     * Calls setColours(), then setPopupMenuColours(). Called by the constructor, by the Id::theme
     * event, by the jam::Format::toType (Id::appearance) event, and by darkModeSettingChanged()
     * when the appearance is automatic.
     * Defined in ENDLookAndFeel.cpp.
     */
    void applyColours();

    /**
     * @brief Sets the JUCE and END colour ids of every component tree from the config rows.
     *
     * Reads one colour row through ConfigModel::getRowValue() for each (table type, row key,
     * colour id) entry of the scrollbar, button, overlay and pane tables.
     * Converts the row with jam::ColourScheme::toColour() and passes it to
     * juce::LookAndFeel::setColour(). Defined in ENDLookAndFeel.cpp.
     */
    void setColours();

    /**
     * @brief Applies window background, opacity, text, and highlight colours
     *        to the juce::PopupMenu colour IDs.
     *
     * Reads jam::Format::toType (Id::menu) properties against the window background colour
     * (jam::Format::toType (Id::window)). Called by applyColours(), and on the
     * jam::Format::toType (Id::menu) colour-refresh event. Defined in ENDLookAndFeel.cpp.
     */
    void setPopupMenuColours();

    /**
     * @brief Populates the events map with ValueTree property/type-keyed callbacks.
     *
     * Registers callbacks for:
     * - Id::theme         → applyColours() + loadGraphics()
     * - jam::Format::toType (Id::scrollbar), jam::Format::toType (Id::button),
     *   jam::Format::toType (Id::overlay), jam::Format::toType (Id::pane)
     *                     → setColours()
     * - jam::Format::toType (Id::appearance)
     *                     → applyColours()
     * - jam::Format::toType (Id::menu)
     *                     → setPopupMenuColours()
     *
     * Each callback is registered in the order listed above. The events map is read in valueTreePropertyChanged().
     * Defined in ENDLookAndFeel.cpp.
     */
    void registerEvents();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDLookAndFeel)
};
