/**
 * @file EncoderDecoder.cpp
 * @brief Interprocess protocol-specific encoding — implementation.
 *
 * @see EncoderDecoder.h
 */

#include "EncoderDecoder.h"

namespace Interprocess
{
/*____________________________________________________________________________*/

juce::MemoryBlock encodePdu (Message kind, const juce::MemoryBlock& payload) noexcept
{
    juce::MemoryBlock message;
    const auto kindValue { static_cast<uint16_t> (kind) };
    message.append (&kindValue, sizeof (kindValue));
    message.append (payload.getData(), payload.getSize());
    return message;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Interprocess
