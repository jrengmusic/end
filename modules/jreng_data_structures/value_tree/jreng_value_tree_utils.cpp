namespace jreng
{
/*____________________________________________________________________________*/

const bool ValueTree::applyFunctionRecursively (const juce::ValueTree& root,
                                                const std::function<bool (const juce::ValueTree&)>& function)
{
    if (function (root))
        return true;

    for (auto&& child : root)
    {
        if (applyFunctionRecursively (child, function))
            return true;
    }

    return false;
}

juce::ValueTree ValueTree::getChildWithName (const juce::ValueTree& root, const juce::Identifier& name)
{
    juce::ValueTree foundChild;

    auto findChildWithName = [&foundChild, &name] (const juce::ValueTree& node) -> bool
    {
        if (node.getType() == name)
        {
            foundChild = node;
            return true;
        }
        return false;
    };

    applyFunctionRecursively (root, findChildWithName);

    return foundChild;
}

bool ValueTree::findAndRemoveChild (juce::ValueTree& root,
                                    const juce::Identifier& name,
                                    juce::UndoManager* undoManager)
{
    auto target = getChildWithName (root, name);

    if (target.isValid())
    {
        auto parent = target.getParent();

        if (parent.isValid())
        {
            parent.removeChild (target, undoManager);
            return true;
        }
    }

    return false;
}

juce::ValueTree ValueTree::getOrCreateChildWithName (juce::ValueTree& root,
                                                     const juce::Identifier& name,
                                                     juce::UndoManager* undoManager)
{
    juce::ValueTree foundChild;

    auto findChildWithName = [&foundChild, &name] (const juce::ValueTree& node) -> bool
    {
        if (node.getType() == name)
        {
            foundChild = node;
            return true;
        }
        return false;
    };

    applyFunctionRecursively (root, findChildWithName);

    if (foundChild.isValid())
        return foundChild;

    return root.getOrCreateChildWithName (name, undoManager);
}

juce::ValueTree ValueTree::getChildWithID (const juce::ValueTree& root, const juce::var& parameterID)
{
    juce::ValueTree childWithID;

    auto findChildWithID = [&childWithID, &parameterID] (const juce::ValueTree& node) -> bool
    {
        if (node.hasProperty (ID::id) and node.getProperty (ID::id) == parameterID)
        {
            childWithID = node;
            return true;
        }
        return false;
    };

    applyFunctionRecursively (root, findChildWithID);

    return childWithID;
}

juce::Value ValueTree::getValueFromChildWithID (const juce::ValueTree& root,
                                                const juce::Identifier& parameterID,
                                                const juce::Identifier& propertyName,
                                                juce::UndoManager* undoManager)
{
    return getChildWithID (root, parameterID.toString()).getPropertyAsValue (propertyName, undoManager);
}

juce::Value ValueTree::getValueFromChildWithName (const juce::ValueTree& root,
                                                  const juce::Identifier& name,
                                                  const juce::Identifier& propertyName,
                                                  juce::UndoManager* undoManager)
{
    return getChildWithName (root, name).getPropertyAsValue (propertyName, undoManager);
}

//==============================================================================
#if JUCE_MODULE_AVAILABLE_juce_gui_basics

juce::ValueTree
ValueTree::getRoot (juce::ValueTree& taproot, juce::Component* component, juce::UndoManager* undoManager)
{
    if (auto componentID { component->getComponentID() }; componentID.isNotEmpty())
    {
        return juce::Identifier (componentID) == taproot.getType()
                   ? taproot
                   : getOrCreateChildWithName (taproot, componentID, undoManager);
    }

    return juce::ValueTree();
}

juce::ValueTree ValueTree::getParent (juce::ValueTree& taproot, juce::Component* child, juce::UndoManager* undoManager)
{
    if (auto childID { child->getComponentID() }; childID.isNotEmpty())
    {
        if (auto parent { getChildWithName (taproot, childID) }; parent.isValid())
        {
            return parent;
        }
        else
        {
            if (auto root { getRoot (taproot, child->getParentComponent(), undoManager) }; root.isValid())
                return getOrCreateChildWithName (root, childID, undoManager);
        }
    }

    return juce::ValueTree();
}

//==============================================================================
void ValueTree::attach (juce::ValueTree& taproot,
                        juce::Component* component,
                        juce::Value::Listener* listener,
                        juce::UndoManager* undoManager)
{
    if (auto root { getRoot (taproot, component, undoManager) }; root.isValid())
    {
        for (auto& child : component->getChildren())
            attachChild (taproot, child, listener, undoManager);
    }
}

void ValueTree::attachChild (juce::ValueTree& taproot,
                             juce::Component* child,
                             juce::Value::Listener* listener,
                             juce::UndoManager* undoManager)
{
    if (auto parent { getParent (taproot, child, undoManager) }; parent.isValid())
    {
        const auto& hasValidChildID = [] (auto& c)
        {
            return std::any_of (c->getChildren().begin(),
                                c->getChildren().end(),
                                [] (auto& grandChild)
                                {
                                    return grandChild->getComponentID().isNotEmpty();
                                });
        };

        for (auto& grandchild : child->getChildren())
        {
            if (hasValidChildID (grandchild))
            {
                attachChild (taproot, grandchild, listener, undoManager);
            }
            else
            {
                if (auto grandchildID { grandchild->getComponentID() }; grandchildID.isNotEmpty())
                {
                    auto& value { Value::getFrom (grandchild) };

                    if (not parent.hasProperty (grandchildID))
                        parent.setProperty (grandchildID, value, undoManager);

                    value.referTo (parent.getPropertyAsValue (grandchildID, undoManager));

                    if (listener != nullptr)
                        value.addListener (listener);
                }
            }
        }
    }
}

//==============================================================================
void ValueTree::attach (juce::ValueTree& state, juce::Component* component, juce::UndoManager* undoManager)
{
    const auto& attachChild = [&state, &undoManager] (auto& c)
    {
        if (auto childID { c->getComponentID() }; childID.isNotEmpty())
        {
            if (Value::isNonVoid (c))
            {
                juce::Value& value { Value::getFrom (c) };

                auto node { state.getOrCreateChildWithName (juce::Identifier (childID), undoManager) };

                if (node.isValid())
                {
                    if (node.hasProperty (ID::value))
                    {
                        value.referTo (node.getPropertyAsValue (ID::value, undoManager));
                    }
                    else
                    {
                        node.setProperty (ID::value, value.getValue(), undoManager);
                        value.referTo (node.getPropertyAsValue (ID::value, undoManager));
                    }
                }
            }

            if (auto comp { dynamic_cast<Value::Object*> (c) })
            {
                if (comp->onAttachment != nullptr)
                    comp->onAttachment();
            }
        }
    };

    attachChild (component);

    for (auto& child : component->getChildren())
        attachChild (child);
}

//==============================================================================
void ValueTree::attach (ValueTree& state, juce::Component* parent, juce::UndoManager* undoManager)
{
    juce::ValueTree top;

    if (auto parentID { parent->getComponentID() }; parentID.isNotEmpty())
        top = state.get().getOrCreateChildWithName (String::toValidID (parentID), undoManager);

    if (top.isValid())
    {
        const auto& attachChild = [&top, &undoManager, &state] (auto& c)
        {
            if (auto childID { c->getComponentID() }; childID.isNotEmpty())
            {
                if (Value::isNonVoid (c))
                {
                    juce::Value& value { Value::getFrom (c) };

                    if (auto child { top.getChildWithName (childID) }; child.isValid())
                    {
                        value.addListener (&state);
                        value.referTo (child.getPropertyAsValue (ID::value, undoManager));
                    }
                    else
                    {
                        child = juce::ValueTree (childID);
                        child.setProperty (ID::value, value.getValue(), undoManager);
                        top.appendChild (child, undoManager);
                        value.addListener (&state);
                        value.referTo (child.getPropertyAsValue (ID::value, undoManager));
                    }
                }

                if (auto comp { dynamic_cast<Value::Object*> (c) })
                {
                    if (comp->onAttachment)
                        comp->onAttachment();
                }
            }
        };

        attachChild (parent);

        for (auto& child : parent->getChildren())
            attachChild (child);
    }
}

#endif // JUCE_MODULE_AVAILABLE_juce_gui_basics

//==============================================================================
void ValueTree::attach (juce::ValueTree& root,
                        juce::Value& value,
                        const juce::String& parameterId,
                        const juce::String& valueId,
                        juce::Value::Listener* listener,
                        juce::UndoManager* undoManager)
{
    if (auto tree { root.getChildWithProperty (ID::id, parameterId) }; tree.isValid())
    {
        if (tree.hasProperty (valueId))
        {
            if (listener != nullptr)
                value.addListener (listener);

            value.referTo (tree.getPropertyAsValue (valueId, undoManager));
        }
        else
        {
            tree.setProperty (valueId, value.getValue(), undoManager);

            if (listener != nullptr)
                value.addListener (listener);

            value.referTo (tree.getPropertyAsValue (valueId, undoManager));
        }
    }
}

//==============================================================================
bool ValueTree::loadState (juce::ValueTree& target, const juce::File& xmlFile)
{
    if (auto xml { juce::parseXML (xmlFile) })
    {
        if (auto source { juce::ValueTree::fromXml (*xml) }; source.isValid())
            return loadState (target, source);
    }

    return false;
}

bool ValueTree::loadState (juce::ValueTree& target, const juce::ValueTree& source)
{
    auto updateProperties = [] (juce::ValueTree& target, const juce::ValueTree& source)
    {
        for (int i = 0; i < source.getNumProperties(); ++i)
        {
            const juce::Identifier propertyName = source.getPropertyName (i);
            target.setProperty (propertyName, source[propertyName], nullptr);
        }
    };

    auto countChildTypes = [] (const juce::ValueTree& tree)
    {
        std::map<juce::Identifier, int> childTypeCount;

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const juce::Identifier childType = tree.getChild (i).getType();
            ++childTypeCount[childType];
        }

        return childTypeCount;
    };

    auto handleChildren = [&] (juce::ValueTree& target,
                               const juce::ValueTree& source,
                               const std::map<juce::Identifier, int>& sourceChildTypeCount)
    {
        for (int i = 0; i < source.getNumChildren(); ++i)
        {
            const juce::ValueTree& sourceChild = source.getChild (i);
            juce::ValueTree targetChild = target.getChildWithName (sourceChild.getType());

            if (sourceChildTypeCount.at (sourceChild.getType()) > 1)
            {
                if (targetChild.isValid() and target.getNumChildren() == source.getNumChildren())
                {
                    if (not loadState (targetChild, sourceChild))
                        return false;
                }
                else
                {
                    target.removeAllChildren (nullptr);

                    for (int j = 0; j < source.getNumChildren(); ++j)
                    {
                        target.addChild (source.getChild (j).createCopy(), -1, nullptr);
                    }

                    return true;
                }
            }
            else
            {
                if (targetChild.isValid())
                {
                    if (not loadState (targetChild, sourceChild))
                        return false;
                }
                else
                {
                    target.addChild (sourceChild.createCopy(), -1, nullptr);
                }
            }
        }

        return true;
    };

    updateProperties (target, source);

    auto sourceChildTypeCount = countChildTypes (source);

    return handleChildren (target, source, sourceChildTypeCount);
}

ValueTree::UniqueNodeMap ValueTree::buildUniqueNodeMap (const juce::ValueTree& root)
{
    std::unordered_map<juce::String, juce::ValueTree> map;

    std::function<void (const juce::ValueTree&)> visit = [&] (const juce::ValueTree& node)
    {
        if (node.getNumProperties() > 0)
        {
            map.emplace (node.getType().toString(), node);
        }

        for (auto&& child : node)
            visit (child);
    };

    visit (root);
    return map;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
