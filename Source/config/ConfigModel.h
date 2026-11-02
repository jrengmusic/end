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
     *  @param treeType  Tree type identifier (Id::toType (Id::background) or Id::toType (Id::postProcessing)).
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
     *  @c Id::toType (Id::graphics).
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

    The constructor init-list builds a THEMES-rooted tree via
    @c jam::ConfigDocument from @c map::FileThemes BinaryData markdown (THEME
    child) and adopts it through @c ConfigDirectory's ValueTree ctor. The
    body appends a FLEX child (from @c map::FileFlex SVGs) as a sibling of THEME.
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
    /** @brief Constructs with the THEMES-rooted tree (THEME) built in
     *         the init-list via @c jam::ConfigDocument and adopted through
     *         @c ConfigDirectory. A FLEX sibling is appended in the constructor body.
     */
    ConfigTheme();

    ~ConfigTheme() override = default;

    /** @brief Reads the theme markdown files and the flex SVG files of the active theme into @c state.
     *
     *  Locates the theme directory via @c ConfigDirectory::Themes::getPath. Builds a disk tree of THEMES from
     *  its markdown files through @c jam::ConfigDocument, and of FLEX from its flex/ files through @c fromFiles.
     *  Overlays the disk tree on @c state with one @c setValuesFrom call. When the directory is missing, adds a
     *  missing-directory error and overlays nothing. Sends a property change for @c Id::toType (Id::theme) in both cases.
     *
     *  @param path  Active theme name from ConfigModel.
     *  @return      The validation errors and the missing-directory error. Empty when successful.
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
    @c theme, @c background (BACKGROUND), and @c postProcessing (POST_PROCESSING) are
    member objects whose constructors build their own subtrees via
    @c jam::ConfigDocument / @c jam::Model::fromFiles and adopt the result
    directly. @c ConfigModel's init-list builds the CONFIG tree from @c map::FileConfig
    BinaryData via @c jam::ConfigDocument and adopts it through @c jam::Model's
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

    @par Three-phase init (in constructor body)
    1. @c saveToPath()     — writes missing root markdown files to @c ConfigDirectory::Config::path.
    2. @c loadFromPath()   — reads markdown from disk and overlays via @c setValuesFrom.
    3. @c startWatcher()   — installs @c jam::File::Watcher on @c ConfigDirectory::Config::path.

    @par Composition via jam::ConfigDocument and jam::Model::fromFiles
    @c jam::ConfigDocument::parse and @c jam::Model::fromFiles are the SSOT builders.
    Each bimap key resolves to one markdown file; its parsed document is validated via
    the member @c jam::ConfigValidator {}.isValid, and its @c getValueTree(rootTag) children are
    appended into one @p rootTag-typed tree. @c fromFiles sets one property per bimap
    entry on a fresh @p rootTag tree — key = stem, value = @c read(key).

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
        @brief Construct the model — adopts CONFIG tree built in the init-list,
               then composes theme/shader subtrees, runs saveToPath,
               loadFromPath, and startWatcher in that fixed order.
    */
    ConfigModel();

    /** @brief Defaulted — ConfigModel is owned by ENDApplication for the process lifetime. */
    ~ConfigModel() override = default;

    /**
        @brief Reads each root markdown config file from disk and overlays @c state.

        Builds a CONFIG-rooted disk tree from the config files, through @c jam::ConfigDocument.
        Loads the theme, the background shader and the post-processing shader, and collects their errors.
        Overlays the disk tree on @c state with @c setValuesFrom.
        Shows the successMessage row of the display table when no error exists. Otherwise shows the collected errors,
        in @c ENDModel's message overlay.
    */
    void loadFromPath();

    /**
        @brief Returns the value of one config row.

        Finds the row child @c Id::toType (name) in the table child @p type of this tree.
        Returns the dark cell when the row has one and the display appearance is dark.
        Otherwise returns the value cell. The table and the row must exist.

        @param type  The table type of the config file, as @c Id::toType gives it.
        @param name  The key of the row. @c Id::toType gives the row child name.
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
        @brief Writes the root markdown files from BinaryData to @c ConfigDirectory::Config::path, then saves the theme.

        Each file that has a BinaryData default goes through @c jam::ConfigDocument::getOrCreateDefaultFile,
        which replaces a file whose version is older. The theme path comes from the display table.
    */
    void saveToPath();

    /**
        @brief Installs @c watcher on @c ConfigDirectory::Config::path with @c coalesceMs
               event coalescing and registers this ConfigModel as a listener.
    */
    void startWatcher();

    /**
        @brief Reloads root markdown config on @c fileUpdated events.

        Only @c fileUpdated for a @c Extensions::md file triggers
        @c loadFromPath(). All other events and extensions are ignored.

        @param file   The file that changed.
        @param event  The change event type.
    */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /**
        @brief Watches @c ConfigDirectory::Config::path (root markdown directory only) for
               @c Extensions::md changes.
    */
    jam::File::Watcher watcher;

    /** @brief Coalescing window in milliseconds for the filesystem watcher. */
    static constexpr int coalesceMs { 300 };

    ConfigTheme theme;
    ConfigShader background { Id::toType (Id::background) };
    ConfigShader postProcessing { Id::toType (Id::postProcessing) };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigModel)
};
