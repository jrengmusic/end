#pragma once

#include <JuceHeader.h>

namespace Terminal
{

struct TextSlot
{
    explicit TextSlot (int maxlen) noexcept : bufferSize { maxlen }
    {
        buffers[0].allocate (maxlen, true);
        buffers[1].allocate (maxlen, true);
    }

    const char* write (const char* src, int length) noexcept
    {
        const int next { 1 - active.load (std::memory_order_relaxed) };
        const int len  { juce::jmin (length, bufferSize - 1) };

        std::memcpy (buffers[next].getData(), src, static_cast<size_t> (len));
        buffers[next][len] = '\0';
        active.store (next, std::memory_order_release);

        return static_cast<const char*> (buffers[next].getData());
    }

private:
    juce::HeapBlock<char> buffers[2];
    std::atomic<int> active { 0 };
    int bufferSize;
};

struct TextBuffer
{
    void addSlot (const juce::Identifier& id, int maxlen)
    {
        slots.emplace (id, std::make_unique<TextSlot> (maxlen));
    }

    const char* write (const juce::Identifier& id, const char* src, int length) noexcept
    {
        jassert (slots.count (id) > 0);
        return slots.at (id)->write (src, length);
    }

private:
    std::unordered_map<juce::Identifier, std::unique_ptr<TextSlot>> slots;
};

} // namespace Terminal
