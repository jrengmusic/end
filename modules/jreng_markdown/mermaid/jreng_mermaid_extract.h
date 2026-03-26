#pragma once
#include <juce_core/juce_core.h>

namespace jreng::Mermaid
{
/*____________________________________________________________________________*/

struct Fence
{
    static bool isStart (const juce::String& line)
    {
        auto trimmed { line.trim() };
        return trimmed.startsWith ("```mermaid") or trimmed.startsWith ("~~~mermaid");
    }

    static bool isEnd (const juce::String& line)
    {
        auto trimmed { line.trim() };
        return trimmed.startsWith ("```") or trimmed.startsWith ("~~~");
    }
};

struct Block
{
    int lineStart { 0 };
    int lineEnd { 0 };
    juce::String code;
};

using Blocks = std::vector<Block>;

Blocks extractBlocks (const juce::String& markdown);

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Mermaid */
