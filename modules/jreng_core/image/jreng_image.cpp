#include <JuceHeader.h>

namespace jreng
{
/*____________________________________________________________________________*/

const juce::Image Image::getFromBinary (const juce::String& resourceFileName)
{
    using namespace BinaryData;

    Raw binary (resourceFileName);
    return getFrom (binary.data, binary.size);
}

const juce::Image Image::getAltFromBinary (const juce::String& resourceFileName)
{
    using namespace BinaryData;

    juce::String name { jreng::String::getFilenameWithoutExtension (resourceFileName) };
    juce::String extension { jreng::String::onlyExtensionFromFilename (resourceFileName) };
    juce::String altName { jreng::String::toFileName (jreng::String::appendWithUnderscore (name, IDref::alt), extension) };

    Raw binary (altName);
    return binary.exists() ? getFrom (binary.data, binary.size) : getFromBinary (resourceFileName);
}

const juce::Image Image::getFromBinary (const juce::String& resourceName, bool shouldGetAltIfAvailable)
{
    return shouldGetAltIfAvailable ? getAltFromBinary (resourceName) : getFromBinary (resourceName);
}

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
