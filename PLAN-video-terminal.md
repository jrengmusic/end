# PLAN: Video Terminal — TETRIS-Compliant Processing Architecture

**RFC:** RFC-video-terminal.md
**Date:** 2026-05-09
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

## Overview

Split Parser monolith into Parser (DFA decoder) + Video (terminal state machine). Processor becomes the orchestrator via `jam::Function::Map` command dispatch. Parser loses `State&` and `Grid&` — holds only `Function::Map&`. State remains the SSOT for ALL terminal state. Processor implements `ValueTree::Listener` on State — reacts to top-down changes (dimensions), pushes to Video. Processor command handlers read Video internal state and write State atomics (value delivery, no raw pointers). Events route through `Processor::events` map. TETRIS achieved: no DSP core has knowledge of the Model.

## Current → Target

| Component | Current | Target |
|-----------|---------|--------|
| Parser | Monolith: DFA + UTF-8 + terminal state + Grid writes + State writes | DFA decoder only: state machine, UTF-8 accumulation, CSI/buffer collection. Holds `Function::Map&` only. |
| Video | Does not exist | Terminal state machine: pen, cursor, modes, Grid writes. Internal plain members for state. Holds `Grid&` + `Function::Map&` (events). No `State&`. |
| Processor | Thin pass-through (`parser->process()`) | Orchestrator: owns commands + events maps, `ValueTree::Listener` on State, sole State writer. Reads Video state, writes State atomics (value delivery). |
| State | Written directly by Parser (~25 atomic setters) | SSOT for ALL terminal state. Written by Processor (bottom-up sync) and Display (top-down resize). Same cross-thread atomics + ValueTree mechanism. |
| Parser→State | Parser holds `State&`, writes directly | Parser has no `State&` — dispatches via `Function::Map&` |
| Parser→Grid | Parser holds `Grid&`, writes directly | Parser has no `Grid&` — dispatches via `Function::Map&` |
| Display→Processor | Display calls `Processor::resized()` directly | Display writes dimensions to State. Processor reacts via `ValueTree::Listener`. No direct call. |
| Parser callbacks | `std::function` members on Parser (brittle, BLESSED violation) | Events route through `Processor::events` (`jam::Function::Map`). Session registers handlers. |

## 1:1 Audio Plugin Analogy

| JFS (audio) | END (terminal) | Role |
|---|---|---|
| APVTS | State | SSOT for all state. Cross-thread atomics + ValueTree. |
| ProcessorChain implements `APVTS::Listener` | Processor implements `ValueTree::Listener` on State | Reacts to top-down changes, pushes to DSP cores |
| `parameterChanged(id, value)` → pushes to DSP core | `valueTreePropertyChanged` → pushes to Video | Top-down value delivery |
| DSP core internal members (coefficients) | Video internal members (cursor, pen, modes) | Calculation inputs — not in Model |
| ProcessorChain reads DSP core state after process | Processor reads Video state after dispatch | Bottom-up: value delivery to Model |
| `ku::Function::Map parameters` | `jam::Function::Map commands` | Command dispatch (Parser → Processor → Video) |
| `ku::Function::Map chainEvents` | `jam::Function::Map events` | Events (Video → Processor → Session) |
| `ku::Function::Map userInterfaceGetters` | (not needed — State + ValueTree::Listener already provides) | — |
| PluginEditor listens to APVTS ValueTree | Display listens to State ValueTree | Bottom-up rendering updates |
| Analyzer 60Hz timer pulls spectrum | Display `onVBlank` reads Grid dirty rows | Visualization pull |
| PluginProcessor | Session | Host interface, lifecycle, owns Processor |

**Key patterns from JFS (no new patterns):**
- `parameterChanged` / `ValueTree::Listener` for top-down value delivery (State → Processor → Video)
- ProcessorChain reads DSP core state, writes APVTS for bottom-up sync (Processor reads Video, writes State atomics)
- `Function::Map` for command dispatch and events — same type, proven in production
- Value delivery through callbacks — no raw atomic pointers stored as members
- DSP cores have zero Model knowledge — Parser holds `Function::Map&` only

## Ownership Chain (Target)

```
Session
  ├── Grid                    (value member — AudioBuffer)
  └── Processor               (unique_ptr — ProcessorChain)
        ├── State             (value member — APVTS, SSOT for ALL terminal state)
        ├── commands          (value member — jam::Function::Map<Command::Type, void>)
        ├── events            (public value — jam::Function::Map<juce::String, void>)
        ├── Video             (value member — DSP core, holds Grid& + events&)
        └── Parser            (unique_ptr — DSP core, holds commands&)
```

## Data Flow

```
TOP-DOWN (resize):
  Display writes State dimensions
    → Processor::valueTreePropertyChanged fires
      → video.resize (cols, rows)

COMMAND DISPATCH (bytes → cells):
  Session → Processor::process (data, len)
    → parser->process (data, len)
      → Parser DFA decodes → commands.get (type, args...)
        → Processor handler:
          1. video.xxx ()           — Video writes Grid, updates internal members
          2. state.setCursorRow ()  — Processor reads Video state, writes State atomics
    → state.flush ()               — timer propagates atomics → ValueTree
      → Display::valueTreePropertyChanged — repaints

EVENTS (Video → Session):
  Video dispatch produces event
    → events.get (key, args...)
      → Session-registered handler (bell, clipboard, notification, image, etc.)
```

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Command::Type Enum

**Scope:** New file `Source/terminal/data/Command.h`

**Action:**
- Define `Command::Type` enum class with Paul Williams external actions: `Print`, `Execute`, `CSIDispatch`, `ESCDispatch`, `OSCEnd`, `DCSHook`, `DCSPut`, `DCSUnhook`, `APCEnd`.
- Minimal file — type definition only.

**Files:**
- `Source/terminal/data/Command.h` — new (re-uses the deleted filename from Grid redesign)

**Validation:** Compiles. Enum values match RFC §Terminal Commands. No logic, pure type.

---

### Step 2: Video Class — State + Method Migration (ATOMIC)

**Scope:** New Video.h/cpp + 9 new Video*.cpp files. Parser.h stripped. 9 Parser*.cpp files deleted.

This is the largest step — primarily a code MOVE, not a rewrite. Method bodies do not change; only the class qualification changes. Must be atomic because dispatch methods cross-reference each other (CSI calls Ops, Edit calls Ops, etc.).

**Action:**

**2a. Create Video class with terminal state (moved from Parser):**
- `pen`, `stamp`, `savedCursor`, `SavedCursor` struct
- `lastGraphicChar`, `useLineDrawing`, `g0LineDrawing`, `g1LineDrawing`
- `graphemeState`, `activeLinkId`
- `responseBuf`, `responseLen`
- `kittyDecoder`
- `tabStops`
- `physCellWidthAtomic`, `physCellHeightAtomic`
- Video constructor: takes `Grid&` + `State&` (State& is temporary — removed in Step 4)
- All `std::function` callbacks move from Parser to Video temporarily (replaced by events map in Step 4)
- Public methods: `resize()`, `reset()`, `calc()`, `flushResponses()`, `setPhysCellDimensions()`
- Public getters for state Processor needs to sync: `getCursorRow()`, `getCursorCol()`, `getActiveScreen()`, mode flags, etc. — same as DSP cores exposing `getCoefficients()` / `getCurrent()` for ProcessorChain

**2b. Move ALL dispatch methods from Parser to Video (ATOMIC):**

| Parser file (deleted) | Video file (new) | Methods moved |
|---|---|---|
| ParserCSI.cpp | VideoCSI.cpp | `csiDispatch()`, all CSI handlers (CUP, CUU/D/F/B, SGR, modes, reports, scroll, erase, insert/delete) |
| ParserESC.cpp | VideoESC.cpp | `escDispatch()`, charset, DEC escape handlers |
| ParserOSC.cpp | VideoOSC.cpp | `oscDispatch()`, all OSC handlers (title, cwd, clipboard, notification, links, shell integration) |
| ParserOSCExt.cpp | VideoOSCExt.cpp | Extended OSC handlers |
| ParserSGR.cpp | VideoSGR.cpp | `handleSGR()`, `applySGR()` |
| ParserEdit.cpp | VideoEdit.cpp | `eraseInDisplay()`, `eraseInLine()`, `shiftLines*()`, `shiftCellsRight()`, `removeCells()`, `eraseCells()`, `repeatCharacter()` |
| ParserOps.cpp | VideoOps.cpp | `scrollUp()`, `scrollDown()`, `print()`, `execute()`, `executeLineFeed()`, `resolveWrapPending()` |
| ParserVT.cpp | VideoVT.cpp | `dcsHook()`, `dcsPut()`, `dcsUnhook()`, `apcEnd()`, `handleSkitFilepath()` |
| ParserDCS.cpp | VideoDCS.cpp | DCS dispatch handlers |

Also move to Video: cursor methods (`cursorMoveUp/Down/Forward/Backward`, `cursorSetPosition`, `cursorSetPositionInOrigin`, `cursorGoToNextLine`, `cursorClamp`, `saveCursor`, `restoreCursor`, `cursorSetScrollRegion`, `cursorResetScrollRegion`, `effectiveScrollBottom`, `effectiveClampBottom`, `activeScrollBottom`), tab stop methods (`initializeTabStops`, `nextTabStop`, `prevTabStop`, `setTabStop`, `clearTabStop`, `clearAllTabStops`), mode/pen reset helpers (`resetModes`, `resetPen`, `resetCursor`).

**2c. Strip Parser to DFA core only:**

Parser retains:
- `State&` and `Grid&` references (temporary — removed in Step 3)
- `Video&` reference (temporary — removed in Step 3)
- DFA state: `dispatchTable`, `currentState`
- CSI accumulator: `csi`, `intermediateBuffer`, `intermediateCount`
- UTF-8 accumulator: `utf8Accumulator`, `utf8AccumulatorLength`
- Payload buffers: `oscBuffer/Size/Capacity`, `dcsBuffer/Size/Capacity`, `dcsFinalByte`, `apcBuffer/Size/Capacity`
- DFA methods: `process()`, `processGroundBlock()` (decode-only, cell write via `video.print()`), `performAction()` (forwards external actions to `video.xxx()`), `processTransition()`, `performEntryAction()`, `handlePrintByte()`, `accumulateUTF8Byte()`, `handleParam()`, `appendToBuffer()`

Parser file map after Step 2:
- `Parser.h` — stripped class definition (DFA + buffers + Video& reference)
- `Parser.cpp` — `process()`, `processGroundBlock()` (decode-only), constructor
- `ParserAction.cpp` — `performAction()` (forwarder to video.xxx()), `processTransition()`, `performEntryAction()`

**2d. Wire ownership:**
- Processor creates Video as value member (before Parser)
- Parser constructor: `Parser (Video& video, State& state, Grid& grid)` — temporary signature
- Processor passes Video& to Parser

**Files:**
- `Source/terminal/logic/Video.h` — new class definition
- `Source/terminal/logic/Video.cpp` — constructor, resize, reset, calc, flushResponses, setPhysCellDimensions
- `Source/terminal/logic/VideoCSI.cpp` — new (from ParserCSI.cpp)
- `Source/terminal/logic/VideoESC.cpp` — new (from ParserESC.cpp)
- `Source/terminal/logic/VideoOSC.cpp` — new (from ParserOSC.cpp)
- `Source/terminal/logic/VideoOSCExt.cpp` — new (from ParserOSCExt.cpp)
- `Source/terminal/logic/VideoSGR.cpp` — new (from ParserSGR.cpp)
- `Source/terminal/logic/VideoEdit.cpp` — new (from ParserEdit.cpp)
- `Source/terminal/logic/VideoOps.cpp` — new (from ParserOps.cpp)
- `Source/terminal/logic/VideoVT.cpp` — new (from ParserVT.cpp)
- `Source/terminal/logic/VideoDCS.cpp` — new (from ParserDCS.cpp)
- `Source/terminal/logic/Parser.h` — stripped to DFA core
- `Source/terminal/logic/Parser.cpp` — stripped (decode-only processGroundBlock)
- `Source/terminal/logic/ParserAction.cpp` — performAction becomes forwarder
- `Source/terminal/logic/Processor.h` — add Video member
- `Source/terminal/logic/Processor.cpp` — construct Video, pass to Parser
- Delete: `ParserCSI.cpp`, `ParserESC.cpp`, `ParserOSC.cpp`, `ParserOSCExt.cpp`, `ParserSGR.cpp`, `ParserEdit.cpp`, `ParserOps.cpp`, `ParserVT.cpp`, `ParserDCS.cpp`

**Validation:** Compiles. Behavior identical. Parser dispatches to Video. Video writes Grid + State (temporary). All existing VT functionality preserved.

---

### Step 3: Processor Orchestration — Command Dispatch + Events

**Scope:** Processor.h/cpp, Parser.h/cpp, ParserAction.cpp, Video.h/cpp

Parser becomes a pure DFA decoder. Processor becomes the orchestrator via `jam::Function::Map` command dispatch. Video callbacks replaced by events map.

**Action:**

**3a. Add command dispatch to Processor:**
- `#include <jam_core/function_map/jam_function_map.h>`
- Add `jam::Function::Map<Command::Type, void> commands` as value member
- Implement `registerCommands()` — called in constructor after Video + Parser creation:

```
commands.add<uint32_t>               (Print,       handler)
commands.add<uint8_t>                (Execute,     handler)
commands.add<const CSI&, uint8_t>    (CSIDispatch, handler)
commands.add<uint8_t, uint8_t>       (ESCDispatch, handler)
commands.add<const char*, int>       (OSCEnd,      handler)
commands.add<const CSI&, uint8_t>    (DCSHook,     handler)
commands.add<uint8_t>                (DCSPut,      handler)
commands.add<>                       (DCSUnhook,   handler)
commands.add<const char*, int>       (APCEnd,      handler)
```

Each handler: calls `video.xxx()` → reads Video state → writes State atomics.

**3b. Add events map to Processor:**
- Add `jam::Function::Map<juce::String, void> events` as public value member
- Same pattern as JFS `ProcessorChain::chainEvents`
- Session registers handlers on `processor->events` directly at construction

**3c. Replace Video callbacks with events map:**
- Video constructor receives `jam::Function::Map<juce::String, void>&` (events)
- Remove `std::function` members from Video: `writeToHost`, `onClipboardChanged`, `onBell`, `onDesktopNotification`, `onImageDecoded`, `onPreviewFile`
- Video calls `events.get(key, ...)` during dispatch instead of callback invocation

**3d. Rewire Parser — Function::Map replaces Video&/State&/Grid&:**
- Parser constructor: `Parser (jam::Function::Map<Command::Type, void>& commands)` — final signature
- Parser loses `Video&`, `State&`, `Grid&` references
- Parser's `performAction()` — external actions call `commands.get<...>(type, args...)` instead of `video.xxx()`
- Parser's `processGroundBlock()` — UTF-8 decode produces codepoint → `commands.get<uint32_t>(Print, codepoint)` instead of `video.print(codepoint)`
- Internal actions (clear, collect, param, ignore) remain on Parser — no change

**Files:**
- `Source/terminal/logic/Processor.h` — add `commands` + `events` members, `registerCommands()`
- `Source/terminal/logic/Processor.cpp` — implement registerCommands(), handler lambdas with state sync
- `Source/terminal/logic/Parser.h` — constructor takes `Function::Map&` only, remove Video&/State&/Grid&
- `Source/terminal/logic/Parser.cpp` — update constructor, processGroundBlock
- `Source/terminal/logic/ParserAction.cpp` — performAction calls commands.get()
- `Source/terminal/logic/Video.h` — replace std::function callbacks with events& reference
- `Source/terminal/logic/Video.cpp` — update constructor
- All Video*.cpp files — replace callback invocations with events.get()
- `Source/terminal/logic/Session.cpp` — register event handlers on processor->events

**Validation:** Compiles. Parser holds only `Function::Map&` — no State, Grid, or Video reference. Events route through Processor. Command dispatch + state sync verified.

---

### Step 4: Video Loses State& — TETRIS Achievement

**Scope:** Video.h/cpp, all Video*.cpp files, Processor.h/cpp

Video becomes a pure DSP core with zero Model knowledge. Processor is the sole State writer via value delivery.

**Action:**

**4a. Video loses State&:**
- Video constructor: `Video (Grid& grid, jam::Function::Map<juce::String, void>& events)` — final signature, no State&
- All `state.set*()` / `state.getRawValue<>()` calls in Video*.cpp become reads/writes to Video's own internal plain members
- Video reads its own members for cursor position, modes, cols, rows during dispatch (feedback loop, reader thread only)
- Cross-thread values (cols, rows pushed from Processor listener on message thread) stored as `std::atomic` in Video

**4b. Processor command handlers sync Video → State:**
- Each handler calls `video.xxx()`, then reads Video's public getters, then calls `state.set*()` with values
- Value delivery — no raw pointers, no State& on Video
- Unconditional sync per command — negligible at terminal frame rates

**4c. Processor implements ValueTree::Listener on State:**
- Listens for top-down property changes (dimensions)
- `valueTreePropertyChanged` → pushes to Video (e.g. `video.resize(cols, rows)`)
- Same pattern as `ProcessorChain::parameterChanged` → pushes to DSP core
- Display writes State dimensions on resize — Processor reacts, no direct `Processor::resized()` call from Display

**4d. Display writes State, not Processor:**
- Display resize: writes new dimensions to State directly
- Remove `Processor::resized()` as public API called by Display
- Display reads State via ValueTree::Listener for rendering (cursor, modes, title — flushed from Processor's bottom-up sync)

**Files:**
- `Source/terminal/logic/Video.h` — remove State&, internal plain members for all terminal state, atomic for cross-thread values (cols, rows)
- `Source/terminal/logic/Video.cpp` — update constructor
- All Video*.cpp files — replace `state.set*()` with internal member writes, replace `state.getRawValue<>()` with internal member reads
- `Source/terminal/logic/Processor.h` — implement `ValueTree::Listener`, remove public `resized()` (or make private)
- `Source/terminal/logic/Processor.cpp` — `valueTreePropertyChanged` handler, command handler state sync
- `Source/component/TerminalDisplay.cpp` — write State dimensions on resize instead of calling `Processor::resized()`

**Validation:** Compiles. Video holds no `State&` — zero Model knowledge. Processor is sole State writer. Display writes State for resize, reads State for rendering. TETRIS contract satisfied.

---

### Step 5: Cleanup + Doc Sync

**Scope:** Multiple files

**Action:**
- Update Processor.h doxygen: orchestrator role, command dispatch, events map, ValueTree::Listener
- Update Parser.h doxygen: DFA decoder only, no State/Grid/Video references
- Write Video.h doxygen: DSP core, terminal state machine, internal members, public getters
- Update State.h doxygen: SSOT for all terminal state, written by Processor (bottom-up) and Display (top-down)
- Fix stale doc comments: Session.h
- Verify all `#include` directives: no stale includes, no missing includes
- Delete superseded RFCs if stale: RFC-atlas-owns-packer.md, RFC-viewport-redesign.md, RFC-terminal-rendering-pipeline.md

**Files:**
- `Source/terminal/logic/Processor.h` — doxygen update
- `Source/terminal/logic/Parser.h` — doxygen update
- `Source/terminal/logic/Video.h` — full doxygen
- `Source/terminal/data/State.h` — doxygen update
- `Source/terminal/logic/Session.h` — fix stale doc comment
- Delete stale RFCs at project root

**Validation:** No stale references. Doxygen complete. BLESSED alignment verified by @Auditor.

---

## BLESSED Alignment

- **B (Bound):** Processor owns State, Video, Parser, commands, events. Session owns Grid. Video borrows Grid& + events&. Parser borrows commands&. Clear ownership chain, all RAII. No raw pointers.
- **L (Lean):** Parser shrinks to ~3 files (DFA core). Video splits into ~10 files (dispatch by VT action category). No god objects.
- **E (Explicit):** Command dispatch via registered Function::Map — visible, traceable. Events via registered Function::Map — same pattern. Processor listens to State via ValueTree::Listener — standard JUCE pattern. No magic. No direct Display→Processor calls for resize.
- **S (SSOT):** State is sole SSOT for ALL terminal state. Parser is sole protocol decoder. Video is sole terminal state machine. Processor is sole State writer (bottom-up) and sole orchestrator. No duplication.
- **S (Stateless):** Parser has only transient decode state (DFA, accumulators). Video has only calculation inputs (pen, cursor, modes — deterministic reflection of dispatch, like DSP core coefficients). Grid is dumb buffer.
- **E (Encapsulation):** Parser doesn't know Video, State, or Grid exist — holds Function::Map& only. Video doesn't know Parser or State exist — holds Grid& + events&. Display writes/reads State, never reaches Processor or Video directly. Processor is the sole mediator. Unidirectional layer flow.
- **D (Deterministic):** Same bytes → same commands → same Video state → same Grid content → same State. Always.

## Risks

1. **Step 2 size** — Moving 9 files of dispatch logic is large. Must be atomic due to cross-references between dispatch methods. Risk: merge conflicts if other work touches Parser*.cpp in parallel.
2. **Step 4 State extraction** — Relocating ~25 `state.set*()` and `state.getRawValue<>()` calls across all Video*.cpp files to internal members. Missing one = compile error (best case) or UI desync (worst case).
3. **Function::Map type safety** — `jam::Function::Map` uses type-erased dispatch. Mismatched template args at `add<>()` vs `get<>()` call sites produce runtime assertion, not compile error.
4. **Cross-thread atomics in Video** — Dimensions (cols, rows) pushed by Processor listener (message thread), read by Video dispatch (reader thread). Must be `std::atomic` in Video.
5. **Resize flow change** — Display currently calls `Processor::resized()` directly. Step 4 changes this to Display → State → Processor listener → Video. All resize callers must be updated.
