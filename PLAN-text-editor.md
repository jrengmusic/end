# PLAN: Universal Text Editor Foundation — Terminal-as-Editor over a Multi-Screen Document

**RFC:** RFC-text-editor.md — §1 (END's text rendering IS a universal text editor) realized literally. §2–§9 (ParagraphsModel, commit/live, two-kind `LineTarget` transport) **superseded** by the in-session decisions captured below.
**Date:** 2026-06-01
**BLESSED Compliance:** verified
**Language / Framework Constraints:** C++17 / JUCE — LANGUAGE.md: C++/JUCE is the BLESSED reference, no overrides. No build commands (ARCHITECT only); @Auditor validates against CONTRACT, not compilation.

---

## Overview

Model the terminal as a **universal text editor**. One `jam::CodeModel` per `Session` holds the document; the document is a **dimensionless** sequence of logical lines (`jam::CodeLine`), and **dimension + wrapping live only in the projector** (`jam::CodeView`). This is the buffer/window separation verbatim from both references — neovim (`buf_T`/`win_T`) and JUCE (`CodeDocument`/`CodeEditorComponent`).

The terminal needs the VT dual-screen (primary + alternate). Rather than two documents or a separate alt surface, `CodeModel` holds **N screens** (`jam::Owner<jam::CodeLine::Screen>`, `numScreens` ctor param — `1` for a plain editor, `2` for a terminal) with an active-screen selector. `CodeView` projects whichever screen is active.

The reader thread runs **Video** as a conformant VT emulator over a **fixed viewport** (`rows × cols`) — the single VT parser. Video writes the **state machine** (cursor, dirty rows, active-screen, modes) to `State` and pushes changed viewport-row cells through `CellFifo` (two `jam::BufferSPSC` drop-oldest channels). The message thread runs **Display**, which mirrors the viewport into the active screen's **live tail** and finalizes scrolled-off lines into **width-free history**.

Because history is dimensionless and **never sourced from the grid**, resize re-wraps it at paint and **content corruption on resize is structurally impossible** — the objective.

---

## Language / Framework Constraints

C++/JUCE reference — no overrides. Cross-thread transport is **`jam::BufferSPSC`** (`jam_core/concurrency/jam_buffer_spsc.h`) — an index-only fork of `juce::AbstractFifo` (same interface) with producer-side **drop-oldest**; CellFifo owns the byte storage + per-slot **seqlock** torn-guard (Decision B — see D7). `std::deque<jam::CodeLine>` per screen; `jam::Owner<Screen>` (owns by `unique_ptr`, exposes by reference) for the screen set — no naked owning pointers, no friend classes, no forward declarations (PIMPL permitted). Submodule headers include nothing; all includes at the module header. Positive checks only; no early returns; no magic indices (screen indexing via enum).

---

## The Model — locked decisions (the correctness law)

These are the decisions reached with ARCHITECT this session. Each is binding; deviation is a discrepancy to STOP on, not a choice.

### Document & screens

- **D1 — One `CodeModel` per `Session`; screens live inside `CodeModel`.** `CodeModel` holds `jam::Owner<jam::CodeLine::Screen>` sized by a `numScreens` ctor param (default `1` = plain editor; terminal passes `2`). An active-screen index selects the live screen. Screen ownership is **not** in `Session` (no external array) and **not** in `CodeView`.
- **D2 — `jam::CodeLine::Screen` bundles lines + policy.** Nested type: `{ std::deque<jam::CodeLine> lines; int capacity; <policy fields>; }` — each screen carries its own capacity/policy with its lines (no parallel arrays on `CodeModel`). Primary (index 0): width-free, scrollback-bounded (`defaultScrollbackCapacity` = 10000), autowrap-join on finalization. Alternate (index 1): capacity = viewport rows, no scrollback, no autowrap-join — a transient grid mirror.
- **D3 — Content is dimensionless; dimension + wrap live only in the projector.** Each `Screen` holds a `std::deque<jam::CodeLine>` of logical lines with no column dimension (verbatim neovim `buf_T`/`memline`, JUCE `CodeDocument`). Width enters once, at paint, via `jam::CodeLine::getWrappedLines` in `CodeView` (verbatim neovim `win_T` wrap-at-`win_line`, JUCE `CodeEditorComponent`). *(Confirmed by neovim source: `buf_T` holds no width/height; `win_T.w_view_width` + `plines_win_nofold` compute wrap at draw over an unchanged buffer.)*
- **D4 — `CodeView` keeps a fixed `const jam::CodeModel&`.** No `setModel`, no reseatable handle. `CodeView` projects the **active** screen; `CodeModel::getLine`/`getNumLines` delegate to the active screen. A screen switch is an internal `CodeModel` state change, invisible to `CodeView`'s reference.

### Threads & transport

- **D5 — Reader thread: `Processor` orchestrates `Video`.** Video is the single VT parser, executing the full VT spec over a fixed viewport (`rows × cols`). All in-viewport editing (autowrap, `ICH`/`DCH`/`IL`/`DL`, erase, scroll) is Video being Video — there is no second VT state machine.
- **D6 — `State` is the state machine (SSOT carrier across the thread boundary).** It carries cursor (viewport space), caret (projection target), dirty rows, active-screen, and modes. Coalesced 60 Hz flush (existing APVTS-style mechanism). `State` never holds viewport cells. **No `topLine`, no `liveRows` State param:** viewport row 0 is the constant 0; the live-tail extent is NOT state-machine truth — it is Display's private record of what it last appended (see D10). The dead `id::liveRows` param was removed.
- **D7 — Transport = `jam::BufferSPSC`, an index-only drop-oldest fork of `juce::AbstractFifo` (Decision B, locked).** `CellFifo` carries `jam::Char` rows reader→message via two method pairs: `pushHistory`/`drainHistory` and `pushActive`/`drainActive`. No `jam::LineTarget` tag — the channel is the discriminator. `CellFifo` owns two `jam::BufferSPSC` instances (history, active) replacing its prior two `juce::AbstractFifo` rings, and owns the two `HeapBlock<jam::Char>` storage buffers + the seqlock guard (D7b).
  - **D7a — `jam::BufferSPSC` is an index-only manager (jam_core), Decision B.** Forked from `juce::AbstractFifo`'s index arithmetic (`validStart`/`validEnd`, `bufferSize`, wrap formulas, free-space sentinel) with the **same interface as `juce::AbstractFifo`** (`getTotalSize`/`getFreeSpace`/`getNumReady`/`reset`/`setTotalSize`/`prepareToWrite`/`finishedWrite`/`prepareToRead`/`finishedRead`). It holds NO payload storage — like AbstractFifo, the caller (CellFifo) owns the byte buffer and drives the memcpy against the returned indices. The ONE behavioral change vs AbstractFifo: **producer-side drop-oldest** — on `prepareToWrite` when full, the producer advances `validStart` via a single `compare_exchange_strong` (no retry loop; CAS-fail means the consumer already freed space) to evict the OLDEST entry until the requested space fits, so the write always succeeds (never refuses/stalls).
  - **D7b — torn-read guard lives in CellFifo, Decision B.** Because drop-oldest makes the producer advance `validStart`, the producer can reclaim the byte region the consumer is mid-`memcpy` on (between `prepareToRead` and `finishedRead`) — a data race on CellFifo's `HeapBlock`. The guard is a per-slot **seqlock** (epoch even=stable, odd=write-in-progress) maintained by **CellFifo** around its own memcpy: producer stamps odd→writes→stamps even; consumer reads epoch before+after the copy and discards any entry the producer reclaimed mid-read. `jam::BufferSPSC` provides indices + drop-oldest only; CellFifo owns storage + seqlock. (Decision B over A: A folds storage+seqlock into the primitive; B keeps the primitive index-only and the guard in CellFifo. ARCHITECT chose B.)
  - **D7c — drop-oldest IS the bounded scrollback, not loss.** On the history channel, evicting the oldest ring entry under flood is lossless w.r.t. the retained scrollback — those lines would be front-evicted from `CodeModel` anyway (`jam_code_model.h:106`). The producer never stalls the reader/PTY. *(Proven: under sustained `seq 10M`-class flood at 60Hz drain, AbstractFifo's drop-NEWEST kept the wrong end; drop-oldest keeps the latest window.)*
- **D8 — Message thread: `Display` orchestrates `CodeView`.** Per flush (on-disk `Display.cpp` `screenDirty`/`activeScreen` branch): read active-screen → `setWrapEnabled(active == normal)` → `setActive(activeScreen)` on the document → remove the previous live tail of that screen using `liveTailExtent[activeScreen]` → `drainHistory` → `CodeModel::append` each departed line into history → `drainActive` → append each live row, counting → store the count into `liveTailExtent[activeScreen]` → `calc()`. Caret is set in the separate `id::cursor` branch. The drained count IS the live-tail extent (D10).

### Cursor, anchor, finalization

- **D9 — One cursor, in viewport dimension, authored by Video.** The logical cursor is Video's viewport cursor (it executes VT, so it owns the position). `State` carries it. The **caret** is its projection — `CodeView`'s render coordinate. Display maps cursor → caret. There is exactly one position; no shadow second cursor.
- **D10 — the live tail is what the consumer just drained; its extent is the count of drained active rows.** Each tick the producer pushes the live region to the active ring (`[0..cursorRow]` for the active screen, `ProcessorEvents.cpp`); the consumer drains them, appends them, and the **count of rows it drained IS the live-tail extent**. Producer and consumer agree by construction — the consumer never needs a formula, it counts what it drained. Display holds the per-screen extent it last laid down as **internal transient** (`std::array<int, Map::Screen::count>`, message-thread-only, no getter) so it can remove the previous tail before appending the new one. NOT shadow state: it is Display's private record of its own document mutation, held nowhere else. The dead `id::liveRows` param (`Parameters.xml`, `Identifier.h` — zero readers/writers) is removed.
- **D11 — Autowrap-join at finalization only.** The live tail stores grid rows 1:1 with the viewport; `jam::CodeLine::isContinued` marks an autowrap continuation. Departed lines arrive on the **history channel** already joined (the join lives in `CellFifo`'s history-ring `pending` accumulator: a contiguous `isContinued` run accumulates into one width-free `CodeLine`, emitted when a non-continued row completes it). Cursor → viewport-row mapping stays direct.
- **D12 — Resize.** History is untouched and re-wrapped at paint (`getWrappedLines`); the live tail is re-mirrored from Video's reflowed viewport; the cursor is re-derived in the new viewport. History is never grid-sourced — this is the resize-safety guarantee.

### Alternate screen

- **D13 — Alt is a screen, not a separate surface.** Switching screens selects the active screen inside `CodeModel`; the primary screen's content is preserved untouched while alt is active (verbatim neovim hidden-buffer). One render path. **Alt is cleared on entry (D13a):** on `?1049h`, the alternate `CodeModel` screen is cleared (VT spec). Alt has no scrollback and is the transient grid.
- **D14 — Screen-switch trigger + alt does not project.** Video parses `DECSET 1047/1049` on the reader thread and sets the active-screen flag in `State`; Display reads it and flips the active screen on the message thread. **Alt does NOT project (D14a):** the normal screen projects (history + live tail, wrap on, caret mapped through wrapped lines above the cursor); the alternate screen is the transient grid — no projection, no wrap, no history offset, no caret-through-wrap. The projection/caret-offset logic in `CodeView` is normal-screen only (`setWrapEnabled(false)` for alt).
- **D15 — Caret projects through wrap on the normal screen (fix).** The cursor is authored in viewport space (row,col). On the normal screen with wrap on, the caret must be placed at the PROJECTED position — mapped through the wrapped row-spans of the live lines above the cursor — not by a flat `projectedRows − viewportHeight` offset. (The current `jam_code_view.cpp` offset is the bug to fix.) Alt screen: viewport row = screen row, no mapping.

---

## Reused existing infrastructure (no reinvention)

These already exist in the tree and are the mechanisms the steps build on — not new work:

- **`State`** — `getActiveScreen`/`setScreen`, `id::screenDirty`, `id::cursor` (per-screen `CursorState::pack/unpack`), `NORMAL`/`ALTERNATE` child nodes, `setRowDirty`/`consumeRowDirty`/`rebuildRowDirtyFlags` (the per-row dirty mechanism = "dirty rows"), 60 Hz flush.
- **`Map::Screen::normal` / `Map::Screen::alternate`** — the screen identity map.
- **`CodeView`** — `setWrapEnabled`, `setCaretPosition` (viewport-relative → maps to document space internally), `calc()`, `scrollToBottom()`, fixed `const CodeModel& document`.
- **`CellFifo`** (`Source/terminal/CellFifo.h`) — two-channel `jam::Char`-row transport, header-per-entry, `[Header(cellCount,flags)|cells]` packing, history-ring continued-row join. Owns two `jam::BufferSPSC` rings (replacing the prior two `juce::AbstractFifo`). Signatures `pushHistory`/`pushActive`/`drainHistory`/`drainActive`/`setSize` — callers (Processor/ProcessorEvents/Display) unchanged.
- **`juce::AbstractFifo`** (vendored, JUCE 8.0.12) — the index-coordination primitive `jam::BufferSPSC` forks (same interface; caller owns the bytes). The prior `CellFifo` paired it with two `HeapBlock<jam::Char>` (non-dropping); `jam::BufferSPSC` is that same pairing with producer drop-oldest added.
- **`Session`** — owns `codeModel` (declared before `textEditor`, outlives it) + `getCodeModel()`/`getTextEditor()`.
- **`jam::Owner<T>`** (`jam_core/utilities/jam_owner.h`) — `unique_ptr` vector with sized ctor `Owner(size_t, Args...)`.

---

## New names (Decision Gate — pending ARCHITECT ratification before introduction)

Per CAROL Rule "no improvised names." These are required by the model; each needs ARCHITECT's word before @Engineer writes it:

- ~~`Screen`~~ → **ratified `jam::CodeLine::Screen`** (nested under `CodeLine`).
- **ratified:** `setActive(int)` / `getActive()` active-screen API on `CodeModel`; `numScreens` ctor param.
- **No State param** for the live/history boundary — the dead `id::liveRows` was removed. The live-tail extent is Display's internal transient `liveTailExtent` (`std::array<int, Map::Screen::count>`, message-thread-only, no getter), set to the drained active-row count each tick. (`topLine` was added then reverted; a stored `liveRows` would be shadow state.)
- **ratified:** `jam::BufferSPSC` (`jam_core/concurrency/jam_buffer_spsc.h`) — index-only fork of `juce::AbstractFifo` (same interface) + producer drop-oldest. Decision B: holds no storage; CellFifo owns the HeapBlock + seqlock guard.
- **ratified:** `CellFifo` = **two independent SPSC rings**, no tag — the channel is the discriminator. `pushHistory`/`drainHistory` (departed scrollback lines, with continued-row join) on the history ring; `pushActive`/`drainActive` (live viewport rows, no join) on the active ring. Replaces the single tagged `pushRow`/`drainNext`+`jam::LineTarget`. ("history" = departed lines; "active" = at-viewport / live region — `activeScreen` separately means *which* screen.) Two rings chosen for: type-safe discriminator (no corruptible tag field), independent backpressure, and no viewport-vs-scrollback starvation under flood (the visible viewport update never waits behind a scrollback flood). Cost: consumer drains history-to-empty before active (one ordering rule).
- **ratified:** `CellFifo::reset` → **`setSize(int historyCapacityInChars, int activeCapacityInChars)`** (rename: it reallocates the rings, a resize not a reset). Caller reads both sizes from State — history = `scrollbackLines × cols`, active = `visibleRows × cols` — and passes them; `CellFifo` holds no State reference (stays a dumb transport).
- Any accessor for per-screen capacity/policy configuration on `CodeModel`.

---

## Validation Gate

Each step is validated by @Auditor before the next against: MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and these locked decisions (D1–D15). No reintroduction of `LineTarget`/commit-live/grid→history sync; no second cursor; no dimension stored in content; no naked owning pointers.

---

## Steps

> Steps 1–5 are the completed foundation. Steps 6–13 implement D1–D15. New names (above) must be ratified by ARCHITECT before the steps that introduce them.

### Step 1–5 (DONE): document + view foundation
`jam::CodeLine`, single-screen `jam::CodeModel`, `jam::CodeView`, `glyph::Arrangement::shape(CodeModel)`, Video `grid` rename, removal of fabrication/`StringArray`/forward-decl hack. No further action.

### Step 6: `CodeModel` — multi-screen
**Scope:** `jam_graphics/detail/jam_code_model.h`; `jam_graphics/detail/` (new `Screen` type).
**Action:** Introduce `jam::CodeLine::Screen` (`std::deque<jam::CodeLine> lines; int capacity; <policy fields>`). `CodeModel` holds `jam::Owner<jam::CodeLine::Screen>` sized by `numScreens` ctor param (default 1). Add active-screen index + `setActive(int)`/`getActive()`. Route `append`/`replaceAt`/`remove`/`clear`/`getLine`/`getNumLines`/`setCapacity` to the active screen. Configure index 0 = primary policy (D2), index 1 = alternate policy when `numScreens == 2`. `CodeModel` indexes screens by `int` only — it is generic (jam_graphics) and must not know terminal concepts; the terminal caller passes the index via its own `Map::Screen` constants. No magic-number literals inside `CodeModel` beyond the active-index default of 0.
**Validation:** dimensionless content (no width/col field on `Screen`); `Owner` ownership (no naked pointers); active-screen delegation correct; default `numScreens == 1` preserves plain-editor behavior; names match ratified set.

### Step 7: `Session` — construct terminal with two screens
**Scope:** `Source/terminal/Session.{h,cpp}`.
**Action:** Construct `codeModel` with `numScreens = 2`. No structural change otherwise (`codeModel` stays declared before `textEditor`; `getCodeModel`/`getTextEditor` unchanged).
**Validation:** construction order intact (model before view, outlives it); terminal gets 2 screens; remote-session ctor path unaffected.

### Step 8: `State` — live/history boundary (DONE — no new param)
**Scope:** `Parameters.xml`, `Identifier.h` (dead-param removal — done on disk).
**Action:** No State parameter for the live/history boundary. The live-tail extent is Display's internal transient `liveTailExtent` (`std::array<int, Map::Screen::count>`), set to the drained active-row count each tick (D10) — a stored param would be shadow state. The dead `id::liveRows` param was removed.
**Validation:** no `liveRows` param remains (grep-confirmed zero); extent is Display-private, no getter; no shadow state.

### Step 9: `CellFifo` — two channels on `jam::BufferSPSC` (drop-oldest)
**Scope:** `jam_core/concurrency/jam_buffer_spsc.h` (the primitive), `Source/terminal/CellFifo.h` (the wiring).
**Action (two parts):**
- **9a — `jam::BufferSPSC`:** index-only fork of `juce::AbstractFifo`, Decision B. Same interface as AbstractFifo (`prepareToWrite`/`finishedWrite`/`prepareToRead`/`finishedRead`/`getFreeSpace`/`getNumReady`/`getTotalSize`/`reset`/`setTotalSize`). Holds no storage. ONE change vs AbstractFifo: `prepareToWrite` on full advances `validStart` (CAS drop-oldest) until the requested space fits, so the write never refuses. Layer-pure: jam_core, names no jam_graphics type. The producer/consumer torn-read on the caller's bytes is NOT the primitive's concern — the guard is in CellFifo (9b).
- **9b — `CellFifo`:** replace its two `juce::AbstractFifo` members with two `jam::BufferSPSC`; CellFifo keeps its two `HeapBlock<jam::Char>` and adds the per-slot **seqlock** (epoch array per ring) around its memcpy — producer stamps odd→writes→stamps even; consumer reads epoch before+after the copy and discards entries reclaimed mid-read. `jam::LineTarget` and the tagged `pushRow`/`drainNext` removed; channel is the discriminator. History ring keeps the continued-row join (`pending`); active ring no join. `pushHistory`/`pushActive`/`drainHistory`/`drainActive`/`setSize(historyCap, activeCap)` signatures unchanged → Processor/ProcessorEvents/Display callers untouched. With drop-oldest in the ring, `pushHistory`'s prior `getFreeSpace`-then-skip guard is removed — the write always succeeds (oldest evicted).
**Validation:** no `LineTarget` remnant; two drop-oldest rings; whole-row eviction (no severed rows — drop advances past whole `[Header|cells]` entries); seqlock guards CellFifo's memcpy (zero torn under concurrent drop); history join preserved on the history ring only; active = one row per entry; CellFifo public signatures unchanged; smoke test zero torn / zero partial / FIFO order / producer never stalls.

### Step 10: Producer — Video emits state machine + cells (reader)
**Scope:** `Source/terminal/Video.*`, `Source/terminal/ProcessorEvents.cpp`, `Source/terminal/Processor.*`.
**Action:** On the reader thread, Video (full VT over the fixed viewport) writes cursor (D9), dirty rows, and the active-screen flag (D14, from `DECSET 1047/1049`) to `State`. The two emission points: `id::pushLine` (departing line) → `cellFifo.pushHistory(...)`; `id::screenDirty` (live viewport rows `[0..cursorRow]`) → `cellFifo.pushActive(...)`. One VT parser; no grid→document sync. (No `liveRows` authored — the live-tail extent is the consumer's drained count, D10.)
**Validation:** reader-thread only; single VT parser; cursor/active-screen authored here; no second cursor; alt and normal output both flow through the dirty-row mechanism.

### Step 11: Consumer — Display mirrors tail, finalizes history, projects caret (message)
**Scope:** `Source/terminal/component/Display.cpp`.
**Action:** Replace the `LineTarget` drain loop with, in order: read active-screen → `setWrapEnabled(active == normal)`; `setActive(activeScreen)` on the document; remove the previous live tail **of that screen** using the per-screen extent `liveTailExtent[activeScreen]`; `drainHistory` → `CodeModel::append` each departed line into history (front-eviction bounds it); `drainActive` → append each live row, counting; store the count into `liveTailExtent[activeScreen]`; `calc()`. The drained count IS the extent — no formula. `liveTailExtent` is `std::array<int, Map::Screen::count>`, private, message-thread-only, no getter. Remove the dead `getTextLineArray` path and the dead `id::liveRows` State param.
**Validation:** message-thread only; single document, one cursor; per-screen `liveTailExtent` (no cross-screen contamination after `setActive`); history immutable + width-free; live tail = drained active rows; extent is internal transient (no getter, no stored param, no shadow state); caret unchanged in the `id::cursor` branch; alt screen has no history (pushHistory gated to normal).

### Step 12: Resize path — re-wrap history, re-mirror tail, re-derive cursor
**Scope:** `Source/terminal/component/Display.cpp`, `Source/terminal/` resize (`Resizer`) wiring, `CodeView` projection.
**Action:** On viewport dimension change: history untouched (re-wrapped at paint via `getWrappedLines`); live tail re-mirrored from Video's reflowed viewport; cursor re-derived in the new viewport (D12). Confirm alt screen capacity tracks viewport rows on resize.
**Validation:** history bytes never re-sourced from the grid; scrollback survives arbitrary resize; cursor lands correctly post-resize; alt capacity = new viewport rows.

### Step 13: Sweep + doxygen + ARCHITECTURE.md
**Scope:** repo-wide.
**Action:** Remove all superseded symbols/comments (`LineTarget`, commit/live, grid-sync, two-target `CellFifo` doc, `id::liveRows`). Fix stale doxygen (`Processor.*` "TextEditor"; `Session` `getTextEditor`/"TextEditor" naming vs `CodeView` — flag for ARCHITECT). Update ARCHITECTURE.md to the terminal-as-editor model: dimensionless multi-screen `CodeModel`, viewport-mirrored live tail (`liveTailExtent` drained-count boundary), width-free history, `jam::BufferSPSC` drop-oldest transport, single-cursor projection, screen-switch path.
**Validation:** no superseded symbol or comment remains; ARCHITECTURE.md mirrors the implemented model.

---

## BLESSED Alignment

- **B (Bedrock/simplicity):** one `CodeModel`, one render path, one projection, one VT parser, one lock-free boundary. Alt is a screen, not a second subsystem.
- **L (Lean/YAGNI):** no `LineTarget`/commit-live/active-channel; `numScreens` fixed at the call site (1 or 2), no speculative N-channel generality beyond the ctor param the universal editor already needs.
- **E (Explicit):** single authoritative cursor; positive checks; no early returns; screen indexing via enum (no magic numbers).
- **S (SSOT):** the document is the single content truth; `State` is the single state-machine truth; history built by finalization, never mirrored from the grid.
- **S (Stateless/dumb objects):** `CellFifo` and `CodeView` are dumb; caret is derived, not stored twice; `Screen` is a passive line store.
- **E (Encapsulation):** `CodeView` never names the terminal; content is dimensionless; alt isolation is structural (a screen), `CodeModel` exposes screens only via API.
- **D (Deterministic):** one cursor + one projection; resize re-wraps deterministically from width-free history.

---

## Risks / Open Questions

- **OPEN — `jam::BufferSPSC` rebuild to Decision B (not yet written).** The on-disk `jam_buffer_spsc.h` is a `std::vector<Slot>` storage-owning ring — superseded. Decision B: rebuild it as an index-only fork of `juce::AbstractFifo` (same interface) + producer drop-oldest, NO storage, NO seqlock (the seqlock moves to CellFifo, 9b). Step 9b is blocked until 9a lands. **This is the one unresolved transport item.**
- **Locked this session:** D1–D15 model decisions; Decision B (BufferSPSC index-only + drop-oldest; CellFifo owns storage + seqlock).
- **Steps 6–8, 10, 11** are implemented on disk (multi-screen `CodeModel`, two-screen `Session`, dead-param removal, producer emission points, Display consumer with `liveTailExtent`). The tree's compile status past these is **unverified** — the build was never confirmed green this session; convergence (Step 12/13 + compile) follows the BufferSPSC rebuild.
- **RFC body §2–§9** fully superseded by D1–D15; ARCHITECT to decide rewrite/strike post-sprint.
