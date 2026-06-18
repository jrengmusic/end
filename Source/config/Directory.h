#pragma once
#include <JuceHeader.h>

namespace config
{
/*____________________________________________________________________________*/

/**
    @brief Abstract base for per-subdir config lifecycle — initialise, saveToPath,
           loadFromPath, startWatcher.

    Directory implements the self-contained four-phase contract for a single
    config subdirectory passed as a resolved @c juce::File:

    1. @c initialise()     — build in-memory defaults from BinaryData.
    2. @c saveToPath(dir)  — write missing files from BinaryData to disk inline.
    3. @c loadFromPath(dir)— clear state and read from disk into @c state,
                             then fire @c state.sendPropertyChangeMessage.
    4. @c startWatcher(dir)— install the filesystem watcher on @c dir.

    Subclasses supply the load and save policy (Theme, Shader) by implementing
    the pure-virtual hooks. The owner (@c config::Model) resolves the directory
    via the bimap and passes a @c juce::File directly into @c load(). Subclasses
    fire their own @c state.sendPropertyChangeMessage inside @c loadFromPath —
    there is no shared notify method on the base.

    @see jam::Model
    @see jam::File::Watcher
*/
class Directory
    : public jam::Model
    , public jam::File::Watcher::Listener
{
public:
    //==========================================================================
    /**
        @brief Constructs the directory model.
        @param treeType  Forwarded to @c jam::Model as the ValueTree type.
    */
    explicit Directory (const juce::Identifier& treeType) : jam::Model (treeType) {}

    ~Directory() override = default;

    //==========================================================================
    /**
        @brief Runs the four-phase contract for the resolved @c dir.

        Calls @c initialise(), @c saveToPath(dir), @c loadFromPath(dir), and
        @c startWatcher(dir) in that fixed order. @c loadFromPath is
        responsible for firing @c state.sendPropertyChangeMessage. Empty or
        invalid @c dir handling is delegated to the subclass virtuals.

        @param dir  Resolved subdirectory to activate. May be invalid — subclass
                    virtuals handle that case.
    */
    void load (const juce::File& dir)
    {
        initialise();
        saveToPath (dir);
        loadFromPath (dir);
        startWatcher (dir);
    }

    /** @brief Event coalescing window shared by all config watchers — 300 ms. */
    static constexpr int coalesceMs { 300 };

protected:
    //==========================================================================
    /** @brief Build in-memory defaults from BinaryData (called before disk ops). */
    virtual void initialise() = 0;

    /** @brief Write missing files to @c dir from BinaryData inline. */
    virtual void saveToPath (const juce::File& dir) = 0;

    /** @brief Clear @c state, populate it by reading files from @c dir, then
     *         fire @c state.sendPropertyChangeMessage with the subclass-owned
     *         channel identifier.
     */
    virtual void loadFromPath (const juce::File& dir) = 0;

private:
    //==========================================================================
    /**
        @brief Installs the filesystem watcher on @c dir with event coalescing.

        Resets watched folders first, then starts watching @c dir only when
        @c dir.isDirectory() is true. Called at the end of phase 4 inside
        @c load() — not intended for subclass use.

        @param dir  The resolved subdir to watch.
    */
    void startWatcher (const juce::File& dir)
    {
        watcher.removeAllFolders();

        if (dir.isDirectory())
        {
            watcher.addFolder (dir);
            watcher.coalesceEvents (coalesceMs);
            watcher.addListener (this);
        }
    }

    jam::File::Watcher watcher;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Directory)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
