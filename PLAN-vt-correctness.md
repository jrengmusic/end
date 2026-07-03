# PLAN — VT Correctness + Conformance Hardening (jam_terminal)

**Source:** RFC-vt-correctness.md (all V1–V7 in scope, zero descoped)
**Date:** 2026-07-03
**Prerequisite of:** RFC-terminal-editor.md (its S7 step 5 Phase-4 wiring consumes this PLAN's outputs)
**Ground:** Pathfinder survey + COUNSELOR file verification, this session

---

## Baseline Ruling (ARCHITECT, this session)

RFC V1 is executed **verbatim** — Engineer re-walks the entire print path against S1, not gap-patching. Code reality recorded for the walk: `Video::print` (`jam_terminal/video/jam_CursorState.cpp:400–533`) already carries UAX #29 step + grapheme fold (`:403–433`), `fromCodepoint` (`:493`), wide advance + SPACER_TAIL (`:518–523`), wide-at-margin wrap-first (`:448–472`). Verified residue: **no mode 2027 anywhere in jam_terminal**; **`graphemeState` never reset on any discontinuity** (`:405` is the sole assignment); no 2026 state; no `CodeLine::mark`; no Char link carrier; zero tests; no `image/`.

LinkManager (END consumer) is **not in this PLAN** — ARCHITECT flagged it for redesign; V5 delivers carrier + table only.

---

## Names Gate (ratify at intake — RFC table verbatim)

| Name | Kind |
|---|---|
| `CodeLine::Mark` — `none / prompt / input / output / promptContinuation` | mark value enum (V3) |
| `jam::Link`, `Link::Entry { uri, id }` | hyperlink interning table (V5) |
| `Char::linkId()`, `Char::withLinkId()` | carrier accessors, padding bits 41–56 (V5) |
| `Video::graphemeState` | per-screen UAX #29 running state (V1; existing name kept, made per-screen — ARCHITECT-edited from RFC's `segState`) |
| `Video::printCodepoint`, `extendGraphemeCluster`, `stampLink` | write-path verbs (V1/V5, shapes per RFC S1; `extendGraphemeCluster` ARCHITECT-edited from RFC's `foldIntoPreviousCell`) |
| `syncResetMs = 1000` | V2 constant |
| `Video::clearSyncOutputIfExpired()` | V2 poll — clears sync if deadline passed; consumer calls once per reader-loop wake (ratified; replaces loop-top-inline shape) |
| `Video::syncOutputDeadlineMs` | V2 expiry instant (double ms) — DECSET 2026 stores now + syncResetMs; poll compares now >= deadline (ratified — deadline stored, not set-time) |
| `Video::disableSyncOutput()` | V2 shared disable path — DECRST 2026 + expiry poll, one body (ratified) |
| `Video::activeMark` | V3 running semantic mark, per-screen — stamped onto every committed row; `active` prefix per activeScreen/activeLinkId convention (ratified — full propagation, not boundary-only) |
| `GRAPHEME_CLUSTERING = 2027` | VtVocabulary mode constant (V1) |
| `Video::graphemeClustering` | mode-2027 storage member, bool, default true (V1 — ratified post-Step-1) |
| `jam_terminal/image/` — `Skit`, `SixelDecoder`, `KittyDecoder`, `ITerm2Decoder`, `ImageDecode` | absorbed SKiT family (V7) |
| `DecodedImage`, `PendingImage`, `ImageSequence` | transfer types, absorbed as-is (V7) |

---

## Step 1 — V1: Write-path re-walk (jam_graphics + jam_terminal/video)

Re-walk `Video::print` against RFC S1 contract, element by element:

1. **Single-codepoint path** — `Char::fromCodepoint (cp, styleId, useLineDrawing)` is the only cell producer for printables. Verify no hardcoded-NARROW `Char::make` printable site survives anywhere in video TUs.
2. **Wide advance** — width 2 → head `WIDE`, next column `SPACER_TAIL`, cursor +2. **Wide-at-last-column:** one column left → DECAWM-wrap FIRST (row marked `isContinued`), then write the pair at new row start. Never split a pair across rows. (Current wrap block `:448–472` verified against this contract, including scroll-region interaction.)
3. **Combining path** — `addToCurrentCell` → fold into previous cell → `CONTENT_GRAPHEME` + `Grapheme::addIfNotAlreadyThere` (Entry ≤ 8 codepoints, `jam_Grapheme.h:38–65`). Cursor does not advance.
4. **graphemeState reset rules (residue, must land)** — `graphemeState = Char::graphemeSegmentationInit()` on EVERY discontinuity: cursor motion (CUP/CUU/CUD/CUF/CUB/HVP, CR, LF, BS, HT), line change, scroll, screen switch (1049/47), RIS, ED/EL touching the cursor cell, DECSC/DECRC restore. UAX #29 state is a run property. RFC names the state **per-screen** — one `graphemeState` per screen, switched with `activeScreen` (today: single shared member).
5. **Mode 2027 (residue, must land)** — `GRAPHEME_CLUSTERING = 2027` in VtVocabulary; DECSET/DECRST 2027 toggles cluster-aware advance (Video flag, default on); DECRQM reports actual state.
6. **Pen/style stamping at write** — one `sid` resolution per cell write, `currentStyleId()` lazy-cache path (`jam_CursorState.cpp:488`) preserved.

Doxygen comments move with any changed signature (`Char`, `Video`). JAM doxygen XML read before delegation.

## Step 2 — V2: DEC 2026 auto-reset — loop-top guard, no timer

- Video records monotonic ms when 2026 sets (`syncOutput.setAtMs` shape per RFC S2); constant `syncResetMs = 1000`.
- Check at reader-loop top: elapsed ≥ 1000ms → force-clear sync with **identical downstream effects to a genuine DECRST 2026** (dirty + flush).
- Loop facts: `TTY::run` owns the reader loop (`jam_TTY.cpp:53–88`), `waitForData (100)` bounds the wake (`jam_TTY.h:83` — 100ms). TTY does not know Video. **Seam (ARCHITECT-ratified):** Video owns the state + `clearSyncOutputIfExpired()` poll; the consumer calls it once per reader-loop wake (one line in Phase 4 wiring — data arrival must NOT gate the check, hung-app case).
- Fact for the walk: current 2026 DECRQSS stub replies "not recognised" (`jam_VideoCSI.cpp:502–510`, response `2026;0$p`) — mode state + honest DECRQSS report land with this step.

## Step 3 — V3: Semantic mark vocabulary + transport

- `CodeLine::Mark : uint8_t { none, prompt, input, output, promptContinuation }` + `Mark mark { Mark::none }` field (`jam_CodeLine.h:35–55` — no mark today).
- CellFifo entry-header flags byte: bit 0 `isContinued`, bit 1 `isJustified` (existing, `jam_CellFifo.h:84–85`), **bits 2–4 mark** (3 bits, 5 values, 3 spare). Header = `jam::Union<int32_t, uint8_t>` (`:304`) — packing verified against seqlock layout (`:44–55`).
- Video's OSC 133 handling (`jam_VideoOSCExt.cpp:69–99`) stamps the current mark onto departed/pushed rows; 133;D exit-code closes the block as the next line's mark transition (ghostty Row semantics).
- Drain stamps header bits onto `CodeLine::mark`. Consumers are RFC-terminal-editor scope.

## Step 4 — V5: Hyperlink carrier + table

- `Char::linkId()` / `Char::withLinkId (Char, uint16_t)` — padding bits 41–56 (layout verified: 23 free bits at 41–63, `jam_Char.h:14–27`); 7 bits remain reserved. `linkId == 0` = no link. `sizeof(Char) == 8` static_assert unchanged (`:291–292`).
- `jam::Link : SharedResources<Link>` — third instance of the interning shape (Stamp `jam_Stamp.h:66–165`, Grapheme). `Entry { juce::String uri; juce::String id; }` — explicit `id=` dedupes by id, implicit by URI. Reader-side interning at OSC 8 parse (`jam_VideoOSCExt.cpp:27–67` already captures both fields); message resolves — Stamp publication contract exactly. Engineer reads the SharedResources implementation (commit `c74601779`) before touching any table.
- Video pen: `activeLinkId` exists today but round-trips through the `ID::registerLink` event and stamps only `UNDERLINE_SINGLE` styling (`jam_CursorState.cpp:479–485`). Delete-first: event round-trip and underline-stamp hack removed; OSC 8 URI interns into `jam::Link`, sets pen linkId; `;;` resets to 0; `stampLink` applies it to every written cell (both head and SPACER_TAIL). Hover/click/underline are consumer scope, later.

## Step 5 — V7: SKiT absorption (endless working tree → jam_terminal/image/)

Move map (RFC S7, source files verified present incl. `endless/PLAN-skit.md`):

```
endless/Source/terminal/                     →  jam_terminal/image/
  Skit.{h,cpp}                                  jam_Skit.{h,cpp}
  SixelDecoder.{h,cpp} + SixelDecoderParse      jam_SixelDecoder.*
  KittyDecoder.{h,cpp} + KittyDecoderDecode     jam_KittyDecoder.*
  ITerm2Decoder.{h,cpp}                         jam_ITerm2Decoder.*
  ImageDecode.{h,cpp,Gif.h,Mac.mm,Win.cpp}      jam_ImageDecode.*  (platform TUs — TTY precedent)
```

- Relocation + module namespace + doxygen — **not redesign**. Transfer types (`DecodedImage`, `PendingImage`, `ImageSequence`) move with headers. Event surface already `jam::Function::Map`.
- Video seams route to attached Skit, passthrough otherwise: DCS (`jam_VideoDCS.cpp:87–91` `dcsPayloadComplete`), APC (`:109–113` `apcPayloadComplete`), OSC 1337 (`jam_VideoOSCExt.cpp:101–110` `osc1337Raw`). No Skit attached = behavior unchanged; opt-in by construction.
- Cursor contract: displayed decode → Video advances by `getLastImageRows()` (pixel height ÷ cellSize); Kitty `getLastResponse()` written back to TTY on the reader thread.
- Reader→message transfer: locked endless pattern — `MessageManager::callAsync` + MOVED HeapBlocks (`endless/PLAN-skit.md` Decision 1, endless `6835980`). No lock, no FIFO.
- Capability advertisement: DA1 gains Sixel (`?62;4c` shape); Kitty `a=q` → `ok`.
- **Boundary:** overlay/placement/rendering/animation scheduling/document anchoring = consumer (END, post terminal-editor). NOT moved.

## Step 6 — V6: Conformance suite — revive the ALL-GREEN harness, permanent

- `b3f0fea` fixture pattern (raw bytes → Parser → Video → assert grid/cursor/mode), Catch2 single-header, standalone CMake test target — **home is JAM** (framework module outlives applications). Deleted-because-ALL-GREEN record: revival of a proven pattern, never "repairing broken infra".
- Coverage targets: V1 write path (width/wide/combining/cluster — every `emoji_test.sh` section as assertions), wide-at-margin (RFC S5 case verbatim), V2 auto-reset, V3 mark transport, V5 linkId stamping, SGR/CSI/OSC dispatch, DECAWM, alt-screen, scroll regions, CellFifo join/flags contract, 2027 DECRQM.
- External passes (esctest, vttest, `emoji_test.sh`, `render-test.sh`, `braille_test.txt` from endless) are ARCHITECT-run gates — agents never build/run.
- Suite findings contradicting spec are DCF violations — resolved in-sprint.

## Step 7 — V4: Resize coalescing — record only

Documented property, no mechanism: keep-last atomic `winsize`, reader applies latest at loop top — coalescing by construction. One doxygen paragraph at the winsize seam (Video/TTY doc); explicitly NOT ghostty's 25ms timer. Nothing else.

---

## Step 8 — Mode-state redesign: DecMode Bimap + jam::terminal::Model (ARCHITECT-directed, supersedes the modePtr/ModeEntry shapes)

Ratified names: `jam::terminal::map::DecMode` (home: `jam_terminal/bimap/`), `jam::terminal::Model`. `Video::modeFlag`/`setModeFlag` (Engineer-introduced replacement for getMode/setMode surface, flagged to ARCHITECT, accepted at proceed).

**Step 8b (ARCHITECT-directed):** thorough jam_terminal audit for other static lookup-table patterns that should adopt the DecMode Bimap shape — deterministic + consistency. Inventory first, ARCHITECT disposes, refactors follow. *Inventory complete (18 patterns, Pathfinder). Disposition evolved into the Step 8c canon.*

**Step 8c — CANON SSOT LUT pattern (ARCHITECT-ratified):**
- `jam::LookupTable` — `jam_core/utilities/` — generic `constexpr` direct-indexed LUT: int-keyed, int/enum/bits-valued, `Capacity` slots + fallback value, `static constexpr` instances in `.rodata`. Identity is the hash (small-int protocol keys) — no custom hash, nothing to regenerate. Hot-path safe by construction; ONE rule for hot and cold tables.
- Composite keys packed homogeneous via `jam::Union` (CursorState/Winsize/CellFifo-header precedent) — e.g. CSI (intermediate, final) pairs.
- Objects (Identifier/Colour/String) never live in a LUT: LUTs carry ints end-to-end; `juce::Identifier` materializes only at the DecMode-Bimap/Model boundary (verified: zero constexpr in juce_Identifier.h); `juce::Colour` stored as raw uint32 ARGB, wrapped at read site (verified: ctor not constexpr, juce_Colour.h:66).
- Whole-machinery performance rule (ARCHITECT): microscopic "slower" is void where wall-clock unmeasurable; measurable tiers are per-byte/per-cell (~10⁸/s flood) — LookupTable is safe even there.
- Conversions this step: SGR value→bits switches (`jam_VideoSGR.cpp:183-263` — setAttributeFlags/unsetAttributeFlags/underlineStyleToBits), OSC 133 subcmd→Mark mapping half (`jam_VideoOSCExt.cpp` — D stays behavior), keyboard VK table if value→value (`jam_Keyboard.cpp:113-133`), UTF-8 length array (`jam_ParserAction.cpp:179-195`), Sixel default palette as uint32 LUT (`jam_SixelDecoderParse.cpp:21-40`).
**Step 8d (ARCHITECT-ratified):**
- Normalize ALL FOUR behavior-dispatch families to the DispatchTable form — value→action enum via `jam::LookupTable`, one executor switch per family: CSI (~30 cases, composite (intermediate, final) key packed via `jam::Union` — absorbs the DECRQM/DECSTR/DECRQSS/DECSCUSR disambiguation branches as rows), ESC (8), OSC (12), C0 (7). Parser DispatchTable is the proven shape.
- `modeFlag()` hot-tier remedy NOW: resolve `Parameter*` per mode once at Video construction (calculation-input/APVTS pattern), hot read = atomic load; SSOT stays the Model.
- Sequenced AFTER Step 8c lands (same-module file overlap — no parallel writers).
- **Post-8d rulings (ARCHITECT):** executor switches are EXEMPT from the 30-line limit — one-switch-per-family is the canon's sanctioned form, arms stay straight-line (covers all five families). `imageDecoded`/`previewFile` jam::ID additions ratified. `jam::Bimap` value type templated (`juce::String` default — 22 existing consumers zero-diff); DecMode = ordinary Bimap subclass #23 (`getDefault()` = empty Identifier, the miss sentinel) — NO tiers, NO new bimap patterns (ARCHITECT ruling: new pattern where one exists is forbidden; COUNSELOR's tier/Vocabulary proposals rejected and withdrawn). V2 expiry test: NO virtual, NO production seam — `syncOutputDeadlineMs` protected, test subclass `SyncOutputDeadlineVideo` injects an elapsed deadline; sleep + [slow] removed.
- **SGR 21 (ARCHITECT-ratified, queued after 8d):** dead mislabeled row (BOLD|DIM clear, unreachable behind the `code >= 22` guard) becomes live per ECMA-48 + xterm/kitty/ghostty consensus — 21 → underline style double (StampEntry 3-bit field); LUT row corrected, guard adjusted, regression test flipped from pinning-dead to asserting-live.

1. **`jam_terminal/bimap/` — `DecMode` Bimap**: decMode number ↔ `jam::ID`, the single vocabulary SSOT (`jam::map::ImageResample` / Sprint-34 aggregator precedent). DECSET dispatch reads one direction, DECRQM/DECRQSS the other, schema building and iteration walk the same object. Pure values — no pointers, no functions.
2. **`jam::terminal::Model`** (`jam::Model` base — dependency edge already present, `jam_terminal.h:15,30`): MODES registered as parameters keyed by those IDs; **defaults live at parameter registration** (autoWrap=true, graphemeClustering=true, rest false); RIS resets through the Model, no hand-written literals. END's terminal::Model consumes and extends the framework base.
3. **Video goes stateless on modes**: the 14 named bool members, `modePtr`, `ModeEntry`, file-static `modeTable`, and `resetModes()` literals are deleted. Video reads Model atomics as calculation inputs (reader writes on DECSET, reader reads at cell-write — same thread, uncontended; APVTS processor pattern). `privateModeTable` (flat array) dissolves into DecMode lookup → Model store; side-effect modes (2026/1049/DECOM/DECTCEM) stay dedicated branches.
4. **P12 amendment (recorded, not silent)**: RFC-terminal-editor's MODES group migrates from END-owned schema into the framework base — END extends.
5. **Carried fixes (verified this session)**: DECRQM/DECRQSS dispatch reachability (`'$'` is intermediate, `case csiFinal::STATUS` at `jam_VideoCSI.cpp:235` is dead — route on final `'p'`/`'q'` + `inter[0]=='$'`, verify DECRQSS's true entry vs DCS), linkId 0-sentinel collision (`activeLinkId = index + 1`, contract on Char/Link docs), unguarded `events.get (ID::activeScreen)` (`jam_VideoEdit.cpp:488`) gets the sibling `contains` guard.

---

## Sequencing (RFC S6)

- Steps 1, 2, 5 touch Video — land together, touch Video once. Step 7 rides them.
- Steps 3, 4 land before terminal-editor drain work (CodeLine/Char shapes are its inputs).
- Step 6 written against steps 1–4 outputs, same sprint.
- Zero overlap with jam_vulkan. RFC-terminal-editor S7 step 1 (CodeView rewrite) independent, parallelizable; its Phase-4 wiring is not.

## Validation

- Per-step: COUNSELOR re-reads changed files against this PLAN; Auditor pass per step group (write path, transport shapes, SKiT move, suite).
- Compile/run gates are ARCHITECT's (`ninja`; esctest/vttest/corpus scripts).
- Doxygen-first on every delegation (JAM `docs/xml/index.xml`); comments move with signatures.
