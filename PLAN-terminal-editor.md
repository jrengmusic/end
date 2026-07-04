# PLAN — Terminal Editor: CodeView Rewrite + jam_terminal Wiring

Date: 2026-07-03
Source: RFC-terminal-editor.md (all P1–P13 ratified, zero open questions)
Prerequisite: RFC-vt-correctness.md — **LANDED** (Sprint 58/59; conformance suite 84/84 green at `tests/`)

## Sequencing Constraint (ARCHITECT-directed this session)

Another COUNSELOR session is expanding `jam_vulkan`. **Step 4 (§1.8 font seam) edits
`jam_vulkan/font/` and is SEQUENCED AFTER that sprint lands — executes only on
ARCHITECT's explicit go signal.** All other steps have zero jam_vulkan file overlap:
CodeView reaches Vulkan only through juce::Graphics dispatch (RFC Handoff). Extra
caution mandate applies to every step regardless.

## Code-Reality Deltas (Pathfinder-surveyed, COUNSELOR-verified)

Divergences between RFC research claims and current source, resolved in-plan:

- **D1** — `jam::CodeModel` has NO `insertAt` and NO `evictedLines`
  (`jam_graphics/detail/jam_CodeModel.h:93–142` — verbs are `append`/`replaceAt`/
  `remove`/`clear`/`setCapacity`; `append` evicts silently, uncounted). → Step 1.
- **D2** — `CellFifo` has TWO rings only (`historyRing`/`activeRing`,
  `jam_CellFifo.h:96–97`); no popback ring, no `pushPopback`/`drainPopback`;
  `setSize` takes two capacities (`:250`). Drain verbs return `bool` (ReadResult is
  private) — S3 scaffold's `ReadResult::ok` comparison adjusts to the real bool API. → Step 6.
- **D3** — RFC R4 claims Video has "protected virtual hooks for subclassing" —
  **stale**: zero virtuals exist anywhere in `jam_terminal/video/`. Video
  communicates via a caller-owned Events map fired on the READER thread
  (`jam_CursorState.h:91–94, :368`). The departed-line hook must be CREATED. → Step 6.
- **D4** — `jam::Typeface`, `jam::Font`, `jam::glyph::Arrangement`,
  `jam::glyph::Graphics` referenced by code_view are DELETED symbols (only dead
  references remain: `jam_CodeView.cpp:40,127`, `jam_CaretComponent.h:121–150`).
  Confirms P1 full rewrite; nothing to preserve. → Step 2.
- **D5** — There is NO `terminal::Controller` (ARCHITECT-corrected). ARCHITECTURE.md
  is meta-MVP (Model–View–Processor, ARCHITECTURE.md:557–583) and already documents
  Session/Processor/Model/View correctly. Stale mentions to sync: CLAUDE.md's
  layer-order line (`terminal::Controller`), and ARCHITECTURE.md's `liveTailExtent`
  references (:118, :194, :531–534) which RFC S2 replaces with projection
  arithmetic. → Step 8.
- **D6** — SPEC §1.8 text (SPEC.md:157–161) names deleted symbols
  (`jam::Typeface`, `jam::glyph::Atlas`); the seam concern survives in GlyphAtlas
  form (`jam_vulkan/font/`). Mechanism per P7: fence-retire at paint/fence boundary. → Step 4.
- **D7** — No buildLayout in END; reference implementation at
  `endless/Source/terminal/Model.cpp:40–125` + `Parameters.xml` (59 LOC) —
  transplant target per RFC P12/R7. → Step 5.

## Steps (S7 order preserved; step 4 gated)

### Step 1 — JAM: CodeModel API additions
`jam_graphics/detail/jam_CodeModel.h`
- `insertAt (int lineIndex, jam::CodeLine&&)` — nvim `ml_append_buf` analog (P3
  table, name ratified). Inserts before `lineIndex` on the active screen; capacity
  eviction from front, same as `append`.
- Per-screen `evictedLines` counter (int, monotonic) — incremented at EVERY
  front-eviction site (`append`, `insertAt`, `setCapacity`); reset by `clear`.
  Getter `evictedLines()` on active screen. nvim `sb_deleted` analog (P3/S2) —
  absolute-line anchor for scroll/selection across FIFO eviction (P10).
- Doxygen moves with API (Delegation Protocol).

### Step 2.5 — CONFORMANCE FOLD (ARCHITECT-directed, this session)
The Step-2 rewrite delegated text layout to juce shaping — WRONG: it bypassed the
measured CPU half of the glyph pipeline (shaping+layout ≈ half the throughput,
ARCHITECT's undocumented rationale, now recorded). Ruling: CONFORMANCE — old
semantics verbatim (recovered from git e8543bf1a^ and END history 40d2163/de22c5c),
today's Vulkan winners underneath. jam_vulkan IS the font domain (both engines).
- `jam_vulkan/font/jam_Typeface.h`: revived `jam::Typeface` as
  `SharedResources<Typeface>`; `Entry { juce::Typeface::Ptr, hb_font_t* }` built
  from bytes (hb_blob→face→font, RAII); Registry owns the instance ("hand in hand
  with Glyph" — nobody uses Typeface without the atlas). hb via JUCE's embedded
  harfbuzz include path (old END precedent, de22c5c CMakeLists).
- `jam_vulkan/font/jam_GlyphArrangement.*`: restored old `glyph::Arrangement`
  semantics — `shape()` → Entry[], `buildDrawRuns()` → Run[] batched by
  (typeface, isEmoji, colour), cell-arithmetic positions, cmap via own hb_font
  nominal lookup; `tryLigature` verbatim (ASCII/same-style window 3→2, ligature
  iff 1 glyph out, cellWidth-normalized advances, skip=len−1, `setLigatures`
  config gate); `draw(g)` dispatches runs via LLGC setFont/drawGlyphs (instanced
  GPU / SIMD CPU — the winners). jam_graphics stub deleted; jam_gui→jam_vulkan
  dependency restored (the old direction).
- Embolden restored at the FT rasterize site (`FT_Outline_Embolden (outline,
  1<<6)` pre-render, `GlyphAtlas::setEmbolden` + flush) — verbatim old chain.
- END: LookAndFeel registration loop feeds jam::Typeface entries; theme wiring
  through LookAndFeel/View handlers: cursor block (char/style/blink/
  blink_interval; force plumbed for Step 6 DECSCUSR gate), `ligatures`,
  `embolden`, font/cell hot-reload. Caret glyph-char rendering restored.
- Blank-first-window defect investigated/fixed in the same round.
- Collision care: edits confined to jam_vulkan/font/ + jam_gui + jam_graphics
  stub removal + END; jam_vulkan/context|registry|resource|shaders untouched.

### Step 2 — JAM: CodeView rewrite (delete-first)
`jam_gui/code_view/` — old implementation deleted first (Refactor-Rewrite
Discipline); endless + old CodeView are reference-only (P1).
- **Contract (S5, unchanged):** pure view, renders externally-owned `const CodeModel&`,
  no state owner, no tree listener, cell-space API only: `setFont`,
  `setViewportLineCount`, `setCaretPosition (row, col)`, `setCaretShape` (DECSCUSR),
  `setWrapEnabled`, `calc`, `scrollToBottom`, `getSelectedText`.
- **Paint:** ContentView::paint builds `jam::GlyphArrangement` for visible wrapped
  rows only → `arrangement.draw (g)` → juce::Graphics → Registry dispatch (GPU LLGC /
  CPU LowLevelGraphicsGlyphRenderer). CodeView never names an engine (R3/P2).
  Extension point stays `jam_GlyphArrangement.cpp:16` — not filled until Step 7.
- **Projection (S2):** `liveFirstLine = numLines − viewportLineCount`;
  `row_to_line(row) = liveFirstLine + row`; `line_to_row(line) = line − liveFirstLine`
  (< 0 ⇒ history); `absoluteLine(line) = evictedLines() + line`. Arithmetic, never
  remembered state — liveTailExtent is the recorded anti-pattern (R7).
- **Wrap (P5):** storage width-free; physical rows exist only in paint-time
  `getWrappedLines(viewWidth)`; physical↔logical mapping (caret, scroll, selection)
  lives in the projection layer.
- **Gutter (P5):** scrollbar width ALWAYS reserved; visibility toggles drawing only,
  never layout. `physicalViewWidth` becomes a consumer of the width SSOT, never an
  authority.
- **Selection + clipboard (P10, in from the first line):** anchors in DOCUMENT
  coordinates; eviction-stable via `evictedLines`; word-wise `Char::isWordChar`;
  grapheme-safe UAX #29 step services; SPACER_TAIL snap; copy extracts via Grapheme
  table expansion. Shift-override when app mouse reporting active.
- **Caret:** CaretComponent rewritten (DECSCUSR block/underline/bar, blink) without
  dead `jam::Font`/`Typeface` symbols — cell metrics from `setFont`.
- Re-enable in `jam_gui.h:102–103`.
- **Validation gate (S7.1, ARCHITECT builds/runs):** hand-seeded CodeModel in an END
  pane — text, wrap, scroll, caret, selection, BOTH engines. Zero concurrency:
  rendering bugs have one home. Gate passes before Step 6 begins.

### Step 3 — END: width SSOT + reserved gutter
`Source/terminal/View.h` (+ .cpp as needed)
- `terminal::View::resized()` is THE width authority (S4):
  `cols = (width − scrollbarThickness) / cellWidth`, `rows = height / cellHeight`;
  `codeView.setViewportLineCount (rows)`. Cell metrics + scrollbarThickness from
  config::Model.
- `model.setWinsize (jam::Size<int16_t> (cols, rows))` line lands in Step 5 when
  terminal::Model exists — the computation and single-authority shape land here.
- Never two independent width computations (P5 consequence).

### Step 4 — §1.8 font seam: fence-retire ⚠ GATED (jam_vulkan collision)
`jam_vulkan/font/` — **waits for the other COUNSELOR's jam_vulkan sprint to land;
executes on ARCHITECT's explicit go signal only.**
- P7: retire old typeface handles at the paint/fence boundary, not at
  config-change time — the shader hot-reload discipline, same cure (SPEC §1.8 is a
  B violation record; mechanism now decided by P7).
- SPEC.md §1.8 text updated afterward (names deleted symbols today — D6).

### Step 5 — END: terminal::Model (P12, full schema)
`Source/terminal/` — new `Model`. **ARCHITECT supersession (this session) of RFC
P12's "XML buildLayout" clause:** END has NO XML parameter path — the established
pattern is code-declared `createAndAddParameter<T> (state, ID::x, default)` at the
owner's `registerParameters()` (config::Model Config.cpp:132, end::View View.cpp:90,
PaneView.h:38, Panes.cpp:14, MessageOverlay.h:95). terminal::Model follows THAT
pattern. **Second ARCHITECT ruling (this session): schema TABLES are also
rejected** — table-per-group encodes the group as code structure (N arrays, N
loops, dual-semantic value columns; "worse than XML"). Final ratified mechanism:
`registerParameters()` as a straight-line `createAndAddParameter` call list,
DEFINED INLINE IN Model.h (header-only Model, Model.cpp deleted) — one line per
parameter, scaling = add/remove a line; NORMAL/ALTERNATE via a private
`registerScreenParameters (screenType)` called twice. No magic strings
(Identifier.h X-macro / jam::ID entries), no tables, no loops, no conditional
dispatch. Registration runs once per Session at Model construction — zero
hot-path involvement. endless Parameters.xml remains the schema INVENTORY
reference only — never a mechanism transplant.
- **Direction A (reader → message):** SESSION: `gridSize` (Size<int16_t>),
  `activeScreen`, `syncOutputActive`, `shellExited`, `bell` (counter), `promptRow`
  (live grid row only), `pasteEchoRemaining`. MODES: `applicationCursor`,
  `applicationKeypad`, `bracketedPaste`, `mouseTracking`, `mouseMotionTracking`,
  `mouseAllTracking`, `mouseSgrEncoding`, `focusEvents`, `win32InputMode`.
  SCREEN ×2 (normal/alternate): `cursor` (packed row|col|visible),
  `cursorShape`, `cursorColor`, `keyboardFlags`, `screenDirty` (counter — the drain
  trigger). TEXT: `title` (256), `cwd` (4096), `foregroundProcess` (256).
- **Direction B (message → reader):** `winsize` (Size<int16_t>), `cellSize`
  (Size<int16_t>), `scrollbackLines`, `clearRequested` (counter).
- **Excluded (ratified):** `preview`/`splitCol`, `hintPage`/`hintTotalPages`,
  `snapshotDirty`, split `width`/`height`, row-indexed OSC 133 block params.
- **Packed-dimension rule:** every dimension pair is ONE atomic via
  `jam::Size<int16_t>::toInt()` round-trip (`jam_Size.h:71`) — split pairs tear.
- **Processor is a `Model::Listener`:** `parameterChanged` fires on the CALLING
  thread (`jam_Model.h:36–48`) = message thread for Direction B. Callback is the
  WAKE (nudge TTY poll wake fd); atomic is the DATA; reader `load()`s at loop top.
  No callback ever executes on the reader thread except its own loop.
- Step 3's `setWinsize` publication line lands here.
- **Validation gate (S7.4):** scripted Processor stub exercises both directions
  before any TTY exists.

### Step 6 — Phase 4 wiring (JAM transport/video + END terminal layer)
**JAM — CellFifo popback ring (P9, D2):** third ring, REVERSED direction
(message-thread producer via `pushPopback`, reader-thread consumer via
`drainPopback`); capacity one screenful (maxRows); `setSize` gains third capacity.
Same seqlock/drop-oldest fabric as existing rings — no new pattern.

**JAM — Video departed-line hook (D3):** Video currently has no subclass hooks; RFC
S1 ratifies `terminal::Video (subclass)` owned by Processor. Create the protected
virtual departed-line hook (departed top row → subclass override →
`CellFifo::pushHistory`) plus whatever sibling hooks the S1 flow requires (live-row
delivery for `pushActive`). Hook mechanism proposal goes to ARCHITECT at
implementation time if any shape exceeds what S1 already names (Decision Gate).

**END — Processor:** owns TTY (UnixTTY/WindowsTTY), Parser, Video subclass,
CellFifo (three rings), `Buffer<Row>` scratch. Reader loop: TTY.read → Parser.feed
→ Video.process → CellFifo push / Model store() — never blocks (no lock, wait,
stall, yield, sleep).
- **Ring sizing (S6, single call site in `Processor::prepare`):**
  `cells = scrollbackLines × maxCols`, `headers = scrollbackLines`,
  `fifo.setSize (cells + headers, activeCapacity, popbackCapacity)` — never called
  with pending entries.
- **Resize protocol (P6, ordering rule):** drain-fully → resize → resume. Endless's
  resize-clear (`Processor::prepare` calling `cellFifo.setSize()` per SIGWINCH) is
  the recorded bug (R7) — never reproduce.
- **Popback sequence (P9):** `parameterChanged(winsize)` (message) → Session drains
  history+active fully → pops CodeModel tail rows (height delta) → `pushPopback` →
  stores winsize atomics → reader wakes, resizes grid, `drainPopback` into grid
  bottom, resumes.

**END — Session::drain (S3, message thread, screenDirty-batched):** two-phase —
Phase 1 history: `drainHistory` (joins isContinued; header flags → `CodeLine::
{isContinued, mark}`) → `insertAt (liveFirstLine(), …)`. Phase 2 live tail:
`drainActive` → `replaceAt (liveFirstLine() + row++, …)`. Drain verbs are bool
returns (D2). HARD INVARIANT: no row in both regions in the same tick.

**Marks (P8):** OSC 133 marks travel WITH rows in the CellFifo entry-header flags
byte, stamped onto `CodeLine::mark` at drain (`Mark` enum already live —
`jam_CodeLine.h:49–55`). Model keeps only live `promptRow`. Row-indexed Model
params are the recorded anti-pattern (R7).

**Input (P8):** keystrokes → TTY always; jam::Keyboard encodes; mode atomics read
lock-free at encode time; input bypasses the Model. CodeModel is mutated ONLY by
drain (MODE_TERMINAL structural analog). Mouse modes shipped: 1000/1002/1003/1006/
1004; Shift-override for local selection.

**DEC 2026:** Video already tracks `syncOutputActive` (`jam_CursorState.h:497`,
deadline `:514`, expiry `:294`); wire accumulate-then-flush into drain scheduling +
`syncOutputActive` param.

**Codepoint-rendering baseline (ARCHITECT-ratified, 2026-07-04):**
`tests/scripts/emoji_test.sh` + `tests/scripts/render-test.sh` are THE acceptance
baseline for all codepoints END supports — run in the live pipeline once this step
lands. Covers: basic emoji, VS16/VS15 presentation + width switch, ZWJ sequences,
flags (regional pairs), skin tones, keycaps, CJK/fullwidth, box drawing (EAW
Ambiguous width 1), combining marks (width 0), NF PUA icons (width 1); SGR
attributes, 16/256/truecolor, ligature-adjacent ASCII. Glyph resolution/cluster
shaping restored at Step 2.5; width/segmentation authority is Parser→Char→Video —
exercised only through this live-TTY baseline, not the S7.1 harness.

### Step 7 — P13 fast lane (LAST — optimization on a working path)
- Codepoint→glyphIndex resolved at DRAIN time (Parser owns codepoints); one flat
  cached map per (typeface, size); immutable history lines shape once, store the
  glyph-index run in the CodeLine, forever; paint = lookup + quad append.
- Slots into `jam_GlyphArrangement.cpp:16` extension point. `GlyphAtlas::Key` stays
  glyphIndex-keyed — NO atlas redesign, NO jam_vulkan edits.

### Step 8 — Doc sync (descriptive, code is ground truth)
- CLAUDE.md layer-order line: `terminal::Controller` → meta-MVP reality
  (Session / Processor / Model / View — ARCHITECTURE.md:557–583 is already correct).
- ARCHITECTURE.md: retire `liveTailExtent` (:118, :194, :531–534) in favor of S2
  projection arithmetic; drain-sequence wording (:187–198) to the S3 two-phase
  shape; CodeView contract section (:586–596) to the S5 surface.
- SPEC.md §1.8 rewrite folds into Step 4's completion (D6), not here, since Step 4
  is gated.

## Reference Discipline (ARCHITECT-directed this session)
- **Current END layout IS the ground of truth.** RFC scaffolds map onto it; they do
  not replace it.
- **endless** (`~/Documents/Poems/dev/endless`) — working reference for LAYOUT
  MAPPING only. NEVER rewrite verbatim from it. Anti-pattern evidence + domain
  inventory (R7); the one transplant is the XML `buildLayout` pattern (P12/D7).
- **neovim** (`~/Documents/Poems/dev/neovim`) — isomorphic abstraction, ANALOGOUS
  to the objective (R8/P3). Structure guides; code is never copied.

## Rejected Alternatives (carried from RFC P11 — binding)
juce::CodeEditorComponent (R1); CodeView as Model::Component; overlay scrollbar;
locally-edited prompt; blank-rows-on-grow; nvim read-pause/same-thread coupling;
nvim dual store + regeneration; unbounded "strictly lossless" transport;
row-indexed OSC 133 params; split viewCols/viewRows; GlyphAtlas::Key redesign.

## Threading Contract (binds every Engineer delegation)
- **Direction-A write path (fact, jam_Model.cpp:46–97):** `ParameterAdapter::
  flushToTree` is CAS-gated on `needsUpdate`, which is set ONLY by
  `parameterValueChanged` — i.e. by `Parameter<T>::setValue()`. Raw
  `getRawParameterValue()->store()` NEVER reaches the ValueTree. Reader-thread
  Direction-A writes therefore go through `setValue()` (atomic store + dirty mark
  + coalesced AsyncUpdater cross-thread; lock-free steady-state). Raw atomics are
  the READ path for non-message threads (ARCHITECTURE.md:92), not the write path.
  RFC P12's "store() atomics; 60Hz flush()" is realized by this mechanism.
- `parameterChanged` fires on the calling thread (`jam_Model.h:36–48`); Processor's
  listener body runs on the MESSAGE thread and only wakes the reader.
- GlyphAtlas and CodeModel are MESSAGE THREAD only.
- Reader's only interfaces: TTY, Parser, Video, Buffer<Row>, CellFifo
  push/drainPopback, Model store().
- No lock, no wait, no stall, no yield, no sleep, FIFO keep-last.

## RFC Traceability
P1→Step 2 · P2→Steps 2/3/5 · P3 vocabulary→Steps 1/2/6 · P4 (single store, no
regeneration layer)→Steps 2/6 · P5→Steps 2/3 · P6→Step 6 · P7→Step 4 (gated) ·
P8→Step 6 · P9→Step 6 · P10→Step 2 · P11→Rejected list · P12→Step 5 · P13→Step 7 ·
S1→Step 6 · S2→Step 2 · S3→Step 6 · S4→Steps 3/5 · S5→Step 2 · S6→Step 6 ·
S7 order→Steps 2,3,4,5,6,7 (step 4 resequenced after jam_vulkan sprint —
ARCHITECT-directed). Step 1 is the D1 enabling delta; Step 8 is the D5/D6 doc sync.
Nothing descoped.
