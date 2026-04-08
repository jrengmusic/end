/**
 * @file Session.cpp
 * @brief Implementation of Nexus::Session — unified session pool and IPC server lifecycle.
 *
 * @see Nexus::Session
 * @see Terminal::Session
 * @see Terminal::Processor
 * @see Nexus::Server
 * @see Nexus::ServerConnection
 */

#include "Session.h"
#include "Client.h"
#include "Server.h"
#include "ServerConnection.h"
#include "Message.h"
#include "Log.h"
#include "Wire.h"
#include "../terminal/logic/Processor.h"
#include "../terminal/logic/Session.h"
#include "../config/Config.h"

#include <algorithm>
#include <cstring>

namespace Nexus
{
/*____________________________________________________________________________*/

/**
 * @brief Constructs the Session in local mode — no IPC.
 */
Session::Session()
{
    Nexus::logLine ("Session ctor: local mode");
}

/**
 * @brief Constructs the Session in daemon mode — constructs Server and starts listening.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Session::Session (DaemonTag)
{
    Nexus::logLine ("Session ctor: daemon mode, starting server");
    startServer();
}

/**
 * @brief Constructs the Session in client mode — constructs Client and begins connect attempts.
 *
 * `onConnectionMade` and `onConnectionFailed` must be set on this Session before
 * or immediately after construction; they are forwarded to the owned Client.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Session::Session (ClientTag)
{
    Nexus::logLine ("Session ctor: client mode, constructing Client");
    client = std::make_unique<Client>();

    client->onConnectionMade = [this]
    {
        if (onConnectionMade != nullptr)
            onConnectionMade();
    };

    client->onConnectionFailed = [this]
    {
        if (onConnectionFailed != nullptr)
            onConnectionFailed();
    };

    const juce::File lockfilePath { Server::getLockfile() };
    Nexus::logLine ("Session ctor: calling beginConnectAttempts on " + lockfilePath.getFullPathName());
    client->beginConnectAttempts (lockfilePath);
}

/**
 * @brief Destructs the Session — disconnects client or stops server before releasing sessions.
 */
Session::~Session()
{
    if (client != nullptr)
    {
        client->disconnectFromHost();
        client = nullptr;
    }

    stopServer();
}

// =============================================================================

/**
 * @brief Creates a new session with a freshly-generated UUID.  Returns Processor reference.
 *
 * Delegates to the 4-argument overload with a generated UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor&
Session::create (const juce::String& shell, const juce::String& args, const juce::String& cwd)
{
    return create (shell, args, cwd, juce::Uuid().toString());
}

/**
 * @brief Creates a new session with an explicit UUID.  Returns Processor reference.
 *
 * ### Local mode
 * Constructs a `Terminal::Session` (PTY+History) and a `Terminal::Processor`
 * (Parser+Grid).  Wires `terminalSession.onBytes → processor.process`.
 * Wires `tty.onDrainComplete → parser.flushResponses / state.clearPasteEchoGate`.
 * Wires `tty.onExit → processorExited lifecycle`.
 * Wires `parser.writeToHost → terminalSession.sendInput`.
 *
 * ### Daemon mode
 * Constructs a `Terminal::Session` (PTY+History) only.  Wires `onBytes` to
 * broadcast `Message::output` to all subscribers of this UUID.  Returns a
 * freshly constructed (but pipeline-only) Processor so callers always get a
 * valid Processor& — daemon processors are just stubs for UUID tracking.
 *
 * ### Client mode
 * Constructs a `Terminal::Processor` (pipeline side) only.  Sends
 * `Message::spawnProcessor` to the daemon.  Returns the new Processor&.
 *
 * @param uuid  Must be non-empty.
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor&
Session::create (const juce::String& shell, const juce::String& args, const juce::String& cwd, const juce::String& uuid)
{
    jassert (uuid.isNotEmpty());

    // Idempotency: if a session with this uuid already exists, return it.
    // Happens on GUI reconnect — client restores saved uuid and re-sends
    // spawnProcessor before the processorList push has updated its isLive cache.
    // Without this guard, the daemon would fork a second shell, fail to emplace,
    // and leave a dangling Processor reference.
    {
        const auto existing { processors.find (uuid) };

        if (existing != processors.end())
        {
            Nexus::logLine ("Session::create: uuid=" + uuid + " already exists, returning existing");
            return *(existing->second);
        }
    }

    if (client != nullptr)
    {
        // Client mode — pipeline side only.
        Nexus::logLine ("Session::create (client mode): uuid=" + uuid);

        const auto liveUuids { client->getProcessorList() };
        const bool isLive { liveUuids.contains (uuid) };

        if (not isLive)
        {
            client->spawnSession (Terminal::Processor::defaultCols,
                                  Terminal::Processor::defaultRows,
                                  shell,
                                  cwd,
                                  uuid);
        }

        client->attachSession (uuid);

        auto proc { std::make_unique<Terminal::Processor> (
            Terminal::Processor::defaultCols,
            Terminal::Processor::defaultRows,
            uuid) };

        proc->state.setId (uuid);

        // Wire parser responses (DSR, DA, CPR, etc.) back to the daemon via Client::sendInput.
        // Without this, vim/neovim start in degraded mode because terminal capability queries
        // get no reply.
        proc->parser->writeToHost = [this, uuid] (const char* data, int len)
        {
            client->sendInput (uuid, data, len);
        };

        client->registerProcessor (std::move (proc));

        auto* procPtr { client->getProcessor (uuid) };
        jassert (procPtr != nullptr);
        return *procPtr;
    }

    // Local or daemon mode — construct Terminal::Session (PTY side).
    const juce::String effectiveShell { shell.isNotEmpty()
        ? shell
        : Config::getContext()->getString (Config::Key::shellProgram) };
    const juce::String effectiveArgs { shell.isNotEmpty()
        ? args
        : Config::getContext()->getString (Config::Key::shellArgs) };

    auto termSession { std::make_unique<Terminal::Session> (
        Terminal::Processor::defaultCols,
        Terminal::Processor::defaultRows,
        effectiveShell,
        effectiveArgs,
        cwd) };

    if (server != nullptr)
    {
        // Daemon mode — wire onBytes to broadcast Message::output to subscribers.
        termSession->onBytes = [this, uuid] (const char* bytes, int len)
        {
            // READER THREAD — build output PDU and push to all subscribers.
            juce::MemoryBlock payload;
            writeString (payload, uuid);
            payload.append (bytes, static_cast<size_t> (len));

            const juce::ScopedLock lock (connectionsLock);
            const auto it { subscribers.find (uuid) };

            if (it != subscribers.end())
            {
                for (auto* conn : it->second)
                    conn->sendPdu (Message::output, payload);
            }
        };

        termSession->onExit = [this, uuid]
        {
            juce::MemoryBlock payload;
            const auto* utf8 { uuid.toRawUTF8() };
            const auto len { static_cast<uint32_t> (uuid.getNumBytesAsUTF8()) };
            payload.append (&len, sizeof (len));
            payload.append (utf8, static_cast<size_t> (len));

            {
                const juce::ScopedLock lock (connectionsLock);

                for (auto* conn : attached)
                    conn->sendPdu (Message::processorExited, payload);
            }

            juce::MessageManager::callAsync (
                [this, uuid]
                {
                    remove (uuid);
                    broadcastProcessorList();

                    if (list().isEmpty() and onAllSessionsExited != nullptr)
                        onAllSessionsExited();
                });
        };

        terminalSessions.emplace (uuid, std::move (termSession));

        // Daemon also needs a stub Processor so get() and forEachProcessor() work.
        auto proc { std::make_unique<Terminal::Processor> (
            Terminal::Processor::defaultCols,
            Terminal::Processor::defaultRows,
            uuid) };

        proc->state.setId (uuid);

        Terminal::Processor& result { *proc };
        processors.emplace (uuid, std::move (proc));

        return result;
    }

    // Local mode — wire onBytes → processor.process; wire parser.writeToHost → sendInput.
    auto proc { std::make_unique<Terminal::Processor> (
        Terminal::Processor::defaultCols,
        Terminal::Processor::defaultRows,
        uuid) };

    proc->state.setId (uuid);

    // Wire parser responses back to PTY.
    Terminal::Session* termSessionPtr { termSession.get() };
    proc->parser->writeToHost = [termSessionPtr] (const char* data, int len)
    {
        termSessionPtr->sendInput (data, len);
    };

    // Wire drain-complete: flush parser responses and handle sync-resize.
    Terminal::Processor* procRawPtr { proc.get() };
    termSession->onDrainComplete = [procRawPtr, termSessionPtr]
    {
        procRawPtr->parser->flushResponses();
        procRawPtr->state.clearPasteEchoGate();

        if (procRawPtr->state.consumeSyncResize())
            termSessionPtr->platformResize (procRawPtr->grid.getCols(), procRawPtr->grid.getVisibleRows());
    };

    // Wire state flush (foreground process introspection).
    procRawPtr->state.onFlush = [procRawPtr, termSessionPtr]
    {
        const int fgPid { termSessionPtr->getForegroundPid() };

        if (fgPid > 0)
        {
            char fgNameBuf[Terminal::State::maxStringLength] {};
            termSessionPtr->getProcessName (fgPid, fgNameBuf, Terminal::State::maxStringLength);
            procRawPtr->state.setForegroundProcess (fgNameBuf, static_cast<int> (std::strlen (fgNameBuf)));

            char cwdBuf[Terminal::State::maxStringLength] {};
            termSessionPtr->getCwd (fgPid, cwdBuf, Terminal::State::maxStringLength);
            procRawPtr->state.setCwd (cwdBuf, static_cast<int> (std::strlen (cwdBuf)));
        }
    };

    // Wire TTY exit → processor lifecycle (via onShellExited callback).
    termSession->onExit = [this, uuid]
    {
        juce::MessageManager::callAsync (
            [this, uuid]
            {
                const auto it { processors.find (uuid) };

                if (it != processors.end() and it->second->onShellExited != nullptr)
                    it->second->onShellExited();
            });
    };

    // Wire onBytes → processor.process (with resize lock as before).
    termSession->onBytes = [procRawPtr] (const char* bytes, int len)
    {
        const juce::ScopedLock lock (procRawPtr->grid.getResizeLock());
        procRawPtr->process (bytes, len);
    };

    // Register processor as ChangeListener for local fan-out.
    proc->addChangeListener (this);

    terminalSessions.emplace (uuid, std::move (termSession));

    Terminal::Processor& result { *proc };
    processors.emplace (uuid, std::move (proc));

    return result;
}

/**
 * @brief Returns a mutable reference to the Processor with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
Terminal::Processor& Session::get (const juce::String& uuid)
{
    if (client != nullptr)
    {
        auto* proc { client->getProcessor (uuid) };
        jassert (proc != nullptr);
        return *proc;
    }

    const auto it { processors.find (uuid) };
    jassert (it != processors.end());

    return *(it->second);
}

/**
 * @brief Returns a const reference to the Processor with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
const Terminal::Processor& Session::get (const juce::String& uuid) const
{
    if (client != nullptr)
    {
        const auto* proc { client->getProcessor (uuid) };
        jassert (proc != nullptr);
        return *proc;
    }

    const auto it { processors.find (uuid) };
    jassert (it != processors.end());

    return *(it->second);
}

/**
 * @brief Removes and destroys the session with the given UUID.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::remove (const juce::String& uuid)
{
    if (client != nullptr)
    {
        client->unregisterProcessor (uuid);
        client->detachSession (uuid);
    }
    else
    {
        const auto procIt { processors.find (uuid) };

        if (procIt != processors.end())
        {
            procIt->second->removeChangeListener (this);
            processors.erase (procIt);
        }

        terminalSessions.erase (uuid);
    }
}

/**
 * @brief Returns a snapshot of all live session UUIDs.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
juce::StringArray Session::list() const
{
    juce::StringArray uuids;

    if (client != nullptr)
    {
        return client->getProcessorList();
    }

    for (const auto& pair : processors)
        uuids.add (pair.first);

    return uuids;
}

/**
 * @brief Forwards raw input bytes to the target session's PTY.
 *
 * Local/daemon mode: looks up Terminal::Session by uuid and calls sendInput.
 * Client mode: calls Client::sendInput to forward over IPC.
 *
 * @note MESSAGE THREAD.
 */
void Session::sendInput (const juce::String& uuid, const void* data, int size)
{
    if (client != nullptr)
    {
        client->sendInput (uuid, data, size);
    }
    else
    {
        Nexus::logLine ("Session::sendInput uuid=" + uuid + " size=" + juce::String (size) + " (local/daemon mode -> Terminal::Session::sendInput)");
        const auto it { terminalSessions.find (uuid) };
        jassert (it != terminalSessions.end());
        it->second->sendInput (static_cast<const char*> (data), size);
    }
}

/**
 * @brief Forwards a terminal resize to the target session's PTY.
 *
 * Local/daemon mode: looks up Terminal::Session by uuid and calls resize.
 * Client mode: calls Client::sendResize to forward over IPC.
 *
 * @note MESSAGE THREAD.
 */
void Session::sendResize (const juce::String& uuid, int cols, int rows)
{
    if (client != nullptr)
    {
        client->sendResize (uuid, cols, rows);
    }
    else
    {
        const auto it { terminalSessions.find (uuid) };
        jassert (it != terminalSessions.end());
        it->second->resize (cols, rows);
    }
}

/**
 * @brief Sends the history snapshot for @p uuid to @p target via Message::history.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::sendHistorySnapshot (const juce::String& uuid, ServerConnection& target)
{
    const auto it { terminalSessions.find (uuid) };

    if (it != terminalSessions.end())
    {
        const juce::MemoryBlock historyBytes { it->second->snapshotHistory() };

        // Resolve current dims from the daemon's stub Processor grid — it is kept
        // up to date by every resizeSession message the client sends.
        const auto procIt { processors.find (uuid) };
        const int cols { procIt != processors.end() ? procIt->second->grid.getCols() : Terminal::Processor::defaultCols };
        const int rows { procIt != processors.end() ? procIt->second->grid.getVisibleRows() : Terminal::Processor::defaultRows };

        juce::MemoryBlock payload;
        writeString (payload, uuid);
        writeUint16 (payload, static_cast<uint16_t> (cols));
        writeUint16 (payload, static_cast<uint16_t> (rows));
        payload.append (historyBytes.getData(), historyBytes.getSize());

        Nexus::logLine ("Session::sendHistorySnapshot: uuid=" + uuid
                        + " cols=" + juce::String (cols)
                        + " rows=" + juce::String (rows)
                        + " historyBytes=" + juce::String ((int) historyBytes.getSize()));

        target.sendPdu (Message::history, payload);
    }
}

/**
 * @brief Routes incoming bytes from daemon to the Processor for @p uuid.
 *
 * @note MESSAGE THREAD (called from Client::messageReceived).
 */
void Session::feedBytes (const juce::String& uuid, const void* data, int size)
{
    Terminal::Processor* proc { client != nullptr ? client->getProcessor (uuid) : nullptr };

    if (proc == nullptr)
    {
        const auto it { processors.find (uuid) };

        if (it != processors.end())
            proc = it->second.get();
    }

    if (proc != nullptr and size > 0)
        proc->process (static_cast<const char*> (data), size);
}

// =============================================================================

/**
 * @brief Creates the Server and starts listening on the default port.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::startServer()
{
    Nexus::logLine ("startServer: entry");
    server = std::make_unique<Server> (*this);
    Nexus::logLine ("startServer: Server constructed, calling start");
    server->start();
    Nexus::logLine ("startServer: Server::start returned, isServing=" + juce::String (isServing() ? 1 : 0));
}

/**
 * @brief Stops the server and removes the lockfile.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::stopServer()
{
    if (server != nullptr)
    {
        server->stop();
        server = nullptr;

        const juce::ScopedLock lock (connectionsLock);
        attached.clear();
    }
}

/**
 * @brief Returns true if the server is active.
 *
 * @note Any thread.
 */
bool Session::isServing() const noexcept { return server != nullptr and server->getPort() > 0; }

// =============================================================================

/**
 * @brief Adds @p connection to the broadcast list.
 *
 * @note Acquires connectionsLock.  Any thread.
 */
void Session::attach (ServerConnection& connection)
{
    const juce::ScopedLock lock (connectionsLock);
    attached.push_back (&connection);
}

/**
 * @brief Removes @p connection from the broadcast list AND every per-processor
 *        subscriber list in one locked pass.
 *
 * @note Acquires connectionsLock.  Any thread.
 */
void Session::detach (ServerConnection& connection)
{
    const juce::ScopedLock lock (connectionsLock);

    attached.erase (std::remove (attached.begin(), attached.end(), &connection), attached.end());

    for (auto it { subscribers.begin() }; it != subscribers.end();)
    {
        auto& list { it->second };
        list.erase (std::remove (list.begin(), list.end(), &connection), list.end());

        if (list.empty())
            it = subscribers.erase (it);
        else
            ++it;
    }
}

/**
 * @brief ChangeListener callback — fired when an owned Processor broadcasts a change.
 *
 * In the byte-forward model, live bytes are pushed eagerly via Terminal::Session::onBytes.
 * This callback is retained so local-mode Display repaint still works.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::changeListenerCallback (juce::ChangeBroadcaster* /*source*/)
{
    Nexus::logLine ("Session::changeListenerCallback fired, calling triggerAsyncUpdate");
    triggerAsyncUpdate();
}

/**
 * @brief AsyncUpdater callback — no-op in the byte-forward architecture.
 *
 * Live output is pushed eagerly via Terminal::Session::onBytes → Message::output.
 * This handler is retained for compatibility with ChangeListener coalescing.
 *
 * @note NEXUS PROCESS MESSAGE THREAD.
 */
void Session::handleAsyncUpdate()
{
    Nexus::logLine ("Session::handleAsyncUpdate: entry (byte-forward mode, no fan-out needed)");
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Nexus
