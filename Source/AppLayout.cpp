/**
 * @file AppLayout.cpp
 * @brief Implementation of AppLayout — application ValueTree and Atom population from XML.
 *
 * @see AppLayout.h
 */

#include "AppLayout.h"
#include "AppIdentifier.h"
#include "AppState.h"

juce::var AppLayout::resolveDefault (const juce::XmlElement& elem,
                                     const Boolean& boolMap) noexcept
{
    using ResolverFn = juce::var (*)(const juce::XmlElement&, const Boolean&);

    struct TypeResolver
    {
        juce::Identifier type;
        ResolverFn resolve;
    };

    static const std::array<TypeResolver, 3> resolvers {{
        { App::ID::boolType,  [] (const juce::XmlElement& e, const Boolean& b) -> juce::var
                              { return juce::var (b.get (e.getStringAttribute (App::ID::xmlDefault.toString()))); } },
        { App::ID::floatType, [] (const juce::XmlElement& e, const Boolean&) -> juce::var
                              { return juce::var (static_cast<float> (e.getDoubleAttribute (App::ID::xmlDefault.toString()))); } },
        { App::ID::intType,   [] (const juce::XmlElement& e, const Boolean&) -> juce::var
                              { return juce::var (e.getIntAttribute (App::ID::xmlDefault.toString())); } }
    }};

    const auto typeStr { elem.getStringAttribute (App::ID::xmlType.toString()) };
    juce::var result { elem.getStringAttribute (App::ID::xmlDefault.toString()) };

    for (const auto& resolver : resolvers)
    {
        if (typeStr == resolver.type.toString())
        {
            result = resolver.resolve (elem, boolMap);
            break;
        }
    }

    return result;
}

int AppLayout::resolveIntDefault (const juce::XmlElement& elem,
                                  const Boolean& boolMap) noexcept
{
    const auto typeStr { elem.getStringAttribute (App::ID::xmlType.toString()) };
    int result { 0 };

    if (typeStr == App::ID::boolType.toString())
        result = boolMap.get (elem.getStringAttribute (App::ID::xmlDefault.toString()));
    else
        result = elem.getIntAttribute (App::ID::xmlDefault.toString());

    return result;
}

void AppLayout::build (const juce::XmlElement& xml, AppState& appState)
{
    Boolean boolMap;

    // Create one AnyMap group per VT node that holds Atom parameters.
    appState.params.add<jam::AnyMap> (App::ID::END);
    appState.params.add<jam::AnyMap> (App::ID::WINDOW);
    appState.params.add<jam::AnyMap> (App::ID::TABS);

    for (auto* child : xml.getChildIterator())
    {
        const auto& tag { child->getTagName() };

        if (tag == jam::ValueTree::PARAM.toString())
        {
            // Root-level PARAM — belongs to the END group.
            const juce::Identifier id { child->getStringAttribute (App::ID::xmlId.toString()) };
            const auto typeStr { child->getStringAttribute (App::ID::xmlType.toString()) };

            auto* rootGroup { appState.params.get<jam::AnyMap> (App::ID::END) };
            auto rootVt { appState.get() };

            if (typeStr == App::ID::intType.toString() or typeStr == App::ID::boolType.toString())
            {
                appState.addParameter (id,
                                       resolveIntDefault (*child, boolMap),
                                       *rootGroup,
                                       rootVt);
            }
            else
            {
                rootVt.setProperty (id, resolveDefault (*child, boolMap), nullptr);
            }
        }
        else
        {
            // Group element (e.g. WINDOW, TABS) — create child VT node + AnyMap.
            const juce::Identifier groupId { tag };
            juce::ValueTree groupNode { groupId };

            auto* group { appState.params.get<jam::AnyMap> (groupId) };

            for (auto* param : child->getChildIterator())
            {
                const juce::Identifier id { param->getStringAttribute (App::ID::xmlId.toString()) };
                const auto typeStr { param->getStringAttribute (App::ID::xmlType.toString()) };

                if (typeStr == App::ID::intType.toString() or typeStr == App::ID::boolType.toString())
                {
                    appState.addParameter (id,
                                           resolveIntDefault (*param, boolMap),
                                           *group,
                                           groupNode);
                }
                else
                {
                    groupNode.setProperty (id, resolveDefault (*param, boolMap), nullptr);
                }
            }

            appState.get().appendChild (groupNode, nullptr);
        }
    }
}
