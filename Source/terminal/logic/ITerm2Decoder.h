/**
 * @file ITerm2Decoder.h
 * @brief iTerm2 OSC 1337 inline image decoder.
 *
 * `Terminal::ITerm2Decoder` parses an OSC 1337 `File=` payload and produces an
 * RGBA8 pixel buffer in the same `DecodedImage` format used by `SixelDecoder`.
 * JUCE handles base64 decoding and image format detection (PNG, JPEG, GIF).
 * The decoder converts JUCE's premultiplied BGRA pixel layout to straight RGBA8
 * so the output is uniform with the Sixel pipeline fed into `ImageAtlas`.
 *
 * @par Protocol summary
 * @code
 *   ESC ] 1337 ; File=[key=value;...] : <base64> BEL
 * @endcode
 * Key=value pairs (separated by `;`, terminated by `:`):
 * - `name=<base64>`          — filename (ignored)
 * - `size=<bytes>`           — declared file size (ignored)
 * - `width=<spec>`           — display width (ignored — Step 11 polish)
 * - `height=<spec>`          — display height (ignored — Step 11 polish)
 * - `preserveAspectRatio=N`  — 0 or 1 (ignored — Step 11 polish)
 * - `inline=N`               — 0 = skip (download only), 1 = display inline
 *
 * @see Terminal::DecodedImage
 * @see Terminal::SixelDecoder
 * @see Terminal::Grid::storeDecodedImage()
 */

#pragma once

#include <JuceHeader.h>

#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class ITerm2Decoder
 * @brief Decodes an OSC 1337 File= payload into an RGBA8 pixel buffer.
 *
 * Parses the `File=` key=value header, validates `inline=1`, base64-decodes
 * the image data, delegates image format detection to JUCE, and converts
 * JUCE's premultiplied BGRA pixel layout to straight RGBA8.
 *
 * @note Constructed and used on the READER THREAD only.
 */
class ITerm2Decoder
{
public:
    ITerm2Decoder() = default;

    /**
     * @brief Decode an OSC 1337 File= payload into RGBA pixels.
     *
     * @param data    Raw OSC payload bytes (everything after "1337;").
     *                Expected to begin with "File=".
     * @param length  Number of bytes.
     * @return DecodedImage with RGBA8 pixels, or invalid if decode fails or inline=0.
     *
     * @note READER THREAD only.
     */
    DecodedImage decode (const uint8_t* data, int length) noexcept;
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
