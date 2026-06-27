/**
 * @file Nexus.h
 * @brief Session host — owns all terminal::Session instances.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Session.h"

namespace end
{
/*____________________________________________________________________________*/

/** @brief Session host — DAW analog. Owns terminal::Session instances.
 *
 *  Singleton via jam::Instance. Creates, retrieves, and destroys terminal
 *  sessions keyed by jam::UUID. Phase 3: standalone mode only.
 *  Phase 15: daemon mode, IPC, session persistence.
 */
struct Nexus : jam::Instance<Nexus>
{
    Nexus() = default;

    /** @brief Creates a new terminal session.
     *  @param uuid  Unique identifier for the session.
     *  @return Reference to the created Session.
     */
    terminal::Session& create (jam::UUID uuid, const jam::Font& font)
    {
        auto [entry, inserted] = sessions.try_emplace (uuid.value, std::make_unique<terminal::Session> (font));
        jassert (inserted);
        auto& [key, session] = *entry;
        return *session;
    }

    /** @brief Returns the Session for the given identifier.
     *  @param uuid  Session identifier.
     *  @return Reference to the Session.
     */
    terminal::Session& get (jam::UUID uuid)
    {
        return *sessions.at (uuid.value);
    }

    /** @brief Destroys the terminal session.
     *  @param uuid  Session identifier.
     */
    void remove (jam::UUID uuid)
    {
        sessions.erase (uuid.value);
    }

private:
    jam::HashMap<int64_t, std::unique_ptr<terminal::Session>> sessions;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nexus)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
