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

    if (bytes.isNotEmpty() and processor.events.contains (Terminal::ID::writeInput))
        processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));

    return true;
}

bool Input::handleKey (const juce::KeyPress& key) noexcept
{
    const int code { key.getKeyCode() };
    const auto mods { key.getModifiers() };

    /**
     * Preview dismiss takes priority over all other modal handling.
     * Any key while isPreviewActive removes the preview IMAGE node and deactivates
     * the split viewport.  The lambda evaluates to true so the result is consumed.
     */
    const bool result
    {
        (processor.getState().isPreviewActive() and processor.getState().getSplitCol() > 0 and [this]
            {
                processor.getState().dismissPreview();
                return true;
            }())
        or (processor.getState().isModal() and handleModalKey (key))
        or Action::Registry::getContext()->handleKeyPress (key)
        or [this, &key]
            {
                clearSelectionAndScroll();
                const auto bytes { processor.encodeKeyPress (key) };

                if (bytes.isNotEmpty() and processor.events.contains (Terminal::ID::writeInput))
                    processor.events.get (Terminal::ID::writeInput, bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));

                return true;
            }()
    };

    return result;
}

void Input::clearSelectionAndScroll() noexcept
{
    if (processor.getState().isDragActive()
        or processor.getState().getSelectionType() != static_cast<int> (Terminal::SelectionType::none))
    {
        processor.getState().setDragActive (false);
        processor.getState().setSelectionType (static_cast<int> (Terminal::SelectionType::none));
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
    const int maxRow { processor.getState().getVisibleRows() + processor.getState().getScrollbackUsed() - 1 };
    const int maxCol { processor.getState().getCols() - 1 };

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
            const int scrollback { processor.getState().getScrollbackUsed() };
            const int visibleStart { scrollback };
            const int cols { processor.getState().getCols() };

            const int anchorVisRow { st.getSelectionAnchorRow() - visibleStart };
            const int cursorVisRow { st.getSelectionCursorRow() - visibleStart };
            const int anchorCol { st.getSelectionAnchorCol() };
            const int cursorCol { st.getSelectionCursorCol() };

            juce::String text;

            // Text extraction from Grid migrated to Screen — stub pending Screen accessor.
            juce::ignoreUnused (anchorVisRow, cursorVisRow, anchorCol, cursorCol, cols);

            juce::SystemClipboard::copyTextToClipboard (text);
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
