namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @brief Provides utility functions for handling images.
 */
struct Image
{
    /*____________________________________________________________________________*/

    /**
     * @brief Retrieves an image from memory.
     * @param data Pointer to the binary data.
     * @param size Size of the binary data.
     * @return The image retrieved from memory.
     */
    inline static const auto& getFrom = static_cast<juce::Image (*) (const void*, int)> (juce::ImageCache::getFromMemory);

    /**
     * @brief Retrieves an image from a binary resource file.
     *
     * This function loads an image from a binary resource file using the BinaryData namespace.
     * If the resource exists, it is extracted and converted into a juce::Image.
     *
     * @param resourceFileName The name of the binary resource file.
     * @return A juce::Image created from the binary resource.
     */
    static const juce::Image getFromBinary (const juce::String& resourceName);

    /**
     * @brief Retrieves an alternative version of an image from a binary resource file.
     *
     * This function attempts to load an alternative version of an image. It constructs a modified
     * filename by appending an alternative identifier to the base resource name. If the alternative
     * resource exists, it is extracted and converted into a juce::Image; otherwise, the default image is used.
     *
     * @param resourceFileName The name of the binary resource file.
     * @return A juce::Image created from either the alternative or the default binary resource.
     */
    static const juce::Image getAltFromBinary (const juce::String& resourceName);

    /**
     * @brief Retrieves an image with an optional alternative resource lookup.
     *
     * This function determines whether to load the standard image or its alternative version
     * based on the value of `shouldGetAltIfAvailable`. If true, the alternative image is retrieved;
     * otherwise, the default image is used.
     *
     * @param resourceName The name of the binary resource file.
     * @param shouldGetAltIfAvailable Boolean flag indicating whether to attempt loading an alternative image.
     * @return A juce::Image created from either the default or the alternative binary resource.
     */
    static const juce::Image getFromBinary (const juce::String& resourceName, bool shouldGetAltIfAvailable);

    /**
     * @brief Checks if a given file name is an image file.
     * @param fileName The name of the file to check.
     * @return True if the file is a supported image file format, false otherwise.
     */
    static const bool isImageFile (const juce::String& fileName)
    {
        return fileName.contains (IDref::png) or fileName.contains (IDref::jpg);
    }
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
