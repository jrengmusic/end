/**
 * @file CursorComponent.h
 * @brief Cursor overlay component with blink, tinting, and colour-emoji support.
 *
 * CursorComponent renders the terminal cursor as a JUCE component overlaid on
 * the terminal grid.  It is positioned and sized by `Terminal::Component::updateCursorBounds()`.
 *
 * ### Cursor glyph
 * The cursor shape is any Unicode codepoint configured via `Config::Key::cursorChar`
 * (default: U+2588 FULL BLOCK █).  The glyph is rasterized at construction via
 * `Fonts::rasterizeToImage()` into `cursorAlpha`.
 *
 * ### Colour emoji support
 * If `rasterizeToImage()` sets `colourEmoji = true`, the rasterized image
 * already contains full colour data and is used directly as `cursorImage`.
 * Otherwise `rebuildTintedImage()` tints the alpha-channel image with
 * `Config::Key::coloursCursor`.
 *
 * ### Wide cursor
 * `cursorWidth` tracks how many grid cells the cursor glyph spans (1 for
 * normal, 2 for double-width glyphs).  `Terminal::Component` multiplies the
 * cell width by this value when calling `setBounds()`.
 *
 * ### Blink
 * If `Config::Key::cursorBlink` is true, a `juce::Timer` toggles `cursorVisible`
 * at the configured interval.  `resetBlink()` restarts the timer and forces the
 * cursor visible — called on every keypress so the cursor is always visible
 * immediately after typing.
 *
 * ### ValueTree listener
 * Listens to the cursor state subtree for `cursorVisible` property changes
 * published by the VT parser (e.g. DECTCEM — DEC private mode 25).
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 *
 * @see Terminal::Component
 * @see Fonts::rasterizeToImage
 * @see Config::Key::cursorChar
 * @see Config::Key::cursorBlink
 * @see Config::Key::coloursCursor
 */

#pragma once

#include <JuceHeader.h>
#include "../terminal/data/Identifier.h"
#include "../terminal/rendering/Fonts.h"
#include "../config/Config.h"

/**
 * @class CursorComponent
 * @brief Cursor overlay: renders a Unicode glyph with blink and colour tinting.
 *
 * Inherits `juce::Component` for rendering, `juce::Timer` (private) for blink,
 * and `juce::ValueTree::Listener` (private) to react to DECTCEM visibility
 * changes published by the VT parser.
 *
 * @par Ownership
 * Owned by `Terminal::Component` as a `unique_ptr`.  The cursor state ValueTree
 * is borrowed (not owned); `rebindToScreen()` is called when the active screen
 * switches between primary and alternate.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public and private methods.
 *
 * @see Terminal::Component::updateCursorBounds
 * @see Terminal::Component::valueTreePropertyChanged
 */
class CursorComponent
    : public juce::Component
    , private juce::Timer
    , private juce::ValueTree::Listener
{
public:
    /**
     * @brief Constructs CursorComponent: rasterizes the cursor glyph and starts blink.
     *
     * Steps:
     * 1. Reads `cursorChar`, `fontSize`, `coloursCursor`, and `cursorBlink` from Config.
     * 2. Calls `Fonts::rasterizeToImage()` to produce `cursorAlpha`.
     * 3. Computes `cursorWidth` (number of grid cells spanned by the glyph).
     * 4. If colour emoji: uses `cursorAlpha` directly as `cursorImage`.
     *    Otherwise: calls `rebuildTintedImage()` to tint with `cursorColour`.
     * 5. Registers as a ValueTree listener on @p cursorState.
     * 6. Starts the blink timer if `cursorBlink` is true.
     *
     * @param cursorState  The cursor state ValueTree subtree from Session.
     * @note MESSAGE THREAD.
     */
    CursorComponent (juce::ValueTree cursorState);

    /**
     * @brief Removes the ValueTree listener before destruction.
     * @note MESSAGE THREAD.
     */
    ~CursorComponent() override;

    /**
     * @brief Rebinds the ValueTree listener to a new screen's cursor state.
     *
     * Called by `Terminal::Component` when the active screen switches between
     * primary and alternate (DEC private mode 1049).  Removes the listener from
     * the old tree and adds it to @p screen.
     *
     * @param screen  The new cursor state ValueTree to listen to.
     * @note MESSAGE THREAD.
     */
    void rebindToScreen (juce::ValueTree screen);

    /**
     * @brief Forces the cursor visible and restarts the blink timer.
     *
     * Called by `Terminal::Component::keyPressed()` on every keypress so the
     * cursor is always visible immediately after typing, regardless of blink phase.
     *
     * @note MESSAGE THREAD.
     */
    void resetBlink();

    /**
     * @brief Returns the number of grid cells spanned by the cursor glyph.
     *
     * 1 for normal-width glyphs, 2 for double-width glyphs.  Used by
     * `Terminal::Component::updateCursorBounds()` to set the correct component width.
     *
     * @return Cell width multiplier (1 or 2).
     */
    int getCellWidth() const noexcept;

    /**
     * @brief Draws the cursor image if visible and valid.
     *
     * Stretches `cursorImage` to fill the component bounds using
     * `RectanglePlacement::stretchToFit`.  Does nothing if `cursorVisible` is
     * false (blink off) or if `cursorImage` is invalid.
     *
     * @param g  JUCE graphics context for this paint pass.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override;

private:
    /**
     * @brief Toggles `cursorVisible` and triggers a repaint.
     *
     * Called by the JUCE timer at the configured blink interval.
     * @note MESSAGE THREAD.
     */
    void timerCallback() override;

    /**
     * @brief Reacts to `cursorVisible` property changes from the VT parser.
     *
     * Listens for `Terminal::ID::value` changes on `Terminal::ID::PARAM` nodes
     * with `id == Terminal::ID::cursorVisible`.  Calls `setVisible()` to show or
     * hide the component in response to DECTCEM (DEC private mode 25).
     *
     * @param tree      The ValueTree node that changed.
     * @param property  The property identifier that changed.
     * @note MESSAGE THREAD.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /**
     * @brief Rebuilds `cursorImage` by tinting `cursorAlpha` with `cursorColour`.
     *
     * Copies `cursorAlpha`, then iterates every pixel and replaces the colour
     * with `cursorColour` while preserving the original alpha channel.  This
     * produces a solid-colour cursor glyph at the configured tint colour.
     *
     * @note Only called for non-colour-emoji glyphs.  Colour emoji glyphs use
     *       `cursorAlpha` directly as `cursorImage`.
     */
    void rebuildTintedImage() noexcept;

    //==============================================================================
    /** @brief The cursor state ValueTree subtree; source of DECTCEM visibility events. */
    juce::ValueTree cursorState;

    /**
     * @brief Alpha-channel rasterization of the cursor glyph at the configured size.
     *
     * For non-colour-emoji glyphs this is a greyscale alpha mask.  For colour
     * emoji it contains full ARGB colour data and is used directly as `cursorImage`.
     */
    juce::Image cursorAlpha;

    /**
     * @brief The final cursor image drawn by `paint()`.
     *
     * For colour emoji: same object as `cursorAlpha`.
     * For normal glyphs: a tinted copy produced by `rebuildTintedImage()`.
     */
    juce::Image cursorImage;

    /**
     * @brief Number of grid cells spanned by the cursor glyph (1 or 2).
     *
     * Computed at construction from the rasterized image width divided by the
     * physical cell width.  Used by `Terminal::Component::updateCursorBounds()`.
     */
    int cursorWidth { 1 };

    /** @brief Tint colour applied to the cursor glyph (from `Config::Key::coloursCursor`). */
    juce::Colour cursorColour;

    /**
     * @brief Current blink state: @c true = cursor drawn, @c false = cursor hidden.
     *
     * Toggled by `timerCallback()` at the configured blink interval.
     * Reset to @c true by `resetBlink()` on every keypress.
     */
    bool cursorVisible { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CursorComponent)
};

//==============================================================================
// Inline implementations
//==============================================================================

inline CursorComponent::CursorComponent (juce::ValueTree state)
    : cursorState (state)
{
    auto* cfg { Config::getContext() };
    const float fontSize { cfg->getFloat (Config::Key::fontSize) };
    const Fonts::Metrics metrics { Fonts::getContext()->calcMetrics (fontSize) };

    const juce::String cursorCharStr { cfg->getString (Config::Key::cursorChar) };
    auto charPtr { cursorCharStr.getCharPointer() };
    const uint32_t codepoint { static_cast<uint32_t> (charPtr.getAndAdvance()) };
    bool colourEmoji { false };
    cursorAlpha = Fonts::getContext()->rasterizeToImage (codepoint, fontSize, colourEmoji);

    if (metrics.physCellW > 0 and cursorAlpha.isValid())
    {
        cursorWidth = (cursorAlpha.getWidth() + metrics.physCellW - 1) / metrics.physCellW;
    }

    cursorColour = cfg->getColour (Config::Key::coloursCursor);

    if (colourEmoji)
    {
        cursorImage = cursorAlpha;
    }
    else
    {
        rebuildTintedImage();
    }

    cursorState.addListener (this);
    setOpaque (false);

    if (cfg->getBool (Config::Key::cursorBlink))
    {
        startTimer (cfg->getInt (Config::Key::cursorBlinkInterval));
    }
}

inline CursorComponent::~CursorComponent()
{
    cursorState.removeListener (this);
}

inline int CursorComponent::getCellWidth() const noexcept { return cursorWidth; }

inline void CursorComponent::rebindToScreen (juce::ValueTree screen)
{
    cursorState.removeListener (this);
    cursorState = screen;
    cursorState.addListener (this);
}

inline void CursorComponent::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == Terminal::ID::value and tree.getType() == Terminal::ID::PARAM)
    {
        const auto paramId { tree.getProperty (Terminal::ID::id).toString() };

        if (paramId == Terminal::ID::cursorVisible.toString())
        {
            setVisible (static_cast<bool> (tree.getProperty (Terminal::ID::value)));
        }
    }
}

inline void CursorComponent::resetBlink()
{
    cursorVisible = true;

    if (isTimerRunning())
        startTimer (getTimerInterval());

    repaint();
}

inline void CursorComponent::rebuildTintedImage() noexcept
{
    if (cursorAlpha.isValid())
    {
        cursorImage = cursorAlpha.createCopy();
        juce::Image::BitmapData data (cursorImage, juce::Image::BitmapData::readWrite);

        for (int y { 0 }; y < data.height; ++y)
        {
            for (int x { 0 }; x < data.width; ++x)
            {
                const juce::Colour src { data.getPixelColour (x, y) };
                data.setPixelColour (x, y, cursorColour.withAlpha (src.getAlpha()));
            }
        }
    }
}

inline void CursorComponent::paint (juce::Graphics& g)
{
    if (cursorVisible and cursorImage.isValid())
    {
        g.drawImage (cursorImage, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::stretchToFit);
    }
}

inline void CursorComponent::timerCallback()
{
    cursorVisible = not cursorVisible;
    repaint();
}
