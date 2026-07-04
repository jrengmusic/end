/**
 * @file TestTerm.h
 * @brief Test::Term — conformance-suite fixture (raw bytes -> Parser -> Video
 *        -> assert grid/cursor/mode state).
 *
 * Revives the `b3f0fea` fixture SHAPE (raw bytes in, Parser drives Video,
 * assertions read grid cells / cursor / mode state back out) against today's
 * jam_terminal API: `Video::printCodepoint` (not `print`), grid access via
 * `Video::getBlock()` / `jam::Block::getRowPointer()` /
 * `jam::Row::chars[col]`, cursor via `getCursorRow()`/`getCursorCol()`, modes
 * via `jam::terminal::Model` (PLAN-vt-correctness.md Step 8 — `Video` is
 * stateless on plain VT modes; `Term::mode()` reads the fixture-owned `model`
 * collaborator directly, the same MODES parameter surface `Video::modeFlag()`
 * reads internally). See RFC-vt-correctness.md S5 for the ratified scaffold
 * shape (`Test::Term`, `t.feed()`, `t.cell(row,col)`, `t.line(row)`,
 * `t.cursorCol()`).
 *
 * @par Provenance (git archaeology, this session)
 * The original Catch2 suite (95 cases, 16 groups) referenced at commit
 * `b3f0fea` was never committed to git — `git show b3f0fea --stat` touches
 * only the two production bug-fix files + `carol/SPRINT-LOG.md`; the SPRINT-LOG
 * entry itself records "Test suite status: Removed after validation... Spec
 * retained at SPEC-unit-tests.md for future rebuild", but `SPEC-unit-tests.md`
 * was likewise never committed (confirmed via `git log --all -S` for
 * `TEST_CASE`, `Test::Term`, `CATCH_CONFIG_MAIN`, `REQUIRE(` — zero hits in
 * any tracked blob; `git fsck --unreachable` found no dangling objects).
 * This fixture is therefore a fresh implementation of the PATTERN (shape)
 * recorded in SPRINT-LOG prose + RFC-vt-correctness.md S5's ratified scaffold
 * — not a literal recovery of prior source.
 *
 * @par SharedResources ownership
 * `jam::Stamp` / `jam::Grapheme` / `jam::Link` are process-scoped
 * `SharedResources<T>` singletons (`jam::Instance<T>` CRTP — see
 * `jam_core/instance/jam_Instance.h`). Every `Test::Term` owns one instance
 * of each (mirrors `jam::VulkanEngine`'s `stamp` / `grapheme` / `link`
 * members, `jam_vulkan/engine/jam_VulkanEngine.h`) so every fixture construction
 * starts a fresh interning table — indices are deterministic per test case.
 *
 * @par Model ownership (PLAN-vt-correctness.md Step 8)
 * `jam::terminal::map::DecMode` is likewise an `Instance<DecMode>` — the
 * fixture owns one (`decModeInstance`), constructed before `model` per the
 * same declaration-order-is-construction-order discipline, since
 * `jam::terminal::Model`'s constructor iterates `DecMode::getInstance()` to
 * register the MODES parameter group. `model` is constructor-bound into
 * `video` exactly like the owning application binds its own Model.
 *
 * @par Deadline-injection seam (V2 expiry testing)
 * `Term` is `TermBase<jam::terminal::Video>` — templated on the owned Video
 * type so a test-only subclass can be substituted without duplicating the
 * fixture's wiring. `Video::syncOutputDeadlineMs` is protected (not
 * private) for exactly this reason (`jam_CursorState.h` field doc);
 * `SyncOutputDeadlineVideo` exposes `setSyncOutputDeadlineMs()` to write it
 * directly. `SyncOutputDeadlineTerm` (`TermBase<SyncOutputDeadlineVideo>`)
 * is the fixture expiry tests (`SyncOutputTests.cpp`) use to force an
 * already-elapsed SYNC_OUTPUT deadline deterministically instead of
 * sleeping past `Video::syncResetMs`.
 */
#pragma once

#include <jam_terminal/jam_terminal.h>

#include <cstring>
#include <initializer_list>
#include <string>

namespace Test
{ /*____________________________________________________________________________*/

/** @brief UTF-8 encodes one Unicode scalar and appends it to @p out. */
inline void appendCodepoint (std::string& out, uint32_t codepoint) noexcept
{
    if (codepoint <= 0x7F)
    {
        out += static_cast<char> (codepoint);
    }
    else if (codepoint <= 0x7FF)
    {
        out += static_cast<char> (0xC0 | (codepoint >> 6));
        out += static_cast<char> (0x80 | (codepoint & 0x3F));
    }
    else if (codepoint <= 0xFFFF)
    {
        out += static_cast<char> (0xE0 | (codepoint >> 12));
        out += static_cast<char> (0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char> (0x80 | (codepoint & 0x3F));
    }
    else
    {
        out += static_cast<char> (0xF0 | (codepoint >> 18));
        out += static_cast<char> (0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char> (0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char> (0x80 | (codepoint & 0x3F));
    }
}

/** @brief UTF-8 encodes a sequence of codepoints (base + combining/ZWJ/VS/etc.) into one string. */
inline std::string utf8 (std::initializer_list<uint32_t> codepoints)
{
    std::string out;
    for (auto cp : codepoints)
        appendCodepoint (out, cp);
    return out;
}

/**
 * @struct Line
 * @brief Lightweight view over one live grid `jam::Row`, returned by `Term::line()`.
 *
 * RFC-vt-correctness.md S5 scaffold shape: `t.line(0).isContinued()`.
 */
struct Line
{
    const jam::Row* row;

    /** @brief DECAWM soft-wrap flag — row content continues on the next row. */
    bool isContinued() const noexcept { return (row->flags & jam::Row::flexWrap) != 0; }

    /** @brief Row contains `Char::FLEX_GAP` — elastic whitespace for reflow. */
    bool isJustified() const noexcept { return (row->flags & jam::Row::justify) != 0; }

    /** @brief OSC 133 semantic mark stamped onto this row (`Row::markShift`/`markMask`). */
    jam::CodeLine::Mark mark() const noexcept
    {
        return static_cast<jam::CodeLine::Mark> ((row->flags & jam::Row::markMask) >> jam::Row::markShift);
    }

    /** @brief Rightmost non-blank column + 1 — content boundary. */
    int usedCols() const noexcept { return row->usedCols; }
};

/**
 * @class SyncOutputDeadlineVideo
 * @brief Test-only Video subclass — writes the protected
 *        `syncOutputDeadlineMs` field directly.
 *
 * Lets deadline-crossing tests (DECSET 2026 / SYNC_OUTPUT auto-reset) force
 * an already-elapsed deadline deterministically instead of sleeping past
 * `Video::syncResetMs`.
 *
 * @see jam::terminal::Video::syncOutputDeadlineMs
 */
class SyncOutputDeadlineVideo : public jam::terminal::Video
{
public:
    using jam::terminal::Video::Video;

    /** @brief Sets `syncOutputDeadlineMs` directly, bypassing the
     *  `handlePrivateMode()` DECSET 2026 arm computation. */
    void setSyncOutputDeadlineMs (double ms) noexcept { syncOutputDeadlineMs = ms; }
};

/**
 * @class TermBase
 * @brief Owns Video + Parser + the SharedResources tables; feeds raw bytes,
 *        exposes grid/cursor/mode/response assertions.
 *
 * @tparam VideoT  The owned Video type — `jam::terminal::Video` (see `Term`)
 *                  or a test subclass (see `SyncOutputDeadlineVideo` /
 *                  `SyncOutputDeadlineTerm`).
 */
template<typename VideoT>
class TermBase
{
public:
    TermBase (int cols, int rows)
        : video (jam::Cell::Rectangle (jam::Cell (cols), jam::Cell (rows)), events, model)
        , parser (video)
    {
        events.add<const char*, int> (jam::ID::writeToHost,
            [this] (const char* data, int length)
            {
                response.append (data, static_cast<size_t> (length));
            });

        events.add<const char*, int> (jam::ID::title,
            [this] (const char* data, int length)
            {
                title.assign (data, static_cast<size_t> (length));
            });

        // Video's ctor allocates the grid from `dims` but leaves cols/visibleRows
        // at their {80}/{24} defaults — setWinsize() is the mandatory sync step
        // (jam_CursorState.h Video::setWinsize doc).
        video.setWinsize (jam::Cell::Rectangle (jam::Cell (cols), jam::Cell (rows)));
    }

    /** @brief Feeds raw bytes of known length through Parser -> Video, then flushes responses. */
    void feed (const char* bytes, int length) noexcept
    {
        parser.process (reinterpret_cast<const uint8_t*> (bytes), static_cast<size_t> (length));
        video.flushResponses();
    }

    /** @brief Feeds a NUL-terminated byte sequence (RFC S5 scaffold shape: `t.feed ("abc...")`). */
    void feed (const char* bytes) noexcept { feed (bytes, static_cast<int> (std::strlen (bytes))); }

    /** @brief Feeds a std::string payload (UTF-8 multi-codepoint sequences built via `Test::utf8()`). */
    void feed (const std::string& bytes) noexcept { feed (bytes.data(), static_cast<int> (bytes.size())); }

    // ---- Grid access (RFC S5 scaffold shape: t.cell(row,col), t.line(row)) --

    /** @brief Cell at (row, col) on the given screen (0 = normal, 1 = alternate). */
    jam::Char cell (int row, int col, int screen = jam::terminal::Screen::normal) const noexcept
    {
        return video.getBlock (screen).getRowPointer (row)->chars[col];
    }

    /** @brief Row view at `row` on the given screen. */
    Line line (int row, int screen = jam::terminal::Screen::normal) const noexcept
    {
        return Line { video.getBlock (screen).getRowPointer (row) };
    }

    // ---- Cursor / mode --------------------------------------------------------

    int cursorRow() const noexcept { return video.getCursorRow().value; }
    int cursorCol() const noexcept { return video.getCursorCol().value; }

    /** @brief Reads a named MODES parameter (`jam::ID::xxx`) directly from the
     *  fixture-owned `jam::terminal::Model` — the same collaborator `Video`
     *  reads through `modeFlag()` (PLAN-vt-correctness.md Step 8). */
    bool mode (juce::Identifier id) const noexcept
    {
        auto* param { model.getParameter<jam::Parameter<int>> (model.getType(), id) };
        jassert (param != nullptr);   // id must be a registered MODES parameter
        return param->getValue() != 0;
    }

    // ---- Device response capture (DECRQM / DECRQSS / DA / CPR) ---------------

    const std::string& lastResponse() const noexcept { return response; }
    void clearResponse() noexcept { response.clear(); }

    // ---- OSC 0/2 title capture (`ID::title`) ---------------------------------

    const std::string& lastTitle() const noexcept { return title; }

    // ---- Direct handle for assertions the const surface above doesn't cover --

    VideoT& videoRef() noexcept { return video; }

private:
    // Declaration order is construction order — Stamp/Grapheme/Link/DecMode
    // have no deps; DecMode must exist before model (Model::registerModes()
    // iterates DecMode::getInstance()); events and model must exist before
    // video (video holds references to both); video must exist before
    // parser (parser holds a reference).
    jam::Stamp    stampInstance;
    jam::Grapheme graphemeInstance;
    jam::Link     linkInstance;
    jam::terminal::map::DecMode decModeInstance;

    jam::Function::Map<juce::Identifier, void> events;
    jam::terminal::Model  model;
    VideoT                video;
    jam::terminal::Parser parser;

    std::string response;
    std::string title;
};

/** @brief Production fixture — owns a plain `jam::terminal::Video`. */
using Term = TermBase<jam::terminal::Video>;

/** @brief Deadline-injection fixture — owns a `SyncOutputDeadlineVideo` for
 *  deterministic V2 expiry tests. */
using SyncOutputDeadlineTerm = TermBase<SyncOutputDeadlineVideo>;

} // namespace Test
