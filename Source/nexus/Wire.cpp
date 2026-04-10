/**
 * @file Wire.cpp
 * @brief Nexus binary wire-format encode/decode helpers — implementation.
 *
 * @see Wire.h
 */

#include "Wire.h"

#include <cstring>

namespace Nexus
{
/*____________________________________________________________________________*/

// =============================================================================
// Writers
// =============================================================================

void writeUint16 (juce::MemoryBlock& block, uint16_t value) noexcept
{
    block.append (&value, sizeof (value));
}

void writeUint32 (juce::MemoryBlock& block, uint32_t value) noexcept
{
    block.append (&value, sizeof (value));
}

void writeUint64 (juce::MemoryBlock& block, uint64_t value) noexcept
{
    block.append (&value, sizeof (value));
}

void writeInt32 (juce::MemoryBlock& block, int32_t value) noexcept
{
    block.append (&value, sizeof (value));
}

void writeString (juce::MemoryBlock& block, const juce::String& str) noexcept
{
    const auto* utf8 { str.toRawUTF8() };
    const auto len { static_cast<uint32_t> (str.getNumBytesAsUTF8()) };
    block.append (&len, sizeof (len));
    block.append (utf8, static_cast<size_t> (len));
}

// =============================================================================
// Readers
// =============================================================================

uint16_t readUint16 (const uint8_t* data) noexcept
{
    uint16_t val { 0 };
    std::memcpy (&val, data, sizeof (val));
    return val;
}

uint32_t readUint32 (const uint8_t* data) noexcept
{
    uint32_t val { 0 };
    std::memcpy (&val, data, sizeof (val));
    return val;
}

uint64_t readUint64 (const uint8_t* data) noexcept
{
    uint64_t val { 0 };
    std::memcpy (&val, data, sizeof (val));
    return val;
}

int32_t readInt32 (const uint8_t* data) noexcept
{
    int32_t val { 0 };
    std::memcpy (&val, data, sizeof (val));
    return val;
}

int readString (const uint8_t* data, int available, juce::String& out) noexcept
{
    int consumed { 0 };

    if (available >= 4)
    {
        uint32_t len { 0 };
        std::memcpy (&len, data, sizeof (len));

        if (available >= static_cast<int> (4 + len))
        {
            out = juce::String::fromUTF8 (reinterpret_cast<const char*> (data + 4),
                                          static_cast<int> (len));
            consumed = static_cast<int> (4 + len);
        }
    }

    return consumed;
}

// =============================================================================
// PDU encoding
// =============================================================================

juce::MemoryBlock encodePdu (Message kind, const juce::MemoryBlock& payload) noexcept
{
    juce::MemoryBlock message;
    const auto kindValue { static_cast<uint16_t> (kind) };
    message.append (&kindValue, sizeof (kindValue));
    message.append (payload.getData(), payload.getSize());
    return message;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
