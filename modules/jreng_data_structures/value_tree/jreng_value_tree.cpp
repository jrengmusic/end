namespace jreng
{
/*____________________________________________________________________________*/

ValueTree::ValueTree (const juce::Identifier& newTreeID)
{
    state.reset (new juce::ValueTree { newTreeID });
}

ValueTree::ValueTree()
{
    state.reset (new juce::ValueTree (String::toUpperCaseUnderscore (projectName)));
}

ValueTree::~ValueTree()
{
}

//==============================================================================
juce::ValueTree& ValueTree::get() const noexcept
{
    return *state;
}

std::unique_ptr<juce::XmlElement> ValueTree::getXml() const noexcept
{
    return state->createXml();
}

void ValueTree::replaceState (const juce::ValueTree& newState)
{
    state->copyPropertiesAndChildrenFrom (newState, nullptr);
}

bool ValueTree::writeToXml (juce::File& destinationFile)
{
    return state->createXml()->writeTo (destinationFile);
}

//==============================================================================

void ValueTree::valueChanged (juce::Value& value)
{
    if (onValueChanged != nullptr)
        onValueChanged();
}

juce::Value ValueTree::getValue (const juce::Identifier& tag,
                                 const juce::Identifier& id,
                                 const juce::Identifier& propertyId) const noexcept
{
    return state->getChildWithName (tag)
        .getChildWithName (id.toString())
        .getPropertyAsValue (propertyId.isNull() ? ID::value : propertyId, nullptr);
}

void ValueTree::setValue (const juce::Identifier& tag,
                          const juce::Identifier& id,
                          const juce::var& newValue,
                          const juce::Identifier& propertyId)
{
    getValue (tag, id, propertyId.isNull() ? ID::value : propertyId).setValue (newValue);
}

void ValueTree::attach (const juce::ValueTree& treeToAttach,
                        juce::UndoManager* undoManager)
{
    if (treeToAttach.getType() == state->getType())
    {
        replaceState (treeToAttach);
    }
    else if (auto child { getChildWithName (*state, treeToAttach.getType()) };
             child.isValid())
    {
        child.copyPropertiesAndChildrenFrom (treeToAttach, undoManager);
    }
    else
    {
        state->appendChild (treeToAttach, undoManager);
    }

    uniqueNodeMap.clear();
    uniqueNodeMap = buildUniqueNodeMap (*state);
}

#if JUCE_MODULE_AVAILABLE_juce_gui_basics
void ValueTree::attach (juce::Component* component,
                        juce::UndoManager* undoManager)
{
    attach (*state, component, this, undoManager);

    uniqueNodeMap.clear();
    uniqueNodeMap = buildUniqueNodeMap (*state);
}
#endif

ValueTree::UniqueNodeMap& ValueTree::getUniqueNodeMap() noexcept
{
    return uniqueNodeMap;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
