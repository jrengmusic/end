#pragma once
#include <JuceHeader.h>
#include "../Bimap.h"
#include "../end/Model.h"
#include "Directory.h"

namespace config
{
/*____________________________________________________________________________*/

/**
    @brief Shader source model — @c config::Directory subclass that holds the
           SHADER tree as its own @c state.

    The constructor init-list builds the SHADER tree via
    @c jam::Model::fromFiles and adopts it through @c Directory's ValueTree
    ctor. After construction, @c config::Model attaches @c shader.state
    directly under the GRAPHICS child — @c shader.state then IS the live
    SHADER tree inside the CONFIG tree. @c loadFromPath() reads pass files
    from disk and sets properties on the live SHADER tree.

    Load policy: @c saveToPath is a no-op (shader source is never seeded from
    BinaryData). @c loadFromPath reads GLSL from the active shader project dir.

    @see config::Directory
    @see config::Model
*/
class Shader : public Directory
{
public:
    /** @brief Constructs with the SHADER tree built in the init-list via
     *         @c jam::Model::fromFiles and adopted through @c Directory.
     */
    Shader();

    ~Shader() override = default;

    /** @brief Reads GLSL source from the active shader project directory into @c state.
     *
     *  Resolves the active project from @c config::Model's GRAPHICS/ID::background
     *  property, locates the shader directory via @c file::Shaders::getPath, and
     *  overlays @c state via @c jam::Model::fromFiles + @c setValuesFrom.
     *  Fires @c state.sendPropertyChangeMessage(IDtype::graphics) so downstream
     *  listeners (shader::Controller) pick up the new source.
     *
     *  @param errors  Accumulation channel (unused by shader — no lua parse).
     *                 Kept to satisfy the @c Directory contract.
     */
    void loadFromPath (juce::String& errors) override;

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Shader)
};

//==============================================================================
/**
    @brief Theme state model — @c config::Directory subclass mirroring the
           on-disk @c themes/ directory as a THEMES subtree.

    The constructor init-list builds a THEMES-rooted tree via
    @c jam::Model::fromLua from @c file::Themes BinaryData lua (THEME and
    WHELMED children) and adopts it through @c Directory's ValueTree ctor. The
    body appends a FLEX child (from @c file::Flex SVGs) as a sibling of THEME
    and WHELMED. @c config::Model attaches the whole @c theme.state THEMES
    subtree under its CONFIG tree with a single @c appendChild — no unwrapping.
    @c theme.state remains the live THEMES tree, so @c loadFromPath() and
    @c saveToPath() operate on it directly.

    @see config::Directory
    @see config::Model
*/
class Theme : public Directory
{
public:
    /** @brief Constructs with the THEMES-rooted tree (THEME, WHELMED) built in
     *         the init-list via @c jam::Model::fromLua and adopted through
     *         @c Directory. A FLEX sibling is appended in the constructor body.
     */
    Theme();

    ~Theme() override = default;

    /** @brief Reads each theme lua from disk and overlays valid properties onto @c state
     *         via @c setValuesFrom. Re-populates FLEX from the flex/ subdirectory. Fires
     *         @c state.sendPropertyChangeMessage(ID::theme). Accumulates errors in @c errors.
     *
     *  Resolves the active theme name from @c config::Model's DISPLAY/ID::theme
     *  property, locates the theme directory via @c file::Themes::getPath, and
     *  performs a single @c setValuesFrom pass after assembling a disk-mirror
     *  THEMES tree (THEME, WHELMED via @c fromLua + FLEX via @c fromFiles).
     *
     *  @param errors  Accumulation channel; lua parse errors are appended here
     *                 and also passed up to the @c config::Model caller.
     */
    void loadFromPath (juce::String& errors) override;

    /** @brief Writes missing theme lua and SVG files to the active theme directory.
     *
     *  Resolves the active theme name from @c config::Model, creates the theme
     *  directory and its @c flex/ subdirectory if absent, then seeds any missing
     *  lua and SVG assets from BinaryData. No-op when the directory already
     *  contains all expected files.
     */
    void saveToPath();

private:

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Theme)
};

//==============================================================================
/**
    @brief END's root configuration model — owns the live CONFIG ValueTree and
           drives the Theme and Shader sub-models.

    @par Build-in-ctor composition
    @c theme and @c shader are member objects whose constructors build their
    own subtrees via @c jam::Model::fromLua / @c jam::Model::fromFiles and
    adopt the result directly. @c Model's init-list builds the CONFIG tree
    from @c file::Config BinaryData via @c jam::Model::fromLua and adopts it
    through @c jam::Model's ValueTree ctor. The constructor body then attaches
    the @c theme (THEMES) and @c shader (SHADER) subtrees into the CONFIG tree.

    @par Construction order
    @c jam::Model base runs first (adopting the CONFIG tree). @c theme and
    @c shader members are constructed before the body executes. The body
    attaches @c theme.state under CONFIG and @c shader.state under GRAPHICS,
    each with a single @c appendChild — both are single-rooted subtrees, so no
    unwrapping is needed.

    @par Three-phase init (in constructor body)
    1. @c saveToPath()     — writes missing root lua files to @c file::Config::path.
    2. @c loadFromPath()   — reads lua from disk and overlays via @c setValuesFrom.
    3. @c startWatcher()   — installs @c jam::File::Watcher on @c file::Config::path.

    @par Composition via jam::Model aggregators
    @c jam::Model::fromLua and @c jam::Model::fromFiles are the SSOT builders.
    @c fromLua iterates any @c jam::HashMap\<int, juce::String\> bimap, calls
    @c read(key) for lua content, parses via the single-source @c fromLua overload,
    and returns a @p rootTag-typed tree. @c fromFiles sets one property per bimap
    entry on a fresh @p rootTag tree — key = stem, value = @c read(key).
    No validation when @p validators and @p errors are nullptr.

    @see config::Directory
    @see config::Theme
    @see config::Shader
    @see jam::Model
    @see jam::lua::Validators
    @see end::Application
*/
class Model
    : public jam::Model
    , public jam::Instance<Model>
    , public jam::File::Watcher::Listener
{
public:
    //==========================================================================
    /** @brief Buffer capacity for GLSL source ParameterText parameters.
     *         64 KiB covers the largest practical shader pass source file.
     */
    static constexpr int glslBufferSize { 65536 };

    //==========================================================================
    /**
        @brief Construct the model — adopts CONFIG tree built in the init-list,
               then composes theme/shader subtrees, runs saveToPath,
               loadFromPath, and startWatcher in that fixed order.
    */
    Model();

    /** @brief Defaulted — Model is owned by end::Application for the process lifetime. */
    ~Model() override = default;

    /**
        @brief Reads each root lua config file from disk and overlays @c state.

        Builds a CONFIG-rooted disk mirror via @c jam::Model::fromLua with
        @c validators, overlays valid properties via @c setValuesFrom, then
        drives @c theme.loadFromPath(errors) and @c shader.loadFromPath(errors)
        in sequence. Writes the final result to @c end::Model's message overlay:
        @c ID::successMessage on success, or the accumulated error string on
        failure.
    */
    void loadFromPath();

    /** @brief Returns the shared validator map from the live Model instance. */
    static jam::lua::Validators& getValidators() noexcept { return getInstance()->validators; }

private:
    end::Model& appModel { *end::Model::getInstance() };

    /**
        @brief Writes missing root lua files from BinaryData to @c file::Config::path.
    */
    void saveToPath();

    /**
        @brief Installs @c watcher on @c file::Config::path with @c coalesceMs
               event coalescing and registers this Model as a listener.
    */
    void startWatcher();

    /**
        @brief Reloads root lua config on @c .lua @c fileUpdated events.

        Only @c fileUpdated for a @c file::Config::extension file triggers
        @c loadFromPath(). All other events and extensions are ignored.

        @param file   The file that changed.
        @param event  The change event type.
    */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /**
        @brief Watches @c file::Config::path (root lua directory only) for
               @c .lua changes.
    */
    jam::File::Watcher watcher;

    /** @brief Coalescing window in milliseconds for the filesystem watcher. */
    static constexpr int coalesceMs { 300 };

    /**
        @brief Bimap and type-based validators consumed during @c loadFromPath().

        Outer key = tree type. Inner key = property name. Bimap validators
        (Position, DropMode) are pre-populated via IIFE at member init time.
        Type-based predicates for all other properties are appended by
        @c jam::Model::fromLua during @c loadFromPath().
    */
    jam::lua::Validators validators = []
    {
        jam::lua::Validators v;

        const auto& add = [&v] (juce::Identifier treeType,
                                juce::Identifier propertyName,
                                std::function<bool (const juce::var&)> validator)
        {
            auto [treeEntry, inserted] = v.try_emplace (treeType);
            auto& [treeKey, treeValidators] = *treeEntry;
            treeValidators.addOrReplace (propertyName, std::move (validator));
        };

        add (IDtype::statusBar, ID::position, end::Position::getValidator());
        add (IDtype::actionList, ID::position, end::Position::getValidator());
        add (IDtype::config, ID::tabOrientation, end::Position::getValidator());
        add (IDtype::popup, ID::position, end::Position::getValidator());
        add (IDtype::terminal, ID::dropMultifiles, end::DropMode::getValidator());

        return v;
    }();

    Theme theme;
    Shader shader;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
