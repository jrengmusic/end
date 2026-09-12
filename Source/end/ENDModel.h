/**
 * @file ENDModel.h
 * @brief Application's own root state model — the single END-rooted ValueTree.
 */
#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"

/**
 * @class ENDModel
 * @brief Root jam::Model for the application — owns the END-rooted
 *        ValueTree that every session, tab, and pane row is appended under.
 *
 * Constructs the OVERLAY row and registers its Id::message ParameterText at
 * construction — the owner establishes this invariant once, so every
 * consumer downstream (ConfigModel, MessageOverlay) can trust the parameter
 * exists without re-checking.
 */
class ENDModel
    : public jam::Model
    , public jam::Instance<ENDModel>
{
public:
    ENDModel();
    ~ENDModel() = default;

    /** @brief Writes a message to the overlay's ParameterText.
     *  The parameter is registered at construction — this is the boundary
     *  verification of that invariant, not a defensive re-check.
     *  Any thread — ParameterText::setValue is lock-free.
     *  @param text  Message text to display.
     */
    void setMessage (const juce::String& text)
    {
        auto* param { getParameter<jam::ParameterText> (Id::toType (Id::overlay), Id::message) };
        jassert (param != nullptr);

        if (param != nullptr)
            param->setValue (text);
    }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDModel)
};
