/**
 * @file TerminalDisplayPreview.cpp
 * @brief Preview/overlay lifecycle methods for Terminal::Display.
 *
 * Extracted from TerminalDisplay.cpp following the Screen file decomposition
 * pattern.  Contains image loading, overlay creation/destruction, and the
 * READER→MESSAGE preview consumption pipeline.
 *
 * @see TerminalDisplay.h
 * @see Terminal::Overlay
 */

#include "TerminalDisplay.h"
#include "../terminal/logic/ImageDecode.h"
#include <vector>

/**
 * @brief Converts a straight RGBA8 pixel buffer to a premultiplied BGRA juce::Image.
 *
 * JUCE `Image::ARGB` stores pixels as premultiplied BGRA in memory: B[0] G[1] R[2] A[3].
 * Source is straight RGBA8: R[0] G[1] B[2] A[3].  Each destination row is obtained via
 * `BitmapData::getLinePointer` — JUCE may pad rows, so stride is never assumed to be
 * `width * 4`.
 *
 * @param rgba    Pointer to straight RGBA8 pixel data, row-major, `width * height * 4` bytes.
 * @param width   Frame width in pixels.
 * @param height  Frame height in pixels.
 * @return A valid `juce::Image::ARGB` image with premultiplied alpha.
 */
static juce::Image imageFromRGBA (const uint8_t* rgba, int width, int height) noexcept
{
    juce::Image result (juce::Image::ARGB, width, height, false);
    juce::Image::BitmapData dst (result, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcLine { rgba + y * width * 4 };
        uint8_t* dstLine { dst.getLinePointer (y) };

        for (int x = 0; x < width; ++x)
        {
            const int si { x * 4 };
            const uint8_t r { srcLine[si] };
            const uint8_t g { srcLine[si + 1] };
            const uint8_t b { srcLine[si + 2] };
            const uint8_t a { srcLine[si + 3] };

            // Premultiply and swizzle RGBA → BGRA
            const int di { x * 4 };
            dstLine[di]     = static_cast<uint8_t> ((b * a) / 255);
            dstLine[di + 1] = static_cast<uint8_t> ((g * a) / 255);
            dstLine[di + 2] = static_cast<uint8_t> ((r * a) / 255);
            dstLine[di + 3] = a;
        }
    }

    return result;
}

/**
 * @brief Loads an image file, decodes all frames, downscales if needed, and activates the overlay preview.
 *
 * Decodes via `loadImageSequenceNative` (all frames, straight RGBA8), converts each frame to a
 * premultiplied BGRA `juce::Image`, and downscales frames whose dimensions exceed
 * `nexus.image.atlasDimension`.  Calls `state.activatePreview()` for State SSOT tracking
 * (imageId = 0, preview-only), then delegates to `activatePreview` with the converted frame
 * vector and per-frame delays.
 *
 * When `previewCols > 0` and `previewLines > 0`, the conform-mode overload of `activatePreview`
 * is called using the protocol-specified bounds.  Otherwise the native-mode overload is called.
 *
 * @param file         The image file to load.
 * @param triggerRow   Visible grid row at which the preview was triggered.
 * @param previewCol   Grid column at the trigger cursor (conform mode).
 * @param previewCols  Protocol-specified overlay width in cells; 0 = use config.
 * @param previewLines Protocol-specified overlay height in cells; 0 = use config.
 * @note MESSAGE THREAD.
 */
void Terminal::Display::handleOpenImage (const juce::File& file, int triggerRow,
                                         int previewCol, int previewCols, int previewLines) noexcept
{
    juce::MemoryBlock fileBytes;

    if (file.loadFileAsData (fileBytes) and not fileBytes.isEmpty())
    {
        Terminal::ImageSequence seq { Terminal::loadImageSequenceNative (fileBytes.getData(), fileBytes.getSize()) };

        if (seq.isValid())
        {
            const int maxDim { config.nexus.image.atlasDimension };
            const int framePixels { seq.width * seq.height * 4 };

            std::vector<juce::Image> frames;
            frames.reserve (static_cast<size_t> (seq.frameCount));

            for (int fi { 0 }; fi < seq.frameCount; ++fi)
            {
                juce::Image frame { imageFromRGBA (seq.pixels.get() + fi * framePixels, seq.width, seq.height) };

                if (frame.getWidth() > maxDim or frame.getHeight() > maxDim)
                {
                    const float scale { juce::jmin (
                        static_cast<float> (maxDim) / static_cast<float> (frame.getWidth()),
                        static_cast<float> (maxDim) / static_cast<float> (frame.getHeight())) };

                    const int scaledWidth { juce::jmax (
                        1, juce::roundToInt (static_cast<float> (frame.getWidth()) * scale)) };
                    const int scaledHeight { juce::jmax (
                        1, juce::roundToInt (static_cast<float> (frame.getHeight()) * scale)) };

                    frame = frame.rescaled (scaledWidth, scaledHeight);
                }

                frames.push_back (std::move (frame));
            }

            std::vector<int> delays;

            if (seq.delays.get() != nullptr)
            {
                delays.assign (seq.delays.get(), seq.delays.get() + seq.frameCount);
            }

            auto& st { processor.getState() };

            if (previewCols > 0 and previewLines > 0)
            {
                const int scrollback { processor.getGrid().getScrollbackUsed() };
                const int scrollOff  { processor.getState().getScrollOffset() };
                const int visibleRow { triggerRow - (scrollback - scrollOff) };

                st.activatePreview (0u, seq.width, seq.height, triggerRow, 0, 0, 0, 0);
                activatePreview (std::move (frames), std::move (delays),
                                 visibleRow, previewCol, previewCols, previewLines);
            }
            else
            {
                const int totalCols { st.getCols() };
                const int panelCols { config.nexus.image.cols };
                const int splitCol  { totalCols - panelCols };

                st.activatePreview (0u, seq.width, seq.height, triggerRow, 0, 0, 0, splitCol);
                activatePreview (std::move (frames), std::move (delays), triggerRow);
            }
        }
    }
}

/**
 * @brief Creates the ephemeral Overlay component and adds it as a child.
 *
 * Computes the overlay pixel bounds from config fractions (nexus.image.width/height)
 * and the trigger row, then sets frames or single image, border colour, padding, and
 * border visibility before calling addAndMakeVisible.
 *
 * When `frames` contains a single element and `delays` is empty, `overlay->setImage` is
 * used (static path).  When `frames` contains more than one element, `overlay->setFrames`
 * is called to start the animation timer.
 *
 * @param frames      Decoded frames as premultiplied BGRA `juce::Image` objects.
 * @param delays      Per-frame delays in milliseconds; empty for static images.
 * @param triggerRow  Visible row of the link span that triggered the open.
 * @note MESSAGE THREAD.
 */
void Terminal::Display::activatePreview (std::vector<juce::Image> frames, std::vector<int> delays, int triggerRow) noexcept
{
    overlayConform    = false;
    overlayTriggerRow = triggerRow;

    juce::Colour borderColour;
    visitScreen ([&] (auto& scr) { borderColour = scr.getTheme().defaultForeground.withAlpha (0.3f); });

    overlay = std::make_unique<Terminal::Overlay>();

    if (frames.size() == 1 and delays.empty())
    {
        overlay->setImage (frames.at (0));
    }
    else
    {
        overlay->setFrames (std::move (frames), std::move (delays));
    }

    overlay->setBorderColour (borderColour);
    overlay->setPadding (config.nexus.image.padding);
    overlay->setShowBorder (config.nexus.image.border);
    overlay->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*overlay);

    // Clear link underlay while overlay is active — links render behind/through the image.
    visitScreen ([&] (auto& s) { s.setLinkUnderlay (nullptr, 0); });

    resized();
}

/**
 * @brief Creates the ephemeral Overlay in SKiT conform mode.
 *
 * Positions the overlay at protocol-specified cell coordinates. No border, no padding.
 * Input-transparent — keyboard and mouse events pass through to the terminal beneath.
 *
 * @param frames      Decoded frames as premultiplied BGRA `juce::Image` objects.
 * @param delays      Per-frame delays in milliseconds; empty for static images.
 * @param triggerRow  Visible row of the protocol cursor.
 * @param triggerCol  Visible column of the protocol cursor.
 * @param cellCols    Overlay width in cells.
 * @param cellRows    Overlay height in cells.
 * @note MESSAGE THREAD.
 */
void Terminal::Display::activatePreview (std::vector<juce::Image> frames, std::vector<int> delays,
                                         int triggerRow, int triggerCol, int cellCols, int cellRows) noexcept
{
    overlayTriggerRow = triggerRow;
    overlayTriggerCol = triggerCol;
    overlayCellCols   = cellCols;
    overlayCellRows   = cellRows;
    overlayConform    = true;

    overlay = std::make_unique<Terminal::Overlay>();

    if (frames.size() == 1 and delays.empty())
    {
        overlay->setImage (frames.at (0));
    }
    else
    {
        overlay->setFrames (std::move (frames), std::move (delays));
    }

    overlay->setShowBorder (false);
    overlay->setPadding (0);
    overlay->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*overlay);

    visitScreen ([&] (auto& s) { s.setLinkUnderlay (nullptr, 0); });

    resized();
}

/**
 * @brief Handles decoded SKiT image pixels and activates overlay in conform mode.
 *
 * Converts RGBA8 pixel frames to juce::Images via `imageFromRGBA`, computes
 * visible row from absolute grid row, and creates the overlay at protocol-specified
 * cell coordinates with no border (SKiT conform mode).
 *
 * @note MESSAGE THREAD — called via callAsync from the onImageDecoded callback.
 */
void Terminal::Display::handleDecodedImage (juce::HeapBlock<uint8_t>& pixels, juce::HeapBlock<int>& delays,
                                            int frameCount, int widthPx, int heightPx,
                                            int gridRow, int gridCol, int cellCols, int cellRows) noexcept
{
    if (widthPx > 0 and heightPx > 0 and frameCount > 0 and pixels.get() != nullptr)
    {
        const int scrollback { processor.getGrid().getScrollbackUsed() };
        const int scrollOff { processor.getState().getScrollOffset() };
        const int visibleRow { gridRow - (scrollback - scrollOff) };

        if (visibleRow >= 0)
        {
            const int framePixels { widthPx * heightPx * 4 };

            std::vector<juce::Image> frames;
            frames.reserve (static_cast<size_t> (frameCount));

            for (int fi { 0 }; fi < frameCount; ++fi)
            {
                frames.push_back (imageFromRGBA (pixels.get() + fi * framePixels, widthPx, heightPx));
            }

            std::vector<int> delayVec;

            if (delays.get() != nullptr)
            {
                delayVec.assign (delays.get(), delays.get() + frameCount);
            }

            activatePreview (std::move (frames), std::move (delayVec), visibleRow, gridCol, cellCols, cellRows);
        }
    }
}

/**
 * @brief Removes the Overlay child component and clears preview State.
 *
 * Removes `overlay` from the component hierarchy and resets the unique_ptr.
 * Also calls `state.dismissPreview()` to clear the split-viewport flag in State.
 * Safe to call when no overlay is active.
 *
 * @note MESSAGE THREAD.
 */
void Terminal::Display::dismissPreview() noexcept
{
    if (overlay != nullptr)
    {
        removeChildComponent (overlay.get());
        overlay.reset();
        resized();

        // Force link rescan for the restored viewport dimensions.
        processor.getState().setSnapshotDirty();
    }

    processor.getState().dismissPreview();
}

/**
 * @brief Returns true when an ephemeral Overlay is currently active.
 * @return `true` if `overlay != nullptr`.
 * @note MESSAGE THREAD.
 */
bool Terminal::Display::isPreviewActive() const noexcept { return overlay != nullptr; }

/**
 * @brief Consumes any pending preview filepath deposited by onPreviewFile on the READER thread.
 *
 * An empty filepath signals preview dismissal.
 *
 * @note MESSAGE THREAD — called from onVBlank().
 */
void Terminal::Display::consumePendingPreview()
{
    juce::String filepath;
    int triggerRow  { 0 };
    int triggerCol  { 0 };
    int previewCols  { 0 };
    int previewLines { 0 };
    bool pending { false };

    {
        const juce::SpinLock::ScopedLockType lock { pendingPreviewLock };
        pending = hasPendingPreview;

        if (pending)
        {
            filepath     = pendingPreviewPath;
            triggerRow   = pendingPreviewRow;
            triggerCol   = pendingPreviewCol;
            previewCols  = pendingPreviewCols;
            previewLines = pendingPreviewLines;
            hasPendingPreview = false;
        }
    }

    if (pending)
    {
        if (filepath.isEmpty())
        {
            dismissPreview();
        }
        else
        {
            handleOpenImage (juce::File { filepath }, triggerRow, triggerCol, previewCols, previewLines);
        }
    }
}
