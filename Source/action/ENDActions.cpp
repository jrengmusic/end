#include "action/ENDActions.h"

bool ENDActions::run (const juce::Identifier& action)
{
    if (actions.contains (action))
    {
        actions.get (action);
        return true;
    }

    return false;
}
