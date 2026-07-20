#pragma once
#include <JuceHeader.h>

/**
    @brief Abstract base for per-directory config lifecycle — loadFromPath.

    ConfigDirectory is the ValueTree-adopting base for config sub-models. Each
    subclass builds its own subtree in its constructor init-list (via
    @c jam::lua::fromLua or @c jam::Model::fromFiles) and adopts it through
    this constructor. The owner (@c ConfigModel) composes the resulting
    subtrees after member construction.

    The only remaining contract method is @c loadFromPath() — subclasses
    implement disk-overlay behaviour there.

    @c ConfigModel is the sole @c jam::File::Watcher::Listener. ConfigDirectory
    subclasses never watch files directly.

    @see jam::Model
    @see ConfigModel
*/
class ConfigDirectory : public jam::Model
{
public:
    //==========================================================================
    /** @brief Constructs by adopting a pre-built subtree as @c state.
     *  @param initialState  The tree to adopt (forwarded to jam::Model).
     */
    explicit ConfigDirectory (juce::ValueTree initialState)
        : jam::Model (std::move (initialState))
    {
    }

    /** @brief Defaulted — lifetime is bound to the owning @c ConfigModel. */
    ~ConfigDirectory() override = default;

    //==========================================================================
    /** @brief Read from disk and overlay into @c state.
     *  @param path    Active config value (e.g. theme name, shader project name)
     *                 passed by the owning ConfigModel.
     *  @param errors  Accumulation channel for parse/IO errors; callers pass
     *                 their own string and append to it across calls.
     */
    virtual void loadFromPath (const juce::var& path, juce::String& errors) = 0;

    /** @brief Write default assets to disk when missing.
     *  @param path  Active config value passed by the owning ConfigModel.
     *  Default no-op — override in subclasses that seed assets (ConfigTheme).
     */
    virtual void saveToPath (const juce::var& path) {}

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigDirectory)
};
