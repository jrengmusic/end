/**
 * @file LookAndFeel.cpp
 * @brief Custom LookAndFeel for Action::List with font dispatch.
 */

#include "LookAndFeel.h"
#include "ActionRow.h"
#include "../config/Config.h"

namespace Action
{ /*____________________________________________________________________________*/

juce::Font LookAndFeel::getLabelFont (juce::Label& label)
{
    auto* cfg { Config::getContext() };
    juce::Font result { juce::LookAndFeel_V4::getLabelFont (label) };

    if (dynamic_cast<NameLabel*> (&label) != nullptr)
    {
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->getString (Config::Key::actionListNameFamily))
                                  .withPointHeight (cfg->getFloat (Config::Key::actionListNameSize)) };
    }
    else if (dynamic_cast<ShortcutLabel*> (&label) != nullptr)
    {
        result = juce::Font { juce::FontOptions()
                                  .withName (cfg->getString (Config::Key::actionListShortcutFamily))
                                  .withPointHeight (cfg->getFloat (Config::Key::actionListShortcutSize)) };
    }

    return result;
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Action
