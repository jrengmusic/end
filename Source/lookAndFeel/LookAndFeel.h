/**
 * @file lookAndFeel/LookAndFeel.h
 * @brief END's theme-driven LookAndFeel — SVG Flex tab/bar graphics, live colour mapping.
 */
#pragma once
#include <JuceHeader.h>
#include <JamFontsBinaryData.h>
#include "Identifier.h"
#include "../config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

/**
 * @class LookAndFeel
 * @brief END's look-and-feel — theme-driven rendering with live ValueTree updates.
 *
 * Inherits jam::LookAndFeel::Methods for tab bar background, highlight, button,
 * and resizer rendering. Listens to both the config root ValueTree and the theme
 * subtree for live theme changes.
 *
 * Event dispatch uses a single-key lookup: property key takes priority; when no
 * event is registered for the property, the lookup falls back to tree.getType().
 * This routes theme rebuild (ID::theme) and per-component colour refresh
 * (IDtype::code, scrollbar, tab, button, overlay, pane, statusBar, hint)
 * through one code path.
 */
class LookAndFeel
    : public jam::LookAndFeel::Methods<LookAndFeel>
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
    LookAndFeel();

    /** @brief Removes the listener from config. */
    ~LookAndFeel();

    /**
     * @brief Registers END's six embedded typefaces — one pass, one parse per
     *        font (SSOT font list) — creating each Typeface::Ptr, storing it in
     *        @ref typefaces for name-based lookup (getTypefaceForFont()), and
     *        registering it with the Vulkan glyph atlas so its mono glyphs
     *        rasterize through FreeType's autofitter instead of the EdgeTable
     *        fallback.
     *
     * Cannot run at LookAndFeel construction time — the atlas (owned by
     * jam::vulkan::Registry) does not exist yet then (end::View constructs its
     * Registry after end::LookAndFeel, per end::Application's member order,
     * Main.h). Called once, externally, immediately after Registry construction
     * (end::View's constructor). Also applies the shipped/user-configured glyph
     * rasterization backend/gamma/contrast (see applyFontRasterization()) before
     * this atlas ever paints a glyph.
     *
     * @param atlas  The Vulkan glyph atlas to register with.
     */
    void registerTypeface (jam::GlyphAtlas& atlas);

    /**
     * @brief Single-key event dispatch through the events map.
     *
     * Checks whether @p property has a registered handler; if not, falls back to
     * @p tree.getType(). Routes theme rebuild (ID::theme) and per-component
     * colour refresh to their respective callbacks.
     *
     * @param tree     The ValueTree whose property changed.
     * @param property The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Renders the tab bar background using SVG Flex segments (ID::tabBar).
     *
     * Applies a rotation AffineTransform for vertical orientations: -90 degrees
     * for left-side bars, +90 degrees for right-side bars.
     *
     * @param g   Graphics context.
     * @param bar The bar background component.
     */
    void drawBarBackground (juce::Graphics& g, juce::Component& bar) override;

    /**
     * @brief Renders the tab highlight indicator using SVG Flex segments (ID::tabHighlight).
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
     * State is resolved via jam::SVG::Button::getState and mapped to a
     * jam::map::ButtonState identifier. Paint occurs only when that state slot
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
     *  No border, centred, colour from Label::textColourId (synced by Tab::buttonStateChanged).
     */
    void drawTabLabel (juce::Graphics& g, juce::Label& label) override;

    /**
     * @brief Returns tab font constructed from theme config (family, size, kerning).
     *
     * Reads IDtype::tab properties: ID::fontFamily, ID::fontSize, ID::kerningFactor.
     */
    juce::Font getTabFont() const override;

    /** @brief Returns horizontal padding in pixels per side of the tab label.
     *  Reads tab.text_padding from the display config (user-configurable).
     */
    int getTabPadding() const override;

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
     * @param font The font whose typeface is being resolved.
     * @return The registered Ptr when @p font names one of the six embedded
     *         fonts; otherwise the base class result.
     */
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    /** @brief Display transform applied to a tab label before measuring and painting.
     *  Reads tab.uppercase from the display config; returns toUpperCase() when set,
     *  identity otherwise. Consumed by both Bar::getBestTabLength (measurement) and
     *  drawTabButton (render) so measured width and painted string never diverge.
     */
    juce::String getTabText (const juce::String& tabName) const override;

    /** @brief Component-level padding for the tab bar from display config.
     *  Reads tab.padding { top, right, bottom, left } (CSS convention).
     */
    juce::BorderSize<int> getTabBarPadding() const override;

    /** @brief Window style parameters from the active theme.
     *  Returns { tint colour, blur radius, windowFX style, window-button visibility }
     *  read from IDtype::window and IDtype::style.  Consumer unpacks via structured binding.
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

    /** @brief Pane resizer bar — SVG Flex rendering with pane colourIds.
     *  Hover/pressed state swaps bar colour to highlight colour.
     */
    void drawResizerBar (juce::Graphics& g, juce::Component& bar) override;

private:
    /** @brief Rendering-context style table — self-registers as jam::Stamp::getInstance(). */
    jam::Stamp stampInstance;

    /** @brief Rendering-context grapheme cluster table — self-registers as jam::Grapheme::getInstance(). */
    jam::Grapheme graphemeInstance;

    // /** @brief Singleton config model reference — source for theme path and top-level config values. */
    config::Model& config { *config::Model::getInstance() };

    //==============================================================================
    /** @brief JUCE embedded font ownership — Ptrs kept alive so font names resolve
     *  via juce::Font name lookup without requiring system-installed fonts.
     *  @see registerTypeface
     */
    jam::HashMap<juce::String, juce::Typeface::Ptr> typefaces;

    /** @brief Parsed SVG assets keyed by their GRAPHICS child property name or
     *  button state identifier.
     *
     *  Each entry is a SVG::Flex::Segments set (9-slice layout, paint-ready)
     *  produced by SVG::Flex::getSegments from the SVG content stored as a
     *  property of the GRAPHICS child of config.state. Rebuilt by loadGraphics()
     *  on construction and on every ID::theme rebuild event.
     *
     *  Keys are resolved from the property name: the suffix after the last '_'
     *  is used when it matches a jam::map::ButtonState entry; otherwise the full
     *  property name is the key. All SVGs are coloured with the full colourMap.
     */
    jam::HashMap<juce::Identifier, jam::SVG::Flex::Segments> graphics;

    /**
     * @brief Event dispatch map keyed by juce::Identifier (property or tree type).
     *
     * Populated by registerEvents(). Handles:
     * - ID::theme         — full theme rebuild via initialiseColours() + loadGraphics()
     * - IDtype::code, IDtype::scrollbar, IDtype::tab, jam::IDtype::button,
     *   jam::IDtype::overlay, IDtype::pane, IDtype::statusBar, IDtype::hint
     *                     — per-component colour refresh via setColours()
     * - ID::fontRasterizer, ID::fontGamma, ID::fontContrast
     *                     — re-applies applyFontRasterization() on config hot-reload
     */
    jam::Function::Map<juce::Identifier, void> events;

    //==============================================================================
    /**
     * @brief Reads graphics.font_rasterizer/font_gamma/font_contrast from
     *        config.lua and calls the Vulkan glyph atlas's setRasterization().
     *
     * Reaches the atlas via jam::vulkan::Registry::getInstance() rather than a
     * stored reference — Registry now always exists once end::View's
     * constructor has run (font events live with the font owner, but the atlas
     * itself stays owned by Registry). Asserts the instance is non-null rather
     * than silently no-op-ing: by the time any of this method's three callers
     * (registerTypeface()'s tail, and the fontRasterizer/fontGamma/fontContrast
     * event handlers below) can run, Registry construction has already
     * happened. Called once from registerTypeface() right after registration
     * (before this Registry's atlas ever paints a glyph), and again by the
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
     * without an actual change. Defined in EventRegistration.cpp.
     */
    void applyFontRasterization();

    /**
     * @brief Reads SVG content from the GRAPHICS child of config.state and
     *        parses each property into SVG::Flex::Segments.
     *
     * Iterates properties of the GRAPHICS child via jam::Model::forEachProperty.
     * For each string property: the key is the suffix after the last '_' when it
     * matches a jam::map::ButtonState entry, or the full property name otherwise.
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
     * @brief Populates the events map with ValueTree property/type-keyed callbacks.
     *
     * Registers handlers for:
     * - ID::theme         → initialiseColours() + loadGraphics()
     * - IDtype::code, IDtype::scrollbar, IDtype::tab, jam::IDtype::button,
     *   jam::IDtype::overlay, IDtype::pane, IDtype::statusBar, IDtype::hint
     *                     → setColours(config.state)
     * - ID::fontRasterizer, ID::fontGamma, ID::fontContrast
     *                     → applyFontRasterization() (font events live with the
     *                       font owner — relocated from end::View)
     *
     * Glyph-identity config coverage audit — every config value that can alter
     * what a glyph looks like was inventoried; only fontRasterizer/fontGamma/
     * fontContrast route to applyFontRasterization(). tab.font_family/font_size/
     * kerning_factor and jam::overlay's font_family/font_size do NOT need a
     * dedicated handler: jam::GlyphAtlas::Key (jam_GlyphAtlas.h) identifies a
     * cached glyph by {typeface pointer, glyphIndex, fontSize} — a new family
     * resolves to a new typeface pointer and a new size is a new Key member, so
     * both cache-miss and re-rasterize correctly on the very next paint with no
     * atlas call required; kerning_factor never reaches Key at all (it shifts
     * glyph pen positions, never glyph identity). getTabFont() and
     * MessageOverlay::paint() already re-read config on every paint, and any
     * theme.lua edit unconditionally fires ID::theme (config::Theme::loadFromPath)
     * into the theme handler above, which repaints the whole component tree
     * (juce::Component::sendLookAndFeelChange descends to every child) — so the
     * new family/size is picked up immediately. status_bar/action_list
     * font_family/font_size and the terminal code font are configured in
     * theme.lua but have no live glyph-rendering call site yet (StatusBar/
     * ActionList components do not exist in Source; registerTypeface() stubs
     * the code typeface pending the new glyph pipeline) — nothing to route to
     * until that pipeline exists. Only fontRasterizer/fontGamma/fontContrast
     * change the rasterized bitmap for an UNCHANGED Key (same typeface/glyphIndex/
     * fontSize, different backend or coverage LUT), which is exactly why
     * GlyphAtlas::setRasterization() must be re-invoked explicitly on those three.
     * Defined in EventRegistration.cpp.
     */
    void registerEvents();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
