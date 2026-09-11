#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"

/**
    @brief Abstract base for per-directory config lifecycle — loadFromPath.

    ConfigDirectory is the ValueTree-adopting base for config sub-models. Each
    subclass builds its own subtree in its constructor init-list (via
    @c jam::ConfigDocument or @c jam::Model::fromFiles) and adopts it through
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
    /**
        @brief On-disk file/directory name resolution for END's config tree —
               bridges the @c map::FileConfig / @c map::FileThemes / @c map::FileFlex
               bimap keys (bare stems) to their extensioned filenames, and the
               config root to the subdirectories rooted under it.

        Each nested struct mirrors one on-disk category under @c Config::path
        (@c ~/.config/end): @c Config itself for the root markdown files, @c Themes
        for the @c themes/ directory, @c Flex for FLEX SVG assets, @c Shaders
        for the @c shaders/ directory. @c getName overloads turn a bimap key into
        a filename via @c jam::Format::toFileName; @c getPath overloads resolve a
        child path relative to @c Config::path.
    */
    /** @brief Root config directory (@c ~/.config/end) and its root markdown files. */
    struct Config
    {
        /** @brief Filename for the root markdown file named by @p key.
         *  @param key  @c map::FileConfig bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Extensions::md.
         */
        static juce::String getName (int key) noexcept
        {
            return jam::Format::toFileName (map::FileConfig::getInstance()->get (key), Extensions::md);
        }

        /** @brief Resolves @p child relative to @c Config::path.
         *  @param child  Child file or directory name.
         *  @return       @c path with @p child appended.
         */
        static juce::File getPath (juce::StringRef child) noexcept
        {
            return path.getChildFile (child);
        }

        /** @brief Config directory name, relative to the user home directory. */
        static inline const char* const configDirectoryName { ".config/end" };

        /** @brief The resolved root config directory: user home directory + @c configDirectoryName. */
        static inline const juce::File path {
            juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (configDirectoryName)
        };
    };

    /** @brief The @c themes/ subdirectory and its per-theme markdown files. */
    struct Themes
    {
        /** @brief Filename for the theme markdown file named by @p key.
         *  @param key  @c map::FileThemes bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Extensions::md.
         */
        static juce::String getName (int key) noexcept
        {
            return jam::Format::toFileName (map::FileThemes::getInstance()->get (key), Extensions::md);
        }

        /** @brief Resolves @p themeName's directory under @c themes/.
         *  @param themeName  Theme directory name.
         *  @return           @c themes/@p themeName under @c Config::path.
         */
        static juce::File getPath (const juce::String& themeName) noexcept
        {
            return Config::getPath (Id::themes).getChildFile (themeName);
        }
    };

    /** @brief FLEX SVG asset filenames, seeded per theme under @c themes/\<name\>/flex/. */
    struct Flex
    {
        /** @brief Filename for the FLEX SVG asset named by @p key.
         *  @param key  @c map::FileFlex bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Id::svg.
         */
        static juce::String getName (int key) noexcept
        {
            return jam::Format::toFileName (map::FileFlex::getInstance()->get (key), Id::svg);
        }
    };

    /** @brief The @c shaders/ subdirectory holding per-project shader trees. */
    struct Shaders
    {
        /** @brief Resolves @p shadersName's directory under @c shaders/.
         *  @param shadersName  Shader project directory name.
         *  @return             @c shaders/@p shadersName under @c Config::path.
         */
        static juce::File getPath (const juce::String& shadersName) noexcept
        {
            return Config::getPath (Id::shaders).getChildFile (shadersName);
        }
    };

    //==========================================================================
    /** @brief Read from disk and overlay into @c state.
     *  @param path  Active config value (e.g. theme name, shader project name)
     *               passed by the owning ConfigModel.
     *  @return      Accumulated parse/IO error text; empty when successful.
     */
    virtual juce::String loadFromPath (const juce::var& path) = 0;

    /** @brief Write default assets to disk when missing.
     *  @param path  Active config value passed by the owning ConfigModel.
     *  Default no-op — override in subclasses that seed assets (ConfigTheme).
     */
    virtual void saveToPath (const juce::var& path) {}

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigDirectory)
};
