/**
 * @file ActionList.h
 * @brief Command palette — a ModalWindow with search box.
 */

#pragma once

#include <JuceHeader.h>
#include "../../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class ActionList
 * @brief Command palette modal glass window. Inherits jreng::ModalWindow.
 *
 * Created on the fly, enters modal state, destroyed on dismiss.
 */
class ActionList : public jreng::ModalWindow
{
public:
    explicit ActionList (juce::Component& caller);
    ~ActionList () override = default;

private:
    static constexpr int searchBoxHeight { 24 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActionList)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Terminal
