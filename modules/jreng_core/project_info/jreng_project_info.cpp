#include <JuceHeader.h>
#if ! JUCE_DONT_DECLARE_PROJECTINFO
namespace jreng
{
/*__________________________________________________________________________________________*/
    const juce::String projectName      { ProjectInfo::projectName };
    const juce::String companyName      { ProjectInfo::companyName };
    const juce::String legalCompanyName { "PT " + companyName + " Teknika" };
    const juce::String versionString    { ProjectInfo::versionString };
/**____________________________________END OF NAMESPACE____________________________________*/
} /** namespace jreng */
#endif
