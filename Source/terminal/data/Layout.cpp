#include "Layout.h"
#include "State.h"

namespace Terminal
{

int Layout::resolveDefault (const juce::XmlElement& elem,
                            const Boolean& boolMap) noexcept
{
    const auto typeStr    { elem.getStringAttribute (ID::type.toString()) };
    const auto defaultStr { elem.getStringAttribute (ID::defaultValue.toString()) };
    int result { 0 };

    if (typeStr == ID::boolType.toString())
    {
        // boolMap.get(string) returns the int key: "true" → 1, "false" → 0.
        result = boolMap.get (defaultStr);
    }
    else
    {
        result = elem.getIntAttribute (ID::defaultValue.toString());
    }

    return result;
}

void Layout::build (const juce::XmlElement& xml,
                    State& state,
                    TextBuffer& textBuffer)
{
    Boolean boolMap;

    // All groups are nested AnyMaps so flush() can iterate uniformly.
    state.params.add<jam::AnyMap> (ID::SESSION);
    state.params.add<jam::AnyMap> (ID::MODES);

    auto* screenCtx { map::Screen::getContext() };

    for (const auto& [index, screenName] : screenCtx->get())
    {
        state.params.add<jam::AnyMap> (juce::Identifier { screenName });
    }

    // Root SESSION VT node — already constructed in State (ID::SESSION).
    juce::ValueTree rootNode { state.get() };

    // MODES VT node — appended to SESSION.
    juce::ValueTree modesNode { ID::MODES };
    rootNode.appendChild (modesNode, nullptr);

    // Screen VT nodes — appended to SESSION.
    for (const auto& [index, screenName] : screenCtx->get())
    {
        const juce::Identifier screenId { screenName };
        juce::ValueTree screenNode { screenId };
        rootNode.appendChild (screenNode, nullptr);
    }

    // Walk XML, dispatch on tag name.
    for (auto* child : xml.getChildIterator())
    {
        const auto& tag { child->getTagName() };

        if (tag == jam::ValueTree::PARAM.toString())
        {
            // Root-level parameter → SESSION group.
            auto* sessionGroup { state.params.get<jam::AnyMap> (ID::SESSION) };
            state.addParameter (juce::Identifier { child->getStringAttribute (ID::id.toString()) },
                                resolveDefault (*child, boolMap),
                                *sessionGroup,
                                rootNode);
        }
        else if (tag == ID::MODES.toString())
        {
            // Mode parameters → MODES group + modesNode.
            auto* modesGroup { state.params.get<jam::AnyMap> (ID::MODES) };

            for (auto* modeChild : child->getChildIterator())
            {
                state.addParameter (juce::Identifier { modeChild->getStringAttribute (ID::id.toString()) },
                                    resolveDefault (*modeChild, boolMap),
                                    *modesGroup,
                                    modesNode);
            }
        }
        else if (tag == ID::SCREEN.toString())
        {
            // Per-screen parameters — schema declared once, duplicated per screen.
            for (const auto& [index, screenName] : screenCtx->get())
            {
                const juce::Identifier screenId { screenName };
                auto* screenGroup { state.params.get<jam::AnyMap> (screenId) };
                auto  screenNode  { rootNode.getChildWithName (screenId) };

                for (auto* screenChild : child->getChildIterator())
                {
                    state.addParameter (juce::Identifier { screenChild->getStringAttribute (ID::id.toString()) },
                                        resolveDefault (*screenChild, boolMap),
                                        *screenGroup,
                                        screenNode);
                }
            }
        }
        else if (tag == ID::TEXT.toString())
        {
            // TEXT parameter — Parameter<const char*> in SESSION group, slot in TextBuffer.
            const juce::Identifier id { child->getStringAttribute (ID::id.toString()) };
            const int maxlen          { child->getIntAttribute (ID::maxlen.toString()) };

            state.addTextParameter (id, rootNode);
            textBuffer.addSlot (id, maxlen);
        }
    }
}

} // namespace Terminal
