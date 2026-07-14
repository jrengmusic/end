#pragma once
#include <JuceHeader.h>
#include "generated/Lexicon.h"

namespace Id
{
/*____________________________________________________________________________*/

/**
    @brief On-disk file/directory name resolution for END's config tree —
           bridges the @c FileConfig / @c FileThemes / @c FileFlex bimap keys
           (bare stems) to their extensioned filenames, and the config root
           to the subdirectories rooted under it.

    Each nested struct mirrors one on-disk category under @c Config::path
    (@c ~/.config/end): @c Config itself for the root lua files, @c Themes
    for the @c themes/ directory, @c Flex for FLEX SVG assets, @c Shaders
    for the @c shaders/ directory. @c getName overloads turn a bimap key into
    a filename via @c jam::Format::toFileName; @c getPath overloads resolve a
    child path relative to @c Config::path.
*/
struct Files
{
    /** @brief Root config directory (@c ~/.config/end) and its root lua files. */
    struct Config
    {
        /** @brief Filename for the root lua file named by @p key.
         *  @param key  @c Id::FileConfig bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Id::lua.
         */
        static const juce::String getName (int key) noexcept
        {
            return jam::Format::toFileName (Id::FileConfig::get (key), Id::lua);
        }

        /** @brief Resolves @p child relative to @c Config::path.
         *  @param child  Child file or directory name.
         *  @return       @c path with @p child appended.
         */
        static const juce::File getPath (juce::StringRef child) noexcept
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

    /** @brief The @c themes/ subdirectory and its per-theme lua files. */
    struct Themes
    {
        /** @brief Filename for the theme lua file named by @p key.
         *  @param key  @c Id::FileThemes bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Id::lua.
         */
        static const juce::String getName (int key) noexcept
        {
            return jam::Format::toFileName (Id::FileThemes::get (key), Id::lua);
        }

        /** @brief Resolves @p themeName's directory under @c themes/.
         *  @param themeName  Theme directory name.
         *  @return           @c themes/@p themeName under @c Config::path.
         */
        static const juce::File getPath (const juce::String& themeName) noexcept
        {
            return Config::getPath (Id::themes).getChildFile (themeName);
        }
    };

    /** @brief FLEX SVG asset filenames, seeded per theme under @c themes/\<name\>/flex/. */
    struct Flex
    {
        /** @brief Filename for the FLEX SVG asset named by @p key.
         *  @param key  @c Id::FileFlex bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Id::svg.
         */
        static const juce::String getName (int key) noexcept
        {
            return jam::Format::toFileName (Id::FileFlex::get (key), Id::svg);
        }
    };

    /** @brief The @c shaders/ subdirectory holding per-project shader trees. */
    struct Shaders
    {
        /** @brief Resolves @p shadersName's directory under @c shaders/.
         *  @param shadersName  Shader project directory name.
         *  @return             @c shaders/@p shadersName under @c Config::path.
         */
        static const juce::File getPath (const juce::String& shadersName) noexcept
        {
            return Config::getPath (Id::shaders).getChildFile (shadersName);
        }
    };
};

/**_____________________________END_OF_NAMESPACE______________________________*/
}// namespace Id
