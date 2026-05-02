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

/**
 * @brief Loads an image file, downscales if needed, and activates the overlay preview.
 *
 * Decodes via `loadImageNative` (single frame), downscales to fit within
 * `nexus.image.atlasDimension` if either dimension exceeds the configured limit.
 * Calls `state.activatePreview()` for State SSOT tracking (imageId = 0, preview-only),
 * then creates and positions the ephemeral Overlay child component.
 *
 * @param file        The image file to load.
 * @param triggerRow  Visible grid row at which the preview was triggered.
 * @note MESSAGE THREAD.
 */
void Terminal::Display::handleOpenImage (const juce::File& file, int triggerRow) noexcept
{
    juce::MemoryBlock fileBytes;

    if (file.loadFileAsData (fileBytes) and not fileBytes.isEmpty())
    {
        juce::Image decoded { Terminal::loadImageNative (fileBytes.getData(), fileBytes.getSize()) };

        if (decoded.isValid())
        {
            const int maxDim { config.nexus.image.atlasDimension };

            if (decoded.getWidth() > maxDim or decoded.getHeight() > maxDim)
            {
                const float scale { juce::jmin (
                    static_cast<float> (maxDim) / static_cast<float> (decoded.getWidth()),
                    static_cast<float> (maxDim) / static_cast<float> (decoded.getHeight())) };

                const int scaledWidth { juce::jmax (
                    1, juce::roundToInt (static_cast<float> (decoded.getWidth()) * scale)) };
                const int scaledHeight { juce::jmax (
                    1, juce::roundToInt (static_cast<float> (decoded.getHeight()) * scale)) };

                decoded = decoded.rescaled (scaledWidth, scaledHeight);
            }

            auto& st { processor.getState() };
            const int totalCols { st.getCols() };
            const int panelCols { juce::roundToInt (static_cast<float> (totalCols) * config.nexus.image.width) };
            const int splitCol { totalCols - panelCols };

            st.activatePreview (0u, decoded.getWidth(), decoded.getHeight(), triggerRow, 0, 0, 0, splitCol);
            activatePreview (decoded, triggerRow);
        }
    }
}

/**
 * @brief Creates the ephemeral Overlay component and adds it as a child.
 *
 * Computes the overlay pixel bounds from config fractions (nexus.image.width/height)
 * and the trigger row, then sets image, border colour, padding, and border visibility
 * before calling addAndMakeVisible.
 *
 * @param imageToShow  The decoded juce::Image to display.
 * @param triggerRow   Visible row of the link span that triggered the open.
 * @note MESSAGE THREAD.
 */
void Terminal::Display::activatePreview (juce::Image imageToShow, int triggerRow) noexcept
{
    overlayTriggerRow = triggerRow;

    juce::Colour borderColour;
    visitScreen ([&] (auto& scr) { borderColour = scr.getTheme().defaultForeground.withAlpha (0.3f); });

    overlay = std::make_unique<Terminal::Overlay>();
    overlay->setImage (imageToShow);
    overlay->setBorderColour (borderColour);
    overlay->setPadding (config.nexus.image.padding);
    overlay->setShowBorder (config.nexus.image.border);
    addAndMakeVisible (*overlay);
    resized();
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
    int triggerRow { 0 };
    bool pending { false };

    {
        const juce::SpinLock::ScopedLockType lock { pendingPreviewLock };
        pending = hasPendingPreview;

        if (pending)
        {
            filepath = pendingPreviewPath;
            triggerRow = pendingPreviewRow;
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
            handleOpenImage (juce::File { filepath }, triggerRow);
        }
    }
}
