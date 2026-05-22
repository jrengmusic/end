#include "Component.h"

namespace whelmed
{
/*____________________________________________________________________________*/

Component::Component()
    : state { docState.getValueTree() }
{
    state.addListener (this);
    mermaidParser = std::make_unique<jam::Mermaid::Parser>();

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
    viewport.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (viewport);
    addChildComponent (loaderOverlay);
    viewport.addMouseListener (this, true);
    AppState::getContext()->getTabs().addListener (this);
    AppState::getContext()->get().addListener (this);

    if (auto* engine { lua::Engine::getContext() }; engine != nullptr)
        inputHandler.buildKeyMap (engine->getSelectionKeys());

    screen.setStateTree (state);
}

Component::~Component()
{
    AppState::getContext()->get().removeListener (this);
    AppState::getContext()->getTabs().removeListener (this);
    viewport.removeMouseListener (this);
    state.removeListener (this);
}

// ============================================================================
// PaneComponent interface
// ============================================================================

bool Component::keyPressed (const juce::KeyPress& key)
{
    return inputHandler.handleKey (key);
}

void Component::switchRenderer (app::RendererType type) { juce::ignoreUnused (type); }

void Component::applyFromAppState() noexcept
{
    screen.load (docState.getDocument(), std::numeric_limits<int>::max());
    resized();

    if (auto* engine { lua::Engine::getContext() }; engine != nullptr)
        inputHandler.buildKeyMap (engine->getSelectionKeys());
}

// ============================================================================
// juce::Component
// ============================================================================

void Component::paint (juce::Graphics& g)
{
    const juce::Colour bg { lua::Engine::getContext()->whelmed.background };
    g.fillAll (bg);
}

void Component::resized()
{
    const auto* cfg { lua::Engine::getContext() };
    auto contentArea { getLocalBounds() };
    contentArea.removeFromTop    (cfg->whelmed.paddingTop);
    contentArea.removeFromRight  (cfg->whelmed.paddingRight);
    contentArea.removeFromBottom (cfg->whelmed.paddingBottom);
    contentArea.removeFromLeft   (cfg->whelmed.paddingLeft);

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
    auto parsed { jam::Markdown::Parser::parse (fileContent) };
    state.setProperty (app::id::totalBlocks, parsed.blockCount, nullptr);

    // Set ValueTree metadata before moving document
    state.setProperty (app::id::filePath, file.getFullPathName(), nullptr);
    state.setProperty (app::id::displayName, file.getFileNameWithoutExtension(), nullptr);
    state.setProperty (app::id::scrollOffset, 0.0f, nullptr);
    state.setProperty (app::id::blockCount, 0, nullptr);
    state.setProperty (app::id::parseComplete, false, nullptr);

    // Move document to State
    docState.setDocument (std::move (parsed));

    screen.setSize (viewport.getMaximumVisibleWidth(), juce::jmax (1, screen.getHeight()));
    const int initialBatch { screen.load (docState.getDocument(), viewport.getHeight()) };
    docState.setInitialBlockCount (initialBatch);

    const int totalBlocks { static_cast<int> (state.getProperty (app::id::totalBlocks)) };

    if (initialBatch < totalBlocks)
    {
        loaderOverlay.show (totalBlocks);
        loaderOverlay.toFront (false);
    }

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

void Component::mouseDown (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    grabKeyboardFocus();
}

void Component::enterSelectionMode() noexcept
{
    const int firstVisible { screen.getFirstVisibleBlock (viewport.getViewPositionY()) };
    state.setProperty (app::id::selCursorBlock, firstVisible, nullptr);
    state.setProperty (app::id::selCursorChar, 0, nullptr);
    state.setProperty (app::id::selAnchorBlock, firstVisible, nullptr);
    state.setProperty (app::id::selAnchorChar, 0, nullptr);
    AppState::getContext()->setSelectionType (static_cast<int> (SelectionType::none));
    AppState::getContext()->setModalType (static_cast<int> (ModalType::selection));
    inputHandler.reset();
    screen.updateCursor (firstVisible, 0);
    screen.repaint();
}

void Component::copySelection() noexcept
{
    const int anchorBlock { static_cast<int> (state.getProperty (app::id::selAnchorBlock)) };
    const int anchorChar  { static_cast<int> (state.getProperty (app::id::selAnchorChar)) };
    const int cursorBlock { static_cast<int> (state.getProperty (app::id::selCursorBlock)) };
    const int cursorChar  { static_cast<int> (state.getProperty (app::id::selCursorChar)) };

    const bool anchorFirst { anchorBlock < cursorBlock
        or (anchorBlock == cursorBlock and anchorChar <= cursorChar) };
    const int startBlock { anchorFirst ? anchorBlock : cursorBlock };
    const int startChar  { anchorFirst ? anchorChar  : cursorChar };
    const int endBlock   { anchorFirst ? cursorBlock : anchorBlock };
    const int endChar    { anchorFirst ? cursorChar  : anchorChar };

    const juce::String text { screen.extractText (startBlock, startChar, endBlock, endChar) };

    if (text.isNotEmpty())
        juce::SystemClipboard::copyTextToClipboard (text);

    AppState::getContext()->setSelectionType (static_cast<int> (SelectionType::none));
    AppState::getContext()->setModalType (static_cast<int> (ModalType::none));
    screen.hideCursor();
    screen.repaint();
}

bool Component::hasSelection() const noexcept
{
    return AppState::getContext()->getSelectionType() != static_cast<int> (SelectionType::none);
}

juce::ValueTree Component::getValueTree() noexcept { return state; }

// ============================================================================
// Private
// ============================================================================

void Component::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == AppState::getContext()->get())
    {
        applyFromAppState();
    }
    else
    {
        if (property == app::id::blockCount)
        {
            const int blockCount { static_cast<int> (tree.getProperty (app::id::blockCount)) };
            const int blockIndex { blockCount - 1 };

            const int totalBlocks { static_cast<int> (state.getProperty (app::id::totalBlocks)) };

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

        if (property == app::id::modalType)
        {
            const int modal { static_cast<int> (tree.getProperty (app::id::modalType)) };

            if (modal == static_cast<int> (ModalType::none))
            {
                setBufferedToImage (true);
            }
        }

        if (property == app::id::selCursorBlock or property == app::id::selCursorChar
            or property == app::id::selAnchorBlock or property == app::id::selAnchorChar)
        {
            const int anchorBlock { static_cast<int> (state.getProperty (app::id::selAnchorBlock)) };
            const int anchorChar  { static_cast<int> (state.getProperty (app::id::selAnchorChar)) };
            const int cursorBlock { static_cast<int> (state.getProperty (app::id::selCursorBlock)) };
            const int cursorChar  { static_cast<int> (state.getProperty (app::id::selCursorChar)) };

            screen.setSelection (anchorBlock, anchorChar, cursorBlock, cursorChar);
            screen.updateCursor (cursorBlock, cursorChar);
            screen.repaint();

            const auto cursorBounds { screen.getCursorBounds() };

            if (not cursorBounds.isEmpty())
            {
                const int viewTop    { viewport.getViewPositionY() };
                const int viewBottom { viewTop + viewport.getViewHeight() };
                const int cursorTop    { static_cast<int> (cursorBounds.getY()) };
                const int cursorBottom { static_cast<int> (cursorBounds.getBottom()) };

                if (cursorTop < viewTop)
                {
                    viewport.setViewPosition (0, cursorTop);
                }
                else if (cursorBottom > viewBottom)
                {
                    viewport.setViewPosition (0, cursorBottom - viewport.getViewHeight());
                }
            }
        }
    }
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace whelmed
