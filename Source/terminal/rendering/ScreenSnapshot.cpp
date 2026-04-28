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
 * 4. **Publication** — calling `jam::GLSnapshotBuffer::write()` to hand
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
 * 7. Calls `jam::GLSnapshotBuffer::write()` to hand the snapshot to the GL THREAD.
 *
 * @param state      Current terminal state; provides cursor position,
 *                   visibility, and active screen type.
 * @param rows       Number of visible rows (= `state.getVisibleRows()`).
 * @param maxGlyphs  Maximum glyph slots per row (= `cacheCols * 2`).
 *
 * @note **MESSAGE THREAD** only.  Must not be called from the GL THREAD.
 *
 * @see Render::Snapshot::ensureCapacity()
 * @see jam::GLSnapshotBuffer::write()
 * @see Screen::buildSnapshot()
 */
template <typename Renderer>
void Screen<Renderer>::updateSnapshot (const State& state, Grid& grid, int rows, int maxGlyphs) noexcept
{
    auto& snapshot { resources.snapshotBuffer.getWriteBuffer() };

    int totalMono  { 0 };
    int totalEmoji { 0 };
    int totalBg    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        if (isRowIncludedInSnapshot (r))
        {
            totalMono  += monoCount[r];
            totalEmoji += emojiCount[r];
            totalBg    += bgCount[r];
        }
    }

    snapshot.ensureCapacity (totalMono, totalEmoji, totalBg);

    // Count image quads from IMAGES ValueTree instead of per-row cache.
    const auto imagesNode    { state.get().getChildWithName (ID::IMAGES) };
    const int scrollbackUsed { state.getScrollbackUsed() };
    const int visibleBase    { scrollbackUsed - state.getScrollOffset() };
    int totalImages          { 0 };

    for (int i { 0 }; i < imagesNode.getNumChildren(); ++i)
    {
        const auto img          { imagesNode.getChild (i) };
        const int imgGridRow    { static_cast<int> (img.getProperty (ID::gridRow)) };
        const int imgCellRows   { static_cast<int> (img.getProperty (ID::cellRows)) };
        const int viewRow       { imgGridRow - visibleBase };
        const bool isPreviewNode { static_cast<bool> (img.getProperty (ID::isPreview, false)) };

        // Preview nodes are always visible regardless of scroll position.
        // Normal inline images are visible only when any part falls in the viewport.
        if (isPreviewNode or (viewRow < rows and viewRow + imgCellRows > 0))
            ++totalImages;
    }

    // Grow image quad capacity if needed.
    if (totalImages > snapshot.imageCapacity)
    {
        snapshot.images.allocate (static_cast<size_t> (totalImages), true);
        snapshot.imageCapacity = totalImages;
    }
    snapshot.gridWidth  = cacheCols;
    snapshot.gridHeight = rows;
    std::memcpy (snapshot.dirtyRows, frameDirtyBits, sizeof (snapshot.dirtyRows));
    snapshot.scrollDelta    = frameScrollDelta;
    snapshot.physCellHeight = physCellHeight;

    int monoOffset  { 0 };
    int emojiOffset { 0 };
    int bgOffset    { 0 };

    for (int r { 0 }; r < rows; ++r)
    {
        if (isRowIncludedInSnapshot (r))
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
    }

    // Build image quads from IMAGES ValueTree nodes.
    int imageOffset { 0 };

    const bool previewOn   { state.isPreviewActive() };
    const int  splitColumn { state.getSplitCol() };

    snapshot.previewActive  = previewOn;
    snapshot.previewSplitCol = splitColumn;

    for (int i { 0 }; i < imagesNode.getNumChildren(); ++i)
    {
        const auto img        { imagesNode.getChild (i) };
        const uint32_t imgId  { static_cast<uint32_t> (static_cast<int> (img.getProperty (ID::imageId))) };
        const int imgGridRow  { static_cast<int> (img.getProperty (ID::gridRow)) };
        const int imgGridCol  { static_cast<int> (img.getProperty (ID::gridCol)) };
        const int imgCellRows { static_cast<int> (img.getProperty (ID::cellRows)) };
        const int viewRow     { imgGridRow - visibleBase };
        const bool isPreviewNode { static_cast<bool> (img.getProperty (ID::isPreview, false)) };

        // Preview nodes are always visible; normal images only when in viewport.
        const bool shouldRender { isPreviewNode or (viewRow < rows and viewRow + imgCellRows > 0) };

        if (shouldRender)
        {
            const auto* region { imageAtlas.lookup (imgId) };

            if (region != nullptr)
            {
                Render::ImageQuad iq;
                iq.uvRect = region->uv;

                if (isPreviewNode)
                {
                    // Position in the right panel, centred, aspect-ratio preserved.
                    const float panelLeft   { static_cast<float> (splitColumn)            * static_cast<float> (physCellWidth) };
                    const float panelWidth  { static_cast<float> (cacheCols - splitColumn) * static_cast<float> (physCellWidth) };
                    const float panelHeight { static_cast<float> (rows)                   * static_cast<float> (physCellHeight) };

                    static constexpr float previewPadding { 16.0f };
                    const float availW { panelWidth  - previewPadding * 2.0f };
                    const float availH { panelHeight - previewPadding * 2.0f };
                    const float imgW   { static_cast<float> (region->widthPx) };
                    const float imgH   { static_cast<float> (region->heightPx) };

                    const float scale { juce::jmin (availW / imgW, availH / imgH, 1.0f) };
                    const float drawW { imgW * scale };
                    const float drawH { imgH * scale };
                    const float drawX { panelLeft + previewPadding + (availW - drawW) * 0.5f };
                    const float drawY { previewPadding + (availH - drawH) * 0.5f };

                    iq.screenBounds = { drawX, drawY, drawW, drawH };
                }
                else
                {
                    iq.screenBounds =
                    {
                        static_cast<float> (imgGridCol) * static_cast<float> (physCellWidth),
                        static_cast<float> (viewRow)    * static_cast<float> (physCellHeight),
                        static_cast<float> (region->widthPx),
                        static_cast<float> (region->heightPx)
                    };
                }

                snapshot.images[imageOffset] = iq;
                ++imageOffset;
            }
        }
    }

    snapshot.monoCount        = totalMono;
    snapshot.emojiCount       = totalEmoji;
    snapshot.backgroundCount  = totalBg;
    snapshot.imageCount       = imageOffset;

    snapshot.cursorFocused = state.isCursorFocused();

    if (selectionModeActive)
    {
        // Selection mode: suppress the terminal cursor and show the selection
        // cursor at the visible-grid position supplied by setSelectionCursor().
        // scrollOffset is forced to 0 so drawCursor() does not suppress it
        // when the viewport is scrolled — the caller already converts the
        // absolute row to a visible-row coordinate before calling us.
        snapshot.cursorPosition.x = selectionCursorCol;
        snapshot.cursorPosition.y = selectionCursorRow;
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

    // Publish staged bitmaps every frame — rasterization occurs regardless of
    // sync-output state, so the Mailbox must be flushed unconditionally.
    packer.publishStagedBitmaps();
    imageAtlas.publishStagedUploads();

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
