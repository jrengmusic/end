#pragma once
#include <JuceHeader.h>
#include "../Identifier.h"
#include "../../Map.h"
#include "../State.h"
namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Row buffer renderer — scrollback ring + alternate buffer for the terminal.
 *
 * Listens to State's ValueTree and re-renders from Buffer on every flush.
 * Display mediates all communication — Screen owns its State and Buffer references
 * and drives itself on valueTreePropertyChanged.
 */
class Screen : public jam::TextEditor
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

    Screen (State& state, jam::Buffer<jam::Row>& buffer) noexcept;
    ~Screen() override;

    /** @brief Resize transitioner — owned by Screen, wired by Display.
     *  Public for Display to wire trigger/onStop and call set(). */
    jam::DiscreteStateTransition<jam::Row> transitioner;

    /** @brief Sets the DECSCUSR cursor shape (terminal VT vocabulary).
     *  Forwards to CaretComponent::setShape(). */
    void setCaretShape (int decscusr) noexcept { caret->setShape (decscusr); }

    /** @brief Reflowed content — produced by DST trigger, rendered during transition,
     *  written to live buffer on onStop. */
    jam::Buffer<jam::Row> reflowedContent;

    /** @brief History row count for the normal screen from the last reflow, consumed by onStop. */
    int reflowedHistoryNormal { 0 };

    /** @brief Guards reflowedContent access between message thread (reflow/onStop) and GL thread (render). */
    juce::CriticalSection reflowLock;

    /** @brief Pure transform: reflows source rows to new column width.
     *  Source column width is source.getNumCols(); destination column width is dest.getNumCols().
     *  dest must be pre-allocated by the caller at the new dimensions.
     *  Content extent = numHistoryNormal + cursorRow + 1 — empty rows below cursor are viewport padding.
     *  @return Total reflowed history row count for the normal screen. */
    static int reflow (jam::Buffer<jam::Row>& dest,
                       const jam::Buffer<jam::Row>& source,
                       int scrollbackLines,
                       int oldVisibleRows,
                       int newVisibleRows,
                       int numHistoryNormal,
                       int numHistoryAlternate,
                       int cursorRow) noexcept;

    /** @brief Hide or show the caret. Called by Display at transition start/stop. */
    void setCaretVisible (bool visible) noexcept { caret->setVisible (visible); }

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    State& terminal;
    jam::Buffer<jam::Row>& buffer;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
