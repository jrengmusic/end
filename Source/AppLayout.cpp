/**
 * @file AppLayout.cpp
 * @brief Implementation of XML-driven parameter schema walker for AppState.
 *
 * @see AppLayout.h
 */

#include "AppLayout.h"
#include "AppState.h"

juce::var AppLayout::resolveDefault (const juce::XmlElement& elem,
                                     const Boolean& boolMap) noexcept
{
    const auto typeStr    { elem.getStringAttribute (App::ID::type.toString()) };
    const auto defaultStr { elem.getStringAttribute (App::ID::defaultValue.toString()) };
    juce::var result {};

    if (typeStr == App::ID::boolType.toString())
    {
        result = boolMap.get (defaultStr);
    }
    else if (typeStr == App::ID::floatType.toString())
    {
        result = elem.getDoubleAttribute (App::ID::defaultValue.toString());
    }
    else if (typeStr == App::ID::stringType.toString())
    {
        result = defaultStr;
    }
    else
    {
        result = elem.getIntAttribute (App::ID::defaultValue.toString());
    }

    return result;
}

void AppLayout::build (const juce::XmlElement& xml,
                       AppState& state)
{
    Boolean boolMap;

    // Root VT node — already constructed in AppState (App::ID::END).
    juce::ValueTree rootNode { state.get() };

    // Walk XML children — dispatch on tag name.
    for (auto* child : xml.getChildIterator())
    {
        const auto& tag { child->getTagName() };

        if (tag == jam::ValueTree::PARAM.toString())
        {
            // Root-level parameter → flat params, root VT node.
            const juce::Identifier id { child->getStringAttribute (jam::ID::id.toString()) };
            const auto typeStr { child->getStringAttribute (App::ID::type.toString()) };

            if (typeStr == App::ID::floatType.toString() or typeStr == App::ID::stringType.toString())
            {
                // Float/string: PARAM child only, no Parameter.
                juce::ValueTree param { jam::ValueTree::PARAM };
                param.setProperty (jam::ID::id, id.toString(), nullptr);
                param.setProperty (jam::ID::value, resolveDefault (*child, boolMap), nullptr);
                rootNode.appendChild (param, nullptr);
            }
            else
            {
                // int/bool: Parameter<int> + PARAM child via addParameter.
                state.addParameter (id,
                                    static_cast<int> (resolveDefault (*child, boolMap)),
                                    state.params,
                                    rootNode);
            }
        }
        else
        {
            // Group tag (WINDOW, TABS) — create structural VT child, recurse into PARAMs.
            const juce::Identifier groupId { tag };
            juce::ValueTree groupNode { groupId };
            rootNode.appendChild (groupNode, nullptr);

            for (auto* groupChild : child->getChildIterator())
            {
                const juce::Identifier paramId { groupChild->getStringAttribute (jam::ID::id.toString()) };
                const auto paramTypeStr { groupChild->getStringAttribute (App::ID::type.toString()) };

                if (paramTypeStr == App::ID::floatType.toString() or paramTypeStr == App::ID::stringType.toString())
                {
                    // Float/string: PARAM child only, no Parameter.
                    juce::ValueTree param { jam::ValueTree::PARAM };
                    param.setProperty (jam::ID::id, paramId.toString(), nullptr);
                    param.setProperty (jam::ID::value, resolveDefault (*groupChild, boolMap), nullptr);
                    groupNode.appendChild (param, nullptr);
                }
                else
                {
                    // int/bool: Parameter<int> + PARAM child, flat AnyMap.
                    state.addParameter (paramId,
                                        static_cast<int> (resolveDefault (*groupChild, boolMap)),
                                        state.params,
                                        groupNode);
                }
            }
        }
    }
}
