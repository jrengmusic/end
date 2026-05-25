#pragma once
#include <JuceHeader.h>
#include "../Identifier.h"
#include "../State.h"
#include "../../Map.h"
#include <atomic>
namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @brief Row buffer renderer — owns double-buffered Buffer<Row>, provides atomic Block views, renders ring into TextEditor.
 *
 * Screen is the sole author of terminal dimensions (cols/rows in cell units).
 * Screen owns two jam::Buffer<Row> instances and two pairs of jam::Block<Row> views.
 * An atomic pointer lets Video load the active Block array lock-free on the reader thread.
 * On resize, Screen builds new blocks on the inactive set, then swaps the atomic pointer.
 * Display parents Screen for rendering via addAndMakeVisible.  Screen always exists
 * (owned by Session) regardless of whether Display is attached.
 *
 * On every State flush, valueTreePropertyChanged:
 *   1. Reads activeScreen, scrollOffset, and writeHead from State.
 *   2. Constructs a Block<Row> view from the live buffer at the flushed head.
 *   3. Calls setText (block) — TextEditor wraps at current display width.
 *
 * Screen holds no scroll state. scrollOffset is read from State each flush.
 *
 * @see Screen.cpp
 * @see terminal::State         — SSOT for scrollOffset, activeScreen, visibleRows, writeHead
 * @see jam::Buffer<jam::Row>   — row storage; Screen constructs Block view
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

    /**
     * @brief Constructs Screen, allocates buffers[0], constructs blockSets[0], and sets activeBlocks.
     *
     * Reads scrollbackLines from AppState to compute initial ring size.
     * Buffer is allocated with 2 channels (normal + alternate).
     * Block views for channel 0 and 1 reference buffers[0].
     * activeBlocks is set to blockSets[0].data() — Video loads this atomically.
     *
     * @param stateMachine  Terminal parameter store — owned by Session.
     * @param cols          Initial column count.
     * @param rows          Initial row count.
     * @note MESSAGE THREAD.
     */
    Screen (State& stateMachine, cell cols, cell rows) noexcept;

    ~Screen() override;

    /** @brief Returns the active Block array pointer loaded atomically.
     *  Video calls this once per process() to cache the current blocks.
     *  @note ANY THREAD — atomic acquire load. */
    jam::Block<jam::Row>* getBlocks() noexcept
    {
        return activeBlocks.load (std::memory_order_acquire);
    }

    /** @brief Returns a reference to the atomic active-blocks pointer.
     *  Session passes this to Processor so Video can load blocks lock-free.
     *  @note MESSAGE THREAD — called before reader thread starts. */
    std::atomic<jam::Block<jam::Row>*>& getActiveBlocksRef() noexcept { return activeBlocks; }

    /** @brief Returns a reference to the currently active buffer.
     *  Used by Session to construct the DST resizer.
     *  @note MESSAGE THREAD. */
    jam::Buffer<jam::Row>& getActiveBuffer() noexcept { return buffers[activeIndex]; }

    /** @brief Callback fired when terminal dimensions (cols/rows) change.
     *  Session wires this to the DST resizer. */
    std::function<void (cell, cell)> onDimensionsChanged;

    /** @brief Resizes buffers with content preservation.
     *
     *  Allocates buffers[nextIndex] with new dimensions, copies all content from
     *  buffers[activeIndex] per-channel in ring order (oldest to newest),
     *  constructs new blockSets, swaps activeBlocks, and updates activeIndex.
     *
     *  @param newRingSize     New ring buffer row count (scrollback + viewport rows).
     *  @param newCols         New column count.
     *  @param writePositions  Ring write position per channel [normal, alternate].
     *  @return New write position per channel in the resized buffer.
     *  @note MESSAGE THREAD — called under suspendProcessing. */
    std::array<int, 2> resizeBuffers (int newRingSize, int newCols, const std::array<int, 2>& writePositions) noexcept;

    /** @brief Sets the DECSCUSR cursor shape (terminal VT vocabulary).
     *  Forwards to CaretComponent::setShape(). */
    void setCaretShape (int decscusr) noexcept { caret->setShape (decscusr); }

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    juce::ValueTree terminalState;

    /** @brief Double-buffered cell storage — 2 channels (normal/alternate), ring-addressed.
     *  Index 0 starts active; index 1 is the next buffer built during resize. */
    jam::Buffer<jam::Row> buffers[2];

    /** @brief Per-buffer pair of Block views (channel 0 = normal, channel 1 = alternate). */
    std::array<jam::Block<jam::Row>, 2> blockSets[2];

    /** @brief Atomic pointer Video loads once per process() call.
     *  Points into blockSets[activeIndex].data().  Swapped on resize. */
    std::atomic<jam::Block<jam::Row>*> activeBlocks { nullptr };

    /** @brief Message-thread index indicating which buffer/blockSet is currently active. */
    int activeIndex { 0 };

    static int getValue (const juce::ValueTree& valueTree, juce::Identifier Id) noexcept
    {
        return static_cast<int> (jam::ValueTree::getValueFromChildWithID (valueTree, Id).getValue());
    }

    std::function<void()> onCellChanged (State& stateMachine)
    {
        return [this, &stateMachine]
        {
            const int vw { getVisibleWidth() };
            const int vh { getVisibleHeight() };
            const auto gridRect { jam::Cell::Rectangle (font.bounds,
                                                        juce::Rectangle<int> { 0, 0, vw, vh }) };
            const jam::Bounds viewportSize { gridRect.getWidth().value, gridRect.getHeight().value };

            jam::debug::Log::write ("onCellChanged vw=" + juce::String (vw) + " vh=" + juce::String (vh)
                + " cols=" + juce::String (viewportSize.width) + " rows=" + juce::String (viewportSize.height)
                + " font=" + juce::String (font.bounds.width) + "x" + juce::String (font.bounds.height));

            if (viewportSize.isValid())
            {
                const int scrollbackLines { AppState::getContext()->getRawParameterValue<int> (app::id::scrollbackLines)->load() };
                const int newRingSize { scrollbackLines + viewportSize.height };
                const int currentCols { buffers[activeIndex].getNumCols() };
                const int currentRing { buffers[activeIndex].getNumRows() };

                stateMachine.setValue (id::viewport, viewportSize.pack());

                if (newRingSize != currentRing or viewportSize.width != currentCols)
                {
                    if (onDimensionsChanged != nullptr)
                        onDimensionsChanged (cell (viewportSize.width), cell (viewportSize.height));
                }
            }
        };
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
