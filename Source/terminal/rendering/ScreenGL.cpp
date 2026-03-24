/**
 * @file ScreenGL.cpp
 * @brief Screen GL thread methods: context lifecycle and draw calls.
 *
 * @note All functions in this file run on the **GL THREAD**.
 *
 * GL resource ownership has been delegated to `jreng::Glyph::GLTextRenderer`.
 * `Screen` retains only terminal-specific GL concerns: viewport management,
 * background blur transparency, snapshot acquisition, and cursor rendering.
 *
 * @see Screen
 * @see Screen.h
 * @see Screen.cpp (MESSAGE THREAD methods)
 * @see jreng::Glyph::GLTextRenderer
 */
#include "Screen.h"
#include <array>

namespace Terminal
{ /*____________________________________________________________________________*/

template <typename Renderer>
void Screen<Renderer>::glContextCreated()
{
    textRenderer.createContext();
}

template <typename Renderer>
void Screen<Renderer>::renderOpenGL (int originX, int originY, int fullHeight)
{
    if (textRenderer.isReady())
    {
        const int x { originX + glViewportX };
        const int y { originY + glViewportY };

        textRenderer.push (x, y, glViewportWidth, glViewportHeight, fullHeight);
        textRenderer.uploadStagedBitmaps (font);

        Render::Snapshot* snapshot { resources.snapshotBuffer.read() };

        if (snapshot != nullptr)
        {
            if (snapshot->backgroundCount > 0)
            {
                textRenderer.drawBackgrounds (snapshot->backgrounds.get(),
                                              snapshot->backgroundCount);
            }

            if (snapshot->monoCount > 0)
            {
                textRenderer.drawQuads (snapshot->mono.get(),
                                        snapshot->monoCount,
                                        false);
            }

            if (snapshot->emojiCount > 0)
            {
                textRenderer.drawQuads (snapshot->emoji.get(),
                                        snapshot->emojiCount,
                                        true);
            }

            drawCursor (*snapshot);
        }

        textRenderer.pop();
    }
}

template <typename Renderer>
void Screen<Renderer>::glContextClosing()
{
    textRenderer.closeContext();
}

template <typename Renderer>
bool Screen<Renderer>::isGLContextReady() const noexcept
{
    return textRenderer.isReady();
}

/**
 * @brief Draws the cursor as a geometric background quad from snapshot data.
 *
 * The cursor is a coloured rectangle whose shape depends on the DECSCUSR Ps
 * value in the snapshot:
 * - Shapes 0, 1, 2 (block): full cell rectangle.
 * - Shapes 3, 4 (underline): thin strip at the bottom of the cell.
 * - Shapes 5, 6 (bar): thin strip at the left edge of the cell.
 *
 * The cursor is hidden when any of these conditions hold:
 * - DECTCEM cursor mode is off (`cursorVisible == false`).
 * - The blink phase is in the hidden half (`cursorBlinkOn == false`).
 * - The terminal component is unfocused (`cursorFocused == false`).
 * - The viewport is scrolled back (`scrollOffset > 0`).
 *
 * Colour is determined by OSC 12 override if active (all three R/G/B >= 0),
 * otherwise falls back to the theme's `cursorColour`.
 *
 * Drawn after glyphs so the cursor overlays the cell content.
 *
 * @param snapshot  The current frame's snapshot (already acquired by `renderOpenGL`).
 * @note **GL THREAD** only.
 */
template <typename Renderer>
void Screen<Renderer>::drawCursor (const Render::Snapshot& snapshot)
{
    const int col { snapshot.cursorPosition.x };
    const int row { snapshot.cursorPosition.y };

    const bool shouldDraw { snapshot.cursorVisible
                            and snapshot.cursorBlinkOn
                            and snapshot.cursorFocused
                            and snapshot.scrollOffset == 0
                            and col >= 0 and col < snapshot.gridWidth
                            and row >= 0 and row < snapshot.gridHeight };

    if (shouldDraw)
    {
        // User glyph cursor (shape 0 or cursor.force): draw the pre-built
        // glyph from the snapshot.  Emoji codepoints live in the RGBA atlas
        // and must go through the emoji shader path to preserve native colour.
        if (snapshot.hasCursorGlyph)
        {
            textRenderer.drawQuads (&snapshot.cursorGlyph, 1, snapshot.cursorGlyphIsEmoji);
        }
        else
        {
            // Geometric cursor (shapes 1–6): coloured rectangle.
            // Colour was resolved once on the message thread in updateSnapshot()
            // so the GL thread never reads resources.terminalColors here.
            const float r { snapshot.cursorDrawColorR };
            const float g { snapshot.cursorDrawColorG };
            const float b { snapshot.cursorDrawColorB };

            const float cellX { static_cast<float> (col * physCellWidth) };
            const float cellY { static_cast<float> (row * physCellHeight) };
            const float cellW { static_cast<float> (physCellWidth) };
            const float cellH { static_cast<float> (physCellHeight) };

            static constexpr float cursorThickness { 0.15f };

            float x { cellX };
            float y { cellY };
            float w { cellW };
            float h { cellH };

            switch (snapshot.cursorShape)
            {
                case 3:
                case 4:
                {
                    // Underline: thin strip at cell bottom.
                    const float thickness { std::max (1.0f, cellH * cursorThickness) };
                    y = cellY + cellH - thickness;
                    h = thickness;
                    break;
                }

                case 5:
                case 6:
                {
                    // Bar: thin strip at cell left edge.
                    const float thickness { std::max (1.0f, cellW * cursorThickness) };
                    w = thickness;
                    break;
                }

                default:
                    // Block (shapes 1, 2): full cell.
                    break;
            }

            const Render::Background cursorBg
            {
                juce::Rectangle<float> { x, y, w, h },
                r, g, b, 1.0f
            };

            textRenderer.drawBackgrounds (&cursorBg, 1);
        }
    }
}

template <typename Renderer>
void Screen<Renderer>::renderPaint (juce::Graphics& g, int originX, int originY, int fullHeight)
{
    textRenderer.setGraphicsContext (g);

    if (not textRenderer.isReady())
        textRenderer.createContext();

    if (textRenderer.isReady())
    {
        const int x { originX + glViewportX };
        const int y { originY + glViewportY };

        textRenderer.push (x, y, glViewportWidth, glViewportHeight, fullHeight);
        textRenderer.uploadStagedBitmaps (font);

        Render::Snapshot* snapshot { resources.snapshotBuffer.read() };

        if (snapshot != nullptr)
        {
            textRenderer.prepareFrame (snapshot->dirtyRows,
                                        snapshot->scrollDelta,
                                        snapshot->physCellHeight,
                                        snapshot->gridHeight,
                                        snapshot->scrollOffset);

            if (snapshot->backgroundCount > 0)
            {
                textRenderer.drawBackgrounds (snapshot->backgrounds.get(),
                                              snapshot->backgroundCount);
            }

            if (snapshot->monoCount > 0)
            {
                textRenderer.drawQuads (snapshot->mono.get(),
                                        snapshot->monoCount,
                                        false);
            }

            if (snapshot->emojiCount > 0)
            {
                textRenderer.drawQuads (snapshot->emoji.get(),
                                        snapshot->emojiCount,
                                        true);
            }

            drawCursor (*snapshot);
        }

        textRenderer.pop();
    }
}

template class Screen<jreng::Glyph::GLTextRenderer>;
template class Screen<jreng::Glyph::GraphicsTextRenderer>;

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
