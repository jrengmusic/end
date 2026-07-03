# RFC — Terminal Editor: CodeView Rewrite + jam_terminal Wiring
Date: 2026-07-03
Status: Ready for COUNSELOR handoff — all questions resolved, zero open
Prerequisite: RFC-vt-correctness.md (VT correctness + conformance hardening) lands first — see its S6/Handoff for the dependency map.

## Problem Statement

END's terminal rendering philosophy is NOT the traditional emulator (text painted on a fixed viewport, index-shifted "faux" scrolling). The goal: render the terminal INTO an actual text editor — a content component that grows taller than its parent, a real viewport, a real scrollbar widget — with terminal live buffer, history, and scrollback decoupled into an editor document whose "live" region is the active prompt. History above the active prompt must be LOSSLESS across resize — a problem ARCHITECT has not found solved in any published terminal emulator project, and which endless (the previous iteration) attempted but did not achieve.

Session questions, all resolved herein:
1. Do we still need CodeView, or can juce::CodeEditorComponent serve?
2. Where can the glyph atlas optimization integrate further (the missing CPU-side shaping pre-rasterization half)?
3. Is "politely hijacking" the data model sufficient for the immutable scrollback store?
4. Must shaping/arrangement also be overridden for MAXIMAL PERFORMANCE with SEAMLESS INTEGRATION?
5. Threading strategy: READER thread → MESSAGE thread, lossless until component destruction or `clear`; SIGWINCH must not clear rendered text. LOCK FREE, event driven, no block, no wait, no stall, no yield, no sleep, first-in-first-out keep-last.
6. Wiring strategy — where to start?
7. VT State communication — terminal::Model as SSOT state machine, bidirectional.

**Critical framing (ARCHITECT-directed):**
- There is NO legacy. NO backward compatibility. NO constraint against rewriting CodeView from scratch. NO baggage.
- We want CORRECTNESS. END's current architecture is clean.
- neovim (`~/Documents/Poems/dev/neovim`) is the battle-tested WORKING REFERENCE.
- endless (`~/Documents/Poems/dev/endless`) is a *suspect* reference — it may carry flaws, bugs, and unfinished implementations toward the same objective. It contributes **anti-pattern evidence** and domain inventory, not design.

## Research Summary

### R1 — juce::CodeEditorComponent: disqualified (four hard facts)

1. **No soft wrap.** Zero reflow logic anywhere; horizontal overflow handled purely via `xOffset` + horizontal scrollbar (`juce_CodeEditorComponent.cpp:554–605`; only "wrap" mention is a doc comment at `.h:149`). The lossless-resize goal IS wrap-at-paint. Unfixable from outside.
2. **Glyph path is fork-or-nothing.** Per-line drawing lives on private non-virtual nested `CodeEditorLine::draw()` using `AttributedString` (`juce_CodeEditorComponent.cpp:205–225`; class private at `.h:453–454`). `paint()` is overridable but cannot reach `CodeEditorLine` (private type) — atlas integration means forking the .cpp.
3. **CodeDocument is hostile to streaming.** Every public mutator allocates an `UndoableAction` (non-undoable `insert()` path is private, `juce_CodeDocument.cpp:663–699, 891–919`). `maximumLineLength` cache invalidated on every insert/remove (`:945, :1025`); `updateScrollBars()` calls `getMaximumLineLength()` on essentially every edit (`juce_CodeEditorComponent.cpp:781`) → O(documentLines) full rescan per PTY append.
4. **No thread contract, no immutable region.** Listeners fire synchronously on the mutating thread (`juce_CodeDocument.cpp:973, 1067`); zero locks, zero message-thread assertions. Read-only is one whole-editor bool (`.h:424`); restricting edits to a region requires intercepting ≥4 independent virtuals and still cannot stop `deleteSection` through the document reference.

### R2 — jam::CodeView / jam::CodeModel: current state

- **CodeView is NOT a juce fork** — pure `juce::Component` subclass (`jam_gui/code_view/jam_CodeView.h:33`), owns `CodeViewViewport` (juce::Viewport subclass, `:134–155`), `ContentView` (paint delegate), `CaretComponent`. Contract documented at `:3, :14–24`: "Pure view that renders externally-owned CodeModel" — no state owner, no tree listener, cell-space API only (`setFont`, `setCaretPosition`, `setWrapEnabled`, `setViewportLineCount`, `calc`, `setCaretShape` DECSCUSR, `scrollToBottom`).
- **CodeView is BROKEN / stubbed out** — commented out of `jam_gui.h:102–103` since commit `e8543bf1a` (2026-07-01, glyph-pipeline absorption). Depends on deleted `jam::glyph::Arrangement`, `jam::glyph::Graphics`, `jam::Font`, `jam::Typeface`. Old paint flow (`ContentView::paint` → `shape()` + `glyphGraphics.drawGlyphs()` → `glyphGraphics.pop()`) is dead. CaretComponent likewise (`jam::Font` dependency).
- **CodeModel is LIVE and complete** (`jam_graphics/detail/jam_CodeModel.h`, 239 LOC): `jam::Owner<Screen>`, each Screen = `std::deque<jam::CodeLine>` + capacity (default 10,000; FIFO eviction). Mutation API deliberately mirrors neovim memline verbs: `append`, `replaceAt`, `remove(Range)`, `clear`, `setCapacity`. Query mirrors CodeDocument: `getLine`, `getNumLines`, `getActive`. MESSAGE THREAD only, no synchronization — single-threaded by design.
- **`physicalViewWidth`** (CodeView protected member) is documented "scrollbar-aware cell width" — as written it derives width from scrollbar state, i.e. it carries the winsize feedback cycle (see P5).

### R3 — jam glyph pipeline (the absorbed atlas, and the missing half)

- **GlyphAtlas** (`jam_vulkan/font/jam_GlyphAtlas.h`, 893 LOC; 15.3K LOC across `font/`): three-tier rasterization (`:22–60`) — Tier 1 registered memory fonts (END's embedded family), Tier 2 lazily-resolved system fonts (CoreText/DirectWrite → FT_Face; color fonts negative-cached), Tier 3 EdgeTable fallback via `juce::Typeface::getLayersForGlyph()`. Three backends (`:159–164`): edgeTable / freetype (autofit-hinted, stem-darkened) / native (CoreText / DirectWrite), dispatched via branchless member-function-pointer table (`:580–585`). LRU cache, maxCachedGlyphs 23,000. GPU mirror: two 4096² images (R8 mono, BGRA8 emoji), `ensureGlyphAtlas()` idempotent, uploads through the calling window's staging arena (Sprint 55 cross-window race fix). Software fast lane: `composite()` — SIMD tinted src-over straight from CPU atlas, used by `jam::LowLevelGraphicsGlyphRenderer` CPU fallback. **Thread contract: MESSAGE THREAD only.**
- **Shaping is the unfinished half.** `jam::GlyphArrangement` (`jam_graphics/fonts/`, 43+23 LOC) inherits `juce::GlyphArrangement`, delegates everything, re-shapes per draw. Its `draw()` override is annotated as "the extension point for future run batching and visible-range shaping" (`jam_GlyphArrangement.cpp:16`). Shaping cost is bounded to visible rows (paint-time projection walk); scrollback depth has zero shaping penalty.
- **Engine dispatch:** JUCE paint → `jam::vulkan::Registry::createContext(peer)` → GPU `jam::vulkan::LowLevelGraphicsContext` OR CPU `jam::LowLevelGraphicsGlyphRenderer` — never null, both resolve through the same shared GlyphAtlas → visual parity by construction. Rendering is agnostic; consumers paint through `juce::Graphics` and cannot tell which engine ran.
- **Fast-lane gap:** `GlyphAtlas::Key` = typeface ptr + glyphIndex + fontSize — carries NO codepoint. `LLGC::drawGlyphs` receives glyph indices only. Codepoints exist at drain time (the Parser owns them) — see P13.

### R4 — jam_terminal: complete, production-ready

`~/Documents/Poems/dev/jam/jam_terminal/` — full module, zero glyph-rendering dependency: `protocol/` (VtVocabulary — DEC modes, OSC, SGR, CSI finals), `parser/` (DFA state machine, O(1) dispatch), `video/` (Screen, CursorState, Video VT command processor with protected virtual hooks for subclassing), `cell/` (Palette), `transport/` (CellFifo), `keyboard/` (JUCE KeyPress → VT sequences), `tty/` (UnixTTY forkpty, WindowsTTY ConPTY).

**CellFifo** (`transport/jam_CellFifo.h`): lock-free SPSC, two independent rings — history (departed lines, `isContinued` join on drain) + active (live rows, 1:1 drain). Per-entry seqlock (atomic epoch sandwich around header + `Char[]` memcpy); entry header = u64 (cellCount | flags byte | padding); drop-oldest (`BufferSPSC::prepareToWrite` advances validStart — producer NEVER stalls); consumer detects torn reads and silently resyncs.

### R5 — jam::Char / jam::CodeLine / jam::Size: the attributed store (no design gap)

- `jam::Char` (`jam_graphics/detail/jam_Char.h`): 8-byte trivially-copyable packed u64 — 21-bit codepoint or Grapheme-table index, 2-bit contentTag, 2-bit wide hint (NARROW/WIDE/SPACER_TAIL/PROPORTIONAL), 16-bit styleId into `jam::Stamp` (fg/bg/SGR flags). Static services: `fromCodepoint()` (DEC line-drawing translation + width + pack), `width()` (East Asian Width), `isWordChar()` (documented "for selection / word-wise navigation"), `isCombining()`, UAX #29 grapheme segmentation step functions (caller-owned state — Char stays stateless).
- `jam::CodeLine` (`jam_graphics/detail/jam_CodeLine.h`): `HeapBlock<Char>` + `cellCount` + `isContinued` (DECAWM soft-wrap) + `isJustified`. **Width-free storage** — doc contract: "Viewport width enters once, at projection time via `getWrappedLines(viewWidth)`. Storage is never baked to a particular viewport width; CodeModel is untouched on resize (I3)." `getWrappedLines` = ceiling division, the projection primitive.
- `jam::Size<T>` (`jam_core/utilities/jam_Size.h`): packed width/height pair over `jam::Union<T,T>`; `Size<int16_t>` = 4 bytes, `toInt()` ValueTree round-trip, structured bindings. Purpose-built for single-atomic dimension parameters.

### R6 — Model::Component doctrine + Listener threading

- `jam::Model` (`jam_data_structures/model/jam_Model.h:29–740`): APVTS analog — atomics any-thread lock-free (`store()/load()`), timer flush → ValueTree (60/120Hz, message thread), message-thread reads.
- **`Model::Listener::parameterChanged` fires on the CALLING thread** (`jam_Model.h:36–48` — "may be audio/reader/GL thread"). Decisive for P12.
- `Model::Component` (`:58–71`) + `Model::Attachment` (`:81–125`): RAII state-subtree graft for widgets that OWN persistent app state. END precedent: `View`, `Tabs`, `PaneView` (PANE/focus), `MessageOverlay` (OVERLAY/ParameterText).
- **Two archetypes coexist by doctrine** (MANIFESTO.md:189–198 — widgets are "dumb workers", "orchestrator tells, never tracks"): (1) stateful Model::Component widgets; (2) pure views projecting externally-owned state. CodeView is the documented exemplar of (2).

### R7 — endless: anti-pattern evidence + domain inventory (suspect reference)

- **The actual resize-loss bug:** `Processor::prepare()` cleared CellFifo on every resize (`cellFifo.setSize()`) — in-flight history destroyed on SIGWINCH. Flood-drop was never the enemy; resize-clear was.
- **`liveTailExtent` fragility** (`Display.cpp:102–131`): remembered "rows laid down last tick" per screen; a torn/empty drain after tail removal made the live region vanish entirely.
- **Latency path:** PTY → reader → atomic Model write → 60Hz timer → ValueTree notify → Display drain — worst-case 16ms lag, batch-style.
- **Silent drop-oldest** on history overflow with no signal (`CellFifo.h:22–23`).
- **Row-indexed OSC 133 Model params** (`Parameters.xml:17–21` outputBlockTop/Bottom/promptRow) — grid-row-anchored prompt marks are not resize-stable; the unfinished-implementation smell. Corrected in P12/P8.
- **Parameter inventory** (`terminal/Parameters.xml` + `Model.h`) — the domain evidence base for P12's schema: SESSION/MODES/per-SCREEN/TEXT structure, packed `cursor` int, `screenDirty`, paste echo gate, mode 2026 flag, shell exit, title/cwd/foregroundProcess ParameterTexts.
- Structure worth keeping (and already kept in END's design): dual-ring CellFifo, Session/Processor/Display layering, two screens in one CodeModel, drain-and-discard on alt-screen entry, XML-declared parameter layout (`buildLayout`).

### R8 — neovim `:terminal`: the working reference (structural map)

Read: `src/nvim/terminal.c` (2718 lines), `memline.c`, `grid.c`, `drawline.c`, `drawscreen.c`, `channel.c`.

- **Callbacks** (`terminal.c:216–226`): `damage`, `moverect`, `movecursor`, `settermprop`, `bell`, `theme`, `sb_pushline`, `sb_popline`, `sb_clear` — one concern each.
- **`term_sb_push`** (`:1698–1749`): departed top row → circular `ScrollbackLine[] sb_buffer` (variable-length cell rows); at capacity, recycles the oldest allocation (width-match) and increments `sb_deleted`; `sb_pending` counts rows awaiting buffer insertion — push decoupled from insert.
- **`term_sb_pop`** (`:1756`): terminal grows taller → bottommost scrollback line pulled BACK into the live grid. Symmetric resize.
- **`refresh_scrollback`** (`:2556–2603`): two-phase — insert pending scrollback rows ABOVE the visible region (`ml_append_buf`), then trim; **pauses PTY reads during buffer mutation** (`read_pause_cb`) — a blocking coordination.
- **`fetch_cell`** (`:2341–2360`): negative row → scrollback storage; row ≥ 0 → live vterm grid. Unified lookup, two namespaces.
- **Coordinate arithmetic:** `row_to_linenr(row) = row + sb_current + 1`; buffer invariant `bufferLines = sb_current + height`. The live region is ALWAYS the last `height` lines by arithmetic — no remembered extent.
- **Batching:** 10ms `REFRESH_DELAY` (`:132`); `terminal_receive` (`:1381`) feeds vterm in the stream callback; `invalidate_terminal` (`:2363`) sets membership + schedules timer; `refresh_timer_cb` (`:2471`) processes all invalidated terminals. Damage as row ranges (`invalid_start/end`).
- **DEC 2026 synchronized output** (`:1600–1668, :2372`): while active, refresh deferred, damage accumulates; on end, immediate full flush.
- **Editability is structural, not flag-based:** MODE_TERMINAL (`terminal_enter :900, :916`) routes input to PTY; scrollback is regenerated on refresh so normal-mode edits never survive. No `modifiable=false`.
- **Storage:** buffer lines stored UNWRAPPED, exactly as the terminal emitted; wrap happens at render in `win_line()` (`drawline.c`). No rewrap of scrollback, ever.
- **Threading:** none — libuv delivers PTY bytes on the main loop; zero locks; and it must PAUSE reads during buffer sync.
- **Dual store:** rendering reads cells (`fetch_cell` → `sb_buffer`/vterm), editor semantics read regenerated UTF-8 buffer text — two stores + a regeneration layer, because nvim cannot unify cell attributes with buffer text.

## Principles and Rationale

### P1 — Keep CodeView; rewrite from scratch (Decision, ratified)
juce::CodeEditorComponent fails four hard requirements (R1); jam::CodeView's CONTRACT is exactly right and survives; its IMPLEMENTATION is dead code and is demoted to reference status — same status as endless. Rewrite fresh against the contract, nvim's structure, and END's architecture. (BLESSED: Lean — the rewrite is mostly deletion of absorbed machinery; Explicit — contract-first.)

### P2 — CodeView stays a pure view; terminal::View is the Model::Component (Decision, ratified)
CodeView has no state of its own to graft: document → CodeModel (Session-owned), cursor/screen state → terminal::Model atomics (reader-written — Attachment cannot bind those; the flush→ValueTree bridge exists for that), font/cell metrics → config::Model. terminal::View (already a PaneView, already a Model::Component) listens on terminal::Model + config::Model and TELLS CodeView via the cell-space API — the endless Display shape, which is END's Phase 4 shape. (MANIFESTO §Stateless "dumb workers", §Encapsulation "orchestrator tells, never tracks", MANIFESTO.md:189–198.)

### P3 — Analogous abstraction to nvim, threading excepted (Decision, ratified)
The structures are isomorphic — END independently arrived at nvim's architecture. The abstraction must be ANALOGOUS (vocabulary table below). The single deliberate exception: threading. nvim's same-thread coupling and `read_pause_cb` blocking pause are structurally impossible under the no-wait constraint and unnecessary — CellFifo is the lock-free replacement, strictly ahead of the reference on this axis.

| nvim | END analog | status |
|---|---|---|
| `Terminal` struct | `terminal::Processor` | named |
| vterm grid | `jam::terminal::Video` + `Buffer<Row>` scratch | named |
| `sb_pushline` | Video departed-line hook → `CellFifo::pushHistory` | named |
| `sb_pending` decoupling | history-ring occupancy (the ring IS the pending queue) | free |
| `sb_popline` | `popback` ring: `pushPopback` (message) / `drainPopback` (reader) | ratified |
| `sb_deleted` counter | `CodeModel` per-screen `evictedLines` | ratified |
| `refresh_scrollback`/`refresh_screen` two-phase | `Session::drain` order: history first, live tail second | ordering rule |
| `ml_append_buf(lnum)` | `CodeModel::insertAt (int, CodeLine&&)` | ratified |
| `row_to_linenr`/`linenr_to_row` | projection arithmetic on `liveFirstLine = numLines − viewportLineCount` | replaces liveTailExtent |
| `REFRESH_DELAY` batching | screenDirty-batched drain (timer/flush already in END design) | named |
| MODE_TERMINAL structural editability | keyboard → PTY only; CodeModel mutated only by drain | structural |
| DEC 2026 sync | Video honors 2026: accumulate, flush on end; `syncOutputActive` param | in scope |
| dual store (sb_buffer cells + regenerated text) | **collapsed**: CodeModel of Char IS both stores | see P4 |

### P4 — Single-store collapse (Rationale)
nvim's regeneration layer exists only because it cannot unify cell attributes with buffer text. `CodeModel` of `CodeLine` of `Char` (R5) carries codepoint/grapheme + wide hint + styleId (Stamp: fg/bg/SGR) in one 8-byte atom — rendering and text semantics (selection, word boundaries, future search/yank) read the same object. The entire `fetch_row`/regeneration machinery evaporates. Faithful in structure, leaner in store. (BLESSED: SSOT, Lean.)

### P5 — Wrap is projection; winsize is a function of component size only; reserved gutter (Decision, ratified)
- Document stores logical lines only (drain joins `isContinued` rows); physical rows exist nowhere except the paint-time projection `getWrappedLines(viewWidth)`. Physical↔logical mapping (caret, scroll, selection) lives in the projection layer. Already the CodeLine contract (R5, "I3").
- Scrollbar visibility MUST NOT affect winsize — the feedback cycle (content rows → scrollbar appears → cols−1 → SIGWINCH → app reformats → content changes → …) has a subprocess in the loop and cannot be iterated to a fixed point. **Reserved gutter (ratified):** scrollbar width always subtracted; visibility toggles drawing only, never layout. `cols = floor((componentWidth − scrollbarThickness) / cellWidth)`, always. Chosen over overlay because under overlay the visible content of the last column is again a function of scrollbar state (occlusion) — full per-cell determinism selects the gutter (MANIFESTO: Deterministic). In a terminal the scrollbar is visible after the first screenful anyway.
- **Consequence — width SSOT:** cols computed ONCE per component resize in terminal::View, published as the single `winsize` parameter (P12) — consumed by BOTH the Video-grid winsize and the CodeView projection width. Never two independent width computations. CodeView's `physicalViewWidth` becomes a consumer of the SSOT, never an authority.

### P6 — Losslessness: one equivalence, one sizing rule, one ordering rule (Decision, ratified)
- **Equivalence:** with history-ring capacity ≥ scrollback capacity, a row dropped from the ring is dropped only because ≥ ringCapacity newer rows arrived undrained — those rows alone push it out of the document's window post-drain. Transport drop-oldest = early eviction of a row that was already doomed. Dropped set ⊆ evicted set, always. Final document state bit-identical to a "lossless" transport. (nvim makes the same trade: `term_sb_push` recycles at `sb_current == sb_size`.) The only observable failure of an UNDERSIZED ring is a mid-history gap (discontinuity), never "less history".
- **Sizing rule (wiring-enforced, not architecture-enforced):** ring capacity ≥ `scrollbackLines × maxCols` cells PLUS `scrollbackLines` header slots (entries are variable-length: u64 header + cells). Without the header allowance the equivalence silently breaks at the margin. Lives at the single `cellFifo.setSize()` call site in `Processor::prepare` (S6).
- **Ordering rule:** SIGWINCH protocol is **drain-fully → resize → resume**. Never resize with pending entries (the endless bug). One ordering rule + one sizing expression carry the entire lossless guarantee; everything else holds by construction. Pop-back rides this same protocol (P9).
- Rendering during flood: paint samples the document tail at frame rate; transport policy cannot change what paints. The user sees the streaming tail plus the last `scrollbackLines` afterward — the stated UX, exactly.
- Active ring needs no such sizing — live rows are overwritten by design; keep-last is exactly correct.

### P7 — Threading: text ≡ GPU discipline (Decision, ratified)
The shader hot-reload already works because mutation is message-thread-owned and fenced against in-flight use. Text has STRONGER ownership: GlyphAtlas message-thread-only (contract), CodeModel message-thread-only (single-threaded), and the only foreign thread (READER) is bridged by an already-built lock-free SPSC. The unresolved SPEC §1.8 font/atlas seam (typeface handles torn down at config-change time while atlas rebuild defers) is NOT text-specific difficulty — it is the same hot-reload lifetime bug as swapping a pipeline mid-frame, with the same cure: **retire old typeface handles at the paint/fence boundary, not at config-change time**. Text is not harder than GPU; the GPU path already proved the pattern in this codebase.

### P8 — Input: PTY-authoritative + OSC 133 (Decision, ARCHITECT-selected)
Keystrokes → TTY, always (jam::Keyboard encodes; mode atomics read lock-free at encode time; input path bypasses the Model). The live region is a rendered projection of the Video grid; caret mirrors terminal cursor state; editing FEEL comes from the shell's own line discipline rendered losslessly (nvim MODE_TERMINAL analog — correct for every application class: vi-mode shells, password prompts, full-screen apps). PLUS shell-integration prompt marks (OSC 133) captured by the Parser. **Mark carrier (resolved):** marks travel WITH rows, not as Model state — the CellFifo entry header flags byte (where `isContinued` lives) carries prompt/output marks, stamped onto `CodeLine::mark` at drain (mark VALUES — ghostty 4-state vocabulary — defined in RFC-vt-correctness.md V3). Resize-stable, document-anchored. The Model keeps only the live `promptRow` (current grid) for jump-to-prompt of the active prompt. Endless's row-indexed Model params (R7) were the bug-shaped version of this. Locally-edited prompt REJECTED (local state diverges from shell state).

### P9 — Pop-back: full sb_popline analog (Decision, ARCHITECT-selected)
When the terminal gains rows, the bottommost history lines are pulled back into the live grid — the prompt stays glued to its history, editor-like; grow-resize is symmetric with shrink. **Mechanism (resolved):** `popback` ring — third ring in CellFifo, reversed direction (message-thread producer, reader-thread consumer), capacity one screenful (max rows). Sequence rides P6's resize protocol: `parameterChanged(winsize)` on message thread → Session drains history+active fully → pops CodeModel tail rows (height delta) → `pushPopback` → stores winsize atomics → reader wakes, resizes grid, `drainPopback` into grid bottom, resumes parsing. No new protocol — one more step inside the existing ordering rule.

### P10 — Selection + clipboard: in RFC, full (Decision, ARCHITECT-selected)
Selection model over the wrapped projection: anchors live in DOCUMENT coordinates (line, cell) — projection-independent, resize-stable; absolute anchoring across eviction via `evictedLines`; screen↔document conversion via the projection arithmetic; word-wise via `Char::isWordChar`; grapheme-safe boundaries via the UAX #29 services; wide-cell (SPACER_TAIL) snap. Clipboard copy extracts text from Char runs (Grapheme table expansion for clusters). **Mouse modes (resolved):** first wiring ships 1000 (click), 1002 (drag), 1003 (any-motion), 1006 (SGR encoding), 1004 (focus events) — the P12 MODES set. When application mouse reporting is active, local selection uses the conventional Shift-override. Part of the CodeView contract from the first line.

### P11 — Rejected alternatives (record)
- **juce::CodeEditorComponent adoption or subclass** — R1.
- **CodeView as Model::Component** — P2; would invent a state subtree with nothing in it and hand a leaf widget the orchestrator's listener role.
- **Overlay scrollbar** — P5; per-cell determinism.
- **Locally-edited prompt** — P8.
- **Blank-rows-on-grow (no pop-back)** — P9.
- **nvim's read-pause + same-thread coupling** — P3/P7; blocking is forbidden and unnecessary.
- **nvim's dual store + regeneration** — P4; Char unifies.
- **"Strictly lossless" unbounded transport redesign** — P6; equivalence makes it a non-question.
- **Row-indexed OSC 133 Model parameters** — P8; grid rows are transient, document marks are not.
- **Split `viewCols`/`viewRows` parameters** — P12; two atomics tear; `jam::Size<int16_t>` packs both into one.
- **GlyphAtlas::Key redesign for the fast lane** — P13; codepoint capture happens at drain, atlas stays glyphIndex-keyed.

### P12 — VT State communication: terminal::Model is the SSOT state machine (Decision, ARCHITECT-directed)
terminal::Processor is the VT command processor — in this architecture the APVTS analog, and MORE CORRECT threading-wise. Bidirectional:

- **Direction A (state): reader → message.** Video/Processor `store()` atomics lock-free; 60Hz `flush()` → ValueTree (message thread); terminal::View reacts via tree listeners. The endless/APVTS shape, kept.
- **Direction B (directives): message → reader.** terminal::View/Session `setValue()`; **Processor is a `Model::Listener`** — `parameterChanged()` is the event dispatch. It fires on the CALLING thread (`jam_Model.h:36–48`), i.e. the message thread — it NEVER executes on the reader thread. The callback is the *wake* (nudge the TTY poll's wake fd); the parameter atomic is the *data*; the reader loop `load()`s at iteration top. No callback ever runs on the reader thread except its own loop. Lock-free both directions, zero new mechanisms.
- **Packed-dimension rule (ARCHITECT-directed):** dimension pairs are SINGLE atomic parameters via `jam::Size<int16_t>` (`toInt()` round-trip) — two separate atomics tear (reader could observe new cols with old rows). Applies to `winsize`, `gridSize`, `cellSize`; endless's packed `cursor` int already followed this pattern.

**Schema — Direction A (reader writes, UI consumes):**

| Group | Parameter | Type | VT source |
|---|---|---|---|
| SESSION | `gridSize` | int (`Size<int16_t>`) | applied winsize ack |
| SESSION | `activeScreen` | int | DECSET 1049/47 |
| SESSION | `syncOutputActive` | bool | DEC 2026 |
| SESSION | `shellExited` | bool | child exit |
| SESSION | `bell` | int (counter) | BEL |
| SESSION | `promptRow` | int | OSC 133 (live grid row only — historical marks are document-anchored, P8) |
| SESSION | `pasteEchoRemaining` | int | bracketed-paste echo gate (endless-proven) |
| MODES | `applicationCursor` | bool | DECCKM |
| MODES | `applicationKeypad` | bool | DECKPAM/DECPNM |
| MODES | `bracketedPaste` | bool | DECSET 2004 |
| MODES | `mouseTracking` / `mouseMotionTracking` / `mouseAllTracking` | bool ×3 | DECSET 1000/1002/1003 |
| MODES | `mouseSgrEncoding` | bool | DECSET 1006 |
| MODES | `focusEvents` | bool | DECSET 1004 |
| MODES | `win32InputMode` | bool | ConPTY |
| SCREEN ×2 (NORMAL/ALTERNATE) | `cursor` | int (packed row\|col\|visible — DECTCEM bit) | CUP/DECTCEM |
| SCREEN ×2 | `cursorShape` | int | DECSCUSR |
| SCREEN ×2 | `cursorColor` | int | OSC 12 |
| SCREEN ×2 | `keyboardFlags` | int | kitty/modifyOtherKeys |
| SCREEN ×2 | `screenDirty` | int (counter) | the drain trigger |
| TEXT | `title` (256) / `cwd` (4096) / `foregroundProcess` (256) | ParameterText | OSC 0/2, OSC 7, process poll |

MODES consumers are message-thread: Keyboard encoder (DECCKM/keypad/2004/kitty flags) and mouse routing (1000/1002/1003/1006 → report to PTY; Shift-override → local selection, P10).

**Schema — Direction B (message writes, Processor listens):**

| Parameter | Type | Source | Reader effect |
|---|---|---|---|
| `winsize` | int (`Size<int16_t>` cols,rows) | terminal::View::resized (width SSOT, P5/S4) | wake → P6 drain→resize→resume protocol (+ P9 popback) |
| `cellSize` | int (`Size<int16_t>` px) | config::Model | CSI 14t / XTWINOPS pixel reports |
| `scrollbackLines` | int | config::Model | ring re-size, same drain-first protocol |
| `clearRequested` | int (counter) | user command | Video clears grid; Session clears CodeModel message-side |

**Excluded from the endless inventory:** `preview`/`splitCol`, `hintPage`/`hintTotalPages` (endless app features, not terminal core), `snapshotDirty` (redundant with per-screen `screenDirty`), `width`/`height` split pair (→ packed `gridSize`), row-indexed OSC 133 block params (→ `CodeLine::mark`, P8). Schema declared in XML via the endless-proven `buildLayout` pattern.

### P13 — Fast lane: pre-shaped runs, atlas untouched (Decision, resolved)
Monospace grid → layout is arithmetic (`col × cellWidth`); shaping collapses to codepoint→glyphIndex per (typeface, size) — one flat cached map, resolved at DRAIN time where the Parser's codepoints exist. Immutable history lines: shape once, store the glyph-index run in the CodeLine, forever; paint becomes lookup + quad append. `GlyphAtlas::Key` stays glyphIndex-keyed — NO redesign; the capture is out-of-band at drain. Renderer-agnostic by construction (both engines resolve through `getOrRasterize`). Slots into the marked extension point (`jam_GlyphArrangement.cpp:16`) without disturbing any non-terminal JUCE text path. Sequenced LAST (S7) — an optimization on a working path.

## Scaffold

Design scaffold — structural code, compile-untested (ORACLE has no build authority). All names below ARCHITECT-ratified this session.

### S1 — Ownership and flow

```
Application ── owns ──► jam::vulkan::Registry (Device, shared GlyphAtlas, per-window Graphics)
Nexus ── owns ──► terminal::Session (per tab)
  Session ── owns ──► jam::CodeModel        (document SSOT; screens: normal + alternate)
  Session ── owns ──► terminal::Model       (SSOT state machine, P12)
  Session ── owns ──► terminal::Processor   (reader-thread engine; Model::Listener)
    Processor ── owns ──► jam::terminal::TTY, Parser, terminal::Video (subclass),
                          CellFifo (history + active + popback rings), Buffer<Row> scratch
terminal::View (PaneView ⇒ Model::Component) ── parents, does not own ──► jam::CodeView
  listens: terminal::Model (tree, post-flush), config::Model (font, cells, scrollbackLines)
  writes:  terminal::Model Direction-B params (winsize, cellSize, …)
  tells CodeView: cell-space API only

READER thread:  TTY.read → Parser.feed → Video.process → Buffer<Row> writes
                → CellFifo.pushHistory / pushActive        (never blocks)
                → terminal::Model.store() atomics           (never blocks)
                ← CellFifo.drainPopback (grow-resize rows)  (never blocks)
MESSAGE thread: Model flush → tree → View reacts; screenDirty → Session::drain (two-phase)
                → CodeModel mutations → CodeView.calc() → paint → juce::Graphics
                → Registry dispatch → GPU LLGC | CPU LowLevelGraphicsGlyphRenderer
                → shared GlyphAtlas
                Processor::parameterChanged (fires HERE, message thread) → wake reader
```

### S2 — Projection arithmetic (replaces liveTailExtent — nvim invariant)

```cpp
// Document invariant: the live region is ALWAYS the last viewportLineCount
// logical lines of the active screen. By arithmetic, never by remembered state.
//
//   liveFirstLine        = model.getNumLines() - viewportLineCount   // ≥ 0 by drain order
//   row_to_line   (row)  = liveFirstLine + row                        // grid row → document line
//   line_to_row   (line) = line - liveFirstLine                       // < 0 ⇒ history (scrollback)
//
// Absolute line identity across FIFO eviction (nvim sb_deleted analog):
//   absoluteLine  (line) = model.evictedLines() + line
// Stable anchor for scroll position and selection (P10) across eviction.
```

### S3 — Two-phase drain (Session, message thread)

```cpp
void Session::drain()
{
    // Phase 1 — history: joined logical lines enter ABOVE the live region.
    // CellFifo::drainHistory joins isContinued rows; entry-header flags
    // (isContinued, OSC 133 marks) land in CodeLine::{isContinued, mark}.
    jam::CodeLine line;
    while (fifo.drainHistory (line) == jam::CellFifo::ReadResult::ok)
        model.insertAt (liveFirstLine(), std::move (line));

    // Phase 2 — live tail: 1:1 replacement of the last viewportLineCount lines.
    int row = 0;
    while (fifo.drainActive (line) == jam::CellFifo::ReadResult::ok)
        model.replaceAt (liveFirstLine() + row++, std::move (line));

    // HARD INVARIANT preserved by phase order: no row exists in both
    // history and live regions in the same tick.
}
```

### S4 — Width SSOT + reserved gutter + winsize publication (terminal::View::resized)

```cpp
void terminal::View::resized()
{
    auto const bounds     = getLocalBounds();
    auto const [cw, ch]   = jam::Size<int16_t> (model.getCellSize());   // config-derived
    int  const gutter     = scrollbarThickness;                          // config-derived

    // The ONLY width computation. Scrollbar visibility never re-enters layout.
    int const cols = (bounds.getWidth() - gutter) / cw;
    int const rows =  bounds.getHeight()          / ch;

    codeView.setBounds (bounds);
    codeView.setViewportLineCount (rows);                    // projection consumes SSOT

    model.setWinsize (jam::Size<int16_t> (cols, rows));      // ONE atomic parameter —
}   // Processor::parameterChanged fires (message thread) → P6 protocol → P9 popback
```

### S5 — CodeView public contract (from-scratch target; contract unchanged, implementation new)

```cpp
namespace jam
{
/** Pure view. Renders externally-owned CodeModel. No state owner, no tree
    listener, cell-space API only. Paints through juce::Graphics — engine
    dispatch (GPU LLGC / CPU glyph renderer) is the Registry's business. */
class CodeView : public juce::Component
{
public:
    explicit CodeView (const CodeModel& document);

    void setFont (const juce::Font&);            // cell metrics from config
    void setViewportLineCount (int rows);        // live-region size (SSOT rows)
    void setCaretPosition (int row, int col);    // grid coords; mirrors terminal cursor
    void setCaretShape (CaretShape);             // DECSCUSR
    void setWrapEnabled (bool);
    void calc();                                 // projection recompute after model mutation
    void scrollToBottom();

    // P10 — selection surface (document coordinates; resize-stable; eviction-stable
    // via evictedLines). Word-wise via Char::isWordChar; grapheme-safe via UAX #29
    // services; SPACER_TAIL snap; Shift-override when app mouse reporting active.
    juce::String getSelectedText() const;        // Grapheme-expanded extraction

private:
    const CodeModel& document;                   // never owned, never mutated
    // Viewport + ContentView + CaretComponent structure retained;
    // ContentView::paint builds jam::GlyphArrangement for visible wrapped
    // rows only → arrangement.draw (g) → JUCE → LLGC → GlyphAtlas.
};
} // namespace jam
```

### S6 — Ring sizing (P6, at the single setSize call site)

```cpp
// Processor::prepare — the lossless-equivalence invariant, explicit:
// capacity ≥ scrollbackLines full-width rows PLUS one u64 header slot per row.
// popback ring: one screenful (maxRows) — P9.
auto const cells   = static_cast<size_t> (scrollbackLines) * static_cast<size_t> (maxCols);
auto const headers = static_cast<size_t> (scrollbackLines);
fifo.setSize (cells + headers /* history */, activeCapacity, popbackCapacity);
// NEVER called with pending entries — P6 ordering rule: drain-fully → resize → resume.
```

### S7 — Build order (risk front-loaded into zero-concurrency contexts)

```
1. CodeView rewrite (JAM)      — validate with hand-seeded CodeModel in an END pane:
                                 text, wrap, scroll, caret, selection, BOTH engines.
                                 No PTY, no parser, no threads: rendering bugs have one home.
2. Width SSOT + gutter         — terminal::View::resized authority (S4); CodeView consumes.
3. §1.8 font seam              — fence-retire typeface handles (shader-swap discipline, P7).
4. terminal::Model (P12)       — XML layout, both directions, Processor as Model::Listener;
                                 validated with a scripted Processor stub before TTY exists.
5. Phase 4 wiring (END)        — Processor + TTY + Parser/Video subclass → CellFifo →
                                 drain (S3) → CodeModel; popback (P9); OSC 133 marks (P8);
                                 DEC 2026; SIGWINCH drain→resize→resume protocol (P6).
6. Pre-shaped run fast lane    — P13: codepoint→glyphIndex at drain, per-CodeLine runs
                                 (immutable history: shape once, forever). Atlas untouched.
```

## BLESSED Compliance Checklist

- [x] **Bounds** — bounded rings (sized by invariant, popback = one screenful), bounded CodeModel (scrollbackLines), bounded glyph cache (LRU 23k), bounded ParameterText (declared maxlen); drop-oldest never stalls the producer.
- [x] **Lean** — CodeView rewrite is mostly deletion (dead glyph machinery absorbed by framework); nvim's regeneration layer eliminated by single-store collapse; endless app-feature params excluded; no new modules — every layer already exists in JAM.
- [x] **Explicit** — one width SSOT, one packed winsize parameter (no tearing), one sizing expression, one drain order, one input route (PTY); wrap never baked into storage; marks travel with rows; all names ratified.
- [x] **SSOT** — CodeModel is THE document; terminal::Model is THE state machine (XML-declared schema); cols computed once; GlyphAtlas shared; Stamp table owns attributes; invariants arithmetic, not remembered state.
- [x] **Stateless** — CodeView pure view (owns no machinery state); Char property services stateless; projection recomputed, never cached across mutations; parameterChanged carries no state (atomics are the data).
- [x] **Encapsulation** — CodeView deaf (told, never listens); Registry hides engine choice; Vulkan never leaks above juce::Graphics; reader thread never touches CodeModel; no callback executes on the reader thread except its own loop.
- [x] **Deterministic** — reserved gutter (per-cell determinism); winsize a pure function of component size, published as one atomic value; drain order fixed; same atlas both engines → visual parity.

## Open Questions

None. All session questions resolved and ratified: P1–P13. Names ratified (P3 table, P12 schema, `CodeLine::mark`, `evictedLines`, `popback`, `insertAt`, `winsize`/`gridSize`/`cellSize` as packed `jam::Size<int16_t>` singles, `bell`, `clearRequested`, `applicationKeypad`, `mouseSgrEncoding`). Any genuinely new name surfacing mid-implementation stops for ARCHITECT per Decision Gate.

## Handoff Notes

- **Prerequisite:** RFC-vt-correctness.md precedes Phase-4 wiring (S7 step 5) — it delivers correct wide/grapheme cursor arithmetic (V1), `CodeLine::mark` values (V3), `Char::linkId` + `jam::Link` (V5), and the revived conformance suite (V6). S7 step 1 (CodeView rewrite) is independent and may run in parallel.
- **Session decisions ARCHITECT-ratified:** P1/P2 (CodeView kept, pure view, from-scratch), P3 (analogous-to-nvim, threading excepted), P5 (reserved gutter, width SSOT), P6 (loss equivalence + sizing + ordering rules), P7 (fence-retire font seam), P8 (PTY-authoritative + OSC 133, marks with rows), P9 (popback in scope, rides resize protocol), P10 (selection + clipboard full, mouse mode set fixed), P12 (terminal::Model SSOT, Processor as Model::Listener, packed `jam::Size` dimension params), P13 (fast lane via drain-time capture, atlas untouched). No legacy, no backward compatibility, no baggage — correctness first.
- **References for Engineer delegation:** nvim `src/nvim/terminal.c` is the positive reference (file:line map in R8); endless is anti-pattern evidence + parameter inventory ONLY (R7) — do not transplant its Display/liveTailExtent/flush-timer/row-indexed-OSC-133 shapes. Endless's XML `buildLayout` pattern IS worth transplanting (P12).
- **Threading contract for every delegated step:** `parameterChanged` fires on the calling thread (`jam_Model.h:36–48`) — Processor's listener body runs on the MESSAGE thread and may only wake the reader, never touch reader-owned state; the reader consumes atomics at loop top. GlyphAtlas and CodeModel are message-thread-only. The reader's only interfaces: TTY, Parser, Video, Buffer<Row>, CellFifo push/drainPopback, Model store().
- **Sequencing:** this work sits in `jam_gui`/`jam_graphics`/`jam_terminal` + END terminal layer — zero file overlap with the in-flight jam_vulkan `vk::` sweep (PLAN-vulkan-hpp-adoption, Steps 3–7 remaining). CodeView reaches Vulkan only through juce::Graphics dispatch. Sprint sequencing vs the sweep is ARCHITECT's call; both gate on ARCHITECT builds.
- **RFC Fidelity:** every principle P1–P13, the vocabulary table (P3), the full P12 schema (both directions, exclusions included), all Scaffold invariants (S2, S3 phase order, S4 SSOT + single winsize write, S6 sizing), and the S7 order must map to PLAN steps or be explicitly descoped by ARCHITECT.
- **Doxygen:** JAM + project doxygen XML mandatory before any code task (CLAUDE.md protocol); regen is ARCHITECT's (`ninja doxygen`).
- **Validation gates:** S7 step 1 is deliberately concurrency-free — if text renders wrong there, the bug has one home. Step 4 validates the Model bidirectionally with a scripted Processor stub before any TTY exists. Both engines (GPU + CPU fallback) must pass the same seeded-CodeModel visual gate before Phase 4 wiring begins.
