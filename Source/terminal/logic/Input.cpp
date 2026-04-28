/**
 * @file Input.cpp
 * @brief Implementation of keyboard input routing for a terminal processor.
 *
 * @see Terminal::Input
 */

#include "Input.h"
#include "Processor.h"
#include "../selection/LinkManager.h"
#include "../../action/Action.h"

namespace Terminal
{

Input::Input (Terminal::Processor& p,
              Terminal::LinkManager& lm) noexcept
    : processor (p)
    , linkManager (lm)
{
}

bool Input::handleKeyDirect (const juce::KeyPress& key) noexcept
{
    const auto bytes { processor.encodeKeyPress (key) };

    if (bytes.isNotEmpty())
        processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));

    return true;
}

bool Input::handleKey (const juce::KeyPress& key) noexcept
{
    const int code { key.getKeyCode() };
    const auto mods { key.getModifiers() };

    const bool isScrollNav { mods.isShiftDown() and not mods.isCommandDown()
                             and (code == juce::KeyPress::pageUpKey or code == juce::KeyPress::pageDownKey
                                  or code == juce::KeyPress::homeKey or code == juce::KeyPress::endKey) };

    const bool result
    {
        (processor.getState().isModal() and handleModalKey (key))
        or Action::Registry::getContext()->handleKeyPress (key)
        or (isScrollNav and [this, code]
            {
                handleScrollNav (code, [this] (int offset) { processor.setScrollOffsetClamped (offset); });
                return true;
            }())
        or [this, &key]
            {
                clearSelectionAndScroll();
                const auto bytes { processor.encodeKeyPress (key) };

                if (bytes.isNotEmpty())
                    processor.writeInput (bytes.toRawUTF8(), static_cast<int> (bytes.getNumBytesAsUTF8()));

                return true;
            }()
    };

    return result;
}

void Input::handleScrollNav (int code,
                              std::function<void (int)> newOffsetFn) noexcept
{
    const juce::ScopedLock lock (processor.getGrid().getResizeLock());
    const int page { processor.getGrid().getVisibleRows() };
    const int current { processor.getState().getScrollOffset() };

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
        newOffsetFn (processor.getGrid().getScrollbackUsed());
    }
    else if (code == juce::KeyPress::endKey)
    {
        newOffsetFn (0);
    }
}

void Input::clearSelectionAndScroll() noexcept
{
    if (processor.getState().isDragActive()
        or processor.getState().getSelectionType() != static_cast<int> (Terminal::SelectionType::none))
    {
        processor.getState().setDragActive (false);
        processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
    }

    if (processor.getState().getScrollOffset() > 0)
    {
        processor.getState().setScrollOffset (0);
    }
}

void Input::buildKeyMap (const lua::Engine::SelectionKeys& keys) noexcept
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

    openFileNextPage = keys.openFileNextPage;
}

void Input::reset() noexcept
{
    pendingG = false;
}

bool Input::isSelectionCopyKey (const juce::KeyPress& key) const noexcept
{
    return key == selectionKeys.copy or key == selectionKeys.globalCopy;
}

bool Input::handleModalKey (const juce::KeyPress& key) noexcept
{
    const auto type { processor.getState().getModalType() };
    bool handled { false };

    if (type == Terminal::ModalType::selection)
    {
        handled = handleSelectionKey (key);
    }
    else if (type == Terminal::ModalType::openFile)
    {
        handled = handleOpenFileKey (key);
    }

    return handled;
}

bool Input::handleSelectionKey (const juce::KeyPress& key) noexcept
{
    const juce::ScopedLock lock (processor.getGrid().getResizeLock());
    const int maxRow { processor.getGrid().getVisibleRows() + processor.getGrid().getScrollbackUsed() - 1 };
    const int maxCol { processor.getGrid().getCols() - 1 };

    auto& st { processor.getState() };

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
            const juce::ScopedTryLock tryLock (processor.getGrid().getResizeLock());

            if (tryLock.isLocked())
            {
                const int scrollback { processor.getGrid().getScrollbackUsed() };
                const int scrollOffset { st.getScrollOffset() };
                const int visibleStart { scrollback - scrollOffset };
                const int cols { processor.getGrid().getCols() };

                const int anchorVisRow { st.getSelectionAnchorRow() - visibleStart };
                const int cursorVisRow { st.getSelectionCursorRow() - visibleStart };
                const int anchorCol { st.getSelectionAnchorCol() };
                const int cursorCol { st.getSelectionCursorCol() };

                juce::String text;

                if (smType == Terminal::SelectionType::visual)
                {
                    const juce::Point<int> start { anchorCol, anchorVisRow };
                    const juce::Point<int> end { cursorCol, cursorVisRow };
                    text = processor.getGrid().extractText (start, end, scrollOffset);
                }
                else if (smType == Terminal::SelectionType::visualLine)
                {
                    const juce::Point<int> start { 0, std::min (anchorVisRow, cursorVisRow) };
                    const juce::Point<int> end { cols - 1, std::max (anchorVisRow, cursorVisRow) };
                    text = processor.getGrid().extractText (start, end, scrollOffset);
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
                    text = processor.getGrid().extractBoxText (topLeft, bottomRight, scrollOffset);
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
        const int visibleRows { processor.getGrid().getVisibleRows() };
        const int scrollback { processor.getGrid().getScrollbackUsed() };
        const int visibleStart { scrollback - st.getScrollOffset() };
        const int visibleEnd { visibleStart + visibleRows - 1 };

        if (cursorRow < visibleStart)
        {
            processor.setScrollOffsetClamped (scrollback - cursorRow);
        }
        else if (cursorRow > visibleEnd)
        {
            processor.setScrollOffsetClamped (scrollback - (cursorRow - visibleRows + 1));
        }

    }

    return true;
}

bool Input::handleOpenFileKey (const juce::KeyPress& key) noexcept
{
    if (key == juce::KeyPress::escapeKey)
    {
        linkManager.clearHints();
        processor.getState().setModalType (Terminal::ModalType::none);
    }
    else if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        linkManager.advanceHintPage();
        processor.getState().setFullRebuild();
        processor.getState().setSnapshotDirty();
    }
    else
    {
        const juce::juce_wchar ch { key.getTextCharacter() };
        const char lower { static_cast<char> (ch >= 'A' and ch <= 'Z' ? ch + 32 : ch) };

        if (lower >= 'a' and lower <= 'z')
        {
            const Terminal::LinkSpan* matched { linkManager.hitTestHint (lower) };

            if (matched != nullptr)
            {
                linkManager.dispatch (*matched);
                linkManager.clearHints();
                processor.getState().setModalType (Terminal::ModalType::none);
            }
        }
    }

    return true;
}

} // namespace Terminal
