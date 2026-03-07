#include <JuceHeader.h>
/**
 * @brief Provides a namespace fallback for binary data resources.
 *
 * This namespace defines fallback mechanisms for accessing binary resources,
 * including the size of the resource list, resource fetching methods, and original filenames.
 */
namespace BinaryDataFallbacks
/* ____________________________________________________________________________*/
{
const int namedResourceListSize { 0 };///< Size of the named resource list.
const char** namedResourceList { nullptr };///< Pointer to the list of named resources.

/**
     * @brief Retrieves a named resource by name and size.
     *
     * @param resourceName The name of the resource.
     * @param size The size of the resource.
     * @return A pointer to the resource data, or nullptr if not found.
     */
const char* getNamedResource (const char* resourceName, int& size) { return nullptr; }

/**
     * @brief Retrieves the original filename of a named resource.
     *
     * @param resourceName The name of the resource.
     * @return The original filename, or nullptr if not found.
     */
const char* getNamedResourceOriginalFilename (const char* resourceName) { return nullptr; }
/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace BinaryDataFallbacks */

/**
 * @brief Binary data namespace that uses fallback mechanisms for resource access.
 */
namespace BinaryData
{
using namespace BinaryDataFallbacks;

/**
     * @brief Represents a binary data object.
     *
     * The Raw class provides functionality to fetch binary data based on filenames.
     */
Raw::Raw (const char* fileToFind)
{
    for (int index = 0; index < namedResourceListSize; ++index)
    {
        auto binaryName { namedResourceList[index] };
        auto fileName { getNamedResourceOriginalFilename (binaryName) };

        if (not strcmp (fileName, fileToFind))
        {
            data = getNamedResource (binaryName, size);
            break;
        }
    }

    // assert (not (data == nullptr)); // File not found
}

/**
     * @brief Checks if the binary data exists.
     * @return True if data exists, false otherwise.
     */
bool Raw::exists() const noexcept
{
    return not (data == nullptr);
}

/**
     * @brief Constructs a Raw object using a JUCE StringRef as the filename.
     *
     * @param fileToFind The JUCE String representing the filename.
     */
Raw::Raw (const juce::String& fileToFind)
    : Raw (static_cast<const char*> (fileToFind.toUTF8()))
{
}
/**_____________________________END OF NAMESPACE______________________________*/
}// namespace BinaryData
