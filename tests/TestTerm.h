/**
 * @file TestTerm.h
 * @brief Test::Term — conformance-suite fixture (raw bytes -> Parser -> Video
 *        -> assert grid/cursor/mode state).
 *
 * Revives the `b3f0fea` fixture SHAPE (raw bytes in, Parser drives Video,
 * assertions read grid cells / cursor / mode state back out) against today's
 * jam_terminal API: `TerminalVideo::printCodepoint` (not `print`), grid access via
 * `TerminalVideo::getBlock()` / `jam::Block::getRowPointer()` /
 * `jam::Row::chars[col]`, cursor via `getCursorRow()`/`getCursorCol()`, modes
 * via `jam::TerminalModel` (`Video` is stateless on plain VT modes;
 * `Term::mode()` reads the fixture-owned `model` collaborator directly, the
 * same root mode-parameter surface Video's own working-copy members mirror
 * internally). See
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
 * @par Model ownership — zero Model knowledge in Video (ARCHITECT ruling 2026-07-05)
 * `jam::TerminalDecMode` is Model-owned by composition (`jam_TerminalDecMode.h`)
 * — `model` (below) holds its own `decMode` plain member and iterates it
 * directly in its constructor to register the mode parameters onto its own
 * root; no fixture-side `DecMode` instance is needed or permitted. `video` no longer
 * takes a Model reference — it fires `stateChanged`/`textChanged`/
 * `modeChanged` (`jam_TerminalEvents.h`) instead; this fixture registers
 * `onStateChanged`/`onTextChanged`/`onModeChanged` trampolines that resolve
 * those channels straight onto `model`, so every conformance assertion on
 * Model parameters keeps passing.
 *
 * @par Deadline-injection seam (V2 expiry testing)
 * `Term` is `TermBase<jam::TerminalVideo>` — templated on the owned Video
 * type so a test-only subclass can be substituted without duplicating the
 * fixture's wiring. `TerminalVideo::syncOutputDeadlineMs` is protected (not private)
 * for exactly this reason (`jam_TerminalVideo.h` field doc); `SyncOutputDeadlineVideo`
 * (derived from `jam::TerminalVideo`) exposes `setSyncOutputDeadlineMs()`
 * to write it directly. `SyncOutputDeadlineTerm` (`TermBase<SyncOutputDeadlineVideo>`)
 * is the fixture expiry tests (`SyncOutputTests.cpp`) use to force an
 * already-elapsed SYNC_OUTPUT deadline deterministically instead of
 * sleeping past `TerminalVideo::syncResetMs`.
 *
 * @par writeToHost capture / title readback
 * `jam::TerminalVideo` fires events through a caller-owned
 * `jam::TerminalEvents&` (one named `TerminalEvents::Entry` member per event, no
 * runtime key, `jam_TerminalEvents.h`) — no override surface. `TermBase` owns
 * the `Events` value and assigns one static trampoline (`onWriteToHost`,
 * context = `this`) to the `writeToHost` member, capturing the response
 * bytes `TermBase::feed()` / `lastResponse()` need directly into a
 * `TermBase`-owned string. `title` is not a data-plane Events member — OSC
 * 0/2 fires `events.textChanged`; this fixture's `onTextChanged` trampoline
 * writes `jam::TerminalModel`'s TEXT/title `jam::ParameterText` directly;
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

    /** @brief Row contains `Char::flexGap` — elastic whitespace for reflow. */
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
 * `TerminalVideo::syncResetMs`. Derives from `jam::TerminalVideo` directly — the
 * response/title capture `TestVideo` used to provide is now `TermBase`-owned
 * `TerminalEvents::Entry` member assignment (event capture no longer needs a subclass).
 *
 * @see jam::TerminalVideo::syncOutputDeadlineMs
 */
class SyncOutputDeadlineVideo : public jam::TerminalVideo
{
public:
    using jam::TerminalVideo::TerminalVideo;

    /** @brief Sets `syncOutputDeadlineMs` directly, bypassing the
     *  `setPrivateModes()` DECSET 2026 arm computation. */
    void setSyncOutputDeadlineMs (double ms) noexcept { syncOutputDeadlineMs = ms; }
};

/**
 * @class TermBase
 * @brief Owns Events + Video + Parser + the SharedResources tables; feeds
 *        raw bytes, exposes grid/cursor/mode/response assertions.
 *
 * Owns the `jam::TerminalEvents` value Video fires through, assigning the
 * `onWriteToHost` trampoline (context = `this`) to the `writeToHost` member
 * so `lastResponse()` reads a fixture-owned capture string directly — no
 * Video subclass needed for event capture. `title` is no longer an Events
 * member (OSC 0/2 now writes `jam::TerminalModel`'s TEXT/title
 * `jam::ParameterText` directly, per the ARCHITECT-ratified "any value Video
 * emits is a parameter" ruling) — `lastTitle()` reads it straight off the
 * fixture-owned `model` collaborator.
 *
 * @tparam VideoT  The owned Video type — `jam::TerminalVideo` (see `Term`)
 *                  or a test subclass of it (see `SyncOutputDeadlineVideo` /
 *                  `SyncOutputDeadlineTerm`).
 */
template<typename VideoT>
class TermBase
{
public:
    TermBase (int cols, int rows)
        : video (jam::Cell::Rectangle (jam::Cell (cols), jam::Cell (rows)), events)
        , parser (video)
    {
        // Plain member assignment — safe any time before the reader thread
        // starts (jam_TerminalEvents.h "Registration is plain member assignment"
        // doc). `events` is constructed (all members default all-fallback)
        // before this body runs, since it precedes `video` in declaration
        // order below.
        events.writeToHost  = { &onWriteToHost, this };
        events.stateChanged = { &onStateChanged, this };
        events.textChanged  = { &onTextChanged, this };
        events.modeChanged  = { &onModeChanged, this };

        // Video's ctor allocates the grid from `dims` but leaves cols/visibleRows
        // at their {80}/{24} defaults — setWinsize() is the mandatory sync step
        // (jam_TerminalVideo.h TerminalVideo::setWinsize doc).
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
    jam::Char cell (int row, int col, int screen = Id::Screen::normal) const noexcept
    {
        return video.getBlock (screen).getRowPointer (row)->chars[col];
    }

    /** @brief Row view at `row` on the given screen. */
    Line line (int row, int screen = Id::Screen::normal) const noexcept
    {
        return Line { video.getBlock (screen).getRowPointer (row) };
    }

    // ---- Cursor / mode --------------------------------------------------------

    int cursorRow() const noexcept { return video.getCursorRow().value; }
    int cursorCol() const noexcept { return video.getCursorCol().value; }

    /** @brief Reads a named root mode parameter (`Id::xxx`) directly from
     *  the fixture-owned `jam::TerminalModel` — kept in sync with Video's
     *  own working-copy member via `events.modeChanged`'s trampoline (Video
     *  holds zero Model knowledge). */
    bool mode (juce::Identifier id) const noexcept
    {
        auto* param { model.getParameter<jam::Parameter<int>> (model.getType(), id) };
        jassert (param != nullptr);   // id must be a registered root mode parameter
        return param->getValue() != 0;
    }

    // ---- Device response capture (DECRQM / DECRQSS / DA / CPR) ---------------
    // Captured by the onWriteToHost trampoline registered at construction.

    const std::string& lastResponse() const noexcept { return responseCapture; }
    void clearResponse() noexcept { responseCapture.clear(); }

    // ---- OSC 0/2 title -------------------------------------------------------
    // OSC 0/2 fires events.textChanged (jam_TerminalVideoOSC.cpp setTitle()); this
    // fixture's onTextChanged trampoline writes jam::TerminalModel's
    // TEXT/title jam::ParameterText — read it straight off the fixture-owned
    // model collaborator, the same TEXT group the trampoline resolves onto.

    juce::String lastTitle() const noexcept
    {
        auto* title { model.getParameter<jam::ParameterText> (Id::toType (Id::text), Id::title) };
        jassert (title != nullptr);   // title must be a registered TEXT parameter
        return title->getValue();
    }

    // ---- Direct handle for assertions the const surface above doesn't cover --

    VideoT& videoRef() noexcept { return video; }

private:
    /** @brief `TerminalEvents::Entry` trampoline for the `writeToHost` member — appends
     *  the response bytes to the owning fixture's `responseCapture`.
     *  @param context  The owning `TermBase*`, opaque to Video. */
    static void onWriteToHost (void* context, const char* data, int length) noexcept
    {
        static_cast<TermBase*> (context)->responseCapture.append (data, static_cast<size_t> (length));
    }

    /** @brief `TerminalEvents::Entry` trampoline for the `stateChanged` member —
     *  resolves `(tag, id)` onto `model`'s `jam::Parameter<int>` and stores
     *  `value`. A null `tag` (the former VIDEO group's four scalars) resolves
     *  to `model.getType()`.
     *  @param context  The owning `TermBase*`, opaque to Video. */
    static void onStateChanged (void* context, juce::Identifier tag, juce::Identifier id, int value) noexcept
    {
        auto* self { static_cast<TermBase*> (context) };
        auto* parameter { self->model.getParameter<jam::Parameter<int>> (
            tag.isValid() ? tag : self->model.getType(), id) };
        jassert (parameter != nullptr);
        parameter->setValue (value);
    }

    /** @brief `TerminalEvents::Entry` trampoline for the `textChanged` member —
     *  resolves `(tag, id)` onto `model`'s `jam::ParameterText` and stores
     *  `(chars, length)`. Backs `lastTitle()`.
     *  @param context  The owning `TermBase*`, opaque to Video. */
    static void onTextChanged (void* context, juce::Identifier tag, juce::Identifier id, const char* chars, int length) noexcept
    {
        auto* self { static_cast<TermBase*> (context) };
        auto* parameter { self->model.getParameter<jam::ParameterText> (tag, id) };
        jassert (parameter != nullptr);
        parameter->setValue (chars, length);
    }

    /** @brief `TerminalEvents::Entry` trampoline for the `modeChanged` member —
     *  forwards the decoded DEC private / ANSI mode change to
     *  `model.setMode()`. Backs `mode()`.
     *  @param context  The owning `TermBase*`, opaque to Video. */
    static void onModeChanged (void* context, bool isPrivate, int number, int value) noexcept
    {
        static_cast<TermBase*> (context)->model.setMode (isPrivate, number, value);
    }

    // Declaration order is construction order — Stamp/Grapheme/Link have no
    // deps; model's own decMode member is iterated by Model::registerParameters()
    // internally, no fixture-side DecMode owner needed. `video` no longer
    // holds a Model reference (zero Model knowledge) — the trampolines above
    // reach `model` only when `events` actually fires (after full
    // construction), so no inter-member reference-lifetime ordering is
    // required between `model` and `video`; `events` must still exist
    // before `video` (video holds a reference to it — the trampolines
    // above capture `this`, valid already during the member-initialiser
    // list); `video` must exist before `parser` (parser holds a reference).
    jam::Stamp    stampInstance;
    jam::Grapheme graphemeInstance;
    jam::Link     linkInstance;

    std::string responseCapture;

    jam::TerminalModel  model;
    jam::TerminalEvents events;
    VideoT                video;
    jam::TerminalParser parser;
};

/** @brief Production fixture — owns a plain `jam::TerminalVideo`. */
using Term = TermBase<jam::TerminalVideo>;

/** @brief Deadline-injection fixture — owns a `SyncOutputDeadlineVideo` for
 *  deterministic V2 expiry tests. */
using SyncOutputDeadlineTerm = TermBase<SyncOutputDeadlineVideo>;

} // namespace Test
