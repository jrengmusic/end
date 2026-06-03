/**
 * @file ModalWindow.cpp
 * @brief terminal::ModalWindow — thin wrapper over jam::ModalWindow that adds
 *        border paint, keyPressed override, cornerSize constants, and
 *        terminal::Display::onRepaintNeeded wiring.  No renderer or shared context.
 *
 * @see jam::ModalWindow
 */

#include "ModalWindow.h"
#include "../../AppModel.h"

namespace terminal
{
/*____________________________________________________________________________*/

//==============================================================================
// ModalWindow
//==============================================================================

ModalWindow::ModalWindow (std::unique_ptr<juce::Component> content,
                          juce::Component& centreAround,
                          std::function<void()> dismissCallback)
    : jam::ModalWindow (std::move (content),
                        centreAround,
                        std::move (dismissCallback),
                        AppModel::getContext()->getValue<float> (app::id::DISPLAY_LUA, app::id::windowOpacity),
                        AppModel::getContext()->getValue<int> (app::id::DISPLAY_LUA, app::id::windowBlurRadius))
{
    // No onRepaintNeeded wiring needed — terminal repaints via JUCE's normal
    // component repaint mechanism.
}

void ModalWindow::paint (juce::Graphics& g)
{
    const auto* appState { AppModel::getContext() };
    const auto borderWidth { appState->getValue<float> (app::id::DISPLAY_LUA, app::id::popupBorderWidth) };

    g.setColour (juce::Colour (static_cast<juce::uint32> (appState->getValue<int> (app::id::DISPLAY_LUA, app::id::popupBorderColour))));
    g.drawRoundedRectangle (getLocalBounds().reduced (static_cast<int> (2.0f * borderWidth)).toFloat(), cornerSize, borderWidth);
}

bool ModalWindow::keyPressed (const juce::KeyPress&) { return false; }

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
