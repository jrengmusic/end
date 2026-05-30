# PLAN: Text-Editor Restructure — Universal Foundation

**RFC:** RFC-text-editor.md (consumed and superseded where this plan diverges — see §Divergences)
**Date:** 2026-05-30
**BLESSED Compliance:** verified
**Language:** C++17 / JUCE — MANIFESTO.md reference implementation, no overrides

## Objective (ARCHITECT)

1. **Fix the mental model** — TextEditor is the lossless content SSOT; Video is the transient viewport grid; projection makes content survive mid-stream viewport-dimension changes.
2. **Fix the foundation, bottom-up** — the actual types and storage we need: `jam::Char` (moved out of `Cell`), `jam::String` (renamed from `Row`), `jam::Buffer<String>` storage. Delete what was wrong.
3. **Fix the logic upstream** — Video / Processor / TextEditor / Display rebuilt on the correct foundation, with the SSOT invariants enforced structurally.

This is a refactor. CAROL Refactor-Rewrite Discipline: delete the old form first; cascade breakage is the ground of truth. Agents run no build commands — @Auditor validates each step against CONTRACT, not compilation.

---

## §0 Mental Model (Objective 1) — the law every step serves

**Facts (ARCHITECT, ground truth):**
- Video sees **only the viewport dimension**, logically. It is the VT execution surface — `print`/`scroll`/`erase` mutate a viewport-sized grid on the **reader thread**. Transient.
- The viewport dimension is **dynamic mid-stream** — the scrollbar appears/disappears and always consumes dimension. Content must render correctly when that dimension changes at any instant. Therefore: **project at render**, never bake width into storage.
- No published terminal preserves history above the active prompt across resize — all are lossy/destructive. Ours is not. This is the deliverable.
- Our renderer **is an actual text editor**: structure/topology verbatim JUCE (`juce::Viewport`, paragraph model), logic verbatim neovim (`buf_T` memline + libvterm grid). neovim is a text editor integrated with the terminal domain — that is the model.

**The SSOT split:**
- `Video` owns the **live viewport grid** (reader thread). Sole source of live truth.
- `TextEditor` owns the **lossless document** — history + the projected visible tail (message thread). The render SSOT.
- `getWrappedLines(viewWidth)` is the projection: logical line → screen rows at the *current* viewport width. Storage is width-free.

**The three invariants (cure for `DEBT-20260530T100000` — resize content destruction):**
- **I1 — Commit is the only history writer.** A row enters history exactly once, on scroll-off, via the cross-thread bridge. Nothing else writes history.
- **I2 — Flush writes only the live visible tail.** The per-frame live update never touches history.
- **I3 — Viewport-dimension change never re-sources history from Video.** Video reflows its *own* grid; TextEditor re-projects history to the new width via `getWrappedLines`; history storage is untouched.

Under I1–I3 the materialized visible tail is a strictly one-way, fully-refreshed projection from a single source — a cache, **not** drift-prone shadow. The current bug is an I2/I3 leak; this plan makes the boundary structural.

**I4 — State is the SSOT for every legitimate state machine.** Per-row dirty is a state machine and is owned by `State` (`State::rowDirtyFlags`, `std::atomic<int>[]`, thread-safe). No object keeps a parallel dirty tracker. Video marks `State::rowDirtyFlags[row]` atomically on cell write (reader thread); the message thread reads/clears via `exchange(0)` on flush. The Video-local `rowTouched` array and the `id::rowDirty` event relay are shadows of this SSOT and are deleted.

## Validation Gate

Each step validated by @Auditor before the next: diff complies with MANIFESTO.md (BLESSED), NAMES.md (Rule -1 — no improvised names), ~/.carol/JRENG-CODING-STANDARD.md, the locked decisions below, **and §0 invariants** for any step touching write paths. Compilation is not an intermediate gate (cascade breakage expected).

---

## §1 Locked Decisions (the foundation, Objective 2)

**Type model — two levels, one line type:**
- `jam::Char` — the 8-byte packed attributed atom. The universal shaping input.
- `jam::String` — a FAM line of `Char` (**renamed from `jam::Row`**). The universal attributed line. `sizeof` = header; content inline via FAM.
- `jam::Cell` — the coordinate scalar (former `Cell::Unit`). Unchanged role, just promoted.

**The inversion (current → target):**
| current | target |
|---|---|
| `jam::Cell` (packed atom) | `jam::Char` (new `jam_char.h`) |
| `jam::Cell::Unit` (coordinate) | `jam::Cell` |
| `cell` alias, `_cell` literal | unchanged → resolve to `jam::Cell` |
| `jam::Cell::Point` / `::Rectangle` | unchanged |
| `jam::Row` (FAM) | `jam::String` (FAM, + `getWrappedLines`) |
| `row->cells[col]`, `Row::cells[]`, `FlexType = Cell` | `string->chars[col]`, `String::chars[]`, `FlexType = Char` |

**Storage — `jam::Buffer<String>`:**
- FAM-native (the only container that natively stores a FAM type with stride; `deque<String>` / `HeapBlock<String>` are invalid for a FAM type). Uniform stride (= max width, bounded by terminal-width history, within SPEC 100 MB budget). Trivially copyable → memcpy commit/scroll/clear and **memcpy state-serialization**. Ring head = scrollback FIFO. 2 channels = normal/alt screen.
- The FAM pointer math stays encapsulated in `Buffer`/`Block` (no caller casts), and `Block` is normalized to the `static_cast`-via-`void*` idiom (Class A, §3).

**Deleted outright (no legacy, no coexistence):**
- RFC `jam::String` as `HeapBlock<Char>` — **off the table.** The line type is the FAM `String`.
- `jam::TextLine`, `jam::TextLineArray` (→ would-be `StringArray`) — dropped; the pipeline uses `Buffer<String>` end to end.
- Owning `ParagraphStorage` + `deque<ParagraphsModel>` machinery — dissolved into `Buffer<String>`.
- `jam::Cell::RowState`, `jam::Cell::getKey` — verified dead.

**Shaped cache:** shaping happens per visible-window line at paint (neovim model). A persistent shaped-`Entry` cache is **deferred (YAGNI)** until profiling shows reshaping the visible window is a bottleneck — it is an optimization, not foundation, and a trivially-copyable `String` cannot hold it anyway. (Removes a NAMES gate from the critical path.)

**Name freed first:** `jam::String` already exists in `jam_core` as a static text-utility toolbox (`jam_core/string/jam_string.h:6`). It is absorbed into `jam::Text` before the line type takes the name (§2 Phase 0).

## §1.5 Screen Channels — NORMAL vs ALTERNATE, content switching

The 2-channel `Buffer<String>` **is** the two screen buffers, each self-contained. This supersedes the old "shared Live + History" model, which could not preserve the normal screen across an alternate excursion (a shared Live is overwritten by the alternate app). Separate channels preserve it structurally.

- **NORMAL channel (0) = history + live tail.** One continuous ring, capacity `app::id::scrollbackLines`. The last `viewportRows` rows are the **live tail** (one-way mirror of Video's normal grid, I2); everything above is **history**. As the shell scrolls, the top live row crosses into history (commit, I1) — the live-window boundary (`previousActiveCount`) advances. Resize **reflows** via `getWrappedLines` (I3, lossless). This is "Live is dynamic, appended/replacing the history tail."
- **ALTERNATE channel (1) = live only.** Viewport-sized, **zero history, no reflow**. `?1049h` clears it; the app owns its layout and redraws on resize — END only resizes the grid (resize/layout is full app responsibility).
- **Both channels persist permanently** in TextEditor. `activeScreen` selects render; the inactive channel is **frozen — never written while inactive.** Content preservation across switches is therefore structural, not bookkept.

**Ownership (no shared "both"):** Video owns the live grids (`Buffer<String>`, 2 channels, reader thread; Processor owns Video). TextEditor owns the document (`Buffer<String>`, 2 channels: normal=history+live, alternate=live; message thread; Session owns TextEditor). `CellFifo` transports; Processor orchestrates the switch.

**The `ls` → `vim` → exit invariant:** `ls` output is normal-channel history; `vim` runs on the alternate channel; on `?1049l` the normal channel — untouched throughout — renders intact. **The `ls` history is still visible. Normal is never cleared.** This is the lossless guarantee, made structural by channel separation.

## §Divergences from RFC (carrier honesty)

- RFC §4.4/§4.5 (`String` = `HeapBlock<Char>`, `StringArray`) — **rejected.** Line type is the FAM `String` (= `Row`); store is `Buffer<String>`. Rationale: one type, trivially copyable, memcpy-serializable, no FAM-in-`deque` impossibility, no owning-handle machinery (verified: `make_unique`/`Owner` cannot allocate a FAM's content).
- RFC §5/§6 (`ParagraphStorage`/`ParagraphsModel` over `deque`) — **dissolved** into `Buffer<String>`.
- RFC §7.3 lazy `getShapedText` — **deferred** (YAGNI), per above.
- All other RFC intent (mental model §1–§3, `Char` bit layout, `PROPORTIONAL`, SIGWINCH projection, screen channels, write-path decoupling) — **retained**.

---

## §2 Steps — Foundation (Objective 2)

### Phase 0 — Free the `String` name (absorb toolbox into `jam::Text`)

**Step 0a — merge toolbox into `jam::Text`.**
Scope: `jam_core/text/jam_text.h`, `jam_text.cpp`. Move every member of `class String` (`jam_core/string/jam_string.h`) — constants, static methods, templates, private tables/helpers — into `struct Text`; move impls into `jam_text.cpp`. Preserve existing `Text`/`URL`. No behavior change.
Validation: all members present in `Text`; no lost/duplicated method; doxygen carried.

**Step 0b — delete old file, repoint call sites.**
Scope: delete `jam_core/string/jam_string.{h,cpp}`; drop the include from `jam_core.h`; repoint `jam::String::` → `jam::Text::` in the 6 evidenced files (`Source/lua/EngineDefaults.cpp` ×183, `jam_core/image/jam_image.cpp` ×3, `jam_gui/menu/jam_menu.mm` ×1, `jam_graphics/colours/jam_colours_utilities.h` ×3, `jam_style/style_manager/jam_style_manager.cpp` ×1).
Validation: zero `jam::String::` residue in dev/ tree (excl. Builds); `jam::String` name free.

### Phase A — jam_graphics types

**Step 1 — `jam_char.h` (new): the packed atom.**
Move packed-character content out of `jam_cell.h` into `jam::Char`: `packed`, bit-layout constants, `codepoint`/`contentTag`/`wide`/`styleId`, `make`/`erase`, `CONTENT_*` and wide constants, both `static_assert`s (size 8, trivially copyable). Rename `SPACER_HEAD` (3) → `PROPORTIONAL` with RFC §4.3 doxygen. No `Unit`/`RowState`/`getKey`. Zero includes beyond `<cstdint>`/`<type_traits>`.
Validation: identical bit layout/accessors; `PROPORTIONAL` complete; static_asserts; NAMES Rule -1 (only approved `Char`/`PROPORTIONAL`).

**Step 2 — `jam_cell.h` (rewrite): coordinate-only `Cell`.**
Replace body with former `Cell::Unit` promoted to `jam::Cell` (`int value`, explicit ctor, arithmetic + comparison + `%`). Keep `struct Point; struct Rectangle;` forward decls. Delete `RowState`, `getKey`. `_cell` → `jam::Cell`; `using cell = jam::Cell`. Doxygen corrected (RFC §4.7).
Validation: coordinate-only; dead code gone; alias/literal resolve to `jam::Cell`; positive nesting.

**Step 3 — `jam_cell_point.h` / `jam_cell_rectangle.h`: `Unit` → `Cell`.**
Replace every internal `Cell::Unit` with `Cell`. No behavior change.
Validation: no `Unit` token; API unchanged.

**Step 4 — `jam_string.h` (rename `jam_row.h`): the FAM line.**
Rename file `jam_row.h` → `jam_string.h`, type `Row` → `String`. `using FlexType = Char`; `Char chars[]` (was `Cell cells[]`); keep `uint16_t usedCols`, `uint8_t flags`, flag constants (`flexWrap`/`collapsed`/`justify`). **Add** `int getWrappedLines (int viewWidth) const noexcept` = `(usedCols + viewWidth - 1) / viewWidth`, returns 1 for empty/zero-width (absorbs the only useful method from the deleted `TextLine`). Doxygen: universal attributed line.
Validation: FAM element `Char chars[]`; `getWrappedLines` ceiling division; trivially copyable; no `Row`/`cells`/`TextLine` residue.

**Step 5 — `jam_graphics.h`: include wiring.**
`jam_char.h` before `jam_string.h`/`jam_row` consumers; `jam_cell.h` before point/rectangle; `jam_string.h` replaces `jam_row.h`. Remove `jam_text_line.h`/`jam_text_line_array.h`/`jam_ParagraphStorage.h` includes (those files deleted). Submodule headers include nothing.
Validation: include order resolves forward decls (`Char` before `String`; `Cell` before Point/Rectangle).

**Step 6 — delete the dead types.**
Delete `jam_text_line.h`, `jam_text_line_array.h`, `jam_ParagraphStorage.h` (owning `ParagraphStorage`/`ParagraphsModel`).
Validation: files gone; no include references remain.

### Phase B — jam storage hygiene + shaping

**Step 7 — Class A: normalize `jam_block.h` casts (in scope).**
Store `Block::base` as `char*` (convert once at construction via `static_cast<void*>`); each accessor returns `static_cast<ElementType*>(static_cast<void*>(base + offset))`. Eliminate every `reinterpret_cast` in `Block` (`:189, :213, :233, :253, :269, :291, :308`). Zero behavior change; one FAM idiom shared with `Buffer::rowAddress`.
Validation: no `reinterpret_cast` in `Block`; addressing identical; consistent with `Buffer` (NAMES Rule 5).

**Step 8 — `jam_glyph_arrangement.{h,cpp}`: `Cell` atom → `Char`, accept `Buffer<String>`.**
Replace `Block<Cell>` → `Block<Char>` overloads; replace `shape(TextLineArray)` with shaping over `Buffer<String>` / a `String`'s `chars`. Update every atom reference (`.codepoint()`/`.wide()`/`SPACER_TAIL`) to `Char`. Coordinate `Cell`/`Point`/`Rectangle` usages preserved. `buildArrangements` stays stride-agnostic — it lays out a `Char` sequence, nothing else. `PROPORTIONAL` advance is WHELMED-future (out of scope).
Validation: no `Cell`-atom references; coordinate usages intact; shaper reads a `Char` sequence per line; Entry struct unchanged.

---

## §3 Steps — Upstream Logic (Objective 3)

### Phase C — Video (reader thread, viewport grid)

**Step 9 — `Video`: `Buffer<Row>` → `Buffer<String>`, `cells` → `chars`.**
Scope: `Source/terminal/Video.{h,cpp}`, `VideoEdit.cpp`, `VideoOSC*.cpp`, `Mouse.cpp`, `CellFifo.h`, any END source referencing `jam::Cell` atom / `jam::Row` / `jam::TextLine*`. Apply the §1 rename map. `print`/`scrollUpAndFill`/`eraseInDisplay` write `string->chars[col]`. Video remains the viewport-sized live grid (Invariant: sole source of live truth).
Validation: zero atom-`Cell`/`Row`/`cells` residue; coordinate `cell`/`Cell::Point` untouched; Video behavior unchanged but for the rename.

### Phase D — TextEditor (message thread, document SSOT)

**Step 10 — `jam_text_editor.{h,cpp}`: own `Buffer<String>`, editing API, projection.**
Replace `paragraphsModel`/`setText(TextLineArray)`/`arrangement`-as-store with `jam::Buffer<String> store` (2 channels = screens) + `int activeScreen`. Editing API mirroring `ml_*`, **history-vs-live aware** to enforce I1/I2:
- commit: append one departed `String` to history (the **only** history writer — I1).
- flush: overwrite the live visible tail rows only (never history — I2).
- `setActiveScreen(int)`; `clear`.
`calc()` sums `string.getWrappedLines(physicalViewWidth)` over all lines → ContentView height. SIGWINCH / viewport-dimension change: recompute heights at new width; **history storage untouched** (I3). `updateWinsize`, `setCaretPosition`, ValueTree state — unchanged.
Validation: single `Buffer<String>` store; I1/I2/I3 structurally enforced (history has exactly one writer; flush cannot reach history; resize re-projects only); no shadow of Video; Tell-don't-Ask; positive nesting.

**Step 11 — `ContentView`: render visible window, shape per line at paint.**
`drawGlyphRuns` iterates lines intersecting the Viewport clip; for each, shape its `String.chars` via `buildArrangements` and draw. No persistent shaped cache (deferred). Reads only the active screen's `Buffer<String>`.
Validation: visible-window-only shaping; reads active store exclusively; no Arrangement-as-SSOT residue.

### Phase E — Processor (the bridge, invariant enforcement)

**Step 12 — `CellFifo` → tagged commit+live bridge (Model A).**
Scope: `CellFifo.{h}`. The bridge becomes the **sole** reader→message contact for both content paths: each pushed row is tagged **commit** (scrolled off) or **live** (dirty viewport row, with its row index). `Cell` → `Char` throughout; entries deliver into `Buffer<String>`, not `TextLine`/`TextLineArray`. Drop the `reinterpret_cast` header packing where the `static_cast`-via-`void*` idiom applies (Class-A-consistent). Video never shares its buffer; this kills the cross-thread Video read behind `DEBT-20260530T100000`.
Validation: single bridge carries tagged commit+live; `Char` payload; no `TextLine` residue; SPSC contract intact.

**Step 13 — `State::rowDirtyFlags` is the per-row dirty SSOT; delete the shadow (I4).**
Scope: `Video.{h,cpp}`, `VideoEdit.cpp`, `ProcessorEvents.cpp`, `State.{h,cpp}`. Delete `Video::rowTouched` (`Video.h:421`) and the `id::rowDirty` event relay (`Video.cpp:173–180`, `ProcessorEvents.cpp:318–319`, `Identifier.h:310`). Video marks `State::rowDirtyFlags[row].store(1, relaxed)` on cell write (the ×11 `rowTouched[...] = true` sites become State marks); message thread selects dirty live rows via `exchange(0)`.
Validation: zero `rowTouched` residue; no `id::rowDirty`; single per-row dirty owner = `State`; reader-thread marks are atomic.

**Step 14 — `Processor`: commit/flush, no shadow, decoupled prepare.**
Scope: `Processor.{h,cpp}`, `ProcessorEvents.cpp`. Remove the `TextLineArray` shadow and `getTextLineArray()`. Add `int previousActiveCount` (plain int; not State, not ValueTree).
- **Commit path (I1):** drain `CellFifo` commit-tagged rows → append to TextEditor history once.
- **Flush path (I2):** drain `CellFifo` live-tagged rows (selected by `State::rowDirtyFlags`) → overwrite the live tail only; never history.
- `prepare()` decoupled (I3): resizes Video's grid only; never touches TextEditor or the bridge; in-flight committed lines survive.
- Remove the `liveRows` State parameter (named cause of resize corruption).
Validation: no `TextLineArray`/shadow (SSOT); `previousActiveCount` plain int; commit writes only history, flush only live tail, prepare touches neither; `liveRows` gone; positive nesting.

**Step 15 — Screen switch + ED 3 (channel freeze/preserve, §1.5).**
`?1049h` (enter alternate): Video → channel 1 + `setActiveScreen(1)`; clear alternate channel; **normal channel (0) frozen — not written.** `?1049l` (exit alternate): Video → channel 0 + `setActiveScreen(0)`; **normal renders exactly as left** (history + live tail intact); shell redraws the prompt into the normal live tail. Inactive channel is never written — preservation is structural, not bookkept. `id::clearBuffer`/ED 3: clear both channels + recompute.
Validation: switch freezes the inactive channel; `ls`→`vim`→exit leaves normal history intact (§1.5 invariant); alternate cleared on entry; ED3 clears both; no history destruction on normal return.

**Step 16 — `Display`: drop the old push, render-orchestrate only.**
Remove `session.getTextEditor().setText(processor.getTextLineArray())`. Content flows via Processor's commit/flush directly into TextEditor; Display orchestrates repaint.
Validation: no `getTextLineArray()`; Display pushes no content (SSOT — Processor owns the write path).

### Phase F — Docs

**Step 17 — doxygen corrections (RFC §4.7) + `ARCHITECTURE.md` sweep.**
Correct false doxygen surfaced in Phases A–C. Update `ARCHITECTURE.md` to mirror reality: `Char` atom, `Cell` coordinate, `String`(=`Row`) FAM line, `Buffer<String>` document store, the SSOT split + I1–I3, eliminated `TextLine*`/`liveRows`/shadow state.
Validation: greps for named false claims return zero; ARCHITECTURE.md matches code (code is ground truth).

---

## §4 BLESSED Alignment

- **B (Bound):** `Buffer<String>` is one owning allocation, RAII, trivially-copyable elements. Reader thread (Video grid) and message thread (TextEditor store) are bound; `CellFifo` is the sole bridge. FAM casts encapsulated in `Buffer`/`Block`; `Block` normalized to no `reinterpret_cast` (Class A).
- **L (Lean):** Deletes `TextLine`/`TextLineArray`/owning `ParagraphStorage`/`ParagraphsModel`/`liveRows`/`getTextLineArray`. One line type, one store. Shaped cache deferred (YAGNI).
- **E (Explicit):** Named editing API; history-vs-live split explicit; `previousActiveCount` visible; no magic; positive nesting.
- **S (SSOT):** TextEditor's `Buffer<String>` is the document truth; Video owns the live grid as sole live source; `State` owns every state machine including per-row dirty (I4). I1–I3 make the visible tail a one-way projection — no drift. Eliminates the `TextLineArray`/`liveRows` shadow **and** the `rowTouched`/`id::rowDirty` shadow (root causes of `DEBT-20260530T100000`).
- **S (Stateless):** Storage width-free; width enters once at projection (`getWrappedLines`). Viewport-dimension change touches projection only.
- **E (Encapsulation):** `Char`/`String`/`Buffer` know nothing of VT/markdown — universal. Video grid-mutates; TextEditor renders; Processor bridges. FAM math hidden in storage primitives.
- **D (Deterministic):** Lossless history — `String.usedCols` fixed at commit; `getWrappedLines` recomputes screen rows at any width; history never re-sourced from Video. Resolves the logged D violation.

## §5 Risks / Open Questions

- **Q4 — RESOLVED.** Capacity SSOT is the existing `AppState` parameter `app::id::scrollbackLines` (`Processor.cpp:68`). `Buffer<String>::setSize`: channel 0 (normal) = `scrollbackLines` rows; channel 1 (alternate) = viewport rows / zero scrollback (`Processor.cpp:71`). Re-`setSize` on parameter change. No new source. (The removed `liveRows` param is unrelated — capacity is separate, legitimate config.)
- **Q-bridge — RESOLVED (Model A).** `CellFifo` becomes the single reader→message bridge carrying tagged **commit** + **live** rows into `Buffer<String>`; live rows selected by `State::rowDirtyFlags` (I4). Video never shares its buffer — the cross-thread Video read behind `DEBT-20260530T100000` is structurally removed. Steps 12–14.
- **Memory characteristic (not a blocker).** `Buffer<String>` uniform stride = `rows × maxWidth`; bounded by terminal-width history, within SPEC 100 MB. The only weak point is a future unbounded WHELMED proportional paragraph — out of scope for END; revisit at WHELMED's line model.
