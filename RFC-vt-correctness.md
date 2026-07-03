# RFC — VT Correctness + Conformance Hardening (jam_terminal)
Date: 2026-07-03
Status: Ready for COUNSELOR handoff — PREREQUISITE of RFC-terminal-editor.md

## Problem Statement

The ghostty comparison (libghostty-vt at `~/Documents/Poems/dev/ghostty/`, read this session) surfaced six findings against jam_terminal. ARCHITECT directed a separate comprehensive RFC; this work is a **prerequisite** of RFC-terminal-editor.md — it hardens the cell-write layer, the transport vocabulary, and the validation story that the terminal-editor build depends on.

The six items (ARCHITECT-commissioned, verbatim scope):
1. Wire `Char::fromCodepoint` + UAX #29 into Video's cell write — the one correctness fix ghostty proves is mandatory; the machinery already exists in-house. Mode 2027 comes almost free after.
2. DEC 2026 auto-reset timeout — one guard, cheap insurance.
3. Row-level semantic prompts — ghostty stores `semantic_prompt` per Row (input/output/prompt/prompt_continuation), independently confirming the `CodeLine::mark` decision; adopt their 4-state vocabulary for the mark values.
4. Resize coalescing — ghostty coalesces at 25ms in a writer thread; END's design coalesces **by construction** (keep-last atomic `winsize`, reader applies latest at loop top). Recorded as a property, not a mechanism.
5. OSC 8 hyperlink document carrier — ghostty stores hyperlinks per-cell (hyperlink bit + per-page string table); jam captures OSC 8 URIs but no document carrier exists for cell↔hyperlink association. `Char` has 23 padding bits. In scope by ARCHITECT direction.
6. Conformance — jam_terminal has zero in-tree tests; ghostty's coverage is why it is trustworthy as a reference. Revive the proven harness; adopt external esctest/vttest passes.
7. **SKiT absorption (ARCHITECT-directed, added):** the whole endless SKiT — orchestrator + Sixel/Kitty/iTerm2 decoders + platform-native image codecs + the SKiT preview protocol — is absorbed into JAM. The endless working tree holds the FINAL iteration: working, just never wired to render images. Absorption = module migration; render wiring stays a consumer concern.

## Research Summary

### R1 — jam_terminal cell-write today (the correctness gap)

- Video writes every cell `Char::make (cp, CONTENT_CODEPOINT, NARROW, sid)` — no width resolution, no segmentation, no combining handling; "width resolution deferred to renderer" (`jam_VideoEdit.cpp` + cursor ops). This is NOT a rendering deferral: **cursor advance arithmetic is wrong for CJK/emoji today** — a 2-column character must advance the grid cursor by 2 at write time, and combining marks must not advance at all.
- The machinery already ships in jam_graphics (`jam_Char.h`):
  - `Char::fromCodepoint (cp, styleId, lineDrawing)` — DEC translation + width lookup + wide hint + pack, single-cell producer (`:215–228`).
  - `Char::width()` — East Asian Width 0/1/2 (`:230–240`); `Char::isCombining()` (`:252–261`).
  - `Char::graphemeSegmentationStep (state, cp)` / `graphemeSegmentationInit()` — UAX #29 step functions; **doc names the owner: "The grapheme segmentation state machine is caller-owned (Video holds the running state)"** (`jam_Char.h:208–212`). The wiring was designed and never landed.
  - Wide encoding contract already defined: left cell `WIDE`, right cell `SPACER_TAIL` (codepoint 0) — "the renderer skips tail cells" (`jam_Char.h:28–31`).
- `jam::Grapheme` (`jam_graphics/detail/jam_Grapheme.h`): `SharedResources<Grapheme>` interning table — `Entry` holds up to 8 codepoints; `addIfNotAlreadyThere(entry) → int`; the index goes into `Char::codepoint()` under `CONTENT_GRAPHEME`. `SegmentationResult` = packed 16-bit state + `addToCurrentCell` flag. Complete, waiting.
- `jam::Stamp` (`jam_graphics/detail/jam_Stamp.h`): the interning-table precedent — `SharedResources<Stamp>`, Video resolves an Entry once per SGR pen change and stamps the styleId on every cell of the run. **Whatever cross-thread publication contract Stamp uses between reader-side interning and message-side resolution applies identically to Grapheme and to the new Link table — same base template, same access pattern.**

### R2 — ghostty facts driving each item

- **Write-time width + clustering:** ghostty resolves East Asian width and UAX #29 AT CELL WRITE — cell wide field (narrow/wide/spacer_tail/spacer_head), multi-codepoint clusters in a sparse per-page grapheme map, stateful `graphemeBreak(cp1, cp2, state)` over a precomputed 8KB break table (`src/unicode/grapheme.zig`, `src/terminal/page.zig:139+`), mode 2027 supported (`src/terminal/modes.zig`).
- **2026 auto-reset:** ghostty auto-resets synchronized output after **1000ms** if the application never clears it — protection against a hung/buggy app freezing the display (per `src/terminal/Terminal.zig` mode handling + writer-thread sync timer).
- **Semantic prompts:** per-Row `semantic_prompt` with 4-state vocabulary — `input / output / prompt / prompt_continuation` — populated from OSC 133 (`src/terminal/Screen.zig` Row).
- **Resize coalescing:** 25ms coalescing timer in the writer thread (`src/termio/Thread.zig`).
- **Hyperlinks:** per-cell hyperlink association (cell hyperlink bit; per-page `hyperlink_set`/`hyperlink_map` + StringAlloc for shared URI/id strings) — OSC 8 links survive in the document, enabling hover/open on scrollback.
- **Trustworthiness:** ghostty's terminal core carries extensive in-tree test coverage; that coverage is why it serves as a comparison reference at all.

### R3 — jam_terminal coverage context (relevant subset)

- OSC 8 URIs + params are already captured (`jam_VideoOSCExt.cpp:27–67`); OSC 133 A/B/C/D markers handled (`jam_VideoOSCExt.cpp:69–99`); mode 2026 tracked with DECRQSS query support (`jam_VideoCSI.cpp:502–510`); DECAWM implemented; parser DFA is the same vt100.net model ghostty uses (15 states, table-driven, O(1)).
- CellFifo entry header = u64: cellCount | flags byte — flags bit 0 `isContinued`, bit 1 `isJustified` (`jam_CellFifo.h`). Budget exists for mark bits.
- `jam::CodeLine`: `isContinued` + `isJustified` bools today; `CodeLine::mark` (uint8) ratified in RFC-terminal-editor.md P8 — this RFC defines its values.

### R4 — Conformance history (this repo + endless; record corrected by ARCHITECT)

- **Catch2 unit suite** (removed at `b3f0fea`): 95 cases, 16 groups (Parser / Grid / State), fixture fed raw bytes and asserted grid/state fields; standalone `Test::Term` CMake target; found 2 real bugs during development. **Deleted because ALL GREEN** — validation scaffolding paid in full (ARCHITECT-corrected record; the "not production-ready" reading of the sprint log was wrong). Revival = re-instantiating a proven pattern, not repairing broken infra.
- **endless working-tree corpus** (`~/Documents/Poems/dev/endless/test/`): `emoji_test.sh` — 11 sections: width-2 emoji, VS16/VS15 variants, ZWJ sequences, flags, skin tones, keycaps, CJK, box drawing, combining marks, Nerd Font icons — a ready-made acceptance corpus for exactly item 1. Plus `render-test.sh` (SGR/colors, manual) and `braille_test.txt`.
- **SKiT context:** jam_terminal's raw APC/DCS passthrough is the designed seam the SKiT orchestrator consumes — passthrough is architecture, not a gap. The FINAL SKiT iteration lives in the endless WORKING TREE (not only git history) — see R5. Absorbed by V7.

### R5 — SKiT final iteration (endless working tree, read this session)

Files at `~/Documents/Poems/dev/endless/Source/terminal/`: `Skit.{h,cpp}` (157+343), `SixelDecoder.{h,cpp}` + `SixelDecoderParse.cpp`, `KittyDecoder.{h,cpp}` + `KittyDecoderDecode.cpp`, `ITerm2Decoder.{h,cpp}`, `ImageDecode.{h,cpp}` + `ImageDecodeGif.h` + `ImageDecodeMac.mm` + `ImageDecodeWin.cpp`; plan at `endless/PLAN-skit.md`. **Status per ARCHITECT: working — just never wired to render images.**

- **Skit** (`Skit.h:44–154`): reader-thread-only orchestrator, owned by Processor. Receives raw payloads from Video's DCS/APC/OSC handlers: `processDCS` (Sixel or `END;` filepath), `processAPC` (Kitty or `GEND;` filepath), `processOSC1337` (iTerm2 or `END;` filepath). Fires events through `jam::Function::Map<juce::Identifier, void>` — **already a JAM type**. `setCellSize(px)` feeds decode math; `getLastImageRows()` lets Video advance the cursor past the placement; `getLastResponse()` carries Kitty acks/query replies back to the PTY.
- **SixelDecoder** (`SixelDecoder.h:84–192`): full Sixel loop — raster attributes, 256-register palette (VT340 defaults), HLS→RGB, repeat/CR/NL bands, internal buffer growth → `DecodedImage` (RGBA8 HeapBlock).
- **KittyDecoder** (`KittyDecoder.h:52–274`): actions t/T/p/d/q; chunked `m=1/0` accumulation keyed by image id; zlib (`o=z`); formats 24/32/100(PNG via native codec); stored-image map for deferred `a=p`; **virtual placements** (`U=1`, `c=`/`r=`); OK/error responses with `q=` quiet levels.
- **ITerm2Decoder** (`ITerm2Decoder.h:49–66`): OSC 1337 `File=` → `ImageSequence` — multi-frame, animated GIF pre-composited with disposal methods, per-frame delays.
- **ImageDecode** (`ImageDecode.h:38–91`): platform-native codecs — macOS CoreGraphics/ImageIO (`.mm`), Windows WIC; `swizzleARGBToRGBA`; `ImageSequence { pixels, delays, frameCount, w, h }`.
- **Transfer types:** `DecodedImage` / `PendingImage` (`SixelDecoder.h:44–71`) — the reader→message handoff shape. **Locked thread decision** (`PLAN-skit.md` Decisions 1, proven at endless `6835980`): `MessageManager::callAsync` with MOVED HeapBlocks — ownership transfer, no SpinLock, no FIFO.
- **SKiT preview protocol:** `END;filepath[;cols;lines]` / `GEND;` envelopes → `previewFile` event (fzf-style file preview overlay; empty filepath = dismiss; shell integration via single OSC 1337 envelope). Capability advertisement proven in endless: DA1 `?62;4c` (Sixel bit), Kitty `a=q` → `ok`, `TERM_PROGRAM=END`.

## Principles and Rationale

### V1 — Write-time truth: width, wide cells, grapheme clusters (Decision)

The grid is column arithmetic; every quantity that affects column arithmetic must be resolved at the moment the cell is written — ghostty proves this is where correctness lives, and jam::Char's own documentation assigns Video the job.

- **Single-codepoint path:** Video's cell write becomes `Char::fromCodepoint (cp, styleId, useLineDrawing)` — translation, width, wide hint, pack in one call. Replaces the hardcoded-NARROW `Char::make` sites.
- **Wide advance:** width 2 → write head cell (`WIDE`), write `SPACER_TAIL` in the next column, advance cursor by 2. **Wide-at-last-column:** if only one column remains before the margin, DECAWM-wrap FIRST (mark row `isContinued`), then write the wide pair at the new row start — never split a wide pair across rows.
- **Combining path (width 0 / `addToCurrentCell`):** Video owns one `Grapheme::SegmentationResult` running state per screen, stepped via `Char::graphemeSegmentationStep` on every printed codepoint. When the step says extend: fold the codepoint into the PREVIOUS cell — cell becomes `CONTENT_GRAPHEME` holding a `jam::Grapheme` interned index (base + marks, up to 8 codepoints per Entry). Cursor does not advance. State resets on cursor motion, line change, screen switch, RIS (any discontinuity — UAX #29 state is a run property).
- **Interning:** `Grapheme::addIfNotAlreadyThere` on the reader thread, exactly the Stamp precedent (R1) — same `SharedResources` base, same reader-interns/message-resolves access pattern. No new cross-thread mechanism.
- **Mode 2027:** with write-time clustering in place, DECSET/DECRST 2027 + DECRQM report land as vocabulary + one Video flag (cluster-aware advance on/off; default per spec draft = on when supported). Near-free, as commissioned.
- **Acceptance:** `emoji_test.sh` corpus (R4) drives visual + unit acceptance for every section: VS16 width promotion, ZWJ, flags, skin tones, keycaps, CJK, combining marks.

### V2 — DEC 2026 auto-reset: loop-top guard, no timer (Decision)

Ghostty parity (1000ms) with zero new machinery: Video records a monotonic timestamp when 2026 sets; the reader loop — which already wakes at least every 100ms (TTY `waitForData` timeout, R3) — checks elapsed at loop top and force-clears sync (with the same downstream effects as a genuine RST: dirty + flush). Constant `syncResetMs = 1000`. No timer thread, no message-thread involvement, deterministic. Insurance against an application that sets 2026 and dies.

### V3 — Semantic mark vocabulary: ghostty's 4 states (Decision)

`CodeLine::mark` (ratified, RFC-terminal-editor.md P8) takes ghostty's independently-validated vocabulary:

```
none = 0 · prompt (133;A) · input (133;B) · output (133;C) · promptContinuation
```

`output` end (133;D exit-code) closes the block — carried as the next line's mark transition, matching ghostty Row semantics. **Transport budget:** CellFifo header flags byte — bit 0 `isContinued`, bit 1 `isJustified`, bits 2–4 mark (3 bits, 5 values, 3 spare). Stamped onto `CodeLine::mark` at drain. Consumers (jump-to-prompt, prompt-aware selection, semantic history boundaries) are RFC-terminal-editor.md scope; this RFC delivers the values and the transport.

### V4 — Resize coalescing is a property, not a mechanism (Record)

Ghostty needs a 25ms coalescing timer because resize events queue as messages. END's `winsize` is a keep-last atomic (`jam::Size<int16_t>` single parameter, RFC-terminal-editor.md P12): a resize storm overwrites one value; `parameterChanged` wakes may fire per-write but the reader applies only the LATEST value at loop top — intermediate sizes are never applied, by construction. First-in-first-out-keep-last, exactly as specified. No timer, nothing to build; recorded so nobody adds one.

### V5 — Hyperlink document carrier: `Char::linkId` + `jam::Link` table (Decision)

- **Carrier:** 16-bit `linkId` in `Char`'s padding (bits 41–56; 7 bits still reserved). `linkId == 0` = no link — zero-initialized Chars are linkless by construction. `sizeof(Char) == 8` unchanged; trivially-copyable unchanged; CellFifo transport carries links for free.
- **Table:** `jam::Link : SharedResources<Link>` — third instance of the established interning shape (Stamp, Grapheme, Link). `Entry` = URI + OSC 8 `id=` param (explicit-id links dedupe by id per spec; implicit links dedupe by URI). Interned on the reader thread at OSC 8 parse (`jam_VideoOSCExt.cpp:27–67` already captures both fields); resolved message-side under the Stamp precedent contract.
- **Video pen:** OSC 8 with URI sets the pen's active linkId (stamped on every subsequent cell, exactly like styleId); OSC 8 `;;` (empty) resets to 0. One uint16 added to pen state.
- **Consumers** (hover underline, click-to-open, link extraction in selection) are RFC-terminal-editor.md / CodeView scope; this RFC delivers carrier + table + pen so links survive into the document from day one — retrofitting a cell-level carrier later would touch every write site, hence prerequisite.

### V6 — Conformance: revive the ALL-GREEN harness, permanent this time (Decision)

- **Pattern:** the `b3f0fea` fixture shape — raw bytes in → Parser → Video → assert grid cells / cursor / mode state — proven end-to-end (95 cases, 16 groups, 2 real bugs found, ran to full green, deleted as paid-in-full scaffolding).
- **Home:** jam_terminal is now a FRAMEWORK module, not app code — the suite's home is JAM (standalone CMake test target, Catch2 single-header as before), permanent, because the module outlives any one application. Coverage targets: the V1 write path (width/wide/combining/cluster tables — every `emoji_test.sh` section as assertions), V2 auto-reset, V3 mark transport, SGR/CSI/OSC dispatch, DECAWM + wide-at-margin, alt-screen, scroll regions, the CellFifo join/flags contract.
- **External passes:** esctest + vttest run against the wired terminal as ARCHITECT-driven validation gates (agents never build/run — CAROL). `emoji_test.sh`, `render-test.sh`, `braille_test.txt` adopted from endless as the visual corpus.
- Conformance-suite findings are DCF violations when they contradict spec — resolved in-sprint, never deferred.

### V7 — SKiT absorption: decoders + orchestrator into JAM (Decision, ARCHITECT-directed)

The endless SKiT is the final, working iteration (R5) — it is absorbed, not rewritten. Endless is a suspect reference for *architecture*; SKiT is proven *implementation* whose only missing piece was render wiring, which is exactly the part that stays out.

- **What moves:** `Skit`, `SixelDecoder`, `KittyDecoder`, `ITerm2Decoder`, `ImageDecode` family (Gif/Mac/Win TUs) → new `jam_terminal/image/` subdirectory, module namespace. Transfer types (`DecodedImage`, `PendingImage`, `ImageSequence`) move with them. The event surface is already `jam::Function::Map` — the absorption is relocation + namespace + doxygen, not redesign. Platform TUs follow the existing jam_terminal precedent (TTY already splits Unix/Windows).
- **Wiring inside the module:** Video's existing DCS/APC/OSC-1337 passthrough seams (`jam_VideoDCS.cpp:87–113`, `jam_VideoOSCExt.cpp:101–110`) route to Skit when a Skit is attached — Processor owns and attaches it (endless shape). No Skit attached = passthrough behavior unchanged; image support is opt-in by construction.
- **Cursor contract:** after a displayed decode, Video advances the cursor by `getLastImageRows()` (rows from pixel height ÷ `cellSize`, fed by the P12 `cellSize` parameter through Processor). Kitty responses (`getLastResponse()`) write back to the TTY on the reader thread.
- **Reader→message transfer:** the locked endless pattern survives — decoded pixels cross via moved HeapBlocks (ownership transfer, no lock, no FIFO), consistent with the no-wait constraint since `callAsync` posts without blocking the reader.
- **Capability advertisement:** DA1 gains the Sixel capability (`?62;4c` shape), Kitty query answers `ok` via Skit — both proven in endless, both module-level.
- **Boundary (explicit):** rendering/placement — overlay component, atlas staging, animated-frame scheduling, document/line anchoring of image placements — is the CONSUMER side and is NOT in this RFC. Skit emits events with decoded RGBA + cell geometry; what a consumer paints with them is END scope (post terminal-editor wiring). This mirrors V3/V5: the module delivers carrier + data; consumers come later. Absorbing now is prerequisite-shaped for the same reason as V5 — the Video seams and cursor-advance contract touch the same write path V1 rebuilds; landing them together avoids touching Video twice.

## Scaffold

Design scaffold — structural code, compile-untested. Names in the Names Gate below.

### S1 — Video cell write (V1 core)

```cpp
// Video print path — replaces hardcoded-NARROW Char::make sites.
void Video::printCodepoint (uint32_t cp)
{
    auto const step = Char::graphemeSegmentationStep (segState, cp);
    segState = step;

    if (step.addToCurrentCell() and hasPreviousCell())      // combining / ZWJ continuation
    {
        foldIntoPreviousCell (cp);                           // → CONTENT_GRAPHEME via
        return;                                              //   Grapheme::addIfNotAlreadyThere
    }

    auto const cell  = Char::fromCodepoint (cp, pen.styleId, useLineDrawing);
    auto const width = Char::width (cp);                     // 1 or 2 (0 handled above)

    if (width == 2 and cursor.col == columns() - 1 and autoWrap)
        wrapLine();                                          // isContinued — never split a pair

    writeCell (cursor.row, cursor.col, stampLink (cell));    // linkId from pen (V5)
    if (width == 2)
        writeCell (cursor.row, cursor.col + 1,
                   stampLink (Char::make (0, Char::CONTENT_CODEPOINT, Char::SPACER_TAIL, pen.styleId)));

    advanceCursor (width);                                   // DECAWM at margin marks isContinued
}
// segState = Char::graphemeSegmentationInit() on: cursor motion, row change,
// screen switch, RIS — any discontinuity (UAX #29 state is a run property).
```

### S2 — 2026 auto-reset (V2)

```cpp
// Reader loop top — TTY::waitForData already bounds the wake interval to 100ms.
if (syncOutput.active and monotonicMs() - syncOutput.setAtMs >= syncResetMs)
    clearSyncOutput();   // identical downstream effects to a genuine DECRST 2026
```

### S3 — Mark vocabulary + transport (V3)

```cpp
struct CodeLine
{
    enum class Mark : uint8_t { none, prompt, input, output, promptContinuation };
    // ... existing fields ...
    Mark mark { Mark::none };            // ratified carrier, values defined here
};
// CellFifo header flags byte: bit0 isContinued | bit1 isJustified | bits2–4 mark
```

### S4 — Hyperlink carrier + table (V5)

```cpp
// jam_Char.h — padding bits 41–56 (7 bits remain reserved above)
uint16_t linkId() const noexcept;                        // 0 = no link
static Char withLinkId (Char c, uint16_t id) noexcept;   // pack helper for the pen

// jam_graphics/detail — third SharedResources instance (Stamp, Grapheme, Link)
struct Link : SharedResources<Link>
{
    struct Entry : SharedResource
    {
        juce::String uri;
        juce::String id;      // OSC 8 id= param; explicit ids dedupe by id, else by uri
        bool  operator== (const SharedResource&) const noexcept override;
        size_t hash()                             const noexcept override;
    };
};
// Video pen: uint16_t activeLinkId { 0 }; set on OSC 8 uri, cleared on OSC 8 ";;".
```

### S5 — Conformance fixture shape (V6, b3f0fea pattern)

```cpp
TEST_CASE ("wide at right margin wraps before writing", "[video][width]")
{
    Test::Term t { 4, 2 };                       // cols, rows
    t.feed ("abc\xE4\xBD\xA0");                  // 'a','b','c', U+4F60 (width 2)
    REQUIRE (t.cell (0, 3).codepoint() == 0);    // col 3 left empty — no split pair
    REQUIRE (t.line (0).isContinued());
    REQUIRE (t.cell (1, 0).wide()      == jam::Char::WIDE);
    REQUIRE (t.cell (1, 1).wide()      == jam::Char::SPACER_TAIL);
    REQUIRE (t.cursorCol()             == 2);
}
```

### S6 — Sequencing vs RFC-terminal-editor.md

```
This RFC (jam_terminal / jam_graphics — framework):
  V1 write path + V6 unit suite      → before/with terminal-editor S7 step 5 (Phase 4 wiring)
  V3 mark values + V5 carrier/table  → before terminal-editor drain lands (CodeLine/Char shapes)
  V2, V4                             → with V1 (same files, same sprint)
  V7 SKiT absorption                 → with V1 (same Video seams + cursor-advance contract;
                                       land together, touch Video once); render wiring = END, later
Zero overlap with the jam_vulkan vk:: sweep. CodeView rewrite (terminal-editor S7 step 1)
is independent of V1–V5 and may proceed in parallel; Phase-4 wiring may not.
```

### S7 — SKiT absorption map (V7)

```
endless/Source/terminal/                    →  jam_terminal/image/
  Skit.{h,cpp}                                 jam_Skit.{h,cpp}
  SixelDecoder.{h,cpp} + SixelDecoderParse     jam_SixelDecoder.*
  KittyDecoder.{h,cpp} + KittyDecoderDecode    jam_KittyDecoder.*
  ITerm2Decoder.{h,cpp}                        jam_ITerm2Decoder.*
  ImageDecode.{h,cpp,Gif.h,Mac.mm,Win.cpp}     jam_ImageDecode.*   (platform TUs — TTY precedent)
Transfer types: DecodedImage / PendingImage / ImageSequence move with their headers.
Video seams: jam_VideoDCS.cpp:87–113 (DCS q), jam_VideoOSCExt.cpp:101–110 (OSC 1337),
             APC route — call attached Skit when present, passthrough otherwise.
Processor: owns Skit (optional), feeds cellSize (P12 param), writes getLastResponse() to TTY,
           advances cursor by getLastImageRows() after displayed decode.
NOT moved: overlay/placement/rendering (consumer, END scope, post terminal-editor).
```

## Names Gate (proposed — ratify at PLAN intake)

| Name | Kind |
|---|---|
| `CodeLine::Mark` — `none / prompt / input / output / promptContinuation` | mark value enum (V3, ghostty vocabulary) |
| `jam::Link`, `Link::Entry { uri, id }` | hyperlink interning table (V5) |
| `Char::linkId()`, `Char::withLinkId()` | carrier accessors, padding bits 41–56 (V5) |
| `Video::segState` | per-screen UAX #29 running state (V1) |
| `Video::printCodepoint`, `foldIntoPreviousCell`, `stampLink` | write-path verbs (V1/V5, shapes per S1) |
| `syncResetMs = 1000` | V2 constant (ghostty parity) |
| `GRAPHEME_CLUSTERING = 2027` | VtVocabulary mode constant (V1) |
| `jam_terminal/image/` — `jam::…::Skit`, `SixelDecoder`, `KittyDecoder`, `ITerm2Decoder`, `ImageDecode` | absorbed SKiT family, module namespace per jam_terminal convention (V7) |
| `DecodedImage`, `PendingImage`, `ImageSequence` | transfer types, absorbed as-is (V7) |

## BLESSED Compliance Checklist

- [x] **Bounds** — Grapheme Entry ≤ 8 codepoints; linkId 16-bit with 0-sentinel; mark 3 transport bits; interning tables dedupe by construction; no unbounded growth paths added.
- [x] **Lean** — zero new mechanisms: fromCodepoint/width/segmentation/SharedResources all exist; V2 is one loop-top check; V4 is a documented property (explicitly NOT building ghostty's timer); Link is the third instance of an existing template; V7 is relocation of proven code, not a rewrite — render machinery explicitly excluded.
- [x] **Explicit** — width resolved at the one write site; cluster state reset rules enumerated; mark values fixed to a published vocabulary; linkId 0-sentinel; sync reset constant named.
- [x] **SSOT** — one write path for all printed codepoints (S1); Stamp/Grapheme/Link one interning pattern; mark values defined once, consumed by transport and document.
- [x] **Stateless** — Char services stay stateless (caller-owned segState per its own contract); tables hold interned values, not machinery state.
- [x] **Encapsulation** — everything lands inside jam_terminal/jam_graphics; consumers (CodeView hover/click, prompt navigation, image overlay/placement) stay outside; Skit is opt-in (no Skit attached = passthrough unchanged); renderer keeps skipping SPACER_TAIL per the existing Char contract.
- [x] **Deterministic** — write-time width makes cursor arithmetic identical across renderers and platforms; keep-last winsize coalescing is order-independent; auto-reset bound is a constant; conformance suite pins behavior permanently.

## Open Questions

None. Names await ratification at the Names Gate; any genuinely new name surfacing mid-implementation stops for ARCHITECT per Decision Gate.

## Handoff Notes

- **Prerequisite contract:** RFC-terminal-editor.md consumes this RFC's outputs — `CodeLine::mark` VALUES (V3), `Char::linkId` + `jam::Link` (V5), and a Video write path whose cursor arithmetic is correct for CJK/emoji (V1) before Phase-4 wiring (its S7 step 5). CodeView rewrite (its S7 step 1) is independent and parallelizable.
- **Scope discipline:** the six commissioned items plus the ARCHITECT-directed SKiT absorption (V7). Adjacent gaps found in the same inventory (OSC 4/10/11/12 query responses, DECLRMM, XTGETTCAP, kitty keyboard encoding, mouse 1005/1015/1016) are RECORDED in session research and are NOT in this RFC — ARCHITECT decides their disposition separately. Image RENDERING (overlay, placement, animation scheduling, document anchoring) is likewise out — V7 delivers decode + events + transfer types only.
- **Cross-thread contract:** Grapheme and Link interning follow the Stamp precedent exactly (same `SharedResources` base, reader interns → message resolves). Engineer delegation must read the SharedResources implementation (`SharedResource redesign`, commit `c74601779`) before touching any table.
- **Record correction:** the b3f0fea unit suite was deleted because ALL GREEN — paid in full. Revival prompts must not characterize it as abandoned or broken infrastructure.
- **SKiT source of truth:** the endless WORKING TREE (`~/Documents/Poems/dev/endless/Source/terminal/`) is the final iteration — absorb from there, not from END git history (the `de5bfbf`-deleted copies are older). `endless/PLAN-skit.md` carries the locked thread-transfer decision (callAsync + moved HeapBlocks, endless `6835980` precedent) — it survives absorption verbatim.
- **Validation:** unit suite is agent-writable; esctest/vttest/emoji_test.sh runs are ARCHITECT's (agents never build/run). Conformance findings that contradict spec are DCF violations — resolved in-sprint.
- **Doxygen:** JAM doxygen XML mandatory before any code task; comments move with changed signatures (Char, CodeLine, Video).
