/**
 * @file LookAndFeel.h
 * @brief Custom LookAndFeel for Action::List with font dispatch.
 */

#pragma once

#include <JuceHeader.h>
#include "../component/LookAndFeel.h"

namespace Action
{ /*____________________________________________________________________________*/

/**
 * @class LookAndFeel
 * @brief Overrides getLabelFont to dispatch fonts for NameLabel and ShortcutLabel.
 *
 * Inherits Terminal::LookAndFeel.  All other tab, popup, and resizer behaviour
 * is inherited unchanged.  Instance is owned by Action::List; lifetime matches
 * the modal window.
 */
class LookAndFeel : public Terminal::LookAndFeel
{
public:
    LookAndFeel() = default;

    juce::Font getLabelFont (juce::Label& label) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
