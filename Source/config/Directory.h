#pragma once
#include <JuceHeader.h>

namespace config
{
/*____________________________________________________________________________*/

/**
    @brief Abstract base for per-directory config lifecycle — loadFromPath.

    Directory is the ValueTree-adopting base for config sub-models. Each
    subclass builds its own subtree in its constructor init-list (via
    @c jam::Model::fromLua or @c jam::Model::fromFiles) and adopts it through
    this constructor. The owner (@c config::Model) composes the resulting
    subtrees after member construction.

    The only remaining contract method is @c loadFromPath() — subclasses
    implement disk-overlay behaviour there.

    @c config::Model is the sole @c jam::File::Watcher::Listener. Directory
    subclasses never watch files directly.

    @see jam::Model
    @see config::Model
*/
class Directory : public jam::Model
{
public:
    //==========================================================================
    /** @brief Constructs by adopting a pre-built subtree as @c state.
     *  @param initialState  The tree to adopt (forwarded to jam::Model).
     */
    explicit Directory (juce::ValueTree initialState)
        : jam::Model (std::move (initialState))
    {
    }

    /** @brief Defaulted — lifetime is bound to the owning @c config::Model. */
    ~Directory() override = default;

    //==========================================================================
    /** @brief Read from disk and overlay into @c state.
     *  @param path    Active config value (e.g. theme name, shader project name)
     *                 passed by the owning config::Model.
     *  @param errors  Accumulation channel for parse/IO errors; callers pass
     *                 their own string and append to it across calls.
     */
    virtual void loadFromPath (const juce::var& path, juce::String& errors) = 0;

    /** @brief Write default assets to disk when missing.
     *  @param path  Active config value passed by the owning config::Model.
     *  Default no-op — override in subclasses that seed assets (Theme).
     */
    virtual void saveToPath (const juce::var& path) {}

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Directory)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
