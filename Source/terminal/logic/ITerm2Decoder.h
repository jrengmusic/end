/**
 * @file ITerm2Decoder.h
 * @brief iTerm2 OSC 1337 inline image decoder.
 *
 * `Terminal::ITerm2Decoder` parses an OSC 1337 `File=` payload and produces an
 * `ImageSequence` with all frames in RGBA8 format.  JUCE handles base64 decoding;
 * the platform-native codec (`loadImageSequenceNative`) handles image decoding
 * including animated GIF frame extraction.  A JUCE-based fallback path handles
 * formats not supported natively.
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
 * @see Terminal::ImageSequence
 * @see Terminal::SixelDecoder
 * @see Terminal::Parser::onImageDecoded
 */

#pragma once

#include <JuceHeader.h>

#include "ImageDecode.h"
#include "SixelDecoder.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class ITerm2Decoder
 * @brief Decodes an OSC 1337 File= payload into a multi-frame RGBA8 ImageSequence.
 *
 * Parses the `File=` key=value header, validates `inline=1`, base64-decodes
 * the image data, and delegates to `loadImageSequenceNative` for image decoding.
 * A JUCE-based fallback handles formats the native codec does not support.
 *
 * @note Constructed and used on the READER THREAD only.
 */
class ITerm2Decoder
{
public:
    ITerm2Decoder() = default;

    /**
     * @brief Decode an OSC 1337 File= payload into an ImageSequence.
     *
     * @param data    Raw OSC payload bytes (everything after "1337;").
     *                Expected to begin with "File=".
     * @param length  Number of bytes.
     * @return ImageSequence with RGBA8 pixels for all frames, or invalid if
     *         decode fails or inline=0.
     *
     * @note READER THREAD only.
     */
    ImageSequence decode (const uint8_t* data, int length) noexcept;
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
