/**
 * @file EncoderDecoder.h
 * @brief Interprocess binary wire-format encode/decode helpers.
 *
 * Protocol-specific encoding (PDU framing, magic header) built on top of
 * jam::BinaryCodec generic serialisation primitives.
 *
 * @see jam::BinaryCodec
 * @see Interprocess::Message
 */

#pragma once

#include <JuceHeader.h>
#include "Message.h"

#include <cstdint>

namespace Interprocess
{
/*____________________________________________________________________________*/

/**
 * @brief Magic header shared between Link and Channel.
 *
 * Single source of truth — declared here, referenced by both connection classes.
 * Passed to juce::InterprocessConnection as the magic number for frame validation.
 */
constexpr juce::uint32 wireMagicHeader { 0xe4d52a7f };

// =============================================================================
// Generic primitives — convenience alias for jam::BinaryCodec
// =============================================================================

using Codec = jam::BinaryCodec;

// =============================================================================
// PDU encoding
// =============================================================================

/**
 * @brief Encodes @p kind and @p payload into a single MemoryBlock.
 *
 * Wire format: uint16_t kind (LE) | payload bytes.
 * Single source of truth — used by both Channel::sendPdu and Link::sendPdu.
 *
 * @param kind     PDU kind identifier.
 * @param payload  Optional payload bytes.
 * @return MemoryBlock ready for sendMessage().
 */
juce::MemoryBlock encodePdu (Message kind, const juce::MemoryBlock& payload) noexcept;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess
