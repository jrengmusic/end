/**
 * @file Input.cpp
 * @brief Implementation of keyboard input routing for a terminal processor.
 *
 * @see terminal::Input
 */

#include "Input.h"

namespace terminal
{
/*____________________________________________________________________________*/

Input::Input (terminal::Processor& p,
              terminal::LinkManager& lm) noexcept
    : processor (p)
    , linkManager (lm)
{
}

bool Input::handleKeyDirect (const juce::KeyPress& key) noexcept
{
    const auto bytes { processor.encodeKeyPress (key) };

    if (bytes.isNotEmpty())
        processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));

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
        or action::Registry::getContext()->handleKeyPress (key)
        or [this, &key]
            {
                clearSelectionAndScroll();
                const auto bytes { processor.encodeKeyPress (key) };

                if (bytes.isNotEmpty())
                    processor.writeInput (bytes.toRawUTF8(), int (bytes.getNumBytesAsUTF8()));

                return true;
            }()
    };

    return result;
}

void Input::clearSelectionAndScroll() noexcept
{
    auto node { processor.getState().get().getChildWithName (jam::TextEditor::properties.at (jam::TextEditor::textEditorId)) };

    if (node.isValid())
    {
        const int selType { node.getProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId)) };

        if (selType != static_cast<int> (terminal::SelectionType::none))
            node.setProperty (jam::TextEditor::properties.at (jam::TextEditor::selectionTypeId), static_cast<int> (terminal::SelectionType::none), nullptr);
    }

    const int activeScreen { processor.getState().getActiveScreen() };
    const juce::Identifier scrollScreenId { Map::Screen::getContext()->get (activeScreen) };
    auto scrollScreenNode { processor.getState().get().getChildWithName (scrollScreenId) };
    auto scrollParamNode { jam::ValueTree::getChildWithID (scrollScreenNode, id::scrollOffset.toString()) };
    scrollParamNode.setProperty (id::value, 0, nullptr);
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

    if (type == terminal::ModalType::selection)
    {
        handled = handleSelectionKey (key);
    }
    else if (type == terminal::ModalType::openFile)
    {
        handled = handleOpenFileKey (key);
    }

    return handled;
}

bool Input::handleSelectionKey (const juce::KeyPress& key) noexcept
{
    const int maxRow { processor.getState().getVisibleRows().value - 1 };
    const int maxCol { processor.getState().getCols().value - 1 };

    using TE = jam::TextEditor;
    const auto& teId            { TE::properties.at (TE::textEditorId) };
    const auto& selTypeId       { TE::properties.at (TE::selectionTypeId) };
    const auto& anchorRowId     { TE::properties.at (TE::selectionAnchorRowId) };
    const auto& anchorColId     { TE::properties.at (TE::selectionAnchorColId) };
    const auto& selCursorRowId  { TE::properties.at (TE::selectionCursorRowId) };
    const auto& selCursorColId  { TE::properties.at (TE::selectionCursorColId) };

    auto& st { processor.getState() };
    auto node { st.get().getChildWithName (teId) };

    const int activeScreen { st.getActiveScreen() };
    const juce::Identifier selScreenId { Map::Screen::getContext()->get (activeScreen) };
    auto selScreenNode { st.get().getChildWithName (selScreenId) };

    if (node.isValid())
    {
        if (key == selectionKeys.exit)
        {
            node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::none), nullptr);
            st.setModalType (terminal::ModalType::none);
            pendingG = false;
        }
        else if (key == selectionKeys.visualBlock)
        {
            const auto current { static_cast<terminal::SelectionType> (static_cast<int> (node.getProperty (selTypeId))) };

            if (current == terminal::SelectionType::visualBlock)
            {
                node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::none), nullptr);
                st.setModalType (terminal::ModalType::none);
            }
            else
            {
                node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::visualBlock), nullptr);

                if (current == terminal::SelectionType::none)
                {
                    const int cursorRow { static_cast<int> (jam::ValueTree::getValueFromChildWithID (selScreenNode, id::cursorRow).getValue()) };
                    const int cursorCol { static_cast<int> (jam::ValueTree::getValueFromChildWithID (selScreenNode, id::cursorCol).getValue()) };
                    node.setProperty (anchorRowId,    cursorRow, nullptr);
                    node.setProperty (anchorColId,    cursorCol, nullptr);
                    node.setProperty (selCursorRowId, cursorRow, nullptr);
                    node.setProperty (selCursorColId, cursorCol, nullptr);
                }
            }
        }
        else if (key == selectionKeys.left)
        {
            const int col { juce::jmax (0, static_cast<int> (node.getProperty (selCursorColId)) - 1) };
            node.setProperty (selCursorColId, col, nullptr);
        }
        else if (key == selectionKeys.down)
        {
            const int row { juce::jmin (maxRow, static_cast<int> (node.getProperty (selCursorRowId)) + 1) };
            node.setProperty (selCursorRowId, row, nullptr);
        }
        else if (key == selectionKeys.up)
        {
            const int row { juce::jmax (0, static_cast<int> (node.getProperty (selCursorRowId)) - 1) };
            node.setProperty (selCursorRowId, row, nullptr);
        }
        else if (key == selectionKeys.right)
        {
            const int col { juce::jmin (maxCol, static_cast<int> (node.getProperty (selCursorColId)) + 1) };
            node.setProperty (selCursorColId, col, nullptr);
        }
        else if (key == selectionKeys.visualLine)
        {
            const auto current { static_cast<terminal::SelectionType> (static_cast<int> (node.getProperty (selTypeId))) };

            if (current == terminal::SelectionType::visualLine)
            {
                node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::none), nullptr);
                st.setModalType (terminal::ModalType::none);
            }
            else
            {
                node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::visualLine), nullptr);

                if (current == terminal::SelectionType::none)
                {
                    const int cursorRow { static_cast<int> (jam::ValueTree::getValueFromChildWithID (selScreenNode, id::cursorRow).getValue()) };
                    node.setProperty (anchorRowId,    cursorRow, nullptr);
                    node.setProperty (anchorColId,    0,         nullptr);
                    node.setProperty (selCursorRowId, cursorRow, nullptr);
                    node.setProperty (selCursorColId, 0,         nullptr);
                }
            }
        }
        else if (key == selectionKeys.visual)
        {
            const auto current { static_cast<terminal::SelectionType> (static_cast<int> (node.getProperty (selTypeId))) };

            if (current == terminal::SelectionType::visual)
            {
                node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::none), nullptr);
                st.setModalType (terminal::ModalType::none);
            }
            else
            {
                node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::visual), nullptr);

                if (current == terminal::SelectionType::none)
                {
                    const int cursorRow { static_cast<int> (jam::ValueTree::getValueFromChildWithID (selScreenNode, id::cursorRow).getValue()) };
                    const int cursorCol { static_cast<int> (jam::ValueTree::getValueFromChildWithID (selScreenNode, id::cursorCol).getValue()) };
                    node.setProperty (anchorRowId,    cursorRow, nullptr);
                    node.setProperty (anchorColId,    cursorCol, nullptr);
                    node.setProperty (selCursorRowId, cursorRow, nullptr);
                    node.setProperty (selCursorColId, cursorCol, nullptr);
                }
            }
        }
        else if (key == selectionKeys.copy or key == selectionKeys.globalCopy)
        {
            // Text extraction stub — pending Screen accessor for grid content.
            juce::SystemClipboard::copyTextToClipboard ({});

            node.setProperty (selTypeId, static_cast<int> (terminal::SelectionType::none), nullptr);
            st.setModalType (terminal::ModalType::none);
            pendingG = false;
        }
        else if (key == selectionKeys.bottom)
        {
            node.setProperty (selCursorRowId, maxRow, nullptr);
        }
        else if (key == selectionKeys.top)
        {
            if (pendingG)
            {
                node.setProperty (selCursorRowId, 0, nullptr);
                pendingG = false;
            }
            else
            {
                pendingG = true;
            }
        }
        else if (key == selectionKeys.lineStart)
        {
            node.setProperty (selCursorColId, 0, nullptr);
        }
        else if (key == selectionKeys.lineEnd)
        {
            node.setProperty (selCursorColId, maxCol, nullptr);
        }
    }

    return true;
}

bool Input::handleOpenFileKey (const juce::KeyPress& key) noexcept
{
    if (key == juce::KeyPress::escapeKey)
    {
        linkManager.clearHints();
        processor.getState().setModalType (terminal::ModalType::none);
    }
    else if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        linkManager.advanceHintPage();
        processor.getState().setSnapshotDirty();
    }
    else
    {
        const juce::juce_wchar ch { key.getTextCharacter() };
        const char lower { static_cast<char> (ch >= 'A' and ch <= 'Z' ? ch + 32 : ch) };

        if (lower >= 'a' and lower <= 'z')
        {
            const terminal::LinkSpan* matched { linkManager.hitTestHint (lower) };

            if (matched != nullptr)
            {
                linkManager.dispatch (*matched);
                linkManager.clearHints();
                processor.getState().setModalType (terminal::ModalType::none);
            }
        }
    }

    return true;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
