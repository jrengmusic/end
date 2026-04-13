/**
 * @file EncoderDecoder.h
 * @brief Interprocess binary wire-format encode/decode helpers.
 *
 * Single source of truth for all little-endian fixed-width integer and
 * length-prefixed string serialisation used by the Interprocess IPC protocol.
 *
 * Previously these helpers were duplicated as file-local statics in:
 *   - Link.cpp
 *   - Channel.cpp
 *   - SessionFanout.cpp
 *
 * All functions operate on `juce::MemoryBlock` (writers) or raw `uint8_t*`
 * pointers (readers).  No allocation beyond MemoryBlock::append.
 *
 * @see Interprocess::Message
 */

#pragma once

#include <juce_core/juce_core.h>
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
// Writers — append a value to a MemoryBlock.
// =============================================================================

/** @brief Appends a little-endian uint16_t to @p block. */
void writeUint16 (juce::MemoryBlock& block, uint16_t value) noexcept;

/** @brief Appends a little-endian uint32_t to @p block. */
void writeUint32 (juce::MemoryBlock& block, uint32_t value) noexcept;

/** @brief Appends a little-endian uint64_t to @p block. */
void writeUint64 (juce::MemoryBlock& block, uint64_t value) noexcept;

/** @brief Appends a little-endian int32_t to @p block. */
void writeInt32 (juce::MemoryBlock& block, int32_t value) noexcept;

/** @brief Appends a length-prefixed (uint32_t LE) UTF-8 string to @p block. */
void writeString (juce::MemoryBlock& block, const juce::String& str) noexcept;

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

// =============================================================================
// Readers — decode a value from a raw byte pointer.
// =============================================================================

/** @brief Reads a little-endian uint16_t from @p data (caller guarantees >= 2 bytes). */
uint16_t readUint16 (const uint8_t* data) noexcept;

/** @brief Reads a little-endian uint32_t from @p data (caller guarantees >= 4 bytes). */
uint32_t readUint32 (const uint8_t* data) noexcept;

/** @brief Reads a little-endian uint64_t from @p data (caller guarantees >= 8 bytes). */
uint64_t readUint64 (const uint8_t* data) noexcept;

/** @brief Reads a little-endian int32_t from @p data (caller guarantees >= 4 bytes). */
int32_t readInt32 (const uint8_t* data) noexcept;

/**
 * @brief Reads a length-prefixed UTF-8 string from @p data.
 *
 * @param data       Pointer to the uint32_t length field.
 * @param available  Bytes available from @p data.
 * @param out        Populated on success.
 * @return Bytes consumed (4 + len), or 0 if insufficient data.
 */
int readString (const uint8_t* data, int available, juce::String& out) noexcept;

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess
