# RFC — Video Terminal: TETRIS-Compliant Processing Architecture
Date: 2026-05-09
Status: Ready for COUNSELOR handoff

## Problem Statement

Parser is a monolith — it owns both protocol decoding (DFA, UTF-8 accumulation, CSI parameter parsing) AND terminal state mutation (pen, cursor, modes, tabstops, scroll, erase, print). Parser holds `State&` and writes State directly. This violates TETRIS: no DSP core should have knowledge of the Model. Processor is a thin pass-through (`parser->process()` + `state.consumePasteEcho()`), not an orchestrator.

ARCHITECT wants the terminal processing architecture to be 1:1 analogous to jreng-filter-strip (JFS) per the TETRIS contract — proven in production for commercial audio plugins with harder real-time constraints than any terminal emulator.

## Current State (Verified from Codebase)

```
Session
  ├── Grid                (value member — AudioBuffer, already implemented)
  └── Processor           (unique_ptr)
        ├── State         (value member — APVTS, already in Processor)
        └── Parser        (unique_ptr, holds State& and Grid&)
              ├── DFA state, DispatchTable, UTF-8 accumulator, CSI accumulator
              ├── pen, stamp, savedCursor, tabStops, charset state
              ├── graphemeState, activeLinkId, lastGraphicChar
              ├── responseBuf, kittyDecoder
              └── ALL VT dispatch (11 .cpp files)
```

Processor::process() is a one-liner forwarding to Parser. Parser does everything. Parser writes State directly — TETRIS violation.

Doc comments in Processor.h:55-56 and Session.h:8 incorrectly claim Session owns State. State is Processor's value member at Processor.h:303.

## Research Summary

### TETRIS Contract (jreng-filter-strip)

TETRIS is the DSP contract for real-time audio: Thread separation, Encapsulation, Trivially copyable, Reference processing, Internal double, Scalar processing. Proven in production across commercial audio plugins.

Key architectural pattern from JFS:

- **DSP cores** (StateVariable, Atan, RBJ, Orfanidis) — dumb processors. Zero knowledge of Model (APVTS). Process input, write output. That's it.
- **ProcessorChain** — the orchestrator. Sole writer of Model state. Receives `parameterChanged()` from APVTS, dispatches to DSP cores via `ku::Function::Map<String, void> parameters` with registered setter lambdas.
- **Composition** — DSP cores compose freely by value ownership. SRPP owns Atan + RBJ, calls them in `process()`. Butterworth owns StateVariable[17], iterates in `process()`. Inner cores have zero knowledge of outer wrappers. Composition is not coupling — it is chaining.
- **SmoothStateTransition** — wraps any trivially copyable DSP core, crossfades `previous`/`current` during parameter changes.

ProcessorChain is the `parameterChanged` dispatcher:
```
APVTS fires parameterChanged(id, value)
  → ProcessorChain::parameterChanged()
    → parameters.get(id, value)
      → setter lambda → DSP core + state
```

### DSP Core Composition Patterns (JFS)

| Pattern | Example | Relationship |
|---|---|---|
| Sibling | preamp, highPass, lowPass in ProcessorChain | Orchestrator calls each sequentially |
| Owned composition | SRPP owns Atan + RBJ, calls both in process() | Outer calls inner inline |
| Cascaded array | Butterworth owns StateVariable[17], iterates | Outer iterates inner |
| Crossfade wrapper | SmoothStateTransition wraps ButterworthMSED | Snapshot + dual process |

Full JFS call chain:
```
ProcessorChain::process(block)
  └─ dualChain(sampleL, sampleR)
       ├─ preamp.process(L, R)           → ChannelProcessor<SRPP>
       │    └─ channels[ch].process()    → SRPP::process
       │         ├─ shaper.process()     → Atan → internal RBJ
       │         └─ highPass.process()   → RBJ
       ├─ highPeak.process(L, R)         → ChannelProcessorMSED<Orfanidis>
       ├─ lowPeak.process(L, R)          → ChannelProcessorMSED<Orfanidis>
       ├─ highPass.process(L, R)         → SmoothStateTransition<ButterworthMSED>
       │    └─ current/previous.process  → Butterworth<StateVariable>
       │         └─ filters[i].process   → StateVariable::processSample
       └─ lowPass.process(L, R)          → SmoothStateTransition<ButterworthMSED>
```

### Paul Williams VT100 DFA (docs/A parser for DEC's ANSI-compatible video terminals)

The parser produces **actions**: print, execute, csi_dispatch, esc_dispatch, osc_start/put/end, hook/put/unhook, clear, collect, param, ignore. Some are internal to the parser (clear, collect, param, ignore). The external ones (print, execute, csi_dispatch, esc_dispatch, osc/dcs lifecycle) are dispatched to the terminal.

The parser is protocol decode. What the terminal DOES with dispatched actions is a separate concern.

### jam::Function::Map

Type-erased string-keyed dispatch table. `add<Args...>(key, callable)` registers any callable. `get<Args...>(key, args...)` dispatches. Accepts free functions, function pointers, lambdas — anything `std::function` wraps. Same mechanism as `ku::Function::Map` in JFS (fork relationship). Proven in production across kuassa plugin framework and jam UI framework.

## Principles and Rationale

### 1:1 Audio Plugin Analogy (Updated)

| Audio Plugin (JFS) | Terminal (END) | Role |
|---|---|---|
| PluginProcessor | Session | True C — host interface, lifecycle |
| ProcessorChain | Processor | Meta C — orchestrator, sole State writer, command dispatch |
| DSP core (StateVariable, Atan) | Video | Terminal state machine — pen/cursor/modes, writes Grid |
| DSP core (input decoder) | Parser | Protocol decoder — DFA, UTF-8, CSI params |
| APVTS (Model) | State | Parameters via atomics → ValueTree |
| AudioBuffer | Grid | Live buffer — read/write in-place |
| `ku::Function::Map parameters` | `jam::Function::Map commands` | Registered dispatch table |
| `parameterChanged(id, value)` | `commands.get(type, args...)` | Single dispatch entry point |
| `registerParameterSetters()` | `registerCommands()` | Handler registration at construction |
| PluginEditor | Display | Orchestrator — reads buffer + state, tells children |
| Spectrum visualizer | Screen | Renders from processed data (jam::Cells) |

### Parser + Video = DSP Core Composition

Parser calling Video is composition, not coupling. Same as SRPP calling Atan + RBJ in its `process()`. Inner core (Video) has zero knowledge of outer (Parser). Parser holds `Video&` implicitly through the command dispatch — Video methods are registered as command handlers, Parser calls the map.

### Processor is parameterChanged

Processor owns the `commands` map. Registers handlers at construction. Each handler dispatches to Video AND writes State. Parser calls `commands.get()` during decode — same as APVTS firing `parameterChanged` on ProcessorChain.

Parser never sees Video or State. Parser sees only `Function::Map<Command::Type, void>&` — the dispatch table passed at construction.

### Terminal Commands

Host commands the terminal. Parser decodes bytes into commands. Processor dispatches commands to Video + State. `Command::Type` enumerates the Paul Williams external actions:

```cpp
struct Command
{
    enum Type
    {
        Print,        // printable codepoint
        Execute,      // C0 control (LF, CR, BS, TAB, BEL, etc.)
        CSIDispatch,  // complete CSI sequence
        ESCDispatch,  // complete ESC sequence
        OSCEnd,       // complete OSC string
        DCSHook,      // DCS passthrough begin
        DCSPut,       // DCS passthrough data
        DCSUnhook,    // DCS passthrough end
    };
};
```

### MVC is Fractal (Updated)

```
READER THREAD
  C: Processor (ProcessorChain — orchestrator, sole State writer)
    Owns command dispatch map
    Parser decodes → commands.get() → handler
    Handler dispatches to Video (writes Grid) + writes State atomics

MESSAGE THREAD
  M: State (ValueTree, flushed from atomics)
  V+C: Display (orchestrator)
    Reads Grid, listens to State
    Tells Screen what to store, render, reflow
  V: Screen (renderer, visualizer)
    Stores jam::Cells, renders via Glyph pipeline
    Dumb — does what Display says
```

### BLESSED Mapping

- **B (Bound):** Processor owns State, Parser, Video, command map. Session owns Grid. Parser borrows `Function::Map&`. Video borrows `Grid&`. Clear ownership chain.
- **L (Lean):** Parser shrinks to ~3 files (DFA + accumulators). Video dispatch splits into VideoCSI.cpp, VideoESC.cpp, etc. (same pattern as ProcessorChainRegistration.cpp). No god objects.
- **E (Explicit):** Command dispatch via registered Function::Map — visible, traceable. Parser constructor takes `Function::Map&` explicitly. No magic.
- **S (SSOT):** Parser is sole protocol decoder. Video is sole terminal state machine. State is sole model. Processor is sole State writer. No duplication.
- **S (Stateless):** Parser has only transient decode state (DFA, accumulators). Video has only calculation inputs (pen, cursor, modes — deterministic reflection of Processor's dispatch). Grid is dumb buffer.
- **E (Encapsulation):** Parser doesn't know Video, State, or Grid exist. Video doesn't know Parser or State exist. Processor orchestrates — lower layers never reach up.
- **D (Deterministic):** Same bytes → same commands → same Video state → same Grid content → same State. Always.

## Scaffold

### Ownership Chain (Target)

```
Session
  ├── Grid                (value member — AudioBuffer)
  └── Processor           (unique_ptr — ProcessorChain)
        ├── State         (value member — APVTS)
        ├── commands      (value member — jam::Function::Map<Command::Type, void>)
        ├── Video         (value member — DSP core, holds Grid&)
        │     pen, cursor, modes, tabstops, savedCursor, stamp
        │     graphemeState, activeLinkId, lastGraphicChar
        │     responseBuf, kittyDecoder
        │     writes Grid via Grid&
        └── Parser        (unique_ptr — DSP core, holds Function::Map&)
              DFA state, DispatchTable, UTF-8 accumulator
              CSI accumulator, intermediate buffer
              OSC/DCS/APC buffers
              calls commands.get() during decode
```

### Command Dispatch

```cpp
// Processor — command registration (like registerParameterSetters)
void Processor::registerCommands()
{
    commands.add<uint32_t>            (Command::Print,       handlePrint);
    commands.add<uint8_t>             (Command::Execute,     handleExecute);
    commands.add<const CSI&, uint8_t> (Command::CSIDispatch, handleCSIDispatch);
    commands.add<uint8_t, uint8_t>    (Command::ESCDispatch, handleESCDispatch);
    commands.add<const char*, int>    (Command::OSCEnd,      handleOSCEnd);
    commands.add<const CSI&, uint8_t> (Command::DCSHook,     handleDCSHook);
    commands.add<uint8_t>             (Command::DCSPut,      handleDCSPut);
    commands.add<>                    (Command::DCSUnhook,   handleDCSUnhook);
}

// Parser — calls during decode (like APVTS firing parameterChanged)
void Parser::csiDispatch()
{
    commands.get<const CSI&, uint8_t> (Command::CSIDispatch, csi, finalByte);
}

// Processor — handler dispatches to Video + writes State
void Processor::handleCSIDispatch (const CSI& csi, uint8_t finalByte)
{
    video.csiDispatch (csi, finalByte);  // → Video writes Grid
    // write State atomics as needed per CSI function
}
```

### Parser (After Split)

```cpp
class Parser
{
public:
    explicit Parser (jam::Function::Map<Command::Type, void>& commands) noexcept;

    void process (const uint8_t* data, size_t length) noexcept;

private:
    // Protocol decode state only — no terminal state
    jam::Function::Map<Command::Type, void>& commands;

    ParserState currentState { ParserState::ground };
    DispatchTable dispatchTable;

    char utf8Accumulator[5] {};
    int utf8AccumulatorLength { 0 };

    CSI csi;
    uint8_t intermediateBuffer[4] {};
    int intermediateCount { 0 };

    // Payload buffers
    char* oscBuffer { nullptr };
    int oscBufferSize { 0 };
    int oscBufferCapacity { 0 };

    char* dcsBuffer { nullptr };
    int dcsBufferSize { 0 };
    uint8_t dcsFinalByte { 0 };

    char* apcBuffer { nullptr };
    int apcBufferSize { 0 };
};
```

### Video (New Class)

```cpp
class Video
{
public:
    explicit Video (Grid& grid) noexcept;

    // Command handlers — called by Processor
    void print (uint32_t codepoint) noexcept;
    void execute (uint8_t byte) noexcept;
    void csiDispatch (const CSI& csi, uint8_t finalByte) noexcept;
    void escDispatch (uint8_t intermediate, uint8_t finalByte) noexcept;
    void oscEnd (const char* buffer, int length) noexcept;
    void dcsHook (const CSI& csi, uint8_t finalByte) noexcept;
    void dcsPut (uint8_t byte) noexcept;
    void dcsUnhook() noexcept;

    void resize (int cols, int rows) noexcept;
    void reset() noexcept;

private:
    Grid& grid;

    // Terminal state — moved from Parser
    Pen pen;
    Pen stamp;
    std::array<SavedCursor, 2> savedCursor;
    std::vector<char> tabStops;

    bool useLineDrawing { false };
    bool g0LineDrawing { false };
    bool g1LineDrawing { false };

    int graphemeState { 0 };
    int activeLinkId { 0 };
    uint32_t lastGraphicChar { 0 };

    char responseBuf[256] {};
    int responseLen { 0 };

    KittyDecoder kittyDecoder;

    // All dispatch logic (from ParserCSI, ParserESC, ParserOSC,
    // ParserSGR, ParserEdit, ParserOps, ParserVT)
    // Split into VideoCSI.cpp, VideoESC.cpp, VideoOSC.cpp, etc.
};
```

### Data Flow (Updated)

```
READER THREAD:
  PTY → 64 KB read → Processor::process(bytes, length)
    → parser->process(bytes, length)
      Parser DFA decodes byte-by-byte
      On print:       commands.get(Print, codepoint)
      On execute:     commands.get(Execute, byte)
      On CSI final:   commands.get(CSIDispatch, csi, finalByte)
      On ESC final:   commands.get(ESCDispatch, intermediate, finalByte)
      On OSC end:     commands.get(OSCEnd, buffer, length)
      On DCS hook:    commands.get(DCSHook, csi, finalByte)
      On DCS put:     commands.get(DCSPut, byte)
      On DCS unhook:  commands.get(DCSUnhook)
    → Processor handler:
      Dispatches to video.xxx()     → Video writes Grid
      Writes state.setXxx()         → State atomics for Display

MESSAGE THREAD (VBlank):
  Display:
    Reads grid.consumeDirtyRows()
    Drains scroll-off → Screen as jam::Cells
    Reads State (ValueTree) for cursor, modes, title
    Tells Screen to store, render
  Screen:
    Stores jam::Cells, renders via Glyph pipeline
```

## TODO Sequence

Grid redesign is complete (implemented). State already in Processor (implemented).

Remaining:

1. Create `Terminal::Video` class — move terminal state (pen, cursor, modes, tabstops, savedCursor, stamp, graphemeState, activeLinkId, lastGraphicChar, responseBuf, kittyDecoder) from Parser
2. Move VT dispatch logic from Parser to Video — ParserCSI→VideoCSI, ParserESC→VideoESC, ParserOSC→VideoOSC, ParserSGR→VideoSGR, ParserEdit→VideoEdit, ParserOps→VideoOps, ParserVT→VideoVT
3. Define `Command::Type` enum
4. Add `jam::Function::Map<Command::Type, void> commands` to Processor
5. Implement `Processor::registerCommands()` — register handlers that dispatch to Video + write State
6. Rewire Parser — remove `State&` and `Grid&`, replace with `Function::Map&`. Parser calls `commands.get()` during decode instead of direct Grid/State writes
7. Fix stale doc comments (Processor.h:55-56, Session.h:8) re State ownership
8. Design Screen as JUCE Viewport with jam::Cells storage (the visualizer)
9. Wire Display → Screen data flow (dirty rows + scroll-off → jam::Cells → render)
10. SIMD UTF-8 optimization in Parser ground fast path (debt)

## BLESSED Compliance Checklist

- [x] Bounds — Processor owns State/Parser/Video/commands. Session owns Grid. References explicit.
- [x] Lean — Parser ~3 files. Video dispatch split into Video*.cpp. Processor split with registerCommands().
- [x] Explicit — Function::Map dispatch. Parser takes Map& at construction. No magic.
- [x] SSOT — Parser=decoder, Video=terminal, State=model, Processor=sole State writer.
- [x] Stateless — Parser: transient decode state only. Video: calculation inputs only. Grid: dumb buffer.
- [x] Encapsulation — Parser doesn't know Video/State/Grid. Video doesn't know Parser/State. Lower layers never reach up.
- [x] Deterministic — same bytes → same commands → same state. Always.

## Open Questions

None. All design decisions resolved during ORACLE session.

## Handoff Notes

- Grid redesign is DONE (implemented). This RFC covers the Parser→Video split only.
- State is already in Processor (Processor.h:303). Doc comments at Processor.h:55-56 and Session.h:8 are stale — fix during implementation.
- The split is primarily a code MOVE, not a rewrite. ParserCSI.cpp logic moves to VideoCSI.cpp. The VT logic itself doesn't change — only where it lives and who orchestrates it.
- `jam::Function::Map` is proven in production (kuassa plugin framework, jam UI framework). Same mechanism as `ku::Function::Map` in JFS. No new invention.
- TETRIS contract is the governing principle. Parser is a DSP core (decoder). Video is a DSP core (terminal state machine). Processor is the ProcessorChain. State is the APVTS. Grid is the AudioBuffer. Command dispatch via Function::Map is parameterChanged via ku::Function::Map.
- Parser's DFA, DispatchTable, and UTF-8 accumulator are correct and faithful. No parser algorithm changes needed.
- Video holds `Grid&` to write cells. Parser holds `Function::Map&` to dispatch commands. Neither holds the other's reference. Composition flows through the dispatch table, not direct coupling.
- After this RFC: Screen design (JUCE Viewport + jam::Cells) is the next architectural piece. Display→Screen data flow completes the rendering pipeline.
