/**
 * @file Winsize.h
 * @brief Packed terminal viewport dimensions — cell + pixel in one atomic uint64.
 *
 * `terminal::Winsize` mirrors POSIX `struct winsize`: terminal dimensions in
 * both cell units (cols, rows) and physical pixels (pixelWidth, pixelHeight).
 *
 * Display is the sole author.  All consumers read from a single
 * Parameter<int64_t> via pack()/unpack().  Four 16-bit fields packed into
 * one int64_t — one atomic store, one atomic load, zero drift.
 *
 * @see Display::resized()  — computes and publishes Winsize
 * @see TTY::setWinsize()   — consumes cell + pixel dims for SIGWINCH
 */

#pragma once

namespace terminal
{
/*____________________________________________________________________________*/

/**
 * @struct Winsize
 * @brief Terminal viewport dimensions — cell counts + pixel dimensions.
 *
 * Trivially copyable, passed by value.  pack() produces an int64_t with
 * layout: cols [63:48] | rows [47:32] | pixelWidth [31:16] | pixelHeight [15:0].
 */
struct Winsize
{
    int cols        { 0 };  ///< Terminal width in cell columns.
    int rows        { 0 };  ///< Terminal height in cell rows.
    int pixelWidth  { 0 };  ///< Viewport width in physical pixels.
    int pixelHeight { 0 };  ///< Viewport height in physical pixels.

    /** Packs all four fields into a single int64_t (16 bits each). */
    constexpr int64_t pack() const noexcept
    {
        return (static_cast<int64_t> (cols)        << 48)
             | ((static_cast<int64_t> (rows)       & 0xFFFF) << 32)
             | ((static_cast<int64_t> (pixelWidth) & 0xFFFF) << 16)
             |  (static_cast<int64_t> (pixelHeight) & 0xFFFF);
    }

    /** Reconstructs a Winsize from a packed int64_t produced by pack(). */
    static constexpr Winsize unpack (int64_t v) noexcept
    {
        return { static_cast<int> (v >> 48),
                 static_cast<int> ((v >> 32) & 0xFFFF),
                 static_cast<int> ((v >> 16) & 0xFFFF),
                 static_cast<int> (v & 0xFFFF) };
    }

    /** Returns true when both cell dimensions are strictly positive. */
    constexpr bool isValid() const noexcept { return cols > 0 and rows > 0; }

    /** Returns a Cell::Rectangle with position (0, 0) and size (cols, rows). */
    constexpr jam::Cell::Rectangle toCellRect() const noexcept
    {
        return { cell (cols), cell (rows) };
    }

    /** Returns the column count as a cell Unit. */
    constexpr cell getCols() const noexcept { return cell (cols); }

    /** Returns the row count as a cell Unit. */
    constexpr cell getRows() const noexcept { return cell (rows); }
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
