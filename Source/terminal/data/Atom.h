#pragma once

#include <JuceHeader.h>
#include "Identifier.h"

namespace Terminal
{

struct AtomBase
{
    virtual ~AtomBase() = default;
    virtual void flush() noexcept = 0;
};

template<typename T>
struct Atom;

template<>
struct Atom<int> final : AtomBase
{
    Atom (int defaultValue, juce::ValueTree node) noexcept
        : value { defaultValue }
        , tree { node }
    {
    }

    int  load()  const noexcept { return value.load (std::memory_order_relaxed); }
    void store (int v) noexcept { value.store (v, std::memory_order_relaxed); }

    std::atomic<int>& raw() noexcept { return value; }

    void flush() noexcept override
    {
        tree.setProperty (Terminal::ID::value, value.load (std::memory_order_relaxed), nullptr);
    }

private:
    std::atomic<int> value;
    juce::ValueTree tree;
};

template<>
struct Atom<const char*> final : AtomBase
{
    Atom (juce::ValueTree node, const juce::Identifier& property) noexcept
        : tree { node }
        , key { property }
    {
    }

    const char* load() const noexcept { return ptr.load (std::memory_order_acquire); }
    void store (const char* p) noexcept { ptr.store (p, std::memory_order_release); }

    void flush() noexcept override
    {
        const char* p { ptr.load (std::memory_order_acquire) };

        if (p != nullptr)
            tree.setProperty (key, juce::String::fromUTF8 (p), nullptr);
    }

private:
    std::atomic<const char*> ptr { nullptr };
    juce::ValueTree tree;
    juce::Identifier key;
};

} // namespace Terminal
