#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"
#include "end/ENDModel.h"
#include "ConfigDirectory.h"

/**
    @brief Shader source model — @c ConfigDirectory subclass that holds a
           single shader tree (BACKGROUND or POST_PROCESSING) as its own @c state.

    The constructor adopts an empty @p treeType-rooted tree through
    @c ConfigDirectory's ValueTree ctor -- pass names are unknown until
    @c loadFromPath first enumerates the active shader project directory (an
    empty project is a valid initial/no-project state, not an error).

    After construction, @c ConfigModel attaches each instance's @c state
    directly under the GRAPHICS child — both are first-class GRAPHICS children,
    not parent/child of each other.

    The two-level parameter key @c (BACKGROUND, Image) vs @c (POST_PROCESSING, Image)
    ensures no collision in @c jam::Model::createAndAddParameter.

    Load policy: @c saveToPath is a no-op (shader source is never seeded from
    BinaryData). @c loadFromPath reads GLSL from disk into @c state.

    @see ConfigDirectory
    @see ConfigModel
*/
class ConfigShader : public ConfigDirectory
{
public:
    /** @brief Constructs with an empty shader tree of the given type.
     *  @param treeType  Tree type identifier (jam::Format::toType (Id::background) or jam::Format::toType (Id::postProcessing)).
     */
    explicit ConfigShader (juce::Identifier treeType);

    ~ConfigShader() override = default;

    /** @brief Reads the GLSL source of the active shader project into @c state.
     *
     *  Finds the shader directory through @c ConfigDirectory::Shaders::getPath.
     *  Reads the first file with the slang manifest extension in that directory,
     *  and parses it with @c jam::VulkanShaderPreset::parse(). The format is
     *  @c jam::VulkanShaderFormat::slang when the parsed preset has passes. An
     *  absent manifest, or one without passes, gives
     *  @c jam::VulkanShaderFormat::shadertoy. @c jam::VulkanShaderFormat::load()
     *  reads the directory in the resolved format. This method does not parse
     *  the sources itself.
     *
     *  Overlays the loaded tree on @c state with @c setValuesFrom. Stamps
     *  @c Id::shaderFormat with the resolved format, and @c Id::path with the
     *  full path of the directory, on @c state. Sends a property change for
     *  @c jam::Format::toType (Id::graphics).
     *
     *  @param path  Active shader project name from ConfigModel.
     *  @return      Always empty. This method reports no error.
     */
    juce::String loadFromPath (const juce::var& path) override;

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigShader)
};

//==============================================================================
/**
    @brief Theme state model — @c ConfigDirectory subclass mirroring the
           on-disk @c themes/ directory as a THEMES subtree.

    The constructor init-list passes the THEMES-rooted tree of getDefaultThemes()
    to @c ConfigDirectory's ValueTree ctor. getDefaultThemes() builds a fresh tree on each call.
    Its THEME child is built from the memoized @c jam::ConfigDocument of each @c map::FileThemes
    BinaryData markdown file, and a FLEX child (from @c map::FileFlex SVGs) is appended as a
    sibling of THEME.
    @c ConfigModel attaches the whole @c theme.state THEMES
    subtree under its CONFIG tree with a single @c appendChild — no unwrapping.
    @c theme.state remains the live THEMES tree, so @c loadFromPath() and
    @c saveToPath() operate on it directly.

    @see ConfigDirectory
    @see ConfigModel
*/
class ConfigTheme : public ConfigDirectory
{
public:
    /** @brief Constructs by adopting the THEMES-rooted tree that getDefaultThemes() builds
     *         from the memoized default documents, through @c ConfigDirectory.
     */
    ConfigTheme();

    ~ConfigTheme() override = default;

    /** @brief Reads the theme markdown files and the flex SVG files of the active theme into @c state.
     *
     *  Starts from the binary defaults of getDefaultThemes(). Locates the theme directory via
     *  @c ConfigDirectory::Themes::getPath. When the directory exists, builds a disk tree of THEMES from its
     *  markdown files through addFileTables, and of FLEX from its flex/ files through @c jam::Model::fromFiles.
     *  A flex SVG that fails @c jam::XmlValidator keeps its binary default, and its path and the validation
     *  message are added to the errors. The disk tree overlays the defaults, and the result overlays @c state
     *  with @c setValuesFrom. When the directory is missing, adds a missing-directory error, and the defaults
     *  still overlay @c state. Sends a property change for @c jam::Format::toType (Id::theme) in both cases.
     *
     *  @param path  Active theme name from ConfigModel.
     *  @return      The markdown and flex validation errors and the missing-directory error. Empty when successful.
     */
    juce::String loadFromPath (const juce::var& path) override;

    /** @brief Writes the default theme files to the active theme directory.
     *
     *  Creates the theme directory and its @c flex/ subdirectory when absent.
     *  Writes each theme markdown file that has a BinaryData default through
     *  @c jam::ConfigDocument::getOrCreateDefaultFile, which replaces a file whose version is older.
     *  Writes each flex SVG file only when the file is absent. Does nothing when the theme path is empty.
     *
     *  @param path  Active theme name from ConfigModel.
     */
    void saveToPath (const juce::var& path) override;

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigTheme)
};

//==============================================================================
/**
    @brief END's root configuration model — owns the live CONFIG ValueTree and
           drives the ConfigTheme and ConfigShader sub-models.

    @par Build-in-ctor composition
    getDefaultThemes() and getDefaultConfig() build a fresh tree on each call, through addDefaultTables.
    addDefaultTables reads each default document from the memoized @c jam::ConfigDocument::getOrCreate, so each
    BinaryData file is parsed one time per process. Errors of the default documents are sent to ENDModel on each call.
    @c theme is a member object whose constructor adopts the THEMES subtree of getDefaultThemes().
    @c background (BACKGROUND) and @c postProcessing (POST_PROCESSING) are member objects whose
    constructors adopt an empty tree of their own type; their content is read by @c loadFromPath().
    @c ConfigModel's init-list adopts the CONFIG tree of getDefaultConfig() through @c jam::Model's
    ValueTree ctor. The constructor body then attaches the @c theme (THEMES),
    @c background (BACKGROUND), and @c postProcessing (POST_PROCESSING) subtrees into
    the CONFIG tree. Both shader instances are first-class GRAPHICS children —
    no key collision via the two-level @c (treeType, propertyId) key scheme.

    @par Construction order
    @c jam::Model base runs first (adopting the CONFIG tree). @c theme,
    @c background, and @c postProcessing members are constructed before the body
    executes. The body attaches @c theme.state under CONFIG and both
    @c background.state and @c postProcessing.state under GRAPHICS, each with a single
    @c appendChild — all are single-rooted subtrees, so no unwrapping is needed.

    @par Init (in constructor body)
    1. @c loadFromPath()   — calls @c saveToPath() first, which writes the root markdown defaults to
       @c ConfigDirectory::Config::path. Then reads markdown from disk and overlays via @c setValuesFrom.
    2. @c startWatcher()   — installs @c jam::File::Watcher on @c ConfigDirectory::Config::path.

    @par Composition via jam::ConfigDocument and jam::Model::fromFiles
    @c jam::ConfigDocument::getOrCreate builds the default documents, memoized one per BinaryData name.
    @c jam::ConfigDocument::parse builds the documents of the disk files. @c jam::Model::fromFiles builds
    the FLEX child from SVG files. These are the SSOT builders.
    Each bimap key of a markdown registry resolves to one markdown file. Its document, memoized for the defaults
    and parsed for the disk files, goes through
    @c jam::ConfigDocument::addTables with the rules of @c jam::ConfigValidator {}.getRules(). When the document
    passes the rules, addTables appends each table tree that has no table failures to the target tree.
    @c jam::Model::fromFiles sets one raw-string property per map entry on a fresh @p rootTag tree,
    key = stem, value = @c read(key). It does no parsing.

    @see ConfigDirectory
    @see ConfigTheme
    @see ConfigShader
    @see jam::Model
    @see jam::ConfigValidator
    @see ENDApplication
*/
class ConfigModel
    : public jam::Model
    , public jam::Instance<ConfigModel>
    , public jam::File::Watcher::Listener
{
public:
    //==========================================================================
    /**
        @brief Construct the model — adopts the CONFIG tree that getDefaultConfig() builds in the init-list,
               attaches the theme and shader subtrees, then calls loadFromPath
               and startWatcher in that order.
    */
    ConfigModel();

    /** @brief Defaulted — ConfigModel is owned by ENDApplication for the process lifetime. */
    ~ConfigModel() override = default;

    /**
        @brief Reads each root markdown config file from disk and overlays @c state.

        Calls saveToPath() first. Builds a CONFIG-rooted disk tree from the config files through addFileTables, and
        overlays it on the binary defaults of getDefaultConfig(). Calls loadResources() with the merged config. It loads
        the theme, the background shader and the post-processing shader, and collects their errors. Then overlays the
        merged config on @c state with @c setValuesFrom.
        Sets the message of ENDModel to the successMessage row of the display table when no error exists. Otherwise sets
        it to the collected errors.
    */
    void loadFromPath();

    /**
        @brief Returns the value of one config row.

        Finds the row child @c jam::Format::toType (name) in the table child @p type of this tree.
        Returns the dark cell when the row has one and the display appearance is dark.
        Otherwise returns the value cell. The table and the row must exist.

        @param type  The table type of the config file, as @c jam::Format::toType gives it.
        @param name  The key of the row. @c jam::Format::toType gives the row child name.
        @return The value of the row.
    */
    juce::var getRowValue (const juce::Identifier& type, const juce::Identifier& name) const noexcept;

    /**
        @brief Returns the key of the handler for a property change.

        The property is the key when @p events registers it. Otherwise the key is the type of @p tree.
        When that key has no handler, the key is the type of the parent of @p tree.

        @param events    Handlers keyed by property name or tree type.
        @param tree      Tree whose property changed.
        @param property  Property that changed.
        @return The key to look up in @p events.
    */
    static juce::Identifier getEventKey (const jam::Function::Map<juce::Identifier, void>& events,
                                         const juce::ValueTree& tree,
                                         const juce::Identifier& property);

private:
    ENDModel& appModel { *ENDModel::getInstance() };
    jam::SharedInstance<map::Appearance> appearance { std::in_place };

    /**
        @brief Creates the config directory, then writes the root markdown defaults from BinaryData to @c ConfigDirectory::Config::path.

        Each file that has a BinaryData default goes through @c jam::ConfigDocument::getOrCreateDefaultFile, which writes
        the default text when @c jam::Format::isVersionOld reports the file's version as older than the default's version.
        An empty file version counts as older. The theme is not saved here: loadResources() saves it through
        ConfigTheme::saveToPath().
    */
    void saveToPath();

    /**
        @brief Loads the theme, the background shader and the post-processing shader that the config names.

        Reads the theme name from the display table, and the background and post-processing names from the graphics table
        of @p config. Calls ConfigTheme::saveToPath() with the theme name, then ConfigTheme::loadFromPath() with the same name.
        Then calls ConfigShader::loadFromPath() for the background and for the post-processing shader.

        @param config  Config tree that holds the display and graphics tables.
        @return        The errors of the three loads, joined in that order. Empty when no load fails.
    */
    juce::String loadResources (const juce::ValueTree& config);

    /**
        @brief Installs @c watcher on @c ConfigDirectory::Config::path with @c coalesceMs
               event coalescing and registers this ConfigModel as a listener.
    */
    void startWatcher();

    /**
        @brief Reloads the config when a file with the @c Extensions::md or @c Extensions::svg extension changes.

        Any event type calls @c loadFromPath(). Files with other extensions are ignored.

        @param file   The file that changed.
        @param event  The change event type. Not read: every event type reloads.
    */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /**
        @brief Watches @c ConfigDirectory::Config::path. fileChanged() reloads the config for
               @c Extensions::md and @c Extensions::svg files.
    */
    jam::File::Watcher watcher;

    /** @brief Coalescing window in milliseconds for the filesystem watcher. */
    static constexpr int coalesceMs { 300 };

    ConfigTheme theme;
    ConfigShader background { jam::Format::toType (Id::background) };
    ConfigShader postProcessing { jam::Format::toType (Id::postProcessing) };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigModel)
};
