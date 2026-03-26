/**
 * @file InputHandler.cpp
 * @brief Implementation of keyboard input routing for a terminal session.
 *
 * @see InputHandler.h
 */

#include "InputHandler.h"
#include "../terminal/logic/Session.h"
#include "../terminal/rendering/Screen.h"
#include "../terminal/selection/LinkManager.h"
#include "../terminal/action/Action.h"
#include "../config/Config.h"

InputHandler::InputHandler (Terminal::Session& s,
                            Terminal::ScreenBase& sc,
                            Terminal::LinkManager& lm) noexcept
    : session (s)
    , screen (sc)
    , linkManager (lm)
{
}

bool InputHandler::handleKey (const juce::KeyPress& key, bool isPopupTerminal) noexcept
{
    if (isPopupTerminal)
    {
        session.handleKeyPress (key);
        return true;
    }

    if (session.getState().isModal())
    {
        if (handleModalKey (key))
            return true;
    }

    bool handled { Terminal::Action::getContext()->handleKeyPress (key) };

    if (not handled)
    {
        const int code { key.getKeyCode() };
        const auto mods { key.getModifiers() };

        const bool isScrollNav { mods.isShiftDown() and not mods.isCommandDown()
                                 and (code == juce::KeyPress::pageUpKey or code == juce::KeyPress::pageDownKey
                                      or code == juce::KeyPress::homeKey or code == juce::KeyPress::endKey) };

        if (isScrollNav)
        {
            handleScrollNav (code, [this] (int offset)
            {
                setScrollOffsetClamped (offset);
            });

            handled = true;
        }
        else
        {
            clearSelectionAndScroll();
            session.handleKeyPress (key);
            handled = true;
        }
    }

    return handled;
}

void InputHandler::handleScrollNav (int code,
                                    std::function<void (int)> newOffsetFn) noexcept
{
    const int page { screen.getNumRows() };
    const int current { session.getState().getScrollOffset() };

    if (code == juce::KeyPress::pageUpKey)
    {
        newOffsetFn (current + page);
    }
    else if (code == juce::KeyPress::pageDownKey)
    {
        newOffsetFn (current - page);
    }
    else if (code == juce::KeyPress::homeKey)
    {
        newOffsetFn (session.getGrid().getScrollbackUsed());
    }
    else if (code == juce::KeyPress::endKey)
    {
        newOffsetFn (0);
    }
}

void InputHandler::clearSelectionAndScroll() noexcept
{
    if (session.getState().isDragActive()
        or session.getState().getSelectionType() != static_cast<int> (Terminal::SelectionType::none))
    {
        session.getState().setDragActive (false);
        session.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
    }

    if (session.getState().getScrollOffset() > 0)
    {
        session.getState().setScrollOffset (0);
    }
}

void InputHandler::buildKeyMap() noexcept
{
    auto* cfg { Config::getContext() };

    selectionKeys.up = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionUp));
    selectionKeys.down = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionDown));
    selectionKeys.left = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionLeft));
    selectionKeys.right = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionRight));
    selectionKeys.visual = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionVisual));
    selectionKeys.visualLine = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionVisualLine));

    // Visual block uses real Ctrl (not Cmd on macOS).  parseShortcut maps
    // "ctrl" → commandModifier on macOS, which conflicts with paste.  We
    // build the KeyPress directly with ctrlModifier so Ctrl+V is unambiguous
    // on all platforms.  The config value is still read so users can remap.
    {
        const juce::String raw { cfg->getString (Config::Key::keysSelectionVisualBlock) };
        const bool hasCtrl { raw.containsIgnoreCase ("ctrl") and not raw.containsIgnoreCase ("cmd") };

        if (hasCtrl)
        {
            const int vChar { raw.toLowerCase().contains ("v") ? 'v' : 'v' };
            selectionKeys.visualBlock = juce::KeyPress (vChar, juce::ModifierKeys::ctrlModifier, 0);
        }
        else
        {
            selectionKeys.visualBlock = Terminal::Action::parseShortcut (raw);
        }
    }

    selectionKeys.copy = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionCopy));
    selectionKeys.globalCopy = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysCopy));
    selectionKeys.top = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionTop));
    selectionKeys.bottom = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionBottom));
    selectionKeys.lineStart = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionLineStart));
    selectionKeys.lineEnd = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionLineEnd));
    selectionKeys.exit = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysSelectionExit));

    openFileNextPage = Terminal::Action::parseShortcut (cfg->getString (Config::Key::keysOpenFileNextPage));
}

void InputHandler::reset() noexcept
{
    pendingG = false;
}

bool InputHandler::handleModalKey (const juce::KeyPress& key) noexcept
{
    const auto type { session.getState().getModalType() };

    if (type == Terminal::ModalType::selection)
        return handleSelectionKey (key);

    if (type == Terminal::ModalType::openFile)
        return handleOpenFileKey (key);

    return false;
}

bool InputHandler::handleSelectionKey (const juce::KeyPress& key) noexcept
{
    const int maxRow { session.getGrid().getVisibleRows() + session.getGrid().getScrollbackUsed() - 1 };
    const int maxCol { session.getGrid().getCols() - 1 };

    auto& st { session.getState() };

    bool consumed { false };

    if (key == selectionKeys.exit)
    {
        st.setSelectionType (static_cast<int> (Terminal::SelectionType::none));
        st.setModalType (Terminal::ModalType::none);
        pendingG = false;
        st.setDragActive (false);
        consumed = true;
    }
    else if (key == selectionKeys.visualBlock)
    {
        const auto current { static_cast<Terminal::SelectionType> (st.getSelectionType()) };

        if (current == Terminal::SelectionType::visualBlock)
        {
            st.setSelectionType (static_cast<int> (Terminal::SelectionType::none));
            st.setModalType (Terminal::ModalType::none);
        }
        else
        {
            st.setSelectionAnchor (st.getSelectionCursorRow(), st.getSelectionCursorCol());
            st.setSelectionType (static_cast<int> (Terminal::SelectionType::visualBlock));
        }

        consumed = true;
    }
    else if (key == selectionKeys.left)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(),
                               std::max (0, st.getSelectionCursorCol() - 1));
        consumed = true;
    }
    else if (key == selectionKeys.down)
    {
        st.setSelectionCursor (std::min (maxRow, st.getSelectionCursorRow() + 1),
                               st.getSelectionCursorCol());
        consumed = true;
    }
    else if (key == selectionKeys.up)
    {
        st.setSelectionCursor (std::max (0, st.getSelectionCursorRow() - 1),
                               st.getSelectionCursorCol());
        consumed = true;
    }
    else if (key == selectionKeys.right)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(),
                               std::min (maxCol, st.getSelectionCursorCol() + 1));
        consumed = true;
    }
    else if (key == selectionKeys.visualLine)
    {
        const auto current { static_cast<Terminal::SelectionType> (st.getSelectionType()) };

        if (current == Terminal::SelectionType::visualLine)
        {
            st.setSelectionType (static_cast<int> (Terminal::SelectionType::none));
            st.setModalType (Terminal::ModalType::none);
        }
        else
        {
            st.setSelectionAnchor (st.getSelectionCursorRow(), st.getSelectionCursorCol());
            st.setSelectionType (static_cast<int> (Terminal::SelectionType::visualLine));
        }

        consumed = true;
    }
    else if (key == selectionKeys.visual)
    {
        const auto current { static_cast<Terminal::SelectionType> (st.getSelectionType()) };

        if (current == Terminal::SelectionType::visual)
        {
            st.setSelectionType (static_cast<int> (Terminal::SelectionType::none));
            st.setModalType (Terminal::ModalType::none);
        }
        else
        {
            st.setSelectionAnchor (st.getSelectionCursorRow(), st.getSelectionCursorCol());
            st.setSelectionType (static_cast<int> (Terminal::SelectionType::visual));
        }

        consumed = true;
    }
    else if (key == selectionKeys.copy or key == selectionKeys.globalCopy)
    {
        const auto smType { static_cast<Terminal::SelectionType> (st.getSelectionType()) };

        if (smType != Terminal::SelectionType::none)
        {
            const juce::ScopedTryLock tryLock (session.getGrid().getResizeLock());

            if (tryLock.isLocked())
            {
                const int scrollback { session.getGrid().getScrollbackUsed() };
                const int scrollOffset { st.getScrollOffset() };
                const int visibleStart { scrollback - scrollOffset };
                const int cols { session.getGrid().getCols() };

                const int anchorVisRow { st.getSelectionAnchorRow() - visibleStart };
                const int cursorVisRow { st.getSelectionCursorRow() - visibleStart };
                const int anchorCol { st.getSelectionAnchorCol() };
                const int cursorCol { st.getSelectionCursorCol() };

                juce::String text;

                if (smType == Terminal::SelectionType::visual)
                {
                    const juce::Point<int> start { anchorCol, anchorVisRow };
                    const juce::Point<int> end { cursorCol, cursorVisRow };
                    text = session.getGrid().extractText (start, end);
                }
                else if (smType == Terminal::SelectionType::visualLine)
                {
                    const juce::Point<int> start { 0, std::min (anchorVisRow, cursorVisRow) };
                    const juce::Point<int> end { cols - 1, std::max (anchorVisRow, cursorVisRow) };
                    text = session.getGrid().extractText (start, end);
                }
                else
                {
                    const juce::Point<int> topLeft {
                        std::min (anchorCol, cursorCol),
                        std::min (anchorVisRow, cursorVisRow)
                    };
                    const juce::Point<int> bottomRight {
                        std::max (anchorCol, cursorCol),
                        std::max (anchorVisRow, cursorVisRow)
                    };
                    text = session.getGrid().extractBoxText (topLeft, bottomRight);
                }

                juce::SystemClipboard::copyTextToClipboard (text);
            }
        }

        st.setSelectionType (static_cast<int> (Terminal::SelectionType::none));
        st.setModalType (Terminal::ModalType::none);
        st.setDragActive (false);
        pendingG = false;
        consumed = true;
    }
    else if (key == selectionKeys.bottom)
    {
        st.setSelectionCursor (maxRow, st.getSelectionCursorCol());
        consumed = true;
    }
    else if (key == selectionKeys.top)
    {
        if (pendingG)
        {
            st.setSelectionCursor (0, 0);
            pendingG = false;
        }
        else
        {
            pendingG = true;
        }

        consumed = true;
    }
    else if (key == selectionKeys.lineStart)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(), 0);
        consumed = true;
    }
    else if (key == selectionKeys.lineEnd)
    {
        st.setSelectionCursor (st.getSelectionCursorRow(), maxCol);
        consumed = true;
    }

    if (consumed)
    {
        const int cursorRow { st.getSelectionCursorRow() };
        const int visibleRows { session.getGrid().getVisibleRows() };
        const int scrollback { session.getGrid().getScrollbackUsed() };
        const int visibleStart { scrollback - st.getScrollOffset() };
        const int visibleEnd { visibleStart + visibleRows - 1 };

        if (cursorRow < visibleStart)
        {
            setScrollOffsetClamped (scrollback - cursorRow);
        }
        else if (cursorRow > visibleEnd)
        {
            setScrollOffsetClamped (scrollback - (cursorRow - visibleRows + 1));
        }

    }

    return true;
}

bool InputHandler::handleOpenFileKey (const juce::KeyPress& key) noexcept
{
    if (key == juce::KeyPress::escapeKey)
    {
        screen.setHintOverlay (nullptr, 0);
        linkManager.clearHints();
        session.getState().setModalType (Terminal::ModalType::none);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        linkManager.advanceHintPage();

        screen.setHintOverlay (linkManager.getActiveHintsData(), linkManager.getActiveHintsCount());
        session.getState().setSnapshotDirty();
        return true;
    }

    const juce::juce_wchar ch { key.getTextCharacter() };
    const char lower { static_cast<char> (ch >= 'A' and ch <= 'Z' ? ch + 32 : ch) };

    if (lower >= 'a' and lower <= 'z')
    {
        const Terminal::LinkSpan* matched { linkManager.hitTestHint (lower) };

        if (matched != nullptr)
        {
            linkManager.dispatch (*matched);

            screen.setHintOverlay (nullptr, 0);
            linkManager.clearHints();
            session.getState().setModalType (Terminal::ModalType::none);
        }
    }

    return true;
}

void InputHandler::setScrollOffsetClamped (int newOffset) noexcept
{
    const int maxOffset { session.getGrid().getScrollbackUsed() };
    const int current { session.getState().getScrollOffset() };
    const int clamped { juce::jlimit (0, maxOffset, newOffset) };

    if (clamped != current)
    {
        session.getState().setScrollOffset (clamped);
    }
}
