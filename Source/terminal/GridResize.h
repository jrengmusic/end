/**
 * @file GridResize.h
 * @brief SmoothStateTransition for terminal Grid resize — VERBATIM SST pattern.
 *
 * GridResize is SmoothStateTransition applied to terminal resize.
 * previous = snapshot of grid before resize. current = live grid.
 * target = requested dimensions. Timer drives process() which interpolates
 * from previous to target, reflowing each tick. Content is NEVER destroyed.
 *
 * @note MESSAGE THREAD only.
 */

#pragma once

#include <JuceHeader.h>
#include "Grid.h"
#include "Video.h"

namespace terminal
{
/*____________________________________________________________________________*/

class GridResize : private juce::Timer
{
public:
    GridResize (Grid& grid, Video& video,
                jam::Function::Map<juce::Identifier, void>& events) noexcept;

    ~GridResize() = default;

    //==========================================================================
    // SST: process(sample) — timer-driven, advances crossfade
    void process() noexcept;

    //==========================================================================
    // SST: reset()
    void reset() noexcept;

    // SST: prepare(sampleRate, blockSize)
    void prepare (int scrollbackLines) noexcept;

    //==========================================================================
    // SST: set(name, value) — told by Processor
    void set (cell cols, cell rows) noexcept;

    // SST: flush() — apply pending immediately, no transition
    void flush() noexcept;

    //==========================================================================
    // SST: getCurrent() — grid ref is always current
    // (Grid& grid is public accessible via Processor::getGrid())

    // SST: getTarget()
    cell getTargetCols() const noexcept { return targetCols; }
    cell getTargetRows() const noexcept { return targetRows; }

    // SST: isInTransition()
    bool isInTransition() const noexcept { return isTransitioning; }

    //==========================================================================
    // Cell pixel size — coalesced, applied at next applyChange
    void setCellSize (int cellWidth, int cellHeight) noexcept;

    // First allocation — cold start, grid not yet allocated
    void allocate (cell cols, cell rows) noexcept;

private:
    //==========================================================================
    // SST: applyChange(name, value)
    void applyChange() noexcept;

    // SST: advanceCrossfade()
    void advanceCrossfade() noexcept;

    // SST: updateCrossfadeIncrement()
    void updateCrossfadeIncrement() noexcept;

    // SST: timerCallback drives process()
    void timerCallback() override { process(); }

    // Snapshot helper — captures live grid into previous
    void captureSnapshot() noexcept;

    //==========================================================================
    Grid& grid;       ///< current (modified in place by reflowFrom)
    Video& video;
    jam::Function::Map<juce::Identifier, void>& events;

    // SST: previous
    jam::Buffer<jam::Row> previous;
    std::array<int, 2> previousHead { 0, 0 };
    std::array<int, 2> previousNumRows { 0, 0 };
    int previousRingMask { 0 };
    int previousViewportRows { 0 };

    // SST: target
    cell targetCols { cell (0) };
    cell targetRows { cell (0) };

    // Interpolated current dims (between start and target during crossfade)
    cell startCols { cell (0) };
    cell startRows { cell (0) };

    // SST: pending
    bool hasPending { false };
    cell pendingCols { cell (0) };
    cell pendingRows { cell (0) };

    // SST: transition state
    bool isReady { false };
    bool isTransitioning { false };
    double crossfadePosition { 1.0 };
    double crossfadeIncrement { 0.0 };

    // Config
    int scrollbackLines { 0 };
    double transitionTimeMs { 200.0 };
    static constexpr int tickIntervalMs { 16 };

    // Cell size coalescing
    int pendingCellWidth { 0 };
    int pendingCellHeight { 0 };
    bool hasPendingCellSize { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridResize)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
