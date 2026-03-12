/**
 * @file CursorComponent.h
 * @brief Cursor overlay with DECSCUSR shape support, OSC 12 color, and user glyph fallback.
 *
 * Renders the terminal cursor as a JUCE component overlaid on the terminal grid.
 * Supports three rendering modes:
 *
 * 1. **User glyph** (DECSCUSR Ps=0 or cursor.force=true): any Unicode codepoint
 *    from `cursor.char`, rasterized via `Fonts::rasterizeToImage()`.
 * 2. **Geometric block** (DECSCUSR Ps=1/2): filled rectangle covering the full cell.
 * 3. **Geometric underline** (DECSCUSR Ps=3/4): thin horizontal rect at cell bottom.
 * 4. **Geometric bar** (DECSCUSR Ps=5/6): thin vertical rect at cell left edge.
 *
 * DECSCUSR odd Ps = blinking, even Ps = steady.
 * OSC 12 overrides cursor color. OSC 112 resets to config default.
 * `cursor.force = true` ignores all VT overrides.
 *
 * @note All methods are called on the **MESSAGE THREAD**.
 */

#pragma once

#include <JuceHeader.h>
#include "../terminal/data/Identifier.h"
#include "../terminal/rendering/Fonts.h"
#include "../config/Config.h"

/**
 * @class CursorComponent
 * @brief Cursor overlay: DECSCUSR geometric shapes + user glyph + blink + color override.
 */
class CursorComponent
    : public juce::Component
    , private juce::Timer
    , private juce::ValueTree::Listener
{
public:
    /**
     * @brief Constructs CursorComponent: rasterizes user glyph, reads config, starts blink.
     * @param cursorState  The cursor state ValueTree subtree (NORMAL or ALTERNATE).
     * @note MESSAGE THREAD.
     */
    CursorComponent (juce::ValueTree cursorState);

    /** @brief Removes the ValueTree listener. @note MESSAGE THREAD. */
    ~CursorComponent() override;

    /**
     * @brief Rebinds the ValueTree listener to a new screen's cursor state.
     * @param screen  The new cursor state ValueTree.
     * @note MESSAGE THREAD.
     */
    void rebindToScreen (juce::ValueTree screen);

    /**
     * @brief Forces the cursor visible and restarts the blink timer.
     * @note MESSAGE THREAD.
     */
    void resetBlink();

    /**
     * @brief Returns the number of grid cells spanned by the cursor glyph.
     * @return Cell width multiplier (1 or 2).
     */
    int getCellWidth() const noexcept;

    /**
     * @brief Draws the cursor: geometric rect for DECSCUSR, glyph for user custom.
     * @param g  JUCE graphics context.
     * @note MESSAGE THREAD.
     */
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    void rebuildTintedImage() noexcept;
    void applyShape (int shape) noexcept;
    juce::Colour getActiveColour() const noexcept;

    //==============================================================================
    juce::ValueTree cursorState;

    juce::Image cursorAlpha;
    juce::Image cursorImage;
    int cursorWidth { 1 };

    juce::Colour configColour;
    juce::Colour overrideColour;
    bool hasColorOverride { false };

    int currentShape { 0 };
    bool forceUserCursor { false };

    bool blinkEnabled { false };
    int blinkInterval { 500 };
    bool cursorVisible { true };

    static constexpr float cursorThickness { 0.15f };

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

    configColour = cfg->getColour (Config::Key::coloursCursor);
    forceUserCursor = cfg->getBool (Config::Key::cursorForce);
    blinkEnabled = cfg->getBool (Config::Key::cursorBlink);
    blinkInterval = cfg->getInt (Config::Key::cursorBlinkInterval);

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

    if (blinkEnabled)
    {
        startTimer (blinkInterval);
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

inline juce::Colour CursorComponent::getActiveColour() const noexcept
{
    if (not forceUserCursor and hasColorOverride)
    {
        return overrideColour;
    }

    return configColour;
}

inline void CursorComponent::applyShape (int shape) noexcept
{
    if (not forceUserCursor)
    {
        currentShape = shape;

        if (shape == 0)
        {
            if (blinkEnabled)
            {
                startTimer (blinkInterval);
            }
            else
            {
                stopTimer();
            }
        }
        else
        {
            const bool shouldBlink { (shape % 2) != 0 };

            if (shouldBlink)
            {
                startTimer (blinkInterval);
            }
            else
            {
                stopTimer();
                cursorVisible = true;
                repaint();
            }
        }
    }
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
        else if (paramId == Terminal::ID::cursorShape.toString())
        {
            applyShape (static_cast<int> (tree.getProperty (Terminal::ID::value)));
            repaint();
        }
        else if (paramId == Terminal::ID::cursorColorR.toString()
              or paramId == Terminal::ID::cursorColorG.toString()
              or paramId == Terminal::ID::cursorColorB.toString())
        {
            if (not forceUserCursor)
            {
                const auto findParam = [&] (const juce::Identifier& id) -> float
                {
                    float result { -1.0f };

                    for (int i { 0 }; i < cursorState.getNumChildren(); ++i)
                    {
                        auto child { cursorState.getChild (i) };

                        if (child.getType() == Terminal::ID::PARAM
                            and child.getProperty (Terminal::ID::id).toString() == id.toString())
                        {
                            result = static_cast<float> (child.getProperty (Terminal::ID::value));
                        }
                    }

                    return result;
                };

                const float r { findParam (Terminal::ID::cursorColorR) };
                const float g { findParam (Terminal::ID::cursorColorG) };
                const float b { findParam (Terminal::ID::cursorColorB) };

                if (r >= 0.0f and g >= 0.0f and b >= 0.0f)
                {
                    hasColorOverride = true;
                    overrideColour = juce::Colour (static_cast<uint8_t> (r),
                                                   static_cast<uint8_t> (g),
                                                   static_cast<uint8_t> (b));
                }
                else
                {
                    hasColorOverride = false;
                }

                if (currentShape == 0)
                {
                    rebuildTintedImage();
                }

                repaint();
            }
        }
    }
}

inline void CursorComponent::resetBlink()
{
    cursorVisible = true;

    if (isTimerRunning())
    {
        startTimer (getTimerInterval());
    }

    repaint();
}

inline void CursorComponent::rebuildTintedImage() noexcept
{
    if (cursorAlpha.isValid())
    {
        const juce::Colour tint { getActiveColour() };
        cursorImage = cursorAlpha.createCopy();
        juce::Image::BitmapData data (cursorImage, juce::Image::BitmapData::readWrite);

        for (int y { 0 }; y < data.height; ++y)
        {
            for (int x { 0 }; x < data.width; ++x)
            {
                const juce::Colour src { data.getPixelColour (x, y) };
                data.setPixelColour (x, y, tint.withAlpha (src.getAlpha()));
            }
        }
    }
}

inline void CursorComponent::paint (juce::Graphics& g)
{
    if (cursorVisible)
    {
        const auto bounds { getLocalBounds().toFloat() };
        const juce::Colour colour { getActiveColour() };

        if (forceUserCursor or currentShape == 0)
        {
            if (cursorImage.isValid())
            {
                g.drawImage (cursorImage, bounds, juce::RectanglePlacement::stretchToFit);
            }
        }
        else
        {
            g.setColour (colour);

            switch (currentShape)
            {
                case 1:
                case 2:
                    g.fillRect (bounds);
                    break;

                case 3:
                case 4:
                {
                    const float thickness { juce::jmax (1.0f, bounds.getHeight() * cursorThickness) };
                    g.fillRect (bounds.getX(), bounds.getBottom() - thickness,
                                bounds.getWidth(), thickness);
                    break;
                }

                case 5:
                case 6:
                {
                    const float thickness { juce::jmax (1.0f, bounds.getWidth() * cursorThickness) };
                    g.fillRect (bounds.getX(), bounds.getY(),
                                thickness, bounds.getHeight());
                    break;
                }

                default:
                    break;
            }
        }
    }
}

inline void CursorComponent::timerCallback()
{
    cursorVisible = not cursorVisible;
    repaint();
}
