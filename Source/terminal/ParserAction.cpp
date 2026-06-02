/**
 * @file ParserAction.cpp
 * @brief VT action dispatch, UTF-8 accumulation, and CSI parameter helpers.
 *
 * This translation unit implements `Parser::performAction()`,
 * `Parser::performEntryAction()`, the UTF-8 byte accumulation path, and the
 * buffer append primitive.  The hot-path loop and state transition machinery
 * live in `Parser.cpp`.
 *
 * ## performAction() dispatch table
 *
 * | Action       | Effect                                                                            |
 * |--------------|-----------------------------------------------------------------------------------|
 * | print        | `handlePrintByte()` → UTF-8 accumulation → `video.print()`                       |
 * | execute      | `video.applyControlCode()` — C0 control characters (CR, LF, BS, …)               |
 * | collect      | Append byte to `intermediateBuffer`                                               |
 * | param        | `handleParam()` — digit/separator into CSI accumulator                            |
 * | escDispatch  | `video.applyESC()` — complete ESC sequence                                        |
 * | csiDispatch  | `video.applyCSI()` — complete CSI sequence                                        |
 * | oscPut       | `appendToBuffer (oscBuffer, …)` — hybrid OSC buffer                               |
 * | oscEnd       | `video.applyOSC()` — complete OSC string                                          |
 * | hook         | `video.storeDCSHeader()` — DCS sequence entry, records final byte                 |
 * | put          | `appendToBuffer (dcsBuffer, …)` — DCS passthrough byte                            |
 * | unhook       | `video.applyDCSPayload()` — DCS sequence exit, dispatches accumulated payload     |
 * | apcPut       | `appendToBuffer (apcBuffer, …)` — APC passthrough byte                            |
 * | apcEnd       | `video.applyAPCPayload()` — Kitty graphics APC exit                               |
 * | ignore/none  | No-op                                                                             |
 *
 * ## Thread model
 *
 * **All methods in this file are READER THREAD only.**
 *
 * @see Parser.h        — class declaration, member documentation, and lifecycle notes
 * @see Parser.cpp      — hot-path loop and state transition machinery
 * @see Video.h         — VT command processor called directly for all semantic actions
 */

#include "Parser.h"

namespace terminal
{
/*____________________________________________________________________________*/

// ============================================================================
// Buffer management
// ============================================================================

void Parser::appendToBuffer (juce::HeapBlock<uint8_t>& buffer, int& size, int& capacity, uint8_t byte, int initialCapacity) noexcept
{
    if (size >= capacity)
    {
        const int newCapacity { (capacity == 0) ? initialCapacity : capacity * 2 };
        juce::HeapBlock<uint8_t> grown;
        grown.allocate (static_cast<size_t> (newCapacity), false);

        if (size > 0)
            std::memcpy (grown.get(), buffer.get(), static_cast<size_t> (size));

        buffer = std::move (grown);
        capacity = newCapacity;
    }

    buffer[size] = byte;
    ++size;
}

// ============================================================================
// Action dispatch
// ============================================================================

void Parser::performAction (ParserAction action, uint8_t byte) noexcept
{
    switch (action)
    {
        case ParserAction::none:
        case ParserAction::ignore:
            break;

        case ParserAction::print:
            handlePrintByte (byte);
            break;

        case ParserAction::execute:
            video.applyControlCode (byte);
            break;

        case ParserAction::collect:
            if (intermediateCount < MAX_INTERMEDIATES)
            {
                intermediateBuffer[intermediateCount] = byte;
                ++intermediateCount;
            }
            break;

        case ParserAction::param:
            handleParam (byte);
            break;

        case ParserAction::escDispatch:
            video.applyESC (intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::csiDispatch:
            csi.finalize();
            video.applyCSI (csi, intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::put:
            appendToBuffer (dcsBuffer, dcsBufferSize, dcsBufferCapacity, byte, 65536);
            break;

        case ParserAction::oscPut:
            appendToBuffer (oscBuffer, oscBufferSize, oscBufferCapacity, byte, OSC_BUFFER_CAPACITY);
            break;

        case ParserAction::oscEnd:
            video.applyOSC (oscBuffer.get(), oscBufferSize);
            break;

        case ParserAction::hook:
            csi.finalize();
            video.storeDCSHeader (csi, intermediateBuffer, intermediateCount, byte);
            break;

        case ParserAction::unhook:
            video.applyDCSPayload (dcsBuffer.get(), dcsBufferSize);
            dcsBufferSize = 0;
            break;

        case ParserAction::apcPut:
            appendToBuffer (apcBuffer, apcBufferSize, apcBufferCapacity, byte, 65536);
            break;

        case ParserAction::apcEnd:
            video.applyAPCPayload (apcBuffer.get(), apcBufferSize);
            apcBufferSize = 0;
            break;
    }
}

void Parser::performEntryAction (ParserState newState) noexcept
{
    switch (newState)
    {
        case ParserState::escape:
            intermediateCount = 0;
            utf8AccumulatorLength = 0;
            break;

        case ParserState::csiEntry:
        case ParserState::dcsEntry:
            csi.reset();
            intermediateCount = 0;
            if (newState == ParserState::dcsEntry)
                dcsBufferSize = 0;
            break;

        case ParserState::oscString:
            oscBufferSize = 0;
            break;

        case ParserState::apcString:
            apcBufferSize = 0;
            break;

        default:
            break;
    }
}

// ============================================================================
// UTF-8 accumulation
// ============================================================================

uint8_t Parser::expectedUTF8Length (uint8_t leadByte) noexcept
{
    static constexpr uint8_t lengths[256] = {
        // 0x00–0xBF: 1 (ASCII or continuation byte)
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        // 0xC0–0xDF: 2-byte sequence
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        // 0xE0–0xEF: 3-byte sequence
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
        // 0xF0–0xF7: 4-byte sequence; 0xF8–0xFF: invalid — treat as 1
        4,4,4,4,4,4,4,4, 1,1,1,1,1,1,1,1
    };

    return lengths[leadByte];
}

void Parser::accumulateUTF8Byte (uint8_t byte) noexcept
{
    const bool isLeadByte { byte >= 0xC0 };
    const bool isContinuationByte { byte >= 0x80 and byte < 0xC0 };

    if (isLeadByte)
    {
        utf8Accumulator[0] = static_cast<char> (byte);
        utf8AccumulatorLength = 1;
    }

    if (isContinuationByte and utf8AccumulatorLength > 0 and utf8AccumulatorLength < 4)
    {
        utf8Accumulator[utf8AccumulatorLength] = static_cast<char> (byte);
        ++utf8AccumulatorLength;
    }

    if (utf8AccumulatorLength > 1)
    {
        const auto expected { expectedUTF8Length (static_cast<uint8_t> (utf8Accumulator[0])) };
        if (utf8AccumulatorLength == expected)
        {
            utf8Accumulator[utf8AccumulatorLength] = '\0';
            juce::CharPointer_UTF8 decoder (utf8Accumulator);
            video.print (static_cast<uint32_t> (*decoder));
            utf8AccumulatorLength = 0;
        }
    }
}

void Parser::handlePrintByte (uint8_t byte) noexcept
{
    const bool isASCII { byte <= 0x7F };

    if (isASCII)
    {
        utf8AccumulatorLength = 0;
        video.print (static_cast<uint32_t> (byte));
    }
    else
    {
        accumulateUTF8Byte (byte);
    }
}

// ============================================================================
// CSI parameter accumulation
// ============================================================================

void Parser::handleParam (uint8_t byte) noexcept
{
    const bool isDigit { byte >= '0' and byte <= '9' };
    const bool isSeparator { byte == ';' or byte == ':' };

    if (isDigit)
    {
        csi.addDigit (static_cast<uint8_t> (byte - '0'));
    }

    if (isSeparator)
    {
        csi.addSeparator (byte);
    }
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
