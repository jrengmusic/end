/**
 * @file Component.h
 * @brief Whelmed markdown viewer pane component.
 *
 * @see whelmed::Screen
 * @see whelmed::InputHandler
 * @see PaneView
 */

#pragma once
#include <JuceHeader.h>
#include "../../terminal/component/PaneView.h"
#include "../State.h"
#include "../Screen.h"
#include "../../terminal/component/LoaderOverlay.h"
#include "../Parser.h"
#include "../InputHandler.h"
#include "../MermaidSVGParser.h"
#include "../../lua/Engine.h"
#include "../../AppModel.h"
#include "../../ModalType.h"
#include "../../SelectionType.h"

namespace whelmed
{
/*____________________________________________________________________________*/

/**
 * @class whelmed::Component
 * @brief PaneView subclass that hosts the Whelmed markdown viewer.
 *
 * Owns the document state, parser, Screen, and InputHandler. Implements the
 * full PaneView interface (selection, config) so it can occupy any split
 * pane alongside terminal::Display instances.
 *
 * @note MESSAGE THREAD — all public methods.
 */
class Component
    : public PaneView
    , private juce::ValueTree::Listener
{
public:
    Component();
    ~Component() override;

    juce::String getPaneType() const noexcept override { return Map::PaneType::getContext()->get (Map::PaneType::document); }
    void applyZoom (float) noexcept override {}

    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    void openFile (const juce::File& file);
    void enterSelectionMode() noexcept override;
    void copySelection() noexcept override;
    bool hasSelection() const noexcept override;
    juce::ValueTree getDocumentTree() noexcept { return state; }

    /**
     * @brief Grafts the DOCUMENT tree into the given PANE node via RAII Attachment.
     *
     * The Attachment is owned by this Component and ungrafts automatically on destruction.
     * Panes::closeWhelmed() needs only to destroy the Component — no manual removeChild.
     *
     * @param paneNode  The PANE juce::ValueTree node to graft into. Must be valid.
     * @note MESSAGE THREAD. Called by Panes::createWhelmed after component creation.
     */
    void graftDocumentInto (juce::ValueTree paneNode);

private:
    void applyFromAppModel() noexcept;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief RAII graft of the DOCUMENT tree into the owning PANE node. Created by graftDocumentInto(). */
    std::unique_ptr<jam::ValueTree::Attachment> documentAttachment;

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
} // namespace whelmed
