#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"

/**
    @brief Abstract base for per-directory config lifecycle — loadFromPath.

    ConfigDirectory is the ValueTree-adopting base for config sub-models. Each
    subclass adopts its tree through this constructor. ConfigTheme adopts the
    THEMES-rooted tree that getDefaultThemes() builds. ConfigShader adopts an empty
    tree of its own type and fills it in loadFromPath(). The owner (@c ConfigModel)
    composes the resulting subtrees after member construction.

    The contract methods are @c loadFromPath(), which is pure virtual, and @c saveToPath(),
    which defaults to a no-op. Subclasses implement disk-overlay behaviour in @c loadFromPath().

    @c ConfigModel derives from @c jam::File::Watcher::Listener and owns the watcher.
    ConfigDirectory declares no watcher.

    @par Names
    The nested structs name the on-disk categories of END's config tree. @c Config covers the root
    directory, named by @c Config::configDirectoryName under the user home directory. @c Themes and
    @c Shaders resolve child directories under it. @c Flex gives the FLEX SVG filenames of the
    @c flex/ directory of each theme. Each bimap key of a registry becomes a filename through
    @c jam::Format::toFileName.

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
    explicit ConfigDirectory (juce::ValueTree initialState);

    /** @brief Defaulted — lifetime is bound to the owning @c ConfigModel. */
    ~ConfigDirectory() override = default;

    //==========================================================================
    /** @brief Root config directory and its root markdown files. */
    struct Config
    {
        /** @brief Filename for the root markdown file named by @p key.
         *  @param key  @c map::FileConfig bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Extensions::md.
         */
        static juce::String getName (int key) noexcept;

        /** @brief Resolves @p child relative to @c Config::path.
         *  @param child  Child file or directory name.
         *  @return       @c path with @p child appended.
         */
        static juce::File getPath (juce::StringRef child) noexcept;

        /** @brief Config directory name, relative to the user home directory. */
        static inline const char* const configDirectoryName { JUCE_STRINGIFY (CAST_CONFIG_DIRECTORY) };

        /** @brief The resolved root config directory: user home directory + @c configDirectoryName. */
        static const juce::File path;
    };

    /** @brief The @c themes/ subdirectory and its per-theme markdown files. */
    struct Themes
    {
        /** @brief Filename for the theme markdown file named by @p key.
         *  @param key  @c map::FileThemes bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Extensions::md.
         */
        static juce::String getName (int key) noexcept;

        /** @brief Resolves @p themeName's directory under @c themes/.
         *  @param themeName  Theme directory name.
         *  @return           @c themes/@p themeName under @c Config::path.
         */
        static juce::File getPath (const juce::String& themeName) noexcept;
    };

    /** @brief FLEX SVG asset filenames, seeded per theme under @c themes/\<name\>/flex/. */
    struct Flex
    {
        /** @brief Filename for the FLEX SVG asset named by @p key.
         *  @param key  @c map::FileFlex bimap key.
         *  @return     @p key's stem, extensioned via @c jam::Format::toFileName
         *              with @c Extensions::svg.
         */
        static juce::String getName (int key) noexcept;
    };

    /** @brief The @c shaders/ subdirectory holding per-project shader trees. */
    struct Shaders
    {
        /** @brief Resolves @p shadersName's directory under @c shaders/.
         *  @param shadersName  Shader project directory name.
         *  @return             @c shaders/@p shadersName under @c Config::path.
         */
        static juce::File getPath (const juce::String& shadersName) noexcept;
    };

    /** @brief Appends the children of the tree of each valid file of @p files to @p target.
     *
     *  Each file is read through @p getFile and parsed by jam::ConfigDocument::getOrCreateFileDocument. When the version of
     *  the parsed file is older than the version of its default document, that function writes @p getDefaultText of the file
     *  name to the file, and parses the written text. The document must pass the row rules and the table rule of
     *  jam::ConfigValidator, built against the default document of the same file name, which @p getDefaultDocument returns.
     *  A document that passes appends the children of the tree that @p getTree builds to @p target. A document that fails
     *  the rules is not appended, and its validation message is added to the result. When the file yields no document, the
     *  message of jam::ConfigDocument::isValid for its text is added to the result.
     *
     *  @param target              Tree that receives the children.
     *  @param files               Registry of bimap keys to file stems.
     *  @param getName             Returns the extensioned filename of a key.
     *  @param getFile             Returns the file of an extensioned filename.
     *  @param getDefaultDocument  Returns the default config document of a filename.
     *  @param getDefaultText      Returns the default text of a filename, written to its file when the file's version is older.
     *  @param getTree             Builds the tree of a parsed config document. Its children are appended to @p target.
     *  @return                    The validation messages of the rejected files, each followed by a newline.
     *                             Empty when every file passes.
     */
    static juce::String addFileTables (juce::ValueTree target,
                                       const jam::Bimap<int>& files,
                                       const std::function<juce::String (int)>& getName,
                                       const std::function<juce::File (const juce::String&)>& getFile,
                                       const std::function<const jam::ConfigDocument& (const juce::String&)>& getDefaultDocument,
                                       const std::function<juce::String (const juce::String&)>& getDefaultText,
                                       const std::function<juce::ValueTree (const jam::ConfigDocument&)>& getTree);

    /** @brief Appends the children of the tree of the default document of each file of @p files to @p target.
     *
     *  @p getDefaultTree returns the tree of the default document of a filename. No rule runs on the default documents.
     *
     *  @param target          Tree that receives the children.
     *  @param files           Registry of bimap keys to file stems.
     *  @param getName         Returns the extensioned filename of a key.
     *  @param getDefaultTree  Returns the tree of the default document of a filename. Its children are appended to @p target.
     */
    static void addDefaultTables (juce::ValueTree target,
                                  const jam::Bimap<int>& files,
                                  const std::function<juce::String (int)>& getName,
                                  const std::function<juce::ValueTree (const juce::String&)>& getDefaultTree);

    //==========================================================================
    /** @brief Read from disk and overlay into @c state.
     *  @param path  Active config value (e.g. theme name, shader project name)
     *               passed by the owning ConfigModel.
     *  @return      Accumulated parse/IO error text; empty when successful.
     */
    virtual juce::String loadFromPath (const juce::var& path) = 0;

    /** @brief Writes the default flex assets of the directory to disk, in a subclass that overrides it.
     *
     *  The base implementation writes nothing. ConfigTheme overrides it.
     *  The override creates the theme directory and its flex/ subdirectory when they are absent, and writes each flex SVG
     *  default only when its file is absent. It writes no markdown file.
     *
     *  @param path  Active config value passed by the owning ConfigModel.
     */
    virtual void saveToPath (const juce::var& path);

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigDirectory)
};
