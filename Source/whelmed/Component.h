/**
 * @file Component.h
 * @brief Whelmed markdown viewer pane component.
 *
 * @see Whelmed::Screen
 * @see Whelmed::InputHandler
 * @see PaneComponent
 */

#pragma once
#include <JuceHeader.h>
#include "../component/PaneComponent.h"
#include "State.h"
#include "Screen.h"
#include "../component/LoaderOverlay.h"
#include "Parser.h"
#include "InputHandler.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

/**
 * @class Whelmed::Component
 * @brief PaneComponent subclass that hosts the Whelmed markdown viewer.
 *
 * Owns the document state, parser, Screen, and InputHandler. Implements the
 * full PaneComponent interface (selection, renderer switching, config) so it
 * can occupy any split pane alongside Terminal::Display instances.
 *
 * @note MESSAGE THREAD — all public methods.
 */
class Component
    : public PaneComponent
    , private juce::ValueTree::Listener
{
public:
    Component();
    ~Component() override;

    juce::String getPaneType() const noexcept override { return App::ID::paneTypeDocument; }
    void switchRenderer (App::RendererType type) override;
    void applyConfig() noexcept override;
    void applyZoom (float) noexcept override {}

    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    void openFile (const juce::File& file);
    void enterSelectionMode() noexcept override;
    void copySelection() noexcept override;
    bool hasSelection() const noexcept override;
    juce::ValueTree getValueTree() noexcept override;

private:
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    State docState;
    juce::ValueTree state;
    std::unique_ptr<Parser> parser;

    juce::Viewport viewport;
    Screen screen;
    InputHandler inputHandler { viewport, screen, state };
    ::LoaderOverlay loaderOverlay;

    std::unique_ptr<jam::Mermaid::Parser> mermaidParser;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Component)
};

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
