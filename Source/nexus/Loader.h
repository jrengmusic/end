/**
 * @file Loader.h
 * @brief Nexus::Loader — one-shot worker thread for backlog replay.
 */

#pragma once

#include <juce_core/juce_core.h>

namespace Terminal { class Processor; }

namespace Nexus
{

/**
 * @class Nexus::Loader
 * @brief One-shot worker thread that loads a Terminal::Processor's state
 *        from a backlog byte blob received via Message::loading.
 *
 * Ownership: constructed with exclusive ownership of its byte buffer. Runs
 * until the buffer is drained, then fires a completion callback on the
 * message thread via juce::MessageManager::callAsync. Caller destroys the
 * Loader after the callback fires.
 *
 * @par Thread context
 * - Ctor / dtor — MESSAGE THREAD
 * - run()       — LOADER THREAD (worker)
 * - onFinished  — fires on MESSAGE THREAD via callAsync
 */
class Loader : public juce::Thread
{
public:
    /**
     * @param processor  The processor whose state to load.
     * @param bytes      Backlog byte buffer (moved into Loader ownership).
     * @param uuid       UUID for logging + completion callback routing.
     */
    Loader (Terminal::Processor& processor,
            juce::MemoryBlock&& bytes,
            const juce::String& uuid);

    ~Loader() override;

    /** @brief Fires on MESSAGE THREAD via callAsync when run() completes. */
    std::function<void()> onFinished;

    void run() override;

private:
    Terminal::Processor& processor;
    juce::MemoryBlock    bytes;
    const juce::String   uuid;

    static constexpr int chunkBytes { 4096 };
    static constexpr int loaderJoinTimeoutMs { 5000 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Loader)
};

}
