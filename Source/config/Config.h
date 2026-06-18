#pragma once
#include <JuceHeader.h>
#include "../Bimap.h"
#include "Directory.h"

namespace config
{
/*____________________________________________________________________________*/

/**
    @brief Shader source model — @c config::Directory subclass that owns the
           live ValueTree of raw GLSL source strings.

    Reads shader pass files (Common, Image, BufferA-D) from the active shader
    project directory passed directly as a resolved @c juce::File. Each present
    file becomes a property on @c state keyed by its bare filename stem. Absent
    files are silently skipped.

    Load policy: no BinaryData defaults (@c initialise is a no-op); no disk
    writing (@c saveToPath is a no-op). Only @c loadFromPath and @c fileChanged
    perform work. @c loadFromPath ends by firing
    @c state.sendPropertyChangeMessage(IDtype::shaders). Controller listens on
    the @c IDtype::shaders notify channel.
*/
class Shader : public Directory
{
public:
    /** @brief Constructs with treeType = shaders. */
    Shader()
        : Directory (IDtype::shaders)
    {
    }

    ~Shader() override = default;

protected:
    /**
        @brief No BinaryData defaults for shaders — no-op.

        Shader projects are user-supplied on disk. There is nothing to
        write to disk from BinaryData at startup.
    */
    void initialise() override {}

    /**
        @brief No files to write for shaders — no-op.

        Shader projects are user-supplied on disk. There is nothing to
        write to disk from BinaryData at startup.
    */
    void saveToPath (const juce::File&) override {}

    /** @brief Clears all source properties, then reads each present pass
     *         file from @c dir into @c state as a string property. Fires
     *         @c state.sendPropertyChangeMessage(IDtype::shaders) last.
     *  @param dir  Shader project directory (may be invalid when name is empty).
     */
    void loadFromPath (const juce::File& dir) override;

    /** @brief On @c fileUpdated — reloads all sources from the changed file's
     *         parent directory. @c loadFromPath handles the notify.
     *  @param file   The file that changed.
     *  @param event  Change event type.
     */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Shader)
};

//==============================================================================
/**
    @brief Theme state model — @c config::Directory subclass that owns the
           live ValueTree of theme lua files and SVG graphics callbacks.

    Manages the full lifecycle for a named theme subdirectory passed as a
    resolved @c juce::File:

    - @c initialise()         builds the default tree from BinaryData (theme.lua,
                              whelmed.lua) so that @c saveToPath can walk it for
                              SVG filenames.
    - @c saveToPath(dir)      writes each theme lua and every graphics SVG listed
                              in the tree into @c dir inline via a local lambda —
                              no shared write method.
    - @c loadFromPath(dir)    clears @c state, reads each lua from disk into the
                              tree, rebuilds @c graphicsCallbacks, then fires
                              @c state.sendPropertyChangeMessage(ID::theme) last.
    - @c fileChanged(...)     on @c fileUpdated for @c .lua — calls
                              @c loadFromPath (which handles the notify);
                              for @c .svg — dispatches via @c graphicsCallbacks.

    LookAndFeel listens on the @c ID::theme notify channel.

    @see config::Directory
    @see end::LookAndFeel
*/
class Theme : public Directory
{
public:
    /** @brief Constructs with treeType = themes. */
    Theme()
        : Directory (IDtype::themes)
    {
    }

    ~Theme() override = default;

protected:
    /** @brief Builds the default theme tree from BinaryData — required so
     *         @c saveToPath can walk the tree for SVG filenames.
     *
     *  For each @c config::Themes::get() entry, runs the BinaryData lua
     *  source through @c jam::Model::fromLua and appends the result to
     *  @c state. Called fresh on every @c load() cycle before disk ops.
     */
    void initialise() override;

    /** @brief Writes each theme lua file and every graphics SVG into @c dir inline.
     *
     *  Guards on @c dir.getFullPathName().isNotEmpty(). Uses a local lambda to
     *  write each file only when absent and the BinaryData entry exists. Calls
     *  @c jam::File::getOrCreateDirectory to create @c dir before writing, then
     *  walks @c state via @c jam::Model::applyFunctionRecursively reading
     *  @c ID::graphics (@c jam::Model::toStringArray) and writes each non-empty
     *  SVG name to the graphics subdirectory.
     *
     *  @param dir  Theme directory to write into (may be invalid if empty name).
     */
    void saveToPath (const juce::File& dir) override;

    /** @brief Clears @c state, reads each lua from disk, rebuilds graphics
     *         callbacks, then fires @c state.sendPropertyChangeMessage(ID::theme).
     *
     *  @c state.removeAllChildren() first. If @c dir.isDirectory(), for each
     *  @c config::Themes::get() entry reads the file from disk through
     *  @c jam::Model::fromLua and appends the result child to @c state.
     *  Always calls @c buildGraphicsCallbacks() then
     *  @c state.sendPropertyChangeMessage(ID::theme) last.
     *
     *  @param dir  Theme directory to read from.
     */
    void loadFromPath (const juce::File& dir) override;

    /** @brief Dispatches theme and SVG file changes.
     *
     *  On @c fileUpdated: if the extension matches @c config::Themes::extension
     *  — calls @c loadFromPath(file.getParentDirectory()); @c loadFromPath
     *  handles the notify. If extension matches @c jam::IDref::svg — looks up
     *  the filename in @c graphicsCallbacks and calls the stored callback.
     *
     *  @param file   The file that changed.
     *  @param event  Change event type.
     */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

private:
    /** @brief Rebuilds @c graphicsCallbacks from the live @c state tree.
     *
     *  Clears the map, then walks @c state via
     *  @c jam::Model::applyFunctionRecursively reading the @c ID::graphics
     *  property (@c jam::Model::toStringArray) on each node. For each
     *  non-empty filename builds a callback that fires
     *  @c state.sendPropertyChangeMessage on the matching identifier — either
     *  the button-state suffix (when present in @c jam::map::ButtonState) or
     *  the bare stem. Called at the end of every @c loadFromPath().
     */
    void buildGraphicsCallbacks();

    /** @brief SVG filename to @c sendPropertyChangeMessage callback map.
     *         Rebuilt by @c buildGraphicsCallbacks() after every disk load.
     */
    jam::Function::Map<juce::String, void> graphicsCallbacks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Theme)
};

//==============================================================================
/**
    @brief END's root configuration model — owns the live ValueTree for all
           root lua config files and drives the Theme/Shader sub-models.

    @details
    End is a terminal emulator. Every user-facing knob lives in lua files
    under @c ~/.config/end/ — @c init.lua, @c popups.lua, @c keys.lua
    (the three root config sections). config::Model loads, validates, and
    exposes them as a live @c juce::ValueTree. It is NOT a @c Directory
    subclass — it handles only the root directory and owns one @c Theme and
    one @c Shader instance, driving their @c load() cycle after every root
    reload.

    @par Inherited roles
    Inherits from @c jam::Model (ValueTree wrapper exposing CONTEXT and tree
    children), @c jam::Instance<Model> (so the live instance is reachable
    from any code that includes this header), and
    @c jam::File::Watcher::Listener (receives root-directory filesystem
    change notifications).

    @par Four-phase init (§1.5 binary-defaults → disk overlay contract)
    The constructor runs four phases in fixed order:

    1. @c initialise() — builds the live tree from BinaryData defaults
       (all three root lua files) and registers type-based + Map-aware
       validators in a single recursive walk.
    2. @c saveToPath() — writes each missing root lua file inline from BinaryData.
       Files already on disk are left untouched (written once). Theme lua and
       SVG writing is owned entirely by @c Theme::saveToPath().
    3. @c loadFromPath() — reads each lua file from disk, overlays the
       result on the live tree via @c setValuesFrom, then drives
       @c theme.load() and @c shader.load() with the dir resolved directly
       via the bimap.
    4. @c startWatcher() — installs the @c jam::File::Watcher on
       @c File::path with @c Directory::coalesceMs (300 ms) coalescing.

    @par Validator map
    @c validators is a nested @c juce::Identifier → Identifier → predicate
    map built during @c initialise() and consumed during @c loadFromPath().
    When a predicate returns @c false for an on-disk value the property is
    dropped and an error is appended to the load report.

    @par File watcher
    Watches @c ~/.config/end/ for @c .lua edits only. On change triggers a
    full @c loadFromPath() cycle (which in turn drives @c theme.load() and
    @c shader.load()). SVG/theme watching is owned entirely by @c Theme.

    @par Lifetime
    One instance per process, owned by @c end::Application. Survives until
    Application shutdown.

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
    /**
        @brief Construct the model — runs initialise, saveToPath,
               loadFromPath, startWatcher in that fixed order.
    */
    Model();

    /** @brief Defaulted — Model is owned by end::Application for the
                process lifetime. */
    ~Model() override = default;

    /**
        @brief Reads each root lua config file from disk and updates state.

        For every entry in @c File::get(), loads the file from @c File::path,
        builds a temporary ValueTree via @c jam::Model::fromLua with the
        populated validator map, collects any per-file errors, and (if the
        tree is valid) overlays the result onto the live tree via
        @c setValuesFrom. After the walk, sets @c loadMessage to @"RELOAD"
        on success or the accumulated error text on failure, fires
        @c sendPropertyChangeMessage on @c ID::loadMessage, then drives
        @c theme.load() and @c shader.load() with the dir resolved directly
        via the bimap.
    */
    void loadFromPath();

    /**
        @brief Returns the most recent load result.
        @return @"RELOAD" on success; the accumulated per-file error text
                on failure. Never stored on the value tree.
    */
    const juce::String& getLoadMessage() const noexcept { return loadMessage; }

    /** @brief Returns the theme model for listener registration. */
    Theme& getTheme() noexcept { return theme; }

    /** @brief Returns the shader model for listener registration. */
    Shader& getShader() noexcept { return shader; }

private:
    /**
        @brief Populates the live tree from BinaryData and builds the
               validator map in a single recursive walk.
        @details
        For every entry in @c File::get(), runs the corresponding BinaryData
        lua source through @c jam::Model::fromLua with @c &validators,
        appending the child to @c state. Then walks every subtree via
        @c jam::Model::applyFunctionRecursively and registers a type-based
        predicate per property; for string properties whose default matches a
        known @c end::Map (Position, DropMode) the predicate is replaced with
        a Map-aware predicate via @c registerValidator.
    */
    void initialise();

    /**
        @brief Writes missing root lua files from BinaryData to @c File::path inline.
        @details
        For every entry in @c File::get(), writes the file from BinaryData to
        @c File::path only when the file does not already exist and the
        BinaryData entry is present (written once). Theme lua and SVG writing is
        owned entirely by @c Theme::saveToPath().
    */
    void saveToPath();

    /**
        @brief Installs the file watcher on @c File::path with
               @c Directory::coalesceMs (300 ms) event coalescing and
               registers this Model as a listener. Watches root only — theme
               directory watching is owned by @c Theme.
    */
    void startWatcher();

    /**
        @brief Reloads root lua config on @c .lua @c fileUpdated events.

        Only @c fileUpdated for a @c File::extension file triggers work — calls
        @c loadFromPath(). All other events and extensions are ignored.
        SVG dispatch is owned by @c Theme::fileChanged().

        @param file   The file that changed.
        @param event  The change event type.
    */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /**
        @brief Registers a domain-constrained validator for the
               (treeType, propertyName) pair.
        @details
        Wraps the underlying @c Validators map's two-level insertion
        with @c try_emplace on the outer key and @c insert_or_assign
        on the inner key. This is the only insertion path used by
        @c initialise() for Map-aware string predicates.
        @param treeType      The tree type identifier (outer key).
        @param propertyName  The property identifier (inner key).
        @param validator     Predicate returning @c true when the
                             candidate @c juce::var is acceptable.
    */
    void registerValidator (juce::Identifier treeType,
                            juce::Identifier propertyName,
                            std::function<bool (const juce::var&)> validator);

    /**
        @brief Watches @c File::path (root lua directory only) for
               @c .lua changes.
    */
    jam::File::Watcher watcher;

    /**
        @brief Most recent load result — @"RELOAD" on success, error
               text on failure. Written by @c loadFromPath(); read via
               @c getLoadMessage(). Never stored on the value tree.
    */
    juce::String loadMessage;

    /**
        @brief Type-based and Map-aware validators built during
               @c initialise() in a single walk.
        @details
        Outer key = tree type. Inner key = property name. String
        properties whose defaults match a known Map (Position, DropMode)
        receive domain-constrained predicates; all others receive plain
        type predicates registered by @c jam::Model::fromLua during the
        BinaryData walk.
    */
    jam::lua::Validators validators;

    Theme theme;
    Shader shader;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
