#include "Main.h"

ENDApplication::ENDApplication() {}
void ENDApplication::initialise (const juce::String& commandLine) {}
void ENDApplication::shutdown() {}
void ENDApplication::systemRequestedQuit() { quit(); }
const juce::String ENDApplication::getApplicationName() { return ProjectInfo::projectName; }
const juce::String ENDApplication::getApplicationVersion() { return ProjectInfo::versionString; }
bool ENDApplication::moreThanOneInstanceAllowed() { return true; }
//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (ENDApplication)
