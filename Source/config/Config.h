#pragma once
#include <JuceHeader.h>
#include "Identifier.h"

namespace config
{
/*____________________________________________________________________________*/
struct File
{
    enum
    {
        config,
        whelmed,
        nexus,
        display,
        actions,
        popups,
        keys,
    };

    //==============================================================================

    static const juce::String getName (int key) noexcept;
    //==============================================================================
    /** @brief Returns the user config directory for END. Default location.
        @return  ~/.config/end/
    */
    static const juce::File path;
    static const juce::String extension;
    static const std::unordered_map<int, juce::String> map;
};

//==============================================================================
/** @brief Configuration model — owns the live ValueTree of all END config.

    Two-phase init/reload:
    - build() seeds the tree with compiled binary defaults (no I/O).
    - load(file) overlays one disk lua file on top (does NOT call build).

    Section list (7 names, single SSOT) — used by build, load, loadPath,
    and the virgin-disk default write.

    Callers must call build() before any load() to ensure the tree is valid.
*/
class Model
    : public jam::Model
    , public jam::Context<Model>
{
public:
    //==========================================================================
    Model();
    ~Model() = default;

    static const juce::ValueTree get() noexcept;
    static const juce::Rectangle<int> getInitWindowSize();
    //==========================================================================
    /** @brief Overlays one disk lua file onto the existing tree in place.

        Parses the lua file, walks the resulting ValueTree in lockstep with
        the live state, and mutates properties that differ (see
        jam::Model::setValuesFrom).  No child add/remove.  Listeners stay
        bound; parameters pick up new values on next flush.

        Filename stem (e.g. "display" from "display.lua") maps to tree section.
        Parse failures reported via errorOut (file:line) — tree keeps existing
        values.  Caller must have called build() first.

        @param file      Lua config file to load.
        @param errorOut  Out-param — populated with "file:line: message" on
                         parse failure.  Cleared on success.
    */
    void load (const juce::File& file, juce::String& errorOut);

    /** @brief Iterates *.lua children in dir and calls load() on each.

        Virgin machine: missing file → write BinaryData content to disk first,
        then continue (no need to overlay what was just written).

        @param dir  Directory containing lua section files.
        @return     Per-file error strings collected across the directory walk.
                    Empty = every file loaded successfully.  Caller surfaces
                    errors to the user (MessageOverlay) in a future sprint —
                    current callers ignore the return.
    */
    juce::StringArray loadPath (const juce::File& dir);

private:
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
