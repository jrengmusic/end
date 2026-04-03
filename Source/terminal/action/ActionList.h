/**
 * @file ActionList.h
 * @brief Command palette — a GlassWindow with search box.
 */

#pragma once

#include <JuceHeader.h>
#include "../../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class ActionList
 * @brief Command palette glass window. Inherits jreng::GlassWindow.
 *
 * Created on the fly, enters modal state, destroyed on dismiss.
 */
class ActionList : public jreng::GlassWindow
{
public:
    explicit ActionList (juce::Component& caller);
    ~ActionList() override = default;

    void closeButtonPressed() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    static constexpr int searchBoxHeight { 24 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActionList)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal

