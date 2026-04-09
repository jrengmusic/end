/**
 * @file Loader.cpp
 * @brief Implementation of Nexus::Loader — one-shot backlog replay thread.
 *
 * @see Nexus::Loader
 */

#include "Loader.h"
#include "Log.h"
#include "../terminal/logic/Processor.h"

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the Loader and starts the worker thread immediately.
 *
 * @note MESSAGE THREAD.
 */
Loader::Loader (Terminal::Processor& processorIn,
                juce::MemoryBlock&& bytesIn,
                const juce::String& uuidIn)
    : juce::Thread ("Nexus::Loader " + uuidIn)
    , processor (processorIn)
    , bytes (std::move (bytesIn))
    , uuid (uuidIn)
{
    startThread();
}

/**
 * @brief Joins the worker thread with a 5-second timeout.
 *
 * @note MESSAGE THREAD.
 */
Loader::~Loader()
{
    stopThread (5000);
}

/**
 * @brief Worker loop — feeds backlog bytes into the Processor in chunks.
 *
 * Processes `bytes` in `chunkBytes`-size pieces, acquiring the Processor's
 * resize lock per chunk so the message thread can paint between chunks.
 * After the loop completes, fires `onFinished` on the message thread via
 * `juce::MessageManager::callAsync`.
 *
 * @note LOADER THREAD.
 */
void Loader::run()
{
    const int total { static_cast<int> (bytes.getSize()) };
    int offset { 0 };

    Nexus::logLine ("Loader::run: uuid=" + uuid + " totalBytes=" + juce::String (total));

    while (not threadShouldExit() and offset < total)
    {
        const int remaining { total - offset };
        const int chunk { remaining < chunkBytes ? remaining : chunkBytes };

        {
            const juce::ScopedLock lock (processor.grid.getResizeLock());
            processor.process (static_cast<const char*> (bytes.getData()) + offset, chunk);
        }

        offset += chunk;
        juce::Thread::yield();
    }

    Nexus::logLine ("Loader::run: uuid=" + uuid + " done, offset=" + juce::String (offset));

    juce::MessageManager::callAsync ([this] { if (onFinished != nullptr) onFinished(); });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
