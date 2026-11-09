#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"
#include "end/ENDModel.h"
#include "ConfigDirectory.h"
#include "ConfigTheme.h"
#include "ConfigShader.h"

//==============================================================================
/**
    @brief END's root configuration model — owns the live CONFIG ValueTree and
           drives the ConfigTheme and ConfigShader sub-models.

    @par Build-in-ctor composition
    getDefaultThemes() and getDefaultConfig() build a fresh tree on each call, through ConfigDirectory::addDefaultTables.
    addDefaultTables reads each default document from the memoized @c jam::ConfigDocument::getOrCreate, so each
    BinaryData file is parsed one time per process.
    @c theme is a member object whose constructor adopts the THEMES subtree of getDefaultThemes().
    @c background (BACKGROUND) and @c postProcessing (POST_PROCESSING) are member objects whose
    constructors adopt an empty tree of their own type; their content is read by @c loadFromPath().
    @c ConfigModel's init-list adopts the CONFIG tree of getDefaultConfig() through @c jam::Model's
    ValueTree ctor. The constructor body then appends the @c theme (THEMES) subtree to the CONFIG tree,
    and the @c background and @c postProcessing subtrees to the GRAPHICS child.

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
    and parsed for the disk files, goes through ConfigDirectory::addFileTables. The document must pass the rules of
    @c jam::ConfigValidator {}.getRules() and the rules of its default document. When it passes, its tables are
    appended to the target tree. When it fails, the whole document is rejected and its error is added to the errors.
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

        Reads the row child @c jam::Format::toType (name) of the table child @p type of this tree directly.
        The display appearance is read from the appearance row of the display table. When the row has a dark
        cell and jam::StyleManager::isDark reports that appearance as dark, returns the dark cell. Otherwise
        returns the value cell. No assertion checks the table or the row.

        @param type  The table type of the config file, as @c jam::Format::toType gives it.
        @param name  The key of the row. This function applies @c jam::Format::toType to it to get the row child name.
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
    static juce::Identifier getEventKey (const jam::Function::Map<juce::Identifier, void (juce::ValueTree&)>& events,
                                         const juce::ValueTree& tree,
                                         const juce::Identifier& property);

private:
    /** @brief Singleton application model. loadFromPath() sets its message. */
    ENDModel& appModel { *ENDModel::getInstance() };

    /** @brief Appearance registry instance held by the model. */
    jam::SharedInstance<map::Appearance> appearance { std::in_place };

    /**
        @brief Creates the config directory, then writes the root markdown defaults from BinaryData to @c ConfigDirectory::Config::path.

        Each file of @c map::FileConfig goes through @c jam::ConfigDocument::getOrCreateDefaultFile, which writes
        the BinaryData default when @c jam::Format::isVersionOld reports the file's version as older than the default's
        version. The theme is not saved here: loadResources() saves it through ConfigTheme::saveToPath().
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

    /** @brief Theme sub-model. Its state is the THEMES subtree under the CONFIG tree. */
    ConfigTheme theme;

public:
    /** @brief BACKGROUND shader sub-model. Its state is a child of the GRAPHICS table. */
    ConfigShader background { jam::Format::toType (Id::background) };

    /** @brief POST_PROCESSING shader sub-model. Its state is a child of the GRAPHICS table. */
    ConfigShader postProcessing { jam::Format::toType (Id::postProcessing) };

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigModel)
};
