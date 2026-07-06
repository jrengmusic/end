/**
 * @file terminal/Session.h
 * @brief Terminal session — DAW host analog. Owns document, model, and engine.
 */
#pragma once
#include <JuceHeader.h>
#include "terminal/Model.h"
#include "terminal/Processor.h"
#include "config/Config.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal session — DAW host per terminal instance.
 *
 *  Owns the document buffer (TextModel), the VT state SSOT (terminal::Model),
 *  and the processing engine (Processor). Nexus
 *  owns Sessions. terminal::View holds a Session reference and parents a
 *  jam::CodeView that renders the owned document.
 *
 *  Construction order: @c model before @c processor — Processor holds a
 *  reference to @c model and registers itself as a jam::Model::Listener in
 *  its constructor.
 *
 *  @par Drain (S3)
 *  @c terminal::View owns the drain trigger — its own @c model ValueTree
 *  listener calls @c drain() on the 60Hz-flushed @c screenDirty Direction-A
 *  parameter (whichever screen is active), then repaints via
 *  @c jam::CodeView::calc(). Session no longer self-listens for this (View
 *  is the sole @c screenDirty consumer) — Session's own @c
 *  valueTreePropertyChanged(), below, is reserved for its own live-resize
 *  seam (@c ID::winsize) only. @c drain() itself is two-phase: Phase 1 drains the
 *  CellFifo history ring (already-joined logical lines, oldest first) and
 *  inserts each at @c liveFirstLine(); Phase 2 drains the CellFifo active
 *  ring (one entry per live viewport row) and lays each down at
 *  @c liveFirstLine() + row, replacing the existing line once the document
 *  has grown to cover the full live region (bootstraps via @c insertAt()
 *  before then — @c jam::TextModel::replaceAt() asserts the target index
 *  already exists). HARD INVARIANT: Phase 1 completes fully before Phase 2
 *  begins — no row is ever touched by both phases in the same @c drain()
 *  call.
 *
 *  @par Live resize (P6/P9)
 *  Session owns a @c jam::Resizer (16ms coalescing timer, endless's own
 *  wiring shape — @c Session::wireResizer(), reproduced here against
 *  today's cooperative-suspend Processor rather than endless's blocking
 *  @c suspendProcessing()). Every Direction B @c ID::winsize change (this
 *  struct's own @c valueTreePropertyChanged(), below) calls @c resizer.set()
 *  with the new cols/rows. The START trigger fires immediately and raises
 *  @c processor.suspendProcessing (true); the STOP trigger fires 16ms after
 *  the LAST change and runs the P9 popback sequence: @c drain() (history +
 *  active fully — SHRINK needs nothing further, the arithmetic @c
 *  liveFirstLine() projection already retains every drained line), then for
 *  a GROW delta reads the document's own tail lines (already-drained,
 *  read-only — @c getLine(), never removed: "zero content loss") and
 *  forwards each to @c processor.pushPopback(), then
 *  @c processor.setWinsize() (Video + TTY), then lowers the suspend flag.
 */
struct Session : public juce::ValueTree::Listener
{
    explicit Session()
        : processor (model)
    {
        jam::Stamp::getInstance()->addIfNotAlreadyThere (jam::Stamp::Entry {});
        model.addListener (this);
        wireResizer();
    }

    ~Session() override
    {
        model.removeListener (this);
    }

    /** @brief Returns the owned TextModel. View drains into this. */
    jam::TextModel& getDocument() noexcept { return document; }

    /** @brief Returns the owned terminal::Model (SSOT). */
    Model& getModel() noexcept { return model; }

    /** @brief Returns the owned Processor. */
    Processor& getProcessor() noexcept { return processor; }

    /** @brief Starts the terminal engine — owner-reads shell.program/
     *  shell.args/terminal.scrollback_lines from @c config::Model itself
     *  (no caller-supplied parameters), publishes scrollbackLines (Direction
     *  B), sizes the CellFifo rings (S6, via @c Processor::prepare()), then
     *  opens the platform TTY.
     *
     *  Idempotent — Session owns its own @c started fact, so
     *  terminal::View's own @c resized() calls this UNGUARDED on every
     *  invocation; the real prepare+open sequence below runs exactly once,
     *  gated on the already-published Direction B @c winsize atom
     *  (@c getWinsize()) becoming positive — the first @c resized() call
     *  fires before the component has real bounds (winsize still 0x0), every
     *  later call after the parent lays it out for real.
     *  @note MESSAGE THREAD.
     */
    void start()
    {
        if (not started)
        {
            const auto [cols, rows] { getWinsize() };

            if (cols > 0 and rows > 0)
            {
                model.setScrollbackLines (config.getValue (IDtype::terminal, ID::scrollbackLines));

                const auto shellProgram { config.getValue (IDtype::shell, ID::program).toString() };
                const auto shellArgs { config.getValue (IDtype::shell, ID::args).toString() };

                processor.prepare();
                processor.start (shellProgram, shellArgs, juce::String {});

                started = true;
            }
        }
    }

    /** @brief Forwards keystroke/paste bytes to the Processor — bypasses the
     *  Model entirely (terminal::View -> here -> Processor -> TTY).
     *  @note MESSAGE THREAD.
     */
    void writeInput (const void* data, int numBytes) noexcept
    {
        processor.writeInput (data, numBytes);
    }

    /** @brief Projection primitive (S2): @c numLines - viewport row count
     *  (Direction B @c winsize's row component). Arithmetic, never
     *  remembered state — liveTailExtent is the recorded anti-pattern (R7).
     *  @note MESSAGE THREAD.
     */
    int liveFirstLine() const noexcept
    {
        const auto [cols, viewportRows] { getWinsize() };
        juce::ignoreUnused (cols);

        return juce::jmax (0, document.getNumLines() - static_cast<int> (viewportRows));
    }

    /** @brief Two-phase drain (S3) — see this struct's own doc comment.
     *  @note MESSAGE THREAD, screenDirty-batched.
     */
    void drain() noexcept
    {
        jam::TextLine line;

        while (processor.drainHistory (line))
            document.insertAt (liveFirstLine(), std::move (line));

        int row { 0 };

        while (processor.drainActive (line))
        {
            const int lineIndex { liveFirstLine() + row };

            // Bootstrap case: the document has not yet grown to cover the
            // full live region (startup, or just after a scrollbackLines/
            // winsize change) — replaceAt() asserts lineIndex < getNumLines(),
            // so the first pass over each not-yet-existing row inserts
            // instead. Steady state (document already covers the live
            // region) always takes the replaceAt() branch, matching S3.
            if (lineIndex < document.getNumLines())
                document.replaceAt (lineIndex, std::move (line));
            else
                document.insertAt (lineIndex, std::move (line));

            ++row;
        }
    }

    // Phase 4: attachInto (daemon mode / View re-parenting).

private:
    /** @brief Single-key dispatch: @c ID::winsize (this class's own
     *  live-resize seam, P6/P9) triggers the Resizer's coalesced START
     *  trigger. Every other property on @c model is ignored here — View owns
     *  the rest of the Direction A/B dispatch (EventRegistration.cpp),
     *  INCLUDING @c jam::ID::screenDirty -> drain() (View's own contract,
     *  this struct's own Drain (S3) doc comment above).
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override
    {
        juce::ignoreUnused (tree);

        if (property == ID::winsize)
        {
            const auto [cols, rows] { getWinsize() };

            if (cols > 0 and rows > 0)
                resizer.set (jam::ID::start, cols, rows);
        }
    }

    /** @brief Reads the current Direction B @c winsize atom (message
     *  thread, lock-free) — shared by @c liveFirstLine() and the
     *  @c ID::winsize dispatch above.
     */
    jam::Size<int16_t> getWinsize() const noexcept
    {
        auto* winsizeParameter { model.getParameter<jam::Parameter<int>> (model.getType(), ID::winsize) };
        jassert (winsizeParameter != nullptr);

        return jam::Size<int16_t> (winsizeParameter->getValue());
    }

    /** @brief Registers the Resizer's start/stop triggers (P6/P9 live-resize
     *  protocol) — called once from the constructor, after @c processor
     *  exists. See this struct's own doc comment for the full sequence.
     *  @note MESSAGE THREAD.
     */
    void wireResizer() noexcept
    {
        resizer.addTrigger<int, int> (jam::ID::start,
            [this] (int, int)
            {
                processor.suspendProcessing (true);
            });

        resizer.addTrigger<int, int> (jam::ID::stop,
            [this] (int newCols, int newRows)
            {
                drain();

                const int oldRows { processor.getVisibleRows() };
                const int delta    { newRows - oldRows };

                if (delta > 0)
                {
                    const int numLines { document.getNumLines() };

                    for (int i { juce::jmax (0, numLines - delta) }; i < numLines; ++i)
                    {
                        const auto& line { document.getLine (i) };

                        const uint8_t flags {
                            static_cast<uint8_t> ((line.isContinued ? jam::terminal::CellFifo::isContinuedFlag : 0)
                                                 | (line.isJustified ? jam::terminal::CellFifo::isJustifiedFlag : 0)
                                                 | (static_cast<uint8_t> (line.mark) << jam::terminal::CellFifo::markShift))
                        };

                        processor.pushPopback (line.chars.getData(), line.cellCount, flags);
                    }
                }

                processor.setWinsize (jam::Cell::Rectangle (jam::Cell (newCols), jam::Cell (newRows)));
                processor.suspendProcessing (false);
            });
    }

    /** @brief Document SSOT — 2 screens (normal + alternate). */
    jam::TextModel document { 2 };

    /** @brief VT state SSOT — constructed before @c processor. */
    Model model;

    /** @brief Terminal engine — AudioProcessor analog. Registers as a
     *  jam::Model::Listener on @c model at construction. */
    Processor processor;

    /** @brief Singleton config model reference — owner-read source for
     *  shell.program/shell.args/terminal.scrollback_lines at start(). */
    config::Model& config { *config::Model::getInstance() };

    /** @brief First-positive-winsize guard (start()'s own idempotency fact)
     *  — the real prepare+open sequence must run exactly once; every later
     *  start() call (terminal::View's resized() calls it unguarded on every
     *  invocation) is then a correct no-op. */
    bool started { false };

    /** @brief Live-resize coalescing timer (P6/P9) — declared LAST so its
     *  destructor (stops the timer) runs FIRST, before @c processor, since
     *  its trigger lambdas capture @c this and reach through @c processor.
     */
    jam::Resizer resizer;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
