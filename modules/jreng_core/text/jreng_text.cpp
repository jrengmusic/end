namespace jreng
{
/*____________________________________________________________________________*/
const juce::String URL::getWebsite() noexcept { return "https://www.kuassa.com/"; }
const juce::String URL::getShop() noexcept { return "https://www.kuassa.com/shop"; }
const juce::String Text::getFft() noexcept { return "FFTReal by Laurent de Soras"; }
const juce::String Text::getImportLicense() noexcept { return "Select *.kuassa license file to authorize"; }
const juce::String Text::getSelectDirectory() noexcept { return "Select directory"; }
const juce::String Text::getSelectFile() noexcept { return "Select file"; }
const juce::String Text::getDefaultForNewInstance() noexcept { return "default for new instance"; }

const juce::String Text::getAlertNewerVersionPreset (juce::StringRef projectName) noexcept
{
    return "was made using a newer version of " + projectName
           + "\nTo use this preset correctly, please install the newest version from: \nkuassa.com/downloads";
}

const juce::String Text::getPresetAlreadyExists (juce::StringRef presetFileName) noexcept
{
    return "Preset " + presetFileName + " already exists.";
}

const juce::String Text::getFileAlreadyExists (juce::StringRef fileName) noexcept
{
    return "File " + fileName + " already exists.";
}

const juce::String Text::getAskReplace() noexcept
{
    return "Do you want to replace it?";
}

const juce::String Text::getAlertUserManualNotFound() noexcept
{
    return "User Manual not found";
}

const juce::String Text::getAlertIRNameTooLong() noexcept
{
    return "Impulse Response file name or location is too long";
}

const juce::String Text::getChooseShorterNameOrLocation() noexcept
{
    return "Please choose another location with shorter name or location.";
}

const juce::String Text::getAlertIRNotRecognized() noexcept
{
    return "Impulse Response file format not recognised";
}

const juce::String Text::getChooseAnotherFile() noexcept
{
    return "Please choose another file.";
}

const juce::String Text::getPleaseTryAgain() noexcept
{
    return "Please try again";
}

const juce::String Text::getNoCigar() noexcept
{
    return "Close, but no cigar.";
}

const juce::String Text::getAlertAuthorizationSuccesful() noexcept
{
    return "Auhorization Successful!";
}

const juce::String Text::getRockNRoll (juce::StringRef productName) noexcept
{
    return "Enjoy your " + productName + "\n\nRock 'n Roll!";
}

const juce::String Text::getAuthorizationFailed() noexcept
{
    return "Auhorization Failed";
}

const juce::String Text::getTryAgainWithCorrectLicense (juce::StringRef fileName) noexcept
{
    return "Please try again with the correct license file:\n\n" + fileName + "\n\nFor assistance, contact support@kuassa.com";
}

const juce::String Text::getProductInDemo (juce::StringRef productName) noexcept
{
    return productName + "\n is in demo mode";
}

const juce::String Text::getAlertNoise() noexcept
{
    return "4.44 second noise will be generated \nevery minute until authorized.";
}
/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
