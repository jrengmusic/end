namespace BinaryData
{
/*____________________________________________________________________________*/

/**
 * @brief Wrapper for accessing JUCE BinaryData resources by original filename.
 *
 * The Raw struct provides a convenient way to fetch binary resources
 * (such as fonts, images, or XML files) that have been embedded into
 * JUCE's BinaryData system. It resolves the original filename to the
 * corresponding resource data and size.
 */
struct Raw
{
    /**
     * @brief Construct a Raw object from a C‑string filename.
     *
     * Attempts to locate the resource with the given filename in the
     * BinaryData arrays. If found, @c data and @c size are set accordingly.
     *
     * @param fileToFind The original filename of the resource to locate.
     */
    explicit Raw (const char* fileToFind);

    /**
     * @brief Construct a Raw object from a JUCE String filename.
     *
     * Convenience overload that forwards to the const char* constructor.
     *
     * @param fileToFind The original filename of the resource to locate.
     */
    explicit Raw (const juce::String& fileToFind);

    /**
     * @brief Check whether the resource was successfully found.
     *
     * @return @c true if the resource exists and @c data is valid,
     *         @c false otherwise.
     */
    bool exists() const noexcept;

    /** Pointer to the resource data, or nullptr if not found. */
    const char* data { nullptr };

    /** Size of the resource in bytes, or 0 if not found. */
    int size { 0 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Raw)
};

/**
 * @brief Resource fetcher function compatible with Style::Manager.
 *
 * This function constructs a BinaryData::Raw object for the given filename
 * and returns its data pointer and size as a pair. It can be passed directly
 * as a ResourceFetcher callback.
 *
 * ### Example
 * @code
 * // Load a stylesheet embedded in JUCE's BinaryData
 * auto style = jreng::Style::Manager::createFromXml (
 *     jreng::XML::getFromBinary ("CustomStyleSheet.xml"),
 *     BinaryData::fetcher
 * );
 *
 * // Fetch a font resource directly with structured binding
 * if (auto [data, size] = BinaryData::fetcher ("OpenSans-Regular.ttf"); data != nullptr)
 * {
 *     juce::Font myFont (juce::Typeface::createSystemTypefaceFor (data, size));
 *     // use myFont...
 * }
 * @endcode
 *
 * @note If the resource is not found, this function returns {nullptr, 0}.
 *       This allows safe use without risk of crashes.
 *
 * @param filenameUTF8 The original filename of the resource to fetch.
 * @return A pair containing the resource data pointer and its size in bytes.
 *         If the resource is not found, the pointer will be nullptr and size 0.
 */
inline static std::pair<const void*, int> fetcher (const char* filenameUTF8)
{
    BinaryData::Raw r (filenameUTF8);
    return { r.data, r.size };
}

static const juce::String getString (const juce::String& resourceFileName)
{
    using namespace BinaryData;

    Raw binary (resourceFileName);

    return juce::String::createStringFromData (binary.data, binary.size);
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace BinaryData
