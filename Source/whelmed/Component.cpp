#include <JuceHeader.h>
#include "Component.h"
#include "MermaidSVGParser.h"
#include "../config/WhelmedConfig.h"
#include "../terminal/action/Action.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

Component::Component()
    : state { docState.getValueTree() }
{
    state.addListener (this);
    mermaidParser = std::make_unique<jreng::Mermaid::Parser>();

    mermaidParser->onReady (
        [this]
        {
            const auto& doc { docState.getDocument() };
            const auto mermaidIndices { screen.getMermaidBlockIndices() };

            for (const auto blockIndex : mermaidIndices)
            {
                const auto& block { doc.blocks[blockIndex] };
                juce::String code { juce::String::fromUTF8 (doc.text + block.contentOffset, block.contentLength) };

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
    addAndMakeVisible (loaderOverlay);
}

Component::~Component() { state.removeListener (this); }

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

    auto pendingPrefix { static_cast<juce::juce_wchar> (static_cast<int> (state.getProperty (App::ID::pendingPrefix))) };

    if (pendingPrefix != 0)
    {
        juce::String sequence { juce::String::charToString (pendingPrefix) + juce::String::charToString (keyChar) };
        state.setProperty (App::ID::pendingPrefix, static_cast<int> (juce::juce_wchar { 0 }), nullptr);

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
            state.setProperty (App::ID::pendingPrefix, static_cast<int> (keyChar), nullptr);
            handled = true;
        }
        else if (scrollBottom.length() > 1 and scrollBottom.substring (0, 1) == keyString)
        {
            state.setProperty (App::ID::pendingPrefix, static_cast<int> (keyChar), nullptr);
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
    screen.load (docState.getDocument(), std::numeric_limits<int>::max());
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
    screen.setSize (viewport.getMaximumVisibleWidth(), juce::jmax (1, screen.getHeight()));
    screen.updateLayout();

    static constexpr int progressBarHeight { 24 };
    loaderOverlay.setBounds (0, getHeight() - progressBarHeight, getWidth(), progressBarHeight);
}

// ============================================================================
// Public API
// ============================================================================

void Component::openFile (const juce::File& file)
{
    const juce::String fileContent { file.loadFileAsString() };
    auto parsed { jreng::Markdown::Parser::parse (fileContent) };
    state.setProperty (App::ID::totalBlocks, parsed.blockCount, nullptr);

    // Set ValueTree metadata before moving document
    state.setProperty (App::ID::filePath, file.getFullPathName(), nullptr);
    state.setProperty (App::ID::displayName, file.getFileNameWithoutExtension(), nullptr);
    state.setProperty (App::ID::scrollOffset, 0.0f, nullptr);
    state.setProperty (App::ID::blockCount, 0, nullptr);
    state.setProperty (App::ID::parseComplete, false, nullptr);

    // Move document to State
    docState.setDocument (std::move (parsed));

    screen.setSize (viewport.getMaximumVisibleWidth(), juce::jmax (1, screen.getHeight()));
    const int initialBatch { screen.load (docState.getDocument(), viewport.getHeight()) };
    docState.setInitialBlockCount (initialBatch);

    loaderOverlay.show (static_cast<int> (state.getProperty (App::ID::totalBlocks)));
    loaderOverlay.toFront (false);

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

    parser = std::make_unique<Parser> (docState, initialBatch);
    parser->start();
    repaint();
}

juce::ValueTree Component::getValueTree() noexcept { return state; }

// ============================================================================
// Private
// ============================================================================

void Component::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == App::ID::blockCount)
    {
        const int blockCount { static_cast<int> (tree.getProperty (App::ID::blockCount)) };
        const int blockIndex { blockCount - 1 };

        const int totalBlocks { static_cast<int> (state.getProperty (App::ID::totalBlocks)) };

        if (blockIndex >= 0 and blockIndex < totalBlocks)
        {
            screen.build (blockIndex, docState.getDocument());
            loaderOverlay.update (blockCount, totalBlocks);

            if (blockCount >= totalBlocks)
            {
                loaderOverlay.hide();
                setBufferedToImage (true);
            }
        }
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace Whelmed
