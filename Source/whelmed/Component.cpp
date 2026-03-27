#include <JuceHeader.h>
#include "Component.h"
#include "MermaidSVGParser.h"
#include "config/Config.h"
#include "../terminal/action/Action.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Component::Component()
{
    mermaidParser = std::make_unique<jreng::Mermaid::Parser>();

    mermaidParser->onReady (
        [this]
        {
            const auto& doc { docState.getDocument() };
            const auto mermaidIndices { screen.getMermaidBlockIndices() };

            for (const auto blockIndex : mermaidIndices)
            {
                const auto& block { doc.blocks[blockIndex] };
                juce::String code { juce::String::fromUTF8 (doc.text + block.contentOffset,
                                                              block.contentLength) };

                mermaidParser->convertToSVG (code,
                                              [this, blockIndex] (const juce::String& svg)
                                              {
                                                  auto result { MermaidSVGParser::parse (svg) };
                                                  screen.updateMermaidBlock (blockIndex, std::move (result));
                                                  screen.repaint();
                                              });
            }
        });

    viewport.setViewedComponent (&screen, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    addAndMakeVisible (spinnerOverlay);
}

Component::~Component() = default;

// ============================================================================
// PaneComponent interface
// ============================================================================

bool Component::keyPressed (const juce::KeyPress& key)
{
    const auto* config { Whelmed::Config::getContext() };
    const int scrollStep { config->getInt (Whelmed::Config::Key::scrollStep) };
    const juce::String scrollDown { config->getString (Whelmed::Config::Key::scrollDown) };
    const juce::String scrollUp { config->getString (Whelmed::Config::Key::scrollUp) };
    const juce::String scrollTop { config->getString (Whelmed::Config::Key::scrollTop) };
    const juce::String scrollBottom { config->getString (Whelmed::Config::Key::scrollBottom) };

    const auto keyChar { key.getTextCharacter() };
    bool handled { false };

    if (pendingPrefix != 0)
    {
        juce::String sequence { juce::String::charToString (pendingPrefix) + juce::String::charToString (keyChar) };
        pendingPrefix = 0;

        if (sequence == scrollTop)
        {
            viewport.setViewPosition (0, 0);
            handled = true;
        }
        else if (sequence == scrollBottom)
        {
            viewport.setViewPosition (0, screen.getHeight() - viewport.getHeight());
            handled = true;
        }
    }

    if (not handled)
    {
        const juce::String keyString { juce::String::charToString (keyChar) };

        if (scrollTop.length() > 1 and scrollTop.substring (0, 1) == keyString)
        {
            pendingPrefix = keyChar;
            handled = true;
        }
        else if (scrollBottom.length() > 1 and scrollBottom.substring (0, 1) == keyString)
        {
            pendingPrefix = keyChar;
            handled = true;
        }
        else if (keyString == scrollDown)
        {
            viewport.setViewPosition (0, viewport.getViewPositionY() + scrollStep);
            handled = true;
        }
        else if (keyString == scrollUp)
        {
            viewport.setViewPosition (0, juce::jmax (0, viewport.getViewPositionY() - scrollStep));
            handled = true;
        }
        else if (keyString == scrollBottom)
        {
            viewport.setViewPosition (0, screen.getHeight() - viewport.getHeight());
            handled = true;
        }
    }

    if (not handled)
        handled = Terminal::Action::getContext()->handleKeyPress (key);

    return handled;
}

void Component::switchRenderer (PaneComponent::RendererType type) { juce::ignoreUnused (type); }

void Component::applyConfig() noexcept
{
    buildDocConfig();
    screen.buildFromDocument (docState.getDocument(), docConfig);
    resized();
}

// ============================================================================
// juce::Component
// ============================================================================

void Component::paint (juce::Graphics& g)
{
    const juce::Colour bg { Whelmed::Config::getContext()->getColour (Whelmed::Config::Key::background) };
    g.fillAll (bg);
}

void Component::resized()
{
    const auto* config { Whelmed::Config::getContext() };
    auto contentArea { getLocalBounds() };
    contentArea.removeFromTop (config->getInt (Whelmed::Config::Key::paddingTop));
    contentArea.removeFromRight (config->getInt (Whelmed::Config::Key::paddingRight));
    contentArea.removeFromBottom (config->getInt (Whelmed::Config::Key::paddingBottom));
    contentArea.removeFromLeft (config->getInt (Whelmed::Config::Key::paddingLeft));

    viewport.setBounds (contentArea);
    static constexpr int progressBarHeight { 24 };
    spinnerOverlay.setBounds (0, getHeight() - progressBarHeight, getWidth(), progressBarHeight);

    screen.relayout (viewport.getMaximumVisibleWidth());
}

// ============================================================================
// Public API
// ============================================================================

void Component::openFile (const juce::File& file)
{
    buildDocConfig();

    const juce::String fileContent { file.loadFileAsString() };
    auto parsed { jreng::Markdown::Parser::parse (fileContent) };
    totalBlocks = parsed.blockCount;

    // Store document
    docState.setDocument (std::move (parsed));

    // Build screen from document — styles resolved inline, layout computed
    screen.buildFromDocument (docState.getDocument(), docConfig);

    // Set ValueTree metadata
    juce::ValueTree tree { docState.getValueTree() };
    tree.setProperty (App::ID::filePath, file.getFullPathName(), nullptr);
    tree.setProperty (App::ID::displayName, file.getFileNameWithoutExtension(), nullptr);
    tree.setProperty (App::ID::scrollOffset, 0.0f, nullptr);

    // Show spinner for mermaid blocks (if any)
    const auto mermaidIndices { screen.getMermaidBlockIndices() };

    if (mermaidIndices.size() > 0)
    {
        spinnerOverlay.show (mermaidIndices.size());
        spinnerOverlay.toFront (false);
    }

    setBufferedToImage (true);
    resized();
}

juce::ValueTree Component::getValueTree() noexcept { return docState.getValueTree(); }

// ============================================================================
// Private
// ============================================================================

void Component::buildDocConfig()
{
    const auto* config { Whelmed::Config::getContext() };
    docConfig.bodyFamily = config->getString (Whelmed::Config::Key::fontFamily);
    docConfig.bodySize = config->getFloat (Whelmed::Config::Key::fontSize);
    docConfig.codeFamily = config->getString (Whelmed::Config::Key::codeFamily);
    docConfig.codeSize = config->getFloat (Whelmed::Config::Key::codeSize);
    docConfig.h1Size = config->getFloat (Whelmed::Config::Key::h1Size);
    docConfig.h2Size = config->getFloat (Whelmed::Config::Key::h2Size);
    docConfig.h3Size = config->getFloat (Whelmed::Config::Key::h3Size);
    docConfig.h4Size = config->getFloat (Whelmed::Config::Key::h4Size);
    docConfig.h5Size = config->getFloat (Whelmed::Config::Key::h5Size);
    docConfig.h6Size = config->getFloat (Whelmed::Config::Key::h6Size);
    docConfig.bodyColour = config->getColour (Whelmed::Config::Key::bodyColour);
    docConfig.codeColour = config->getColour (Whelmed::Config::Key::codeColour);
    docConfig.linkColour = config->getColour (Whelmed::Config::Key::linkColour);
    docConfig.h1Colour = config->getColour (Whelmed::Config::Key::h1Colour);
    docConfig.h2Colour = config->getColour (Whelmed::Config::Key::h2Colour);
    docConfig.h3Colour = config->getColour (Whelmed::Config::Key::h3Colour);
    docConfig.h4Colour = config->getColour (Whelmed::Config::Key::h4Colour);
    docConfig.h5Colour = config->getColour (Whelmed::Config::Key::h5Colour);
    docConfig.h6Colour = config->getColour (Whelmed::Config::Key::h6Colour);
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
