#pragma once
#include <JuceHeader.h>
#include "../Bimap.h"

namespace config
{
/*____________________________________________________________________________*/

/**
    @brief Shader source model — owns the live ValueTree of raw GLSL source strings.

    Reads shader pass files (Common, Image, BufferA-D) from the active shader
    project directory. Each present file becomes a child tree with the source
    string stored as a property. Mirrors config::Theme lifecycle.
*/
class Shader : public jam::Model
{
public:
    Shader() = default;
    ~Shader() = default;

    /** @brief Loads shader sources from the given project directory.
     *  @param shaderName  Shader project directory name (e.g. "sea-at-night").
     *                     Empty string clears all sources.
     */
    void load (const juce::String& shaderName);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Shader)
};

//==============================================================================
/**
    @brief Theme state model — owns the live ValueTree of theme lua files.

    Reads theme.lua and whelmed.lua from the active theme directory
    and rebuilds the live state tree. Used by end::LookAndFeel for
    colour, font, and graphics resolution.
*/
class Theme : public jam::Model
{
public:
    Theme() = default;
    ~Theme() = default;

    /** @brief Rebuilds the theme state tree from the given theme directory.
     *  @param themeName  Theme directory name (e.g. @c "gfx").
     */
    void load (const juce::Identifier& themeName);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Theme)
};

//==============================================================================
/**
    @brief END's configuration model — owns the live ValueTree of all lua-driven config.

    @details
    End is a terminal emulator. Every knob the user can twist lives in lua
    files under @c ~/.config/end/ — @c end.lua, @c popups.lua, @c keys.lua
    (3 config sections). Theme files (@c theme.lua, @c whelmed.lua) and SVG
    assets live under @c ~/.config/end/themes/\<name\>/. config::Model is the
    single object that loads, validates, and exposes them as a live
    juce::ValueTree to the rest of the app.

    @par Inherited roles
    Inherits from jam::Model (ValueTree wrapper exposing CONTEXT and tree
    children), jam::Instance<Model> (so the live instance is reachable from
    any code that includes this header), and jam::File::Watcher::Listener
    (receives filesystem change notifications).

    @par Four-phase init
    The constructor runs four phases in fixed order — see HARD RULES in
    CAROL.md, the constructor sequence is contract:

    1. @c initialise() — builds the live tree from BinaryData snapshots
       baked into the binary at compile time, and walks every subtree
       once to register type-based + Map-aware validators.
    2. @c saveToPath() — creates @c ~/.config/end/ on disk if absent and
       writes any BinaryData-backed lua/svg file that is not already
       present (seed-once).
    3. @c loadFromPath() — reads each lua file from disk, walks the
       table, applies validators, and (if validation passes) overlays
       the result on the live tree via @c setValuesFrom.
    4. @c startWatcher() — installs the file watcher for hot reload.

    @par Validator map
    @c validators is a nested juce::Identifier → Identifier → predicate
    map. Outer key = tree type. Inner key = property name. Value =
    predicate accepting the candidate @c juce::var. The map is populated
    during @c initialise() and consumed during @c loadFromPath() — when
    a property is found in the live tree and the predicate returns false
    for the on-disk value, the property is dropped and an error is
    appended to the load report.

    @par Line map
    Populated during @c loadFromPath() by jam::lua::State's parser hook:
    captures source-line numbers for every (tag, key) so the validator
    walk can produce "<line> '<key>: invalid value "..."' style errors.

    @par File watcher
    Watches @c ~/.config/end/ for @c .lua edits and the active theme
    directory (@c ~/.config/end/themes/\<name\>/) for @c .svg edits. Lua
    edits trigger a full @c loadFromPath() cycle. SVG edits look up the
    filename in @c graphicsCallbacks and fire a property-change message
    on the matching identifier so the affected look-and-feel element
    repaints.

    @par Lifetime
    One instance per process, owned by @c end::Application. Survives
    until Application shutdown.

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
    ~Model() = default;

    /**
        @brief Reads each lua config file from disk and updates state.

        For every lua section loads the file from
        @c File::path, builds a temporary ValueTree via
        @c jam::lua::ValueTree::from with the populated validator map and
        line map, collects any per-file errors, and (if the tree is valid)
        overlays the result onto the live tree via @c setValuesFrom.

        After the walk, rebuilds @c graphicsCallbacks from the live
        @c IDtype::graphics subtree's current filenames, sets
        @c loadMessage to @"RELOAD" on success or the accumulated error
        text on failure, and fires @c sendPropertyChangeMessage on
        @c ID::loadMessage.
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
        For every key in @c File::get(), runs
        the corresponding lua source string from @c BinaryData through
        @c jam::lua::ValueTree::from with @c &validators, appending the
        child to the live @c state. Then walks every subtree and
        registers a type-based predicate per property; for string
        properties whose default value matches a known end::Map
        (Position, DropMode) the type-based predicate is replaced with
        a Map-aware predicate via @c registerValidator.
    */
    void initialise();

    /**
        @brief Creates @c ~/.config/end/ and writes any missing lua +
               svg seed files from BinaryData.
        @details
        For every key in @c File::get() writes @"stem.lua" to
        @c File::path if not already present on disk. Then writes theme
        lua files (@c File::Theme::get()) to the active theme directory
        @c ~/.config/end/themes/\<name\>/, and every
        @c File::Graphics stem (e.g. @"tab_bar.svg") to the graphics
        subdirectory inside that theme directory. Files already on disk
        are left untouched — this is a seed-once walk, not a forced
        overwrite.
    */
    void saveToPath();

    /**
        @brief Installs the file watcher on @c File::path and the active
               theme directory with 300 ms event coalescing and registers
               this Model as a listener.
    */
    void startWatcher();

    /**
        @brief Reloads lua config on @c .lua update; dispatches a
               property-change message for matching @c .svg asset updates.
        @param file   The file that changed.
        @param event  The change event type. Only @c fileUpdated triggers
                      any work; create/remove are ignored.
    */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /**
        @brief Populates @c graphicsCallbacks from the live
               @c IDtype::graphics subtree's current filenames.
        @details
        Must be called after @c loadFromPath() so the runtime filenames
        (which may differ from BinaryData seeds) are present on the
        tree. Clears the existing map first, then for every
        @c File::Graphics::get() entry builds a callback that fires
        @c sendPropertyChangeMessage on the matching identifier when
        invoked. Reads the graphics subtree from @c theme.state,
        not config @c state — graphics section now lives in the theme
        tree.
    */
    void buildGraphicsCallbacks();

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
        @brief Watches @c File::path (lua config root) and the active
               theme directory for @c .svg asset changes.
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
        type predicates registered
        by @c jam::lua::ValueTree::from during the BinaryData walk.
    */
    jam::lua::Validators validators;

    /**
        @brief SVG filename → @c sendPropertyChangeMessage callbacks,
               keyed by filename. Rebuilt by @c buildGraphicsCallbacks()
               on every @c loadFromPath() so the watcher can dispatch
               property changes for the current on-disk filename of
               each graphics asset.
    */
    jam::Function::Map<juce::String, void> graphicsCallbacks;

    Theme theme;
    Shader shader;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
