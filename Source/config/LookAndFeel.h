#pragma once
#include <JuceHeader.h>
#include "end/Map.h"

namespace config
{
/*____________________________________________________________________________*/

/**
    @brief Theme-specific visual properties model.

    @details
    Owns the live juce::ValueTree of colours, fonts, and metrics sourced
    from the active theme's lua files (e.g. @c themes/gfx/theme.lua).
    The tree ID is the theme name passed at construction (e.g. @c "gfx").

    Owned by config::Model. One instance per process.

    @see jam::Model
    @see config::Model
*/
class LookAndFeel : public jam::Model
{
public:
    LookAndFeel() = default;

    ~LookAndFeel() = default;

    /** @brief Rebuilds the theme state tree from the given theme directory.
     *
     *  Reads theme.lua and whelmed.lua from
     *  @c ~/.config/end/themes/@p theme/ and rebuilds the live state tree.
     *  Called by config::Model after loadFromPath() and on theme value change.
     *
     *  @param theme  Theme directory name (e.g. @c "gfx").
     */
    void load (const juce::Identifier& theme);

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
