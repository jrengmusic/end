#pragma once
#include <JuceHeader.h>
#include "../Identifier.h"
#include "../State.h"
#include "../../Map.h"
namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Terminal viewport renderer — renders TextLineArray content via TextEditor.
 *
 * Screen is the sole author of terminal dimensions (cols/rows in cell units).
 * Session calls setText(textLineArray) to set the content reference; Screen reads
 * it during paint via the stored lineContent pointer in TextEditor base.
 * Display parents Screen for rendering via addAndMakeVisible.  Screen always exists
 * (owned by Session) regardless of whether Display is attached.
 *
 * Screen reads during paint only — it never writes to TextLineArray.
 * The flush path (Video → TextLineArray live slots) is owned by Session.
 *
 * On every State flush, valueTreePropertyChanged calls calc() to re-project
 * content height and trigger repaint.
 *
 * @see Screen.cpp
 * @see terminal::State         — SSOT for scrollOffset, activeScreen
 * @see jam::TextLineArray      — SSOT content storage (owned by Session)
 * @see jam::TextEditor         — winsize property is the SSOT for terminal dimensions (cols/rows)
 */
class Screen : public jam::TextEditor
             , public juce::ValueTree::Listener
{
public:
    enum ColourIds
    {
        cursorColourId = 0x3000003,
        selectionColourId = 0x3000004,
        selectionCursorColourId = 0x3000005,
        hintLabelFgColourId = 0x3000006,
        hintLabelBgColourId = 0x3000007,
        ansi0ColourId = 0x3000010,
        ansi1ColourId = 0x3000011,
        ansi2ColourId = 0x3000012,
        ansi3ColourId = 0x3000013,
        ansi4ColourId = 0x3000014,
        ansi5ColourId = 0x3000015,
        ansi6ColourId = 0x3000016,
        ansi7ColourId = 0x3000017,
        ansi8ColourId = 0x3000018,
        ansi9ColourId = 0x3000019,
        ansi10ColourId = 0x300001A,
        ansi11ColourId = 0x300001B,
        ansi12ColourId = 0x300001C,
        ansi13ColourId = 0x300001D,
        ansi14ColourId = 0x300001E,
        ansi15ColourId = 0x300001F,
    };

    /**
     * @brief Constructs Screen and registers as a ValueTree listener.
     *
     * @param stateMachine  Terminal parameter store — owned by Session.
     * @note MESSAGE THREAD.
     */
    Screen (State& stateMachine, const jam::Font& font) noexcept;

    ~Screen() override;

    /** @brief Sets the DECSCUSR cursor shape (terminal VT vocabulary).
     *  Forwards to CaretComponent::setShape(). */
    void setCaretShape (int decscusr) noexcept { caret->setShape (decscusr); }

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    juce::ValueTree terminalState;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
