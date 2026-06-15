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
 * This routes theme rebuild (ID::theme), SVG reload (IDtype::graphics,
 * IDtype::tabButton), and per-component colour refresh (IDtype::code, scrollbar,
 * tab, button, overlay, pane, statusBar, hint) through one code path.
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
     * @brief Registers typefaces, initialises colours from theme, loads SVG
     *        graphics, registers events, and adds listeners to config and theme.
     */
    LookAndFeel();

    /** @brief Removes listeners from theme and config. */
    ~LookAndFeel();

    /**
     * @brief Single-key event dispatch through the events map.
     *
     * Checks whether @p property has a registered handler; if not, falls back to
     * @p tree.getType(). Routes theme rebuild (ID::theme), SVG reload
     * (IDtype::graphics, IDtype::tabButton), and per-component colour refresh to
     * their respective callbacks.
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

    /** @brief Packed window glass parameters from the active theme.
     *  Returns { argb colour, blur radius, windowFX style } packed into
     *  Union<uint32_t, int16_t, int16_t>. Consumer unpacks via structured binding.
     *  Returns zeros when theme is not loaded.
     */

    using Glass = jam::Union<juce::Colour, int16_t, int16_t>;

    Glass getWindowGlass() const;

    /** @brief Pane resizer bar — SVG Flex rendering with pane colourIds.
     *  Hover/pressed state swaps bar colour to highlight colour.
     */
    void drawResizerBar (juce::Graphics& g, juce::Component& bar) override;

private:
    /** @brief Rendering-context typeface registry and shared glyph atlas. */
    jam::TypefaceResources typefaceResources;

    /** @brief Rendering-context style table — self-registers as jam::Stamp::getInstance(). */
    jam::Stamp stampInstance;

    /** @brief Rendering-context grapheme cluster table — self-registers as jam::Grapheme::getInstance(). */
    jam::Grapheme graphemeInstance;

    /** @brief Singleton config model reference — source for theme path and top-level config values. */
    config::Model& config { *config::Model::getInstance() };

    /** @brief Theme subtree reference from the config model — source for all per-component colour and metric values. */
    config::Theme& theme { config.getTheme() };

    //==============================================================================
    /** @brief JUCE embedded font ownership — Ptrs kept alive so font names resolve
     *  via juce::Font name lookup without requiring system-installed fonts.
     *  @see registerTypeface
     */
    jam::HashMap<juce::String, juce::Typeface::Ptr> typefaces;

    /** @brief Parsed SVG assets keyed by their GRAPHICS property id or button
     *  state identifier.
     *
     *  Each entry is a SVG::Flex::Segments set (9-slice layout, paint-ready)
     *  produced by SVG::Flex::getSegments from the SVG file named by the
     *  matching property in the IDtype::graphics ValueTree. Rebuilt by
     *  loadGraphics() on construction and on every SVG file-change event.
     *
     *  Keys are either:
     *  - GRAPHICS property identifiers (ID::tabBar, ID::tabHighlight) for the
     *    bar background and sliding highlight assets, or
     *  - tab-button state identifiers (jam::ID::normal, jam::ID::over, …) for
     *    the per-state button Segments, one entry per authored state slot.
     */
    jam::HashMap<juce::Identifier, jam::SVG::Flex::Segments> graphics;

    /**
     * @brief Event dispatch map keyed by juce::Identifier (property or tree type).
     *
     * Populated by registerEvents(). Handles:
     * - ID::theme         — full theme rebuild via initialiseColours()
     * - IDtype::graphics  — SVG asset reload via loadGraphics()
     * - IDtype::tabButton — SVG asset reload via loadGraphics()
     * - IDtype::code, IDtype::scrollbar, IDtype::tab, jam::IDtype::button,
     *   jam::IDtype::overlay, IDtype::pane, IDtype::statusBar, IDtype::hint
     *                     — per-component colour refresh via setColours()
     */
    jam::Function::Map<juce::Identifier, void> events;

    //==============================================================================
    /**
     * @brief Registers embedded JUCE typefaces and builds the composite code
     *        typeface with platform emoji and nerd-font fallbacks.
     *
     * JUCE side: creates Typeface::Ptrs from JAM embedded binary data and stores
     * them in @ref typefaces so font name lookup resolves without system-installed
     * fonts. JAM side: constructs a jam::Typeface for the code font family from
     * theme, adds platform emoji fallback (Apple Color Emoji / Segoe UI Emoji /
     * Noto Color Emoji), DisplayMonoBook nerd-font fallback, SymbolsNerdFont
     * fallback, and a DisplayMonoBold style variant. Defined in EventRegistration.cpp.
     */
    void registerTypeface();

    /**
     * @brief Parses SVG assets from the theme graphics directory into SVG::Flex::Segments.
     *
     * Iterates the theme ValueTree recursively, reads each file listed in the
     * ID::graphics property, resolves the key as a button-state identifier when the
     * filename suffix matches jam::map::ButtonState, or as the filename stem
     * otherwise. Stores the resulting jam::SVG::Flex::Segments in @ref graphics,
     * keyed by that identifier. Colour map for each tree node is sourced from the
     * matching child of colourMap, falling back to the root colourMap.
     * Defined in EventRegistration.cpp.
     */
    void loadGraphics();

    /**
     * @brief Builds ColourMap from theme state and applies all component colours.
     *
     * Calls jam::ColourMap::fromValueTree on the theme state, then maps every
     * component-tree colour property to its corresponding JUCE or END ColourId via
     * setColourId (code, scrollbar, tab, button, overlay, pane, statusBar, hint).
     * Finalises by calling setColours on the full theme state.
     * Defined in EventRegistration.cpp.
     */
    void initialiseColours();

    /**
     * @brief Populates the events map with ValueTree property/type-keyed callbacks.
     *
     * Registers handlers for:
     * - ID::theme         → initialiseColours()
     * - IDtype::graphics  → loadGraphics()
     * - IDtype::tabButton → loadGraphics()
     * - IDtype::code, IDtype::scrollbar, IDtype::tab, jam::IDtype::button,
     *   jam::IDtype::overlay, IDtype::pane, IDtype::statusBar, IDtype::hint
     *                     → setColours(theme.state)
     * Defined in EventRegistration.cpp.
     */
    void registerEvents();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
