/**
 * @file ScreenSnapshot.cpp
 * @brief Snapshot packing and publication for the terminal renderer.
 *
 * This translation unit implements `Screen::updateSnapshot()`, the final step
 * of the per-frame rendering pipeline.  It is responsible for:
 *
 * 1. **Glyph packing** — delegated to `glyph.packSnapshot()`, which handles
 *    capacity management, per-row memcpy, count assignment, and staged bitmap
 *    publication via `packer.publishStagedBitmaps()`.
 *
 * 2. **Cursor state** — writing the cursor position and visibility from
 *    `State` into the snapshot.
 *
 * 3. **Publication** — calling `jam::GLSnapshotBuffer::write()` to hand
 *    the snapshot to the GL THREAD.  Double-buffer rotation is handled
 *    internally by `GLSnapshotBuffer`.
 *
 * @see Screen.h
 * @see ScreenRender.cpp
 * @see jam::GLSnapshotBuffer
 * @see Render::Snapshot
 */

#include "Screen.h"

namespace Terminal
{ /*____________________________________________________________________________*/

// MESSAGE THREAD

/// @brief DECSCUSR Ps value for the block cursor shape (forced in selection mode).
static constexpr int cursorShapeBlock { 1 };

/// @brief Sentinel value indicating no OSC 12 cursor colour override is active.
static constexpr float cursorColorNoOverride { -1.0f };

/**
 * @brief Packs per-row caches into a `Render::Snapshot` and publishes it.
 *
 * Called at the end of `Screen::buildSnapshot()` after all dirty rows have
 * been processed.  Performs the following steps:
 *
 * 1. Obtains the write buffer from `GLSnapshotBuffer::getWriteBuffer()`.
 * 2. Delegates glyph packing (capacity, memcpy, count assignment, bitmap publication)
 *    to `glyph.packSnapshot()`.
 * 3. Sets `gridWidth`, `gridHeight`, dirty rows, scroll delta, and `physCellHeight`.
 * 4. Writes cursor state into the snapshot.
 * 5. Calls `jam::GLSnapshotBuffer::write()` to hand the snapshot to the GL THREAD.
 *
 * @param state  Current terminal state; provides cursor position, visibility,
 *               column count, and active screen type.
 * @param rows   Number of visible rows (= `state.getVisibleRows()`).
 *
 * @note **MESSAGE THREAD** only.  Must not be called from the GL THREAD.
 *
 * @see Render::Snapshot::ensureCapacity()
 * @see jam::GLSnapshotBuffer::write()
 * @see Screen::buildSnapshot()
 */
template <typename Context>
void Screen<Context>::updateSnapshot (const State& state, Grid& grid, int rows) noexcept
{
    auto& snapshot { resources.snapshotBuffer.getWriteBuffer() };

    glyph.packSnapshot (snapshot, rows, frameDirtyBits);

    snapshot.gridWidth  = state.getCols();
    snapshot.gridHeight = rows;
    std::memcpy (snapshot.dirtyRows, frameDirtyBits, sizeof (snapshot.dirtyRows));
    snapshot.scrollDelta    = frameScrollDelta;
    snapshot.physCellHeight = physCellHeight;

    snapshot.cursorFocused = state.isCursorFocused();

    if (selectionModeActive)
    {
        // Selection mode: suppress the terminal cursor and show the selection
        // cursor.  Position is read from State (absolute coords) and converted
        // to visible-grid coords here — no shadow state on Screen.
        const int selAbsRow { state.getSelectionCursorRow() };
        const int visibleBase { state.getScrollbackUsed() - state.getScrollOffset() };
        snapshot.cursorPosition.x = state.getSelectionCursorCol();
        snapshot.cursorPosition.y = selAbsRow - visibleBase;
        snapshot.cursorVisible    = true;
        snapshot.cursorShape      = cursorShapeBlock;
        snapshot.cursorColorR     = cursorColorNoOverride;
        snapshot.cursorColorG     = cursorColorNoOverride;
        snapshot.cursorColorB     = cursorColorNoOverride;
        snapshot.cursorBlinkOn    = true;
        snapshot.scrollOffset     = 0;

        // Resolve selection cursor colour on the message thread.
        snapshot.cursorDrawColorR = resources.terminalColors.selectionCursorColour.getFloatRed();
        snapshot.cursorDrawColorG = resources.terminalColors.selectionCursorColour.getFloatGreen();
        snapshot.cursorDrawColorB = resources.terminalColors.selectionCursorColour.getFloatBlue();
    }
    else
    {
        snapshot.cursorPosition.x = state.getCursorCol();
        snapshot.cursorPosition.y = state.getCursorRow();
        snapshot.cursorVisible    = state.isCursorVisible();
        snapshot.cursorShape      = state.getCursorShape();
        snapshot.cursorColorR     = state.getCursorColorR();
        snapshot.cursorColorG     = state.getCursorColorG();
        snapshot.cursorColorB     = state.getCursorColorB();
        snapshot.cursorBlinkOn    = state.isCursorBlinkOn();
        snapshot.scrollOffset     = state.getScrollOffset();

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
    }

    // Build the user cursor glyph (shape 0 or cursor.force) on the message
    // thread so the GL thread can draw it without touching the atlas.
    // In selection mode we always draw a geometric block — suppress the glyph
    // path entirely so cursor.force does not override the selection cursor.
    snapshot.hasCursorGlyph    = false;
    snapshot.cursorGlyphIsEmoji = false;

    const bool useUserGlyph { not selectionModeActive
                              and (resources.terminalColors.cursorForce
                                   or snapshot.cursorShape == 0) };

    if (useUserGlyph and snapshot.cursorVisible)
    {
        const uint32_t cp { resources.terminalColors.cursorCodepoint };

        // Block elements and box drawing characters are rendered procedurally
        // by the terminal renderer — they have no font glyph.  Skip the glyph
        // path so drawCursor() falls through to the geometric block.
        if (not jam::Glyph::BoxDrawing::isProcedural (cp))
        {
            // Try the emoji font first — if the codepoint shapes there it must be
            // drawn via the RGBA atlas so the native colour is preserved.
            const jam::Typeface::GlyphRun emojiShaped { font.getResolvedTypeface()->shapeEmoji (&cp, 1) };

            // HarfBuzz returns count > 0 even for .notdef (glyph index 0).
            // Only treat as emoji when the font actually has the codepoint.
            const bool isEmoji { emojiShaped.count > 0
                                 and emojiShaped.glyphs[0].glyphIndex != 0 };

            uint32_t glyphIndex { 0 };
            void* cursorFontHandle { nullptr };

            if (isEmoji)
            {
                glyphIndex = emojiShaped.glyphs[0].glyphIndex;
                cursorFontHandle = font.getResolvedTypeface()->getEmojiFontHandle();
            }
            else
            {
                const jam::Typeface::GlyphRun textShaped { font.getResolvedTypeface()->shapeText (
                    jam::Typeface::Style::regular, &cp, 1) };

                if (textShaped.count > 0)
                {
                    glyphIndex = textShaped.glyphs[0].glyphIndex;
                    cursorFontHandle = textShaped.fontHandle != nullptr
                        ? textShaped.fontHandle
                        : font.getResolvedTypeface()->getFontHandle (jam::Typeface::Style::regular);
                }
            }

            if (glyphIndex != 0 and cursorFontHandle != nullptr)
            {
                jam::Glyph::Key cursorKey;
                cursorKey.glyphIndex = glyphIndex;
                cursorKey.fontFace   = cursorFontHandle;
                cursorKey.fontSize   = baseFontSize;
                cursorKey.span       = 0;

                const jam::Glyph::Constraint noConstraint;
                jam::Glyph::Region* ag { packer.getOrRasterize (cursorKey, cursorFontHandle, isEmoji,
                                                                 noConstraint,
                                                                 physCellWidth, physCellHeight,
                                                                 physBaseline) };

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

    // Synchronized output (mode 2026): keep building the snapshot every frame
    // but do NOT publish to the GL thread.  The display stays frozen at the
    // pre-sync frame.  When sync ends, setSnapshotDirty fires → render → publish.
    if (not state.isSyncOutputActive())
        resources.snapshotBuffer.write();
}

template class Screen<jam::Glyph::GLContext>;
template class Screen<jam::Glyph::GraphicsContext>;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
