/**
 * @file InputHandler.cpp
 * @brief Implementation of keyboard input routing for a Whelmed document pane.
 *
 * @see InputHandler.h
 */

#include "InputHandler.h"
#include "Screen.h"
#include "../config/WhelmedConfig.h"
#include "../action/Action.h"
#include "../AppIdentifier.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

void InputHandler::buildKeyMap (const Scripting::Engine::SelectionKeys& keys) noexcept
{
    selectionKeys.up          = keys.up;
    selectionKeys.down        = keys.down;
    selectionKeys.left        = keys.left;
    selectionKeys.right       = keys.right;
    selectionKeys.visual      = keys.visual;
    selectionKeys.visualLine  = keys.visualLine;
    selectionKeys.visualBlock = keys.visualBlock;
    selectionKeys.copy        = keys.copy;
    selectionKeys.globalCopy  = keys.globalCopy;
    selectionKeys.top         = keys.top;
    selectionKeys.bottom      = keys.bottom;
    selectionKeys.lineStart   = keys.lineStart;
    selectionKeys.lineEnd     = keys.lineEnd;
    selectionKeys.exit        = keys.exit;
}

bool InputHandler::handleKey (const juce::KeyPress& key) noexcept
{
    const auto modal { static_cast<ModalType> (AppState::getContext()->getModalType()) };
    bool handled { false };

    if (modal == ModalType::selection)
    {
        if (handleCursorMovement (key))
        {
            screen.repaint();
            handled = true;
        }

        if (not handled and handleSelectionToggle (key))
        {
            handled = true;
        }

        if (not handled)
        {
            handled = true;  // Selection mode is fully modal — consume all keys
        }
    }

    if (not handled)
    {
        // Handle copy keys when mouse selection is active (no modal)
        const auto selType { static_cast<SelectionType> (AppState::getContext()->getSelectionType()) };

        if (selType != SelectionType::none
            and (key == selectionKeys.copy or key == selectionKeys.globalCopy))
        {
            copyAndClearSelection();
            handled = true;
        }
    }

    if (not handled)
    {
        handled = Action::Registry::getContext()->handleKeyPress (key);
    }

    if (not handled)
    {
        handled = handleNavigation (key);
    }

    return handled;
}

void InputHandler::reset() noexcept
{
    pendingG = false;
    pendingPrefix = 0;
    preferredX = 0.0f;
}

bool InputHandler::handleNavigation (const juce::KeyPress& key) noexcept
{
    const auto* config { Whelmed::Config::getContext() };
    const int scrollStep   { config->getInt (Whelmed::Config::Key::scrollStep) };
    const juce::String scrollDown   { config->getString (Whelmed::Config::Key::scrollDown) };
    const juce::String scrollUp     { config->getString (Whelmed::Config::Key::scrollUp) };
    const juce::String scrollTop    { config->getString (Whelmed::Config::Key::scrollTop) };
    const juce::String scrollBottom { config->getString (Whelmed::Config::Key::scrollBottom) };

    const auto keyChar { key.getTextCharacter() };
    bool handled { false };

    if (pendingPrefix != 0)
    {
        juce::String sequence { juce::String::charToString (pendingPrefix)
                                + juce::String::charToString (keyChar) };
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
    }

    return handled;
}

bool InputHandler::handleCursorMovement (const juce::KeyPress& key) noexcept
{
    const int cursorBlock { static_cast<int> (state.getProperty (App::ID::selCursorBlock)) };
    const int cursorChar  { static_cast<int> (state.getProperty (App::ID::selCursorChar)) };
    const int totalBlocks { static_cast<int> (state.getProperty (App::ID::totalBlocks)) };
    const int maxBlock { juce::jmax (0, totalBlocks - 1) };

    bool consumed { false };
    Cursor::Position newPos { cursorBlock, cursorChar };
    bool updatePreferredX { false };

    if (key == selectionKeys.left)
    {
        const int prevLen { cursorBlock > 0 ? screen.getBlockTextLength (cursorBlock - 1) : 0 };
        newPos = Cursor::moveLeft (cursorBlock, cursorChar, prevLen);
        updatePreferredX = true;
        consumed = true;
    }
    else if (key == selectionKeys.right)
    {
        const int blockLen { screen.getBlockTextLength (cursorBlock) };
        newPos = Cursor::moveRight (cursorBlock, cursorChar, blockLen, maxBlock);
        updatePreferredX = true;
        consumed = true;
    }
    else if (key == selectionKeys.up)
    {
        const int currentLine { screen.getBlockLineForChar (cursorBlock, cursorChar) };
        const int charForPrevLine { currentLine > 0
            ? screen.getBlockCharForLine (cursorBlock, currentLine - 1, preferredX)
            : 0 };
        const int prevBlock { cursorBlock - 1 };
        const int charForPrevBlockLastLine { cursorBlock > 0
            ? screen.getBlockCharForLine (prevBlock, screen.getBlockLineCount (prevBlock) - 1, preferredX)
            : 0 };

        newPos = Cursor::moveUp (cursorBlock, currentLine,
                                 charForPrevLine,
                                 prevBlock, charForPrevBlockLastLine);
        consumed = true;
    }
    else if (key == selectionKeys.down)
    {
        const int currentLine { screen.getBlockLineForChar (cursorBlock, cursorChar) };
        const int lineCount { screen.getBlockLineCount (cursorBlock) };
        const int charForNextLine { currentLine < lineCount - 1
            ? screen.getBlockCharForLine (cursorBlock, currentLine + 1, preferredX)
            : 0 };
        const int nextBlock { cursorBlock + 1 };
        const int charForNextBlockFirstLine { cursorBlock < maxBlock
            ? screen.getBlockCharForLine (nextBlock, 0, preferredX)
            : 0 };

        newPos = Cursor::moveDown (cursorBlock, currentLine, lineCount,
                                   charForNextLine,
                                   nextBlock, charForNextBlockFirstLine,
                                   maxBlock);
        consumed = true;
    }
    else if (key == selectionKeys.top)
    {
        if (pendingG)
        {
            newPos = Cursor::moveToTop();
            pendingG = false;
            updatePreferredX = true;
        }
        else
        {
            pendingG = true;
        }

        consumed = true;
    }
    else if (key == selectionKeys.bottom)
    {
        const int lastLen { screen.getBlockTextLength (maxBlock) };
        newPos = Cursor::moveToBottom (maxBlock, lastLen);
        updatePreferredX = true;
        consumed = true;
    }
    else if (key == selectionKeys.lineStart)
    {
        const int currentLine { screen.getBlockLineForChar (cursorBlock, cursorChar) };
        const auto range { screen.getBlockLineCharRange (cursorBlock, currentLine) };
        newPos = Cursor::moveToLineStart (cursorBlock, range.getStart());
        updatePreferredX = true;
        consumed = true;
    }
    else if (key == selectionKeys.lineEnd)
    {
        const int currentLine { screen.getBlockLineForChar (cursorBlock, cursorChar) };
        const auto range { screen.getBlockLineCharRange (cursorBlock, currentLine) };
        newPos = Cursor::moveToLineEnd (cursorBlock, juce::jmax (range.getStart(), range.getEnd() - 1));
        updatePreferredX = true;
        consumed = true;
    }

    if (consumed and (newPos.block != cursorBlock or newPos.character != cursorChar))
    {
        state.setProperty (App::ID::selCursorBlock, newPos.block, nullptr);
        state.setProperty (App::ID::selCursorChar, newPos.character, nullptr);

        if (updatePreferredX)
            preferredX = screen.getBlockCharX (newPos.block, newPos.character);

        screen.repaint();
    }

    return consumed;
}

void InputHandler::toggleSelectionType (SelectionType target) noexcept
{
    auto* appState { AppState::getContext() };
    const auto current { static_cast<SelectionType> (appState->getSelectionType()) };
    const int cursorBlock { static_cast<int> (state.getProperty (App::ID::selCursorBlock)) };
    const int cursorChar  { static_cast<int> (state.getProperty (App::ID::selCursorChar)) };

    if (current == target)
    {
        appState->setSelectionType (static_cast<int> (SelectionType::none));
        appState->setModalType (static_cast<int> (ModalType::none));
        screen.hideCursor();
    }
    else
    {
        state.setProperty (App::ID::selAnchorBlock, cursorBlock, nullptr);
        state.setProperty (App::ID::selAnchorChar, cursorChar, nullptr);
        appState->setSelectionType (static_cast<int> (target));
        appState->setModalType (static_cast<int> (ModalType::selection));
    }
}

void InputHandler::copyAndClearSelection() noexcept
{
    const int cursorBlock { static_cast<int> (state.getProperty (App::ID::selCursorBlock)) };
    const int cursorChar  { static_cast<int> (state.getProperty (App::ID::selCursorChar)) };
    const int anchorBlock { static_cast<int> (state.getProperty (App::ID::selAnchorBlock)) };
    const int anchorChar  { static_cast<int> (state.getProperty (App::ID::selAnchorChar)) };

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

bool InputHandler::handleSelectionToggle (const juce::KeyPress& key) noexcept
{
    auto* appState { AppState::getContext() };
    bool consumed { false };

    if (key == selectionKeys.visual)
    {
        toggleSelectionType (SelectionType::visual);
        consumed = true;
    }
    else if (key == selectionKeys.visualLine)
    {
        toggleSelectionType (SelectionType::visualLine);
        consumed = true;
    }
    else if (key == selectionKeys.visualBlock)
    {
        toggleSelectionType (SelectionType::visualBlock);
        consumed = true;
    }
    else if (key == selectionKeys.exit)
    {
        appState->setSelectionType (static_cast<int> (SelectionType::none));
        appState->setModalType (static_cast<int> (ModalType::none));
        screen.hideCursor();
        consumed = true;
    }
    else if (key == selectionKeys.copy or key == selectionKeys.globalCopy)
    {
        const auto selType { static_cast<SelectionType> (appState->getSelectionType()) };

        if (selType != SelectionType::none)
        {
            copyAndClearSelection();
            pendingG = false;
        }
        else
        {
            appState->setSelectionType (static_cast<int> (SelectionType::none));
            appState->setModalType (static_cast<int> (ModalType::none));
            screen.hideCursor();
        }

        consumed = true;
    }

    if (consumed)
        screen.repaint();

    return consumed;
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
