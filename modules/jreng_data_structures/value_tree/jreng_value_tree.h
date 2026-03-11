namespace jreng
{
/*____________________________________________________________________________*/

class ValueTree : public juce::Value::Listener
{
public:
    explicit ValueTree (const juce::Identifier& newTreeID);
    ValueTree();
    ~ValueTree();

    //==============================================================================
    juce::ValueTree& get() const noexcept;
    std::unique_ptr<juce::XmlElement> getXml() const noexcept;
    void replaceState (const juce::ValueTree& newState);
    bool writeToXml (juce::File& destinationFile);

    //==============================================================================
    std::function<void()> onValueChanged;
    void valueChanged (juce::Value& value) override;

    //==============================================================================
    juce::Value getValue (const juce::Identifier& tag,
                          const juce::Identifier& id,
                          const juce::Identifier& propertyId = juce::Identifier()) const noexcept;

    void setValue (const juce::Identifier& tag,
                   const juce::Identifier& id,
                   const juce::var& newValue,
                   const juce::Identifier& propertyId = juce::Identifier());

    void attach (const juce::ValueTree& treeToAttach, juce::UndoManager* undoManager = nullptr);

#if JUCE_MODULE_AVAILABLE_juce_gui_basics
    void attach (juce::Component* component, juce::UndoManager* undoManager = nullptr);
#endif

    //==============================================================================
    static const bool applyFunctionRecursively (const juce::ValueTree& root,
                                                const std::function<bool (const juce::ValueTree&)>& function);

    static juce::ValueTree getChildWithName (const juce::ValueTree& root,
                                             const juce::Identifier& name);

    static bool findAndRemoveChild (juce::ValueTree& root,
                                    const juce::Identifier& name,
                                    juce::UndoManager* undoManager = nullptr);

    static juce::ValueTree getOrCreateChildWithName (juce::ValueTree& root,
                                                     const juce::Identifier& name,
                                                     juce::UndoManager* undoManager = nullptr);

    static juce::ValueTree getChildWithID (const juce::ValueTree& root,
                                           const juce::var& parameterID);

    static juce::Value getValueFromChildWithID (const juce::ValueTree& root,
                                                const juce::Identifier& parameterID,
                                                const juce::Identifier& propertyName = ID::value,
                                                juce::UndoManager* undoManager = nullptr);

    static juce::Value getValueFromChildWithName (const juce::ValueTree& root,
                                                  const juce::Identifier& name,
                                                  const juce::Identifier& propertyName = ID::value,
                                                  juce::UndoManager* undoManager = nullptr);

    //==============================================================================
#if JUCE_MODULE_AVAILABLE_juce_gui_basics
    static juce::ValueTree getRoot (juce::ValueTree& tapRoot,
                                    juce::Component* component,
                                    juce::UndoManager* undoManager);

    static juce::ValueTree getParent (juce::ValueTree& tapRoot,
                                      juce::Component* child,
                                      juce::UndoManager* undoManager);

    static void attach (juce::ValueTree& taproot,
                        juce::Component* component,
                        juce::Value::Listener* listener,
                        juce::UndoManager* undoManager = nullptr);

    static void attachChild (juce::ValueTree& taproot,
                             juce::Component* child,
                             juce::Value::Listener* listener,
                             juce::UndoManager* undoManager);

    static void attach (juce::ValueTree& state,
                        juce::Component* component,
                        juce::UndoManager* undoManager = nullptr);

    static void attach (ValueTree& state,
                        juce::Component* component,
                        juce::UndoManager* undoManager = nullptr);
#endif

    //==============================================================================
    static void attach (juce::ValueTree& root,
                        juce::Value& value,
                        const juce::String& parameterId,
                        const juce::String& valueId,
                        juce::Value::Listener* listener = nullptr,
                        juce::UndoManager* undoManager = nullptr);

    //==============================================================================
    static bool loadState (juce::ValueTree& target, const juce::File& xmlFile);
    static bool loadState (juce::ValueTree& target, const juce::ValueTree& source);

    //==============================================================================
    using UniqueNodeMap = std::unordered_map<juce::String, juce::ValueTree>;
    static UniqueNodeMap buildUniqueNodeMap (const juce::ValueTree& root);

    UniqueNodeMap& getUniqueNodeMap() noexcept;

private:
    std::unique_ptr<juce::ValueTree> state;
    UniqueNodeMap uniqueNodeMap;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTree)
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
