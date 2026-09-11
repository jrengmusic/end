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

    /** @brief Reads GLSL source from the shader project directory into @c state.
     *
     *  Locates the shader directory via @c ConfigDirectory::Shaders::getPath, then reads
     *  that directory's own @c .slangp manifest (any filename, extension-only
     *  discovery via @c jam::VulkanShaderFormat::getExtension() — only
     *  @c slang carries a manifest-extension entry, the SAME wildcard both
     *  this detection and @c jam::VulkanShaderFormat's own directory readers
     *  resolve through) and parses it via @c jam::VulkanShaderPreset::parse()
     *  — the ONE lex both this detection and those readers share, never a
     *  second, independently hand-rolled scan. Format is content-derived from
     *  the parsed result: a non-empty @c preset.passes (a @c shaders=
     *  directive present) is @c jam::VulkanShaderFormat::slang; an absent
     *  @c .slangp, or one with no passes at all (an END-extension resource
     *  manifest carrying only @c textures=/mesh=), is @c jam::
     *  VulkanShaderFormat::shadertoy. The resolved format ordinal is then handed to
     *  @c jam::VulkanShaderFormat::load(), which owns both formats' own
     *  directory-to-ValueTree reading (shadertoy's fixed Common/Image/BufferX
     *  @c .frag files, plus its own @c .slangp resource-manifest text when one
     *  exists; slang's own @c .slangp @c shaders/shaderN directive parsing) —
     *  this method never parses either format itself.
     *
     *  The read tree is overlaid onto @c state via @c setValuesFrom, and
     *  @c Id::shaderFormat is stamped with the resolved format ordinal (a plain
     *  int — @c jam::VulkanShaderFormat::shadertoy or @c ::slang) so callers
     *  always read a definite, resolved format — never an empty or missing
     *  value, even before any project has ever been loaded. @c Id::path
     *  is also stamped at @c state's own root level with @p dir's own full
     *  path, so @c jam::VulkanShaderCompiler::compile() can absolutize every
     *  parsed @c textures=/mesh= path against it (both are as-written,
     *  relative to the project directory).
     *
     *  Fires @c state.sendPropertyChangeMessage(Id::toType (Id::graphics)) so downstream
     *  listeners (jam::VulkanShaderCompiler, via ENDView's funnels) pick up the
     *  new source.
     *
     *  @param path  Active shader project name from ConfigModel.
     *  @return      Always empty — the @c .slangp parse this method drives
     *               cannot itself fail.
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

    /** @brief Reads each theme markdown file from disk and overlays valid properties onto
     *         @c state via @c setValuesFrom. Re-populates FLEX from the flex/ subdirectory.
     *         Fires @c state.sendPropertyChangeMessage(Id::theme). Accumulates errors in @c errors.
     *
     *  Locates the theme directory via @c ConfigDirectory::Themes::getPath and performs a
     *  single @c setValuesFrom pass after assembling a disk-mirror THEMES tree
     *  (THEME via @c jam::ConfigDocument + FLEX via @c fromFiles).
     *
     *  @param path  Active theme name from ConfigModel.
     *  @return      Accumulated markdown validation errors; empty when successful.
     */
    juce::String loadFromPath (const juce::var& path) override;

    /** @brief Writes missing theme markdown and SVG files to the active theme directory.
     *
     *  Creates the theme directory and its @c flex/ subdirectory if absent, then
     *  seeds any missing markdown and SVG assets from BinaryData. No-op when the
     *  directory already contains all expected files.
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
    @c jam::ConfigValidator::isValid, and its @c getValueTree(rootTag) children are
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

        Builds a CONFIG-rooted disk mirror via @c jam::ConfigDocument, overlays
        valid properties via @c setValuesFrom, then drives @c theme.loadFromPath()
        and @c shader.loadFromPath(), accumulating each returned error string.
        Writes the final result to @c ENDModel's message overlay:
        @c Id::successMessage on success, or the accumulated error string on failure.
    */
    void loadFromPath();

private:
    ENDModel& appModel { *ENDModel::getInstance() };

    /**
        @brief Writes missing root markdown files from BinaryData to @c ConfigDirectory::Config::path.
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
