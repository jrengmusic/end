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
 * via `jam::terminal::Model` (`Video` is stateless on plain VT modes;
 * `Term::mode()` reads the fixture-owned `model` collaborator directly, the
 * same MODES parameter surface `Video::modeFlag()` reads internally). See
 * this fixture's own ratified scaffold shape (`Test::Term`, `t.feed()`,
 * `t.cell(row,col)`, `t.line(row)`, `t.cursorCol()`).
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
 * recorded in SPRINT-LOG prose and this fixture's own ratified scaffold —
 * not a literal recovery of prior source.
 *
 * @par SharedResources ownership
 * `jam::Stamp` / `jam::Grapheme` / `jam::Link` are process-scoped
 * `SharedResources<T>` singletons (`jam::Instance<T>` CRTP — see
 * `jam_core/instance/jam_Instance.h`). Every `Test::Term` owns one instance
 * of each (mirrors `jam::VulkanEngine`'s `stamp` / `grapheme` / `link`
 * members, `jam_vulkan/engine/jam_VulkanEngine.h`) so every fixture construction
 * starts a fresh interning table — indices are deterministic per test case.
 *
 * @par Model ownership
 * `jam::terminal::map::DecMode` is Model-owned by composition (ARCHITECT
 * ruling 2026-07-05, `jam_DecMode.h`) — `model` (below) holds its own
 * `decMode` plain member and iterates it directly in its constructor to
 * register the MODES parameter group; no fixture-side `DecMode` instance is
 * needed or permitted. `model` is constructor-bound into `video` exactly
 * like the owning application binds its own Model.
 *
 * @par Deadline-injection seam (V2 expiry testing)
 * `Term` is `TermBase<jam::terminal::Video>` — templated on the owned Video
 * type so a test-only subclass can be substituted without duplicating the
 * fixture's wiring. `Video::syncOutputDeadlineMs` is protected (not private)
 * for exactly this reason (`jam_CursorState.h` field doc); `SyncOutputDeadlineVideo`
 * (derived from `jam::terminal::Video`) exposes `setSyncOutputDeadlineMs()`
 * to write it directly. `SyncOutputDeadlineTerm` (`TermBase<SyncOutputDeadlineVideo>`)
 * is the fixture expiry tests (`SyncOutputTests.cpp`) use to force an
 * already-elapsed SYNC_OUTPUT deadline deterministically instead of
 * sleeping past `Video::syncResetMs`.
 *
 * @par writeToHost capture / title readback
 * `jam::terminal::Video` fires events through a caller-owned
 * `jam::terminal::Events&` (one named `Events::Entry` member per event, no
 * runtime key, `jam_VideoEvents.h`) — no override surface. `TermBase` owns
 * the `Events` value and assigns one static trampoline (`onWriteToHost`,
 * context = `this`) to the `writeToHost` member, capturing the response
 * bytes `TermBase::feed()` / `lastResponse()` need directly into a
 * `TermBase`-owned string. `title` is not an Events member — OSC 0/2 writes
 * `jam::terminal::Model`'s TEXT/title `jam::ParameterText` directly;
 * `lastTitle()` reads it straight off the fixture-owned `model`.
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
 * Scaffold shape: `t.line(0).isContinued()`.
 */
struct Line
{
    const jam::Row* row;

    /** @brief DECAWM soft-wrap flag — row content continues on the next row. */
    bool isContinued() const noexcept { return (row->flags & jam::Row::flexWrap) != 0; }

    /** @brief Row contains `Char::FLEX_GAP` — elastic whitespace for reflow. */
    bool isJustified() const noexcept { return (row->flags & jam::Row::justify) != 0; }

    /** @brief OSC 133 semantic mark stamped onto this row (`Row::markShift`/`markMask`). */
    jam::TextLine::Mark mark() const noexcept
    {
        return static_cast<jam::TextLine::Mark> ((row->flags & jam::Row::markMask) >> jam::Row::markShift);
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
 * `Video::syncResetMs`. Derives from `jam::terminal::Video` directly — the
 * response/title capture `TestVideo` used to provide is now `TermBase`-owned
 * `Events::Entry` member assignment (event capture no longer needs a subclass).
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
 * @brief Owns Events + Video + Parser + the SharedResources tables; feeds
 *        raw bytes, exposes grid/cursor/mode/response assertions.
 *
 * Owns the `jam::terminal::Events` value Video fires through, assigning the
 * `onWriteToHost` trampoline (context = `this`) to the `writeToHost` member
 * so `lastResponse()` reads a fixture-owned capture string directly — no
 * Video subclass needed for event capture. `title` is no longer an Events
 * member (OSC 0/2 now writes `jam::terminal::Model`'s TEXT/title
 * `jam::ParameterText` directly, per the ARCHITECT-ratified "any value Video
 * emits is a parameter" ruling) — `lastTitle()` reads it straight off the
 * fixture-owned `model` collaborator.
 *
 * @tparam VideoT  The owned Video type — `jam::terminal::Video` (see `Term`)
 *                  or a test subclass of it (see `SyncOutputDeadlineVideo` /
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
        // Plain member assignment — safe any time before the reader thread
        // starts (jam_VideoEvents.h "Registration is plain member assignment"
        // doc). `events` is constructed (all members default all-fallback)
        // before this body runs, since it precedes `video` in declaration
        // order below.
        events.writeToHost = { &onWriteToHost, this };

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

    /** @brief Feeds a NUL-terminated byte sequence (scaffold shape: `t.feed ("abc...")`). */
    void feed (const char* bytes) noexcept { feed (bytes, static_cast<int> (std::strlen (bytes))); }

    /** @brief Feeds a std::string payload (UTF-8 multi-codepoint sequences built via `Test::utf8()`). */
    void feed (const std::string& bytes) noexcept { feed (bytes.data(), static_cast<int> (bytes.size())); }

    // ---- Grid access (scaffold shape: t.cell(row,col), t.line(row)) --

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
     *  reads through `modeFlag()`. */
    bool mode (juce::Identifier id) const noexcept
    {
        auto* param { model.getParameter<jam::Parameter<int>> (model.getType(), id) };
        jassert (param != nullptr);   // id must be a registered MODES parameter
        return param->getValue() != 0;
    }

    // ---- Device response capture (DECRQM / DECRQSS / DA / CPR) ---------------
    // Captured by the onWriteToHost trampoline registered at construction.

    const std::string& lastResponse() const noexcept { return responseCapture; }
    void clearResponse() noexcept { responseCapture.clear(); }

    // ---- OSC 0/2 title -------------------------------------------------------
    // OSC 0/2 now writes jam::terminal::Model's TEXT/title jam::ParameterText
    // directly (jam_VideoOSC.cpp handleOscTitle()) — read it straight off the
    // fixture-owned model collaborator, the same TEXT group Video resolved
    // titleParam from at construction.

    juce::String lastTitle() const noexcept
    {
        auto* title { model.getParameter<jam::ParameterText> (jam::IDtype::text, jam::ID::title) };
        jassert (title != nullptr);   // title must be a registered TEXT parameter
        return title->getValue();
    }

    // ---- Direct handle for assertions the const surface above doesn't cover --

    VideoT& videoRef() noexcept { return video; }

private:
    /** @brief `Events::Entry` trampoline for the `writeToHost` member — appends
     *  the response bytes to the owning fixture's `responseCapture`.
     *  @param context  The owning `TermBase*`, opaque to Video. */
    static void onWriteToHost (void* context, const char* data, int length) noexcept
    {
        static_cast<TermBase*> (context)->responseCapture.append (data, static_cast<size_t> (length));
    }

    // Declaration order is construction order — Stamp/Grapheme/Link have no
    // deps; model must exist before video (video holds a reference to it —
    // model's own decMode member is iterated by Model::registerModes()
    // internally, no fixture-side DecMode owner needed); events must exist
    // before video (video holds a reference to it — the trampoline above
    // captures `this`, valid already during the member-initialiser list);
    // video must exist before parser (parser holds a reference).
    jam::Stamp    stampInstance;
    jam::Grapheme graphemeInstance;
    jam::Link     linkInstance;

    std::string responseCapture;

    jam::terminal::Model  model;
    jam::terminal::Events events;
    VideoT                video;
    jam::terminal::Parser parser;
};

/** @brief Production fixture — owns a plain `jam::terminal::Video`. */
using Term = TermBase<jam::terminal::Video>;

/** @brief Deadline-injection fixture — owns a `SyncOutputDeadlineVideo` for
 *  deterministic V2 expiry tests. */
using SyncOutputDeadlineTerm = TermBase<SyncOutputDeadlineVideo>;

} // namespace Test
