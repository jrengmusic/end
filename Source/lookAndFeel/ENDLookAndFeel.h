/**
 * @file lookAndFeel/ENDLookAndFeel.h
 * @brief END's theme-driven LookAndFeel — SVG Flex tab/bar graphics, live colour mapping.
 */
#pragma once
#include <JuceHeader.h>
#include <jam_gui/keyboard/jam_CaretComponent.h>
#include <jam_gui/code_view/jam_CodeView.h>
#include <JamFontsBinaryData.h>
#include "generated/Lexicon.h"
#include "config/ConfigModel.h"

/**
 * @class ENDLookAndFeel
 * @brief END's look-and-feel — theme-driven rendering with live ValueTree updates.
 *
 * Inherits jam::StyleMethods for tab bar background, highlight, button,
 * and pane edge rendering. Listens to both the config root ValueTree and the theme
 * subtree for live theme changes.
 *
 * Event dispatch uses a single-key lookup: property key takes priority; when no
 * event is registered for the property, the lookup falls back to tree.getType().
 * This routes theme rebuild (Id::theme) and per-component colour refresh
 * (Id::toType (Id::code), scrollbar, tab, button, overlay, pane, statusBar, hint)
 * through one code path.
 */
class ENDLookAndFeel
    : public jam::StyleMethods<ENDLookAndFeel>
    , public jam::Instance<ENDLookAndFeel>
    , public juce::ValueTree::Listener
{
public:
    /**
     * @brief END-private colour identifiers.
     *
     * Keyed by LookAndFeel::setColour / findColour for components that have no
     * JUCE-native colour id. Components sourcing JUCE-native ids (CaretComponent,
     * TextEditor, ScrollBar, ResizableWindow, juce::TextButton, juce::Label) use
     * those directly — this enum covers END-only components only.
     */
    enum ColourIds
    {
        cursorColourId = 0x2000001,
        paneBarColourId = 0x2000009,
        paneBarHighlightColourId = 0x200000A,
        statusBarBackgroundColourId = 0x2000100,
        statusBarLabelBackgroundColourId = 0x2000101,
        statusBarLabelTextColourId = 0x2000102,
        statusBarSpinnerColourId = 0x2000103,
        hintLabelBgColourId = 0x2000200,
        hintLabelFgColourId = 0x2000201,
        selectionCursorColourId = 0x2000301,
    };

    /**
     * @brief Initialises colours from config, loads SVG graphics from GRAPHICS
     *        child properties, registers events, and adds a listener to config.
     *        Typeface registration happens later — see registerTypeface()'s
     *        doc comment for why it cannot run here.
     */
    ENDLookAndFeel();

    /** @brief Removes the listener from config. */
    ~ENDLookAndFeel();

    /**
     * @brief Registers END's six embedded typefaces — one pass, one parse per
     *        font (SSOT font list) — creating each Typeface::Ptr, storing it in
     *        @ref typefaces under its {name, style} composite key (see
     *        typefaceKey()) for name+style lookup (getTypefaceForFont()), and
     *        registering it with the Vulkan glyph atlas so its mono glyphs
     *        rasterize through FreeType's autofitter instead of the EdgeTable
     *        fallback.
     *
     * Name-only keys collide: the two families (Display, DisplayMono) each
     * register three weights (Bold/Book/Medium) that share one juce::Typeface
     * name, so a name-only key lets addOrReplace() release two of every three
     * Ptrs, recycling their heap addresses into later createSystemTypefaceFor()
     * calls and desyncing the atlas's pointer-keyed FreeType face registry from
     * the live Typeface identities. The composite key keeps all six Ptrs alive
     * with unique, stable addresses.
     *
     * Cannot run at LookAndFeel construction time — the atlas (owned by
     * jam::VulkanEngine) does not exist yet then (ENDApplication constructs
     * vulkanEngine after ENDLookAndFeel, per ENDApplication's member order,
     * Main.h). Called once, externally, immediately after VulkanEngine
     * construction (ENDApplication::initialiseVulkan()). Also applies the
     * shipped/user-configured glyph rasterization backend/gamma/contrast (see
     * setFontRasterization()) and
     * the embolden state (see setEmbolden()) before this atlas ever paints a
     * glyph.
     *
     * @param atlas  The Vulkan glyph atlas to register with.
     */
    void registerTypeface (jam::GlyphAtlas& atlas);

    /**
     * @brief Single-key event dispatch through the events map.
     *
     * Checks whether @p property has a registered handler; if not, falls back to
     * @p tree.getType(). Routes theme rebuild (Id::theme) and per-component
     * colour refresh to their respective callbacks.
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
     * Id::ButtonState identifier. Paint occurs only when that state slot
     * was authored in the theme.lua graphics section (sparse bank — missing states
     * are silently skipped). Applies rotation transform for vertical bars.
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
     *  No border, centred, colour from Label::textColourId (synced by ButtonTab::buttonStateChanged).
     */
    void drawTabLabel (juce::Graphics& g, juce::Label& label) override;

    /**
     * @brief Returns tab font constructed from theme config (family, size, kerning).
     *
     * Reads Id::toType (Id::tab) properties: Id::fontFamily, Id::fontSize, Id::kerningFactor.
     */
    juce::Font getTabFont() const override;

    /** @brief Returns the popup menu font constructed from the Display font
     *  config (same source as getTabFont(), no kerning).
     */
    juce::Font getPopupMenuFont() override;

    /** @brief Returns horizontal padding in pixels per side of the tab label.
     *  Reads tab.text_padding from the display config (user-configurable).
     */
    int getTabPadding() const override;
    int getTabBarDepth (const jam::TabbedComponent&) const noexcept;
    int getTabPosition() const noexcept;

    /** @brief Display transform applied to a tab label before measuring and painting.
     *  Reads tab.uppercase from the display config; returns toUpperCase() when set,
     *  identity otherwise. Consumed by both ButtonBar::getBestTabLength (measurement) and
     *  drawTabButton (render) so measured width and painted string never diverge.
     */
    juce::String getTabText (const juce::String& tabName) const override;

    /**
     * @brief Resolves the exact Typeface::Ptr registered in @ref typefaces for
     *        one of END's six embedded fonts, bypassing JUCE's TypefaceCache
     *        platform lookup by name.
     *
     * TypefaceCache resolves juce::Font name lookups through a platform-specific
     * path and would otherwise hand back a *different* Typeface object than the
     * Ptr registerTypeface() created and registered with the Vulkan glyph atlas.
     * Returning the same object identity here guarantees it
     * flows unchanged from name-based Font construction through shaping into
     * GlyphAtlas::Key::typeface, so the atlas's pointer-keyed FreeType face
     * registry hits for END's embedded fonts instead of missing on every glyph.
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

    /** @brief Window style parameters from the active theme.
     *  Returns { tint colour, blur radius, windowFX style, window-button visibility }
     *  read from Id::toType (Id::window) and Id::toType (Id::style).  Consumer unpacks via structured binding.
     *  Returns a default-constructed Style when theme is not loaded.
     */
    struct Style
    {
        juce::Colour colour;
        int16_t blur;
        int16_t fx;
        bool windowButtons;
    };

    Style getWindowStyle() const;

    /**
     * @brief Applies the window's tint/blur style and menu opacity to a newly
     *        created popup menu window.
     *
     * Deferred to the next message-loop iteration via
     * juce::MessageManager::callAsync, guarded by a SafePointer, since the
     * window is not yet fully constructed at the point JUCE calls this.
     * Background blur is skipped and full opacity used when
     * jam::BackgroundBlur is disabled.
     *
     * @param newWindow The popup menu's top-level window component.
     */
    void preparePopupMenuWindow (juce::Component& newWindow) override;

    /** @brief Windows-only opaque background fill when background blur is disabled; no-op otherwise. */
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&,
                                             int,
                                             int,
                                             const juce::PopupMenu::Options&) override;

    /**
     * @brief Renders a single popup menu row — separator, icon, tick, text,
     *        submenu arrow, and shortcut key description.
     *
     * Icons are resolved by item ID via an internal SVG lookup (split/join
     * entries); items without a matching icon fall back to a tick mark when
     * ticked. Highlighted, enabled, and ticked states each select their own
     * colour from the LookAndFeel's popup menu colours.
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

    /** @brief Pane EDGE seam — SVG Flex rendering with pane colourIds.
     *  Hover/pressed state swaps bar colour to highlight colour.
     */
    void drawPaneEdge (juce::Graphics& g, juce::Component& bar) override;

    /** @brief Pane outline indicator — focused pane strokes with the focused
     *  outline colour, unfocused panes with the plain outline colour.
     */
    void drawPaneOutline (juce::Graphics& g, juce::Component& pane) override;

    /** @brief Pane EDGE seam thickness in pixels from pane config.
     *  Reads pane.resize_bar_thickness (user-configurable). Consumed by
     *  jam::PaneEdge::getSeam() to size the drag-hit region.
     */
    int getPaneEdgeSize() const noexcept override;

    /** @brief Sidebar dock seed ratio from pane config (pane.sidebar_size). */
    float getPaneSidebarSize() const noexcept;

    /**
     * @brief Returns the terminal code font constructed from theme config
     *        (family, size).
     *
     * Reads Id::toType (Id::code) properties: Id::fontFamily, Id::fontSize. Unlike
     * getTabFont(), carries no kerning factor (code.font_family/font_size
     * has no kerning_factor property) and no zoom — zoom is an
     * EditorView parameter applied inside getCodeMetrics() below.
     */
    juce::Font getCodeFont() const;

    /**
     * @brief Computed terminal cell metrics at @p zoom — LnF is the font
     *        owner AND the sanctioned jam::GlyphAtlas caller; EditorView
     *        never touches glyph machinery directly.
     *
     * Applies @p zoom to getCodeFont() via juce::Font::withPointHeight()
     * (size × zoom), resolves the zoomed font's typeface, and calls
     * jam::GlyphAtlas::getInstance()->calcMetrics() at the exact FT size the
     * glyphs will rasterize at. The raw FT cell width/height are then scaled
     * by the theme's own cell ratios (code.cell_width / code.line_height)
     * to produce the final pixel cell size. Returns the zoomed font itself
     * alongside the computed metrics — EditorView's lookAndFeelChanged()
     * consumes every field directly (codeView::setFont/setCellSize/
     * setBaseline, mouse.setCellSize, model.setCellSize) with no ratio-only
     * getter anywhere in the chain.
     *
     * @param zoom  EditorView's own zoom factor (Direction B Id::zoom).
     * @return Populated CodeMetrics — font, cellWidth, cellHeight, baseline,
     *         all in pixels except font (points).
     */
    struct CodeMetrics
    {
        juce::Font font;
        int cellWidth;
        int cellHeight;
        int baseline;
    };

    CodeMetrics getCodeMetrics (float zoom) const;

    /** @brief Component-level padding for the terminal code area from
     *  display config. Reads code.padding { top, right, bottom, left }
     *  (CSS convention) — mirrors getTabBarPadding().
     */
    juce::BorderSize<int> getCodePadding() const;

    /** @brief Terminal gutter width in pixels from scrollbar config.
     *  Reads scrollbar.width (user-configurable).
     */
    int getGutterWidth() const noexcept;

    /** @brief Cursor block parameters from the active theme.
     *  Returns { style, blink, blinkInterval, cursorChar, force } read from
     *  Id::toType (Id::cursor). Consumer unpacks via structured binding.
     */
    struct CursorStyle
    {
        juce::String style;
        bool blink;
        int blinkInterval;
        juce::String cursorChar;
        bool force;
    };

    CursorStyle getCursorStyle() const;

    /** @brief Terminal ligature toggle from the active theme (code.ligatures)
     *  — the 5th getCode* visual getter. Routes through LnF like every
     *  other code-family value; EditorView no longer reads config
     *  directly for ligatures.
     */
    bool getCodeLigatures() const noexcept;

private:
    // /** @brief Singleton config model reference — source for theme path and top-level config values. */
    ConfigModel& config { *ConfigModel::getInstance() };

    //==============================================================================
    /** @brief JUCE embedded font ownership — Ptrs kept alive so font names resolve
     *  via juce::Font name lookup without requiring system-installed fonts.
     *
     *  Keyed by the {name, style} composite built by typefaceKey(), NOT by name
     *  alone — the two embedded families each register three weights sharing
     *  one juce::Typeface name; a name-only key collides across weights (see
     *  registerTypeface()'s doc comment).
     *  @see registerTypeface
     *  @see typefaceKey
     */
    jam::HashMap<juce::String, juce::Typeface::Ptr> typefaces;

    /** @brief Parsed SVG assets keyed by their GRAPHICS child property name or
     *  button state identifier.
     *
     *  Each entry is a SVG::Flex::Segments set (9-slice layout, paint-ready)
     *  produced by SVG::Flex::getSegments from the SVG content stored as a
     *  property of the GRAPHICS child of config.state. Rebuilt by loadGraphics()
     *  on construction and on every Id::theme rebuild event.
     *
     *  Keys are resolved from the property name: the suffix after the last '_'
     *  is used when it matches a Id::ButtonState entry; otherwise the full
     *  property name is the key. All SVGs are coloured with the full colourMap.
     */
    jam::HashMap<juce::Identifier, jam::SVG::Flex::Segments> graphics;

    /**
     * @brief Event dispatch map keyed by juce::Identifier (property or tree type).
     *
     * Populated by registerEvents(). Handles:
     * - Id::theme         — full theme rebuild via initialiseColours() + loadGraphics()
     * - Id::toType (Id::code), Id::toType (Id::scrollbar), Id::toType (Id::tab), Id::toType (Id::button),
     *   Id::toType (Id::overlay), Id::toType (Id::pane), Id::toType (Id::statusBar), Id::toType (Id::hint)
     *                     — per-component colour refresh via setColours()
     * - Id::fontRasterizer, Id::fontGamma, Id::fontContrast
     *                     — re-applies setFontRasterization() on config hot-reload,
     *                       which itself cascades Component::sendLookAndFeelChange()
     *                       (see setFontRasterization()'s own doc comment)
     */
    jam::Function::Map<juce::Identifier, void> events;

    /**
     * @brief Builds the composite key @ref typefaces is keyed by: name and
     *        style joined by "/".
     *
     * Both registerTypeface() (EventRegistration.cpp) and getTypefaceForFont()
     * use this single definition so the key building never drifts between
     * insertion and lookup. See @ref typefaces and registerTypeface() for why
     * a name-only key is unsafe.
     *
     * @param name  juce::Typeface::getName() / juce::Font::getTypefaceName().
     * @param style juce::Typeface::getStyle() / juce::Font::getTypefaceStyle().
     * @return The composite key, e.g. "Display/Book".
     */
    static juce::String typefaceKey (const juce::String& name, const juce::String& style);

    /** @brief Display font constructed from theme config (family, size), no kerning.
     *  Reads Id::toType (Id::tab) properties: Id::fontFamily, Id::fontSize — same source
     *  as getTabFont() minus the kerning factor.
     */
    juce::Font getCommonFont() const;

    //==============================================================================
    /**
     * @brief Reads graphics.font_rasterizer/font_gamma/font_contrast from
     *        config.lua and calls the Vulkan glyph atlas's setRasterization().
     *
     * Reaches the atlas via jam::GlyphAtlas::getInstance() directly rather
     * than a stored reference — the atlas now always exists once
     * ENDApplication::initialiseVulkan() has run (font events live with the font
     * owner, but the atlas itself stays owned by jam::VulkanEngine). Asserts
     * the instance is non-null rather than silently no-op-ing: by the time any
     * of this method's three callers (registerTypeface()'s tail, and the
     * fontRasterizer/fontGamma/fontContrast event handlers below) can run,
     * VulkanEngine construction has already happened. Called once from
     * registerTypeface() right after registration (before this atlas ever
     * paints a glyph), and again by the
     * fontRasterizer/fontGamma/fontContrast event handlers on config hot-reload.
     *
     * Only these three properties warrant this call: they change the
     * rasterized bitmap for an otherwise-unchanged jam::GlyphAtlas::Key (same
     * typeface/glyphIndex/fontSize, different backend or coverage LUT) — font
     * family and size changes are a different Key outright and need no
     * explicit atlas action (see registerEvents()'s doc comment for the full
     * glyph-identity config audit). GlyphAtlas::setRasterization() itself only
     * rebuilds the LUT and flushes the cache when backend/gamma/contrast
     * actually differ from their current values — reapplying identical values
     * here is a correct no-op, not a gap, since no caller ever needs a flush
     * without an actual change. Tail-calls Component::sendLookAndFeelChange()
     * on every juce::Desktop top-level window (ENDLookAndFeel owns no
     * Component of its own to call it on directly) — the atlas rebuild above
     * invalidates every cached glyph bitmap for an otherwise-unchanged
     * GlyphAtlas::Key, and EditorView's own lookAndFeelChanged() is the
     * sole path that recomputes cell metrics against the now-current atlas
     * state and repaints. A no-op at startup (registerTypeface()'s own tail
     * call runs before ENDWindow exists). Defined in EventRegistration.cpp.
     */
    void setFontRasterization();

    /**
     * @brief Reads code.embolden from config.lua and calls the Vulkan glyph
     *        atlas's setEmbolden() (`FT_Outline_Embolden` restoration at the
     *        FreeType rasterize site).
     *
     * Reaches the atlas via jam::GlyphAtlas::getInstance() directly, same
     * precedent as setFontRasterization() (font events live with the font
     * owner). Called once from registerTypeface()'s tail — the same place
     * setFontRasterization() runs — and again by the embolden event handler
     * on config hot-reload. Tail-calls the same juce::Desktop-reached
     * Component::sendLookAndFeelChange() cascade as setFontRasterization()'s
     * own tail comment — embolden changes the rasterized bitmap for an
     * otherwise-unchanged GlyphAtlas::Key too. Defined in EventRegistration.cpp.
     */
    void setEmbolden();

    /**
     * @brief Reads SVG content from the GRAPHICS child of config.state and
     *        parses each property into SVG::Flex::Segments.
     *
     * Iterates properties of the GRAPHICS child via jam::Model::forEachProperty.
     * For each string property: the key is the suffix after the last '_' when it
     * matches a Id::ButtonState entry, or the full property name otherwise.
     * All SVGs are coloured with the full colourMap. No disk I/O.
     * Defined in EventRegistration.cpp.
     */
    void loadGraphics();

    /**
     * @brief Builds ColourMap from config.state and applies all component colours.
     *
     * Calls jam::ColourMap::fromValueTree on config.state, then maps every
     * component-tree colour property to its corresponding JUCE or END ColourId via
     * setColourId (code, scrollbar, tab, button, overlay, pane, statusBar, hint).
     * Finalises by calling setColours on config.state.
     * Defined in EventRegistration.cpp.
     */
    void initialiseColours();

    /**
     * @brief Applies window background, opacity, text, and highlight colours
     *        to the juce::PopupMenu colour IDs.
     *
     * Reads Id::toType (Id::menu) properties against the window background colour
     * (Id::toType (Id::window)). Called once at construction (after initialiseColours())
     * and again on Id::toType (Id::menu)'s colour-refresh event. Defined in
     * EventRegistration.cpp.
     */
    void setPopupMenuColours();

    /**
     * @brief Populates the events map with ValueTree property/type-keyed callbacks.
     *
     * Registers handlers for:
     * - Id::theme         → initialiseColours() + loadGraphics()
     * - Id::toType (Id::code), Id::toType (Id::scrollbar), Id::toType (Id::tab), Id::toType (Id::button),
     *   Id::toType (Id::overlay), Id::toType (Id::pane), Id::toType (Id::statusBar), Id::toType (Id::hint)
     *                     → setColours(config.state)
     * - Id::fontRasterizer, Id::fontGamma, Id::fontContrast
     *                     → setFontRasterization() (font events live with the
     *                       font owner — relocated from ENDView), whose own
     *                       tail cascades Component::sendLookAndFeelChange()
     *                       to every juce::Desktop top-level window (see
     *                       setFontRasterization()'s own doc comment)
     * - Id::embolden      → setEmbolden() (same font-owner precedent as
     *                       fontRasterizer/fontGamma/fontContrast, same
     *                       tail cascade)
     *
     * Glyph-identity config coverage audit — every config value that can alter
     * what a glyph looks like was inventoried; only fontRasterizer/fontGamma/
     * fontContrast/embolden route to the atlas. tab.font_family/font_size/
     * kerning_factor and jam::overlay's font_family/font_size do NOT need a
     * dedicated handler: jam::GlyphAtlas::Key (jam_GlyphAtlas.h) identifies a
     * cached glyph by {typeface pointer, glyphIndex, fontSize} — a new family
     * resolves to a new typeface pointer and a new size is a new Key member, so
     * both cache-miss and re-rasterize correctly on the very next paint with no
     * atlas call required; kerning_factor never reaches Key at all (it shifts
     * glyph pen positions, never glyph identity). getTabFont() and
     * MessageOverlay::paint() already re-read config on every paint, and any
     * theme.lua edit unconditionally fires Id::theme (ConfigTheme::loadFromPath)
     * into the theme handler above, which repaints the whole component tree
     * (juce::Component::sendLookAndFeelChange descends to every child) — so the
     * new family/size is picked up immediately. status_bar/action_list
     * font_family/font_size need no dedicated handler here either —
     * StatusBar/ActionList components do not exist in Source yet, nothing to
     * route to until they do. The terminal code font (code.font_family/
     * font_size) is no longer this audit's gap: the glyph-rendering pipeline
     * now exists, and EditorView owns its own zoom event handler
     * which funnels into EditorView's own lookAndFeelChanged(), which
     * calls this class's getCodeMetrics (zoom) to recompute cell metrics and
     * re-applies them to jam::CodeView, entirely independent of this class's
     * own event map. Only fontRasterizer/fontGamma/fontContrast/embolden
     * change the rasterized bitmap for an UNCHANGED Key (same typeface/
     * glyphIndex/fontSize, different backend, coverage LUT, or synthetic
     * embolden), which is exactly why setFontRasterization()/setEmbolden()
     * must be re-invoked explicitly on those four — and, since ENDView no
     * longer cascades Component::sendLookAndFeelChange() unconditionally on
     * every runtime property change (ARCHITECT ruling — runtime state must
     * never cascade LookAndFeel), why those same two methods now cascade it
     * themselves, reached via juce::Desktop (see setFontRasterization()'s
     * own doc comment). Defined in EventRegistration.cpp.
     */
    void registerEvents();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDLookAndFeel)
};
