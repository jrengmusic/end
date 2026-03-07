namespace jreng
{
/*____________________________________________________________________________*/
/**
 * @struct XML
 * @brief Utility helpers for working with juce::XmlElement trees.
 *
 * Provides recursive traversal, attribute and tag lookups, typed property
 * access, and convenience functions for loading XML from JUCE BinaryData.
 *
 * Example usage:
 * @code
 * XML::applyFunctionRecursively (parentXML, [] (juce::XmlElement* e)
 * {
 *     DBG (e->getTagName());
 * });
 * @endcode
 */
struct XML
{
    /**
     * @brief Apply a function recursively to an XML element and all its children.
     *
     * @tparam Function A callable type taking (juce::XmlElement*).
     * @param xml       Pointer to the root XmlElement. Must not be nullptr.
     * @param function  Function to apply to each element.
     *
     * @note Traverses depth‑first. Asserts if xml is nullptr.
     */
    template <typename Function>
    static void applyFunctionRecursively (juce::XmlElement* xml, const Function& function)
    {
        function (xml);

        for (auto* e : xml->getChildIterator())
            applyFunctionRecursively (e, function);

        /** If you hit this assertion, your xml pointer is null*/
        assert (xml != nullptr);
    }

    //==============================================================================
    /**
     * @brief Find the first child element (recursively) with a matching attribute.
     *
     * @param xml            Pointer to the XmlElement to search.
     * @param attributeName  Name of the attribute to match.
     * @param attributeValue Value to compare (case‑insensitive).
     * @return Pointer to the matching XmlElement, or nullptr if not found.
     */
    static juce::XmlElement* getChildByAttribute (juce::XmlElement* xml,
                                                  juce::StringRef attributeName,
                                                  juce::StringRef attributeValue)
    {
        if (xml->getStringAttribute (attributeName).equalsIgnoreCase (attributeValue))
            return xml;

        for (auto* e : xml->getChildIterator())
            if (auto* child { getChildByAttribute (e, attributeName, attributeValue) })
                return child;

        return nullptr;
    }
    
    /// Overload for unique_ptr<XmlElement>.
    static juce::XmlElement* getChildByAttribute (const std::unique_ptr<juce::XmlElement>& xml,
                                                  juce::StringRef attributeName,
                                                  juce::StringRef attributeValue)
    {
        return getChildByAttribute (xml.get(), attributeName, attributeValue);
    }
    
    /**
     * @brief Find a child element by its "id" attribute.
     *
     * @param xml        Pointer to the XmlElement to search.
     * @param idToLookFor Value of the id attribute to match.
     * @return Pointer to the matching XmlElement, or nullptr if not found.
     */
    static juce::XmlElement* getChildByID (juce::XmlElement* xml,
                                           juce::StringRef idToLookFor)
    {
        return getChildByAttribute (xml, IDref::id, idToLookFor);
    }
    
    /// Overload for unique_ptr<XmlElement>.
    static juce::XmlElement* getChildByID (const std::unique_ptr<juce::XmlElement>& xml,
                                           juce::StringRef idToLookFor)
    {
        return getChildByAttribute (xml.get(), IDref::id, idToLookFor);
    }

    /**
     * @brief Find the first child element (recursively) with a given tag name.
     *
     * @param xml              Pointer to the XmlElement to search.
     * @param tagNameToLookFor Tag name to match.
     * @return Pointer to the matching XmlElement, or nullptr if not found.
     */
    static juce::XmlElement* getChildByName (juce::XmlElement* xml,
                                             juce::StringRef tagNameToLookFor)
    {
        if (auto* child { xml->getChildByName (tagNameToLookFor) })
            return child;

        for (auto* e : xml->getChildIterator())
        {
            if (auto* child { getChildByName (e, tagNameToLookFor) })
                return child;
        }

        return nullptr;
    }

    /// Overload for unique_ptr<XmlElement>.
    static juce::XmlElement* getChildByName (const std::unique_ptr<juce::XmlElement>& xml,
                                             juce::StringRef tagNameToLookFor)
    {
        return getChildByName (xml.get(), tagNameToLookFor);
    }

    /**
     * @brief Retrieve a typed attribute value from a nested child element.
     *
     * Looks up xmlElement->getChildByName(name)->getChildByName(childName),
     * then returns the specified property as the requested ValueType.
     *
     * @tparam ValueType One of int, double, juce::String, or bool.
     * @param xmlElement Pointer to the XmlElement root.
     * @param name       Name of the parent element.
     * @param childName  Name of the child element.
     * @param property   Attribute name to retrieve.
     * @return The attribute value converted to ValueType, or default if not found.
     */
    template <typename ValueType>
    static const ValueType get (juce::XmlElement* xmlElement,
                                juce::StringRef name,
                                juce::StringRef childName,
                                juce::StringRef property) noexcept
    {
        if (auto e { xmlElement->getChildByName (name)->getChildByName (childName) })
        {
            if constexpr (std::is_same_v<int, ValueType>)
            {
                ValueType v { e->getIntAttribute (property) };
                return v;
            }
            else if constexpr (std::is_same_v<double, ValueType>)
            {
                ValueType v { e->getDoubleAttribute (property) };
                return v;
            }
            else if constexpr (std::is_same_v<juce::String, ValueType>)
            {
                ValueType v { e->getStringAttribute (property) };
                return v;
            }
            else if constexpr (std::is_same_v<bool, ValueType>)
            {
                ValueType v { e->getBoolAttribute (property) };
                return v;
            }
        }

        return {};
    }

    /// Overload for unique_ptr<XmlElement>.
    template <typename ValueType>
    static const ValueType get (const std::unique_ptr<juce::XmlElement>& xmlElement,
                                juce::StringRef name,
                                juce::StringRef childName,
                                juce::StringRef property) noexcept
    {
        return get<ValueType> (xmlElement.get(), name, childName, property);
    }

    /**
     * @brief Retrieve a typed attribute value from a direct child element.
     *
     * Looks up getChildByName(xmlElement, name), then returns the specified
     * property as the requested ValueType.
     *
     * @tparam ValueType One of int, juce::String, or bool.
     * @param xmlElement Pointer to the XmlElement root.
     * @param name       Name of the child element.
     * @param property   Attribute name to retrieve.
     * @return The attribute value converted to ValueType, or default if not found.
     */
    template <typename ValueType>
    static const ValueType get (juce::XmlElement* xmlElement,
                                juce::StringRef name,
                                juce::StringRef property) noexcept
    {
        if (auto e { getChildByName (xmlElement, name) })
        {
            if constexpr (std::is_same_v<int, ValueType>)
            {
                ValueType v { e->getIntAttribute (property) };
                return v;
            }
            else if constexpr (std::is_same_v<juce::String, ValueType>)
            {
                ValueType v { e->getStringAttribute (property) };
                return v;
            }
            else if constexpr (std::is_same_v<bool, ValueType>)
            {
                ValueType v { e->getBoolAttribute (property) };
                return v;
            }
        }

        return {};
    }

    /// Overload for unique_ptr<XmlElement>.
    template <typename ValueType>
    static const ValueType get (const std::unique_ptr<juce::XmlElement>& xmlElement,
                                juce::StringRef name,
                                juce::StringRef property) noexcept
    {
        return get<ValueType> (xmlElement.get(), name, property);
    }

    //==============================================================================
    /**
     * @brief Load an XML element from a JUCE BinaryData resource.
     *
     * @param resourceFileName Name of the resource file in BinaryData.
     * @return A std::unique_ptr<juce::XmlElement> parsed from the resource.
     *
     * @note Uses BinaryData::Raw to access the embedded resource.
     */
    static const auto getFromBinary (const juce::String& resourceFileName)
    {
        /** fallback if BinaryData namespace use as default to call
         getNamedResourceOriginalFilename (const char*) */
        using namespace BinaryData;
        /*________________________________________________________________________*/

        Raw binary (resourceFileName);

        return juce::parseXML (juce::String::createStringFromData (binary.data, binary.size));
    }
    
    /**
     * @brief Load an XML string from a JUCE BinaryData resource.
     *
     * @param resourceFileName Name of the resource file in BinaryData.
     * @return A juce::String containing the raw XML text.
     */
    static const juce::String getStringFromBinary (const juce::String& resourceFileName)
    {
        /** fallback if BinaryData namespace use as default to call
         getNamedResourceOriginalFilename (const char*) */
        using namespace BinaryData;
        /*________________________________________________________________________*/

        Raw binary (resourceFileName);

        return juce::String::createStringFromData (binary.data, binary.size);
    }
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
