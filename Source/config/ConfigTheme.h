#pragma once
#include "ConfigDirectory.h"

/**
    @brief Theme state model — @c ConfigDirectory subclass mirroring the
           on-disk @c themes/ directory as a THEMES subtree.

    The constructor init-list passes the THEMES-rooted tree of getDefaultThemes()
    to @c ConfigDirectory's ValueTree ctor. getDefaultThemes() builds a fresh tree on each call.
    Its THEME tables are built from the memoized @c jam::ConfigDocument of each @c map::FileThemes
    markdown file. Each one is parsed from its default text, which is the BinaryData markdown of the file,
    a newline, and the markdown that @c jam::ConfigDocument::toDocument writes from the settings sheet of
    the theme stylesheet. The default text is built once per file name.
    A FLEX child (from @c map::FileFlex SVGs) is appended to the THEMES tree beside the THEME tables.
    @c ConfigModel attaches the whole @c theme.state THEMES
    subtree under its CONFIG tree with a single @c appendChild — no unwrapping.
    @c loadFromPath() overlays @c state. @c saveToPath() writes the default flex SVG files and does not read @c state.

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

    /** @brief Defaulted. The owning ConfigModel destroys this object. */
    ~ConfigTheme() override = default;

    /** @brief Reads the theme markdown files and the flex SVG files of the active theme into @c state.
     *
     *  Starts from the defaults of getDefaultThemes(). Locates the theme directory via
     *  @c ConfigDirectory::Themes::getPath. When the directory exists, builds a disk tree of THEMES from its
     *  markdown files through addFileTables, and of FLEX from its flex/ files through addFileFlex.
     *  A flex SVG that fails @c jam::XmlDocument::parse keeps its binary default. Its path, the diagnostic separator,
     *  and the @c jam::XmlDocument::isValid message are added to the errors. The disk tree overlays the defaults, and the result overlays @c state
     *  with @c setValuesFrom. When the directory is missing, adds a missing-directory error, and the defaults
     *  still overlay @c state. Sends a property change for @c jam::Format::toType (Id::theme) in both cases.
     *
     *  @param path  Active theme name from ConfigModel.
     *  @return      The markdown and flex validation errors and the missing-directory error. Empty when successful.
     */
    juce::String loadFromPath (const juce::var& path) override;

    /** @brief Writes the default flex SVG files to the active theme directory.
     *
     *  Gets or creates the theme directory and its @c flex/ subdirectory.
     *  Writes each flex SVG file of @c map::FileFlex only when the file is absent, from its BinaryData text.
     *  It writes no markdown file. The markdown files are written by @c ConfigDirectory::addFileTables, through
     *  @c jam::ConfigDocument::getOrCreateFileDocument, when a file's version is older than its default's.
     *
     *  @param path  Active theme name from ConfigModel.
     */
    void saveToPath (const juce::var& path) override;

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigTheme)
};
