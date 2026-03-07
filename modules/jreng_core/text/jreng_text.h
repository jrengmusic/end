#pragma once
namespace jreng
{
/*____________________________________________________________________________*/

struct URL
{
    static const juce::String getWebsite() noexcept;
    static const juce::String getShop() noexcept;
};

struct Text
{
    /*____________________________________________________________________________*/

    static const juce::String getFft() noexcept;
    static const juce::String getImportLicense() noexcept;
    static const juce::String getSelectDirectory() noexcept;
    static const juce::String getSelectFile() noexcept;
    static const juce::String getDefaultForNewInstance() noexcept;
    static const juce::String getAlertNewerVersionPreset(juce::StringRef projectName) noexcept;
    static const juce::String getPresetAlreadyExists(juce::StringRef presetFileName) noexcept;
    static const juce::String getFileAlreadyExists(juce::StringRef fileName) noexcept;
    static const juce::String getAskReplace() noexcept;
    static const juce::String getAlertUserManualNotFound() noexcept;
    static const juce::String getAlertIRNameTooLong() noexcept;
    static const juce::String getChooseShorterNameOrLocation() noexcept;
    static const juce::String getAlertIRNotRecognized() noexcept;
    static const juce::String getChooseAnotherFile() noexcept;
    static const juce::String getPleaseTryAgain() noexcept;
    static const juce::String getNoCigar() noexcept;
    static const juce::String getAlertAuthorizationSuccesful() noexcept;
    static const juce::String getRockNRoll(juce::StringRef productName) noexcept;
    static const juce::String getAuthorizationFailed() noexcept;
    static const juce::String getTryAgainWithCorrectLicense(juce::StringRef fileName) noexcept;
    static const juce::String getProductInDemo (juce::StringRef productName) noexcept;
    static const juce::String getAlertNoise() noexcept;
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
