/**
 * @file ScreenSnapshot.cpp
 * @brief Snapshot packing and publication for the terminal renderer.
 *
 * This translation unit implements `Screen::updateSnapshot()`, the final step
 * of the per-frame rendering pipeline.  It is responsible for:
 *
 * 1. **Capacity management** — calling `Render::Snapshot::ensureCapacity()` to
 *    grow the snapshot's `HeapBlock` arrays if the current frame has more
 *    glyphs or backgrounds than the previous one.
 *
 * 2. **Data packing** — copying per-row glyph and background data from the
 *    `cachedMono`, `cachedEmoji`, and `cachedBg` arrays into the contiguous
 *    `Render::Snapshot` arrays via `memcpy`.
 *
 * 3. **Cursor state** — writing the cursor position and visibility from
 *    `State` into the snapshot.
 *
 * 4. **Publication** — calling `jreng::GLSnapshotBuffer::write()` to hand
 *    the snapshot to the GL THREAD.  Double-buffer rotation is handled
 *    internally by `GLSnapshotBuffer`.
 *
 * @see Screen.h
 * @see ScreenRender.cpp
 * @see jreng::GLSnapshotBuffer
 * @see Render::Snapshot
 */

#include "Screen.h"
#include "BoxDrawing.h"
#include "FontCollection.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

/**
 * @brief Packs per-row caches into a `Render::Snapshot` and publishes it.
 *
 * Called at the end of `Screen::buildSnapshot()` after all dirty rows have
 * been processed.  Performs the following steps:
 *
 * 1. Obtains the write buffer from `GLSnapshotBuffer::getWriteBuffer()`.
 * 2. Totals `monoCount[r]`, `emojiCount[r]`, and `bgCount[r]` across all rows.
 * 3. Calls `Render::Snapshot::ensureCapacity()` to grow the snapshot arrays
 *    if needed.
 * 4. Sets `gridWidth` and `gridHeight` on the snapshot.
 * 5. Copies per-row glyph and background data into the contiguous snapshot
 *    arrays via `memcpy`, advancing offsets as each row is packed.
 * 6. Writes the total counts (`monoCount`, `emojiCount`, `backgroundCount`)
 *    and cursor state (`cursorPosition`, `cursorVisible`, `cursorShape`,
 *    `cursorColorR/G/B`, `scrollOffset`, `cursorBlinkOn`, `cursorFocused`)
 *    into the snapshot.
 * 7. Calls `jreng::GLSnapshotBuffer::write()` to hand the snapshot to the GL THREAD.
 *
 * @param state      Current terminal state; provides cursor position,
 *                   visibility, and active screen type.
 * @param rows       Number of visible rows (= `state.getVisibleRows()`).
 * @param maxGlyphs  Maximum glyph slots per row (= `cacheCols * 2`).
 *
 * @note **MESSAGE THREAD** only.  Must not be called from the GL THREAD.
 *
 * @see Render::Snapshot::ensureCapacity()
 * @see jreng::GLSnapshotBuffer::write()
 * @see Screen::buildSnapshot()
 */
void Screen::updateSnapshot (const State& state, int rows, int maxGlyphs) noexcept
{
    const ActiveScreen scr { state.getScreen() };

    auto& snapshot { resources.snapshotBuffer.getWriteBuffer() };

    int totalMono  { 0 };
    int totalEmoji { 0 };
    int totalBg    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        totalMono  += monoCount[r];
        totalEmoji += emojiCount[r];
        totalBg    += bgCount[r];
    }

    snapshot.ensureCapacity (totalMono, totalEmoji, totalBg);
    snapshot.gridWidth  = cacheCols;
    snapshot.gridHeight = rows;

    int monoOffset  { 0 };
    int emojiOffset { 0 };
    int bgOffset    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        if (monoCount[r] > 0)
        {
            std::memcpy (snapshot.mono.get() + monoOffset,
                         cachedMono.get() + r * maxGlyphs,
                         static_cast<size_t> (monoCount[r]) * sizeof (Render::Glyph));
            monoOffset += monoCount[r];
        }

        if (emojiCount[r] > 0)
        {
            std::memcpy (snapshot.emoji.get() + emojiOffset,
                         cachedEmoji.get() + r * maxGlyphs,
                         static_cast<size_t> (emojiCount[r]) * sizeof (Render::Glyph));
            emojiOffset += emojiCount[r];
        }

        if (bgCount[r] > 0)
        {
            std::memcpy (snapshot.backgrounds.get() + bgOffset,
                         cachedBg.get() + r * bgCacheCols,
                         static_cast<size_t> (bgCount[r]) * sizeof (Render::Background));
            bgOffset += bgCount[r];
        }
    }

    snapshot.monoCount        = totalMono;
    snapshot.emojiCount       = totalEmoji;
    snapshot.backgroundCount  = totalBg;

    snapshot.cursorPosition.x = state.getCursorCol (scr);
    snapshot.cursorPosition.y = state.getCursorRow (scr);
    snapshot.cursorVisible    = state.isCursorVisible (scr);
    snapshot.cursorShape      = state.getCursorShape (scr);
    snapshot.cursorColorR     = state.getCursorColorR (scr);
    snapshot.cursorColorG     = state.getCursorColorG (scr);
    snapshot.cursorColorB     = state.getCursorColorB (scr);
    snapshot.scrollOffset     = state.getScrollOffset();
    snapshot.cursorBlinkOn    = state.isCursorBlinkOn();
    snapshot.cursorFocused    = state.isCursorFocused();

    // Resolve final cursor draw colour on the message thread so the GL
    // thread never reads resources.terminalColors in drawCursor().
    const bool hasColorOverride { snapshot.cursorColorR >= 0.0f
                                  and snapshot.cursorColorG >= 0.0f
                                  and snapshot.cursorColorB >= 0.0f };

    snapshot.cursorDrawColorR = hasColorOverride ? snapshot.cursorColorR / 255.0f
                                                 : resources.terminalColors.cursorColour.getFloatRed();
    snapshot.cursorDrawColorG = hasColorOverride ? snapshot.cursorColorG / 255.0f
                                                 : resources.terminalColors.cursorColour.getFloatGreen();
    snapshot.cursorDrawColorB = hasColorOverride ? snapshot.cursorColorB / 255.0f
                                                 : resources.terminalColors.cursorColour.getFloatBlue();

    // Build the user cursor glyph (shape 0 or cursor.force) on the message
    // thread so the GL thread can draw it without touching the atlas.
    snapshot.hasCursorGlyph    = false;
    snapshot.cursorGlyphIsEmoji = false;

    const bool useUserGlyph { resources.terminalColors.cursorForce
                              or snapshot.cursorShape == 0 };

    if (useUserGlyph and snapshot.cursorVisible)
    {
        const uint32_t cp { resources.terminalColors.cursorCodepoint };

        // Block elements and box drawing characters are rendered procedurally
        // by the terminal renderer — they have no font glyph.  Skip the glyph
        // path so drawCursor() falls through to the geometric block.
        if (not BoxDrawing::isProcedural (cp))
        {
            // Try the emoji font first — if the codepoint shapes there it must be
            // drawn via the RGBA atlas so the native colour is preserved.
            const Fonts::ShapeResult emojiShaped { Fonts::getContext()->shapeEmoji (&cp, 1) };

            // HarfBuzz returns count > 0 even for .notdef (glyph index 0).
            // Only treat as emoji when the font actually has the codepoint.
            const bool isEmoji { emojiShaped.count > 0
                                 and emojiShaped.glyphs[0].glyphIndex != 0 };

            void* fontHandle { nullptr };
            uint32_t glyphIndex { 0 };

            if (isEmoji)
            {
                fontHandle = Fonts::getContext()->getEmojiFontHandle();
                glyphIndex = emojiShaped.glyphs[0].glyphIndex;
            }
            else
            {
                // FontCollection first — resolves NF icons and fallback-font codepoints
                // exactly as buildCellInstance does for normal cells.
                auto* fc { FontCollection::getContext() };

                if (fc != nullptr)
                {
                    const int8_t fcSlot { fc->resolve (cp) };

                    if (fcSlot > 0)
                    {
                        const FontCollection::Entry* entry { fc->getEntry (static_cast<int> (fcSlot)) };

                        if (entry != nullptr and entry->hbFont != nullptr)
                        {
                            uint32_t glyphId { 0 };

                            if (hb_font_get_nominal_glyph (entry->hbFont, cp, &glyphId) and glyphId != 0)
                            {
#if JUCE_MAC
                                fontHandle = entry->ctFont;
#else
                                fontHandle = static_cast<void*> (entry->ftFace);
#endif
                                glyphIndex = glyphId;
                            }
                        }
                    }
                }

                // ShapeText fallback — regular chars ("a", digits, punctuation, etc.)
                if (fontHandle == nullptr)
                {
                    const Fonts::ShapeResult textShaped { Fonts::getContext()->shapeText (
                        Fonts::Style::regular, &cp, 1) };

                    if (textShaped.count > 0)
                    {
                        fontHandle = textShaped.fontHandle != nullptr
                                     ? textShaped.fontHandle
                                     : Fonts::getContext()->getFontHandle (Fonts::Style::regular);
                        glyphIndex = textShaped.glyphs[0].glyphIndex;
                    }
                }
            }

            if (fontHandle != nullptr)
            {
                GlyphKey key;
                key.glyphIndex = glyphIndex;
                key.fontFace   = fontHandle;
                key.fontSize   = Fonts::getContext()->getPixelsPerEm (Fonts::Style::regular);
                key.cellSpan   = 1;

                AtlasGlyph* ag { resources.glyphAtlas.getOrRasterize (
                    key, fontHandle, isEmoji, GlyphConstraint {},
                    physCellWidth, physCellHeight, physBaseline) };

                if (ag != nullptr)
                {
                    const int col { snapshot.cursorPosition.x };
                    const int row { snapshot.cursorPosition.y };
                    const float ascender { static_cast<float> (physBaseline) };

                    // For emoji the shader samples colour from the texture, but we
                    // populate the fields consistently so the struct is never uninitialised.
                    // cursorDrawColor* was resolved above on the message thread — read
                    // directly so no theme access is needed here or on the GL thread.
                    const float cr { isEmoji ? 1.0f : snapshot.cursorDrawColorR };
                    const float cg { isEmoji ? 1.0f : snapshot.cursorDrawColorG };
                    const float cb { isEmoji ? 1.0f : snapshot.cursorDrawColorB };

                    snapshot.cursorGlyph.screenPosition = juce::Point<float> {
                        static_cast<float> (col * physCellWidth) + static_cast<float> (ag->bearingX),
                        static_cast<float> (row * physCellHeight) + ascender - static_cast<float> (ag->bearingY) };
                    snapshot.cursorGlyph.glyphSize = juce::Point<float> {
                        static_cast<float> (ag->widthPixels),
                        static_cast<float> (ag->heightPixels) };
                    snapshot.cursorGlyph.textureCoordinates = ag->textureCoordinates;
                    snapshot.cursorGlyph.foregroundColorR = cr;
                    snapshot.cursorGlyph.foregroundColorG = cg;
                    snapshot.cursorGlyph.foregroundColorB = cb;
                    snapshot.cursorGlyph.foregroundColorA = 1.0f;

                    snapshot.hasCursorGlyph    = true;
                    snapshot.cursorGlyphIsEmoji = isEmoji;
                }
            }
        }
    }

    resources.snapshotBuffer.write();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
