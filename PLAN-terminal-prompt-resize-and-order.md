# PLAN: Terminal Prompt Resize Signal and Live-History Render Order

**RFC:** none — objective from ARCHITECT `/pay` session (`DEBT-20261121T004127` and `DEBT-20261121T004129`)
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM (`~/.carol/LANGUAGE.md` — C++ / JUCE)

## Overview

This plan resolves two terminal debts by contract:
1. `eve-cygwinPTY` `Session::applySizes` sends `SIGWINCH` (`windowChangeSignal { 28 }`) to `shell` after `TIOCSWINSZ`, matching `TerminalUnixTTY::setWinsize`, so an active prompt redraws immediately on pane resize (`DEBT-20261121T004127`).
2. `TerminalBuffer::writeToStream` stamps each live frame with `numHistoryRows`, `TerminalHistory::addLiveLines` stores it on the live root as `Id::firstLine`, and `TextEditor` renders `live` only when `getHistoryEnd() >= liveFirstLine`, so the active prompt never appears before `seq` output finishes (`DEBT-20261121T004129`).

## Language / Framework Constraints

- **C++ / JUCE (`LANGUAGE.md`):** One object, one translation unit. Non-template bodies stay in `.cpp` (`CODING.md` *TEMPLATE, CONSTEXPR, AND TRANSLATION UNIT PLACEMENT*).
- **`eve-cygwinPTY` ABI:** `eve-cygwinPTY` is a native Win32 binary that loads `msys-2.0.dll` / `cygwin1.dll` at runtime (`Runtime.cpp:5-35`) without POSIX headers. It declares Cygwin ABI numbers as named `static constexpr` constants in function scope (`Session.cpp:37-40,145,166,186-191`).
- **JAM / EVE code:** Follow `CODING.md` CRITICAL RULES verbatim (`not`, `and`, `or`, brace initialization, no bail-out guards, positive checks, no pre-audit comments).

## Framework Inventory

### 1. API

| Need | Module | API | Source | Step |
|---|---|---|---|---|
| Send signal to Cygwin shell process | `eve-cygwinPTY` | `Runtime::kill (int, int)` | `eve/cygwinPTY/Source/Runtime.h:67`, `Runtime.cpp:24` | Step 1 |
| Apply pending packed winsize records | `eve-cygwinPTY` | `Session::applySizes()` / `jam::TerminalFIFO::applyPendingData` | `eve/cygwinPTY/Source/Session.cpp:164-182` | Step 1 |
| Write live frame header to swap stream | `jam_terminal` | `TerminalBuffer::writeToStream (juce::OutputStream&, jam::Cell)` / `writeFrame` | `jam/jam_terminal/buffers/jam_TerminalBuffer.h:109,454-471`, `.cpp:414-435` | Step 2 |
| Read live frame stream into live document | `jam_terminal` | `TerminalSwap::setLiveData` / `TerminalHistory::addLiveLines` | `jam/jam_terminal/document/jam_TerminalSwap.h:136-148`, `jam_TerminalHistory.cpp:288-302` | Step 2 |
| Store and read typed attribute on `Document::Element` | `jam_core` | `Element::add<juce::int64>`, `Element::contains`, `Element::get<juce::int64>` | `jam/jam_terminal/document/jam_TerminalHistory.cpp:113`, `jam_TextEditor.cpp:1577,1616` | Step 2, Step 3 |
| Query end of built history blocks | `jam_gui` | `TextEditor::getHistoryEnd()` | `jam/jam_gui/widgets/jam_TextEditor.cpp:1609-1621` | Step 3 |

### 2. Pattern usage

| Pattern | Exemplar | Use sites | Step |
|---|---|---|---|
| POSIX PTY resize sends `ioctl (TIOCSWINSZ)` then `kill (childProcess, SIGWINCH)` | `jam/jam_terminal/tty/native/jam_TerminalTTY_posix.cpp:328-333` | `eve/cygwinPTY/Source/Session.cpp:177` (missing `runtime.kill`) | Step 1 |
| Cygwin signal constant + `runtime.kill` in `Session` | `eve/cygwinPTY/Source/Session.cpp:143-148` (`hangupSignal { 1 }`) | `eve/cygwinPTY/Source/Session.cpp:164-182` | Step 1 |
| Binary live frame producer-consumer pair | `jam_TerminalBuffer.cpp:428-434` -> `jam_TerminalSwap.cpp:71` -> `jam_TerminalHistory.cpp:290-301` | Single producer (`TerminalSwap::setLiveLines`), single consumer (`TerminalHistory::addLiveLines`) | Step 2 |
| Document root metadata attribute on `TextBlock` | `jam_TerminalHistory.cpp:113` (`document.getRoot()->add<int> (Id::size, ...)`) | `jam_TextEditor.cpp:1418,1552,1593,1616` | Step 2, Step 3 |
| History + live block traversal in `TextEditor` | `jam_TextEditor.h:912-920,990-997`, `jam_TextEditor.cpp:1848-1851,2022-2025,2294-2307,2816-2820` | All 6 terminal `history + live` display/metric/hit-test/caret sites | Step 3 |

### 3. Names

| Family | Members | Intent | Step |
|---|---|---|---|---|
| `Session` signal constants | `hangupSignal { 1 }` (`Session.cpp:145`), `backgroundTerminalSignals { 21, 22 }` (`Session.cpp:38`) | New function-local constant `windowChangeSignal { 28 }` (Cygwin/MSYS2 `SIGWINCH`, verified via `zsh -c "kill -l WINCH"`) | Step 1 |
| Terminal line identifiers | `Id::firstLine` (`jam/cast/terminal.md:57`), `Id::endLine` (`:55`), `Id::closedLine` (`:45`) | Reuse `Id::firstLine` on live document root for the history line index where row 0 of `live` starts | Step 2, Step 3 |
| `TextEditor` boolean queries | `hasKeyboardFocus`, `isViewAtEnd`, `isVisualBlock`, `isReadOnly` (`jam_TextEditor.h`) | New private `const` query `hasLive()` — answers whether `live` is contiguous with `history`, changes nothing (`NAMES.md` Rule 1 Verb Contract) | Step 3 |

### 4. Synthesis

| Rule | Established form | Exemplar |
|---|---|---|
| "Aggregate (brace) initialization is ALWAYS preferred" | `static constexpr int windowChangeSignal { 28 };` | `eve/cygwinPTY/Source/Session.cpp:145` |
| "Use C++ alternative tokens: `not`, `and`, `or`" | `if (consumed > 0 and shell > 0)` | `eve/cygwinPTY/Source/Session.cpp:140,212` |
| "Owner first, narrowest scope" | Function-local `static constexpr` in `Session::applySizes` | `eve/cygwinPTY/Source/Session.cpp:166` |
| "One object, one translation unit" | `TextEditor::hasLive()` declared in `jam_TextEditor.h`, defined in `jam_TextEditor.cpp` | `jam/jam_gui/widgets/jam_TextEditor.cpp:1604-1621` |
| "Line continuation: operator at start of continuation line" | `return live.document == nullptr` / `or not ...` / `or ...` | `CODING.md:109-112` |
| "Code Hygiene — no comments pre-audit" | Write code only in Steps 1–3; update doxygen in Step 4 post-audit | `CAROL.md` Code Hygiene |

### 5. Absence

| Need | Query | Result |
|---|---|---|
| Any second caller of `TerminalBuffer::writeToStream (OutputStream&, Cell)` | Read `jam/docs/xml/index.xml:17176-17177`, `classjam_1_1TerminalBuffer.xml:501-588`, and all `jam_terminal/`, `jam_gui/`, `eve/Source/`, `end/Source/` callers | Single caller: `TerminalSwap::setLiveLines` (`jam_TerminalSwap.cpp:71`). Second overload `(OutputStream&, LookAndFeel&)` is separate VT lane (`jam_TerminalComponentPeer.cpp:144`). |
| Any reader of `frameId` from the live stream | Read `TerminalSwap::setLiveData` (`jam_TerminalSwap.h:136-148`) and its sole caller `TerminalHistory::addLiveLines` (`jam_TerminalHistory.cpp:288-302`) | Line 292 calls `input.skipNextBytes (sizeof (juce::int64))` — zero readers consume `frameId` from the live stream. |
| POSIX `<signal.h>` in `eve-cygwinPTY` | Read `eve/cygwinPTY/Source/Runtime.h:1-82`, `Runtime.cpp:1-42`, `Session.cpp:1-232` | `eve-cygwinPTY` is native Win32 loading `msys-2.0.dll` dynamically; all Cygwin constants are function-local `static constexpr`. |

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against
MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions
(no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering
the whole sprint — never per step.

## Steps

### Step 1: Send `SIGWINCH` after `TIOCSWINSZ` in `eve-cygwinPTY` (`DEBT-20261121T004127`)
**Scope:** `D:/Poems/dev/eve/cygwinPTY/Source/Session.cpp`
**Action:**
In `Session::applySizes()` (`D:/Poems/dev/eve/cygwinPTY/Source/Session.cpp:164-182`):
1. Add `static constexpr int windowChangeSignal { 28 };` next to `static constexpr int setWindowSize { ('T' << 8) | 2 };`.
2. After the `for` loop inside the `sizes.applyPendingData` lambda, add:
```cpp
        if (consumed > 0 and shell > 0)
            runtime.kill (shell, windowChangeSignal);
```
Change nothing else. Add no comments.
**Inventory:**
- API: `Runtime::kill` (`Runtime.h:67`, `Runtime.cpp:24`), `Session::applySizes` (`Session.cpp:164-182`).
- Pattern: `TerminalUnixTTY::setWinsize` (`jam_TerminalTTY_posix.cpp:328-333`), `Session::applyHangup` (`Session.cpp:143-148`).
- Name family: `windowChangeSignal` joining `hangupSignal` (`Session.cpp:145`).
- Synthesis: brace initialization, `and` token, narrowest scope, zero pre-audit comments.
**Validation:**
COUNSELOR reads `D:/Poems/dev/eve/cygwinPTY/Source/Session.cpp:160-190`, verifies CODING.md formatting (`and`, brace init, positive check, zero comments), NAMES.md compliance, and confirms `runtime.kill (shell, windowChangeSignal)` runs once per applied resize batch when `shell > 0`.

### Step 2: Stamp live frame with `numHistoryRows` and read as `Id::firstLine` (`DEBT-20261121T004129`)
**Scope:**
- `D:/Poems/dev/jam/jam_terminal/buffers/jam_TerminalBuffer.cpp`
- `D:/Poems/dev/jam/jam_terminal/document/jam_TerminalHistory.cpp`
**Action:**
1. In `TerminalBuffer::writeToStream (juce::OutputStream& output, jam::Cell cursorRow)` (`jam_TerminalBuffer.cpp:430`), replace `static_cast<juce::int64> (frameId)` with `numHistoryRows`:
```cpp
                       [this, numRows] (juce::OutputStream& stream) { return stream.writeInt64 (numHistoryRows) and stream.writeInt (numRows); },
```
2. In `TerminalHistory::addLiveLines` (`jam_TerminalHistory.cpp:292`), replace `input.skipNextBytes (sizeof (juce::int64));` with:
```cpp
        document.getRoot()->add<juce::int64> (Id::firstLine, input.readInt64());
```
Change nothing else. Add no comments.
**Inventory:**
- API: `TerminalBuffer::writeToStream` (`jam_TerminalBuffer.cpp:414-435`), `TerminalHistory::addLiveLines` (`jam_TerminalHistory.cpp:288-302`), `Element::add<juce::int64>`.
- Pattern: binary live frame producer-consumer pair (`jam_TerminalSwap.cpp:71` -> `jam_TerminalHistory.cpp:290-301`); document root metadata (`jam_TerminalHistory.cpp:113`).
- Name family: `Id::firstLine` (`jam/cast/terminal.md:57`).
- Synthesis: zero pre-audit comments, exact wire layout preserved (8-byte `int64` + 4-byte `int`).
**Validation:**
COUNSELOR reads `jam_TerminalBuffer.cpp:414-436` and `jam_TerminalHistory.cpp:288-303`, confirms `numHistoryRows` is written and stored as `Id::firstLine` on `document.getRoot()`, verifies CODING.md compliance, and confirms zero comments added.

### Step 3: Gate `TextEditor` live block display on `hasLive()` (`DEBT-20261121T004129`)
**Scope:**
- `D:/Poems/dev/jam/jam_gui/widgets/jam_TextEditor.h`
- `D:/Poems/dev/jam/jam_gui/widgets/jam_TextEditor.cpp`
**Action:**
1. In `D:/Poems/dev/jam/jam_gui/widgets/jam_TextEditor.h`, declare `bool hasLive() const;` in the private section next to `getFirstLine()` and `getHistoryEnd()`.
2. In `D:/Poems/dev/jam/jam_gui/widgets/jam_TextEditor.cpp`, define `TextEditor::hasLive()` right after `TextEditor::getHistoryEnd()` (`jam_TextEditor.cpp:1621`):
```cpp
bool TextEditor::hasLive() const
{
    return live.document == nullptr
           or not live.document->getRoot()->contains (Id::firstLine)
           or getHistoryEnd() >= *live.document->getRoot()->get<juce::int64> (Id::firstLine);
}
```
3. Gate the 6 terminal `history + live` sites on `hasLive()`:
   - `jam_TextEditor.h:920` (`applyToVisibleLines`):
     ```cpp
     return hasLive() and applyToBlock (live, blockTop, firstLine);
     ```
   - `jam_TextEditor.h:997` (`applyToPaintedBlocks`):
     ```cpp
     if (hasLive()) applyToBlock (live, top, firstLine);
     ```
   - `jam_TextEditor.cpp:1850` (`getNumLines`):
     ```cpp
     return getNumHistoryLines() + (hasLive() ? live.index->getNumLines() : 0);
     ```
   - `jam_TextEditor.cpp:2024` (`setContentSize`):
     ```cpp
     content.setSize (getTerminalBounds().getWidth(), jam::toInt (getNumHistoryPixels() + (hasLive() ? live.index->getNumPixels() : 0.0f), true));
     ```
   - `jam_TextEditor.cpp:2296` (`getCell`):
     ```cpp
     if (hasLive() and position.y >= getNumHistoryPixels())
     ```
   - `jam_TextEditor.cpp:2816` (`Content::paintOverChildren`):
     ```cpp
     if (owner.hasKeyboardFocus (true) and (owner.getMode() != map::EditorMode::terminal or owner.hasLive()))
     ```
Change nothing else. Add no comments.
**Inventory:**
- API: `TextEditor::getHistoryEnd()` (`jam_TextEditor.cpp:1609-1621`), `Element::contains`, `Element::get<juce::int64>`.
- Pattern: `history + live` traversal sites (`jam_TextEditor.h:912-920,990-997`, `jam_TextEditor.cpp:1848-1851,2022-2025,2294-2307,2816-2820`).
- Name family: `hasLive()` (`NAMES.md` Rule 1 Verb Contract `has` — returns `bool`, changes nothing).
- Synthesis: one object, one TU (body in `.cpp`), `not`/`and`/`or`, line continuation operator at start of line, positive checks, zero pre-audit comments.
**Validation:**
COUNSELOR reads every modified hunk in `jam_TextEditor.h` and `jam_TextEditor.cpp`, confirms all 6 sites use `hasLive()`, confirms `LiveJob` still drains `liveSlots` unconditionally, confirms non-terminal / read-only modes (where `Id::firstLine` is absent) return `true` from `hasLive()`, and verifies CODING.md and NAMES.md compliance.

### Step 4: Fix Windows `HWNDComponentPeer` child-HWND focus contract and `EditorView` `Id::focus` listener (`DEBT-20261121T004125`)
**Scope:**
- `D:/Poems/dev/jam/patch/juce-plugin-window-focus-hook.patch`
- `D:/Poems/dev/jam/user-modules-info.md` (line 1412 description cell only)
- `D:/Poems/dev/end/Source/ui/EditorView.cpp`
**Action:**
1. In `D:/Poems/dev/jam/patch/juce-plugin-window-focus-hook.patch`, append a diff for `modules/juce_gui_basics/native/juce_Windowing_windows.cpp` (against unpatched `D:/Poems/JUCE/modules/juce_gui_basics/native/juce_Windowing_windows.cpp` lines 1587-1590 and 3925-3939):
   - In `HWNDComponentPeer::isFocused()`:
     ```cpp
     bool isFocused() const override
     {
         auto* focused = (HWND) callFunctionIfNotLocked (&getFocusCallback, nullptr);
         return focused == hwnd || (focused != nullptr && IsChild (hwnd, focused));
     }
     ```
   - In `WM_KILLFOCUS`:
     ```cpp
     if ((HWND) wParam == nullptr || ! IsChild (hwnd, (HWND) wParam))
         handleFocusLoss();
     ```
2. Update the description cell of `juce-plugin-window-focus-hook.patch` in `D:/Poems/dev/jam/user-modules-info.md:1412` to note that `HWNDComponentPeer` treats a focused child `HWND` as focused (matching macOS `[window isKeyWindow]`).
3. In `D:/Poems/dev/end/Source/ui/EditorView.cpp` inside `EditorView::registerEvents()` (`:196-236`), add an `Id::focus` event entry:
   ```cpp
   events.add (Id::focus,
               [this] (const juce::Identifier&, juce::ValueTree& tree)
               {
                   if (tree == state and editor != nullptr and isShowing() and static_cast<bool> (tree.getProperty (Id::focus)))
                       editor->grabKeyboardFocus();
               });
   ```
Change nothing else. Add no comments.
**Inventory:**
- API: `HWNDComponentPeer::isFocused` (`juce_Windowing_windows.cpp:1587-1590`), `IsChild` (Win32 SDK), `EditorView::events` (`EditorView.h:221`, `EditorView.cpp:196-236`), `Component::grabKeyboardFocus`.
- Pattern: `NSViewComponentPeer::isFocused()` (`juce_NSViewComponentPeer_mac.mm:1587-1592`), `EditorView::createProcessorEditor` (`EditorView.cpp:277`), `EVEView::registerTerminalEvents` (`EVEView.cpp:191-197`).
- Name family: `Id::focus`, `events` (`EditorView::registerEvents`).
- Synthesis: `and` token, positive check, no OS macros in END/EVE, zero pre-audit comments.
**Validation:**
COUNSELOR reads `juce-plugin-window-focus-hook.patch`, `user-modules-info.md:1410-1415`, and `EditorView.cpp:196-245`, verifies patch context matches `D:/Poems/JUCE/modules/juce_gui_basics/native/juce_Windowing_windows.cpp` (after prior patches), and checks CODING.md compliance.

### Step 5: Audit and Post-Audit Doxygen Pass
**Scope:**
- `@Auditor` once across Steps 1–4 (`eve/cygwinPTY/Source/Session.cpp`, `jam_TerminalBuffer.cpp`, `jam_TerminalHistory.cpp`, `jam_TextEditor.h`, `jam_TextEditor.cpp`, `juce-plugin-window-focus-hook.patch`, `user-modules-info.md`, `EditorView.cpp`).
- `@Pathfinder` doxygen pass on `eve/cygwinPTY/Source/Session.h` (`applySizes`), `jam_TerminalBuffer.h` (`writeToStream`), `jam_TerminalHistory.h` (`addLiveLines`), `jam_TextEditor.h` (`hasLive`, `applyToVisibleLines`, `applyToPaintedBlocks`), and `EditorView.h` (`registerEvents`).
**Action:**
Run `@Auditor` once; resolve any validated DCF findings via `@Engineer`; delegate post-audit doxygen pass to `@Pathfinder` and verify every block against code; hand off to ARCHITECT to rebuild (`cast` in EVE to repack `eve-cygwinPTY`, `cast` in END to re-apply `juce-plugin-window-focus-hook.patch`, build jam + EVE + END) and verify at runtime.
**Inventory:** All rows from Steps 1–4.
**Validation:** COUNSELOR validates every Auditor finding per Design by Contract Filter and reads every Pathfinder doxygen edit against implementation.

## BLESSED Alignment

- **B (Bound):** `Session` owns the Cygwin shell PID and sends `SIGWINCH` at the PTY boundary. `TerminalBuffer` owns `numHistoryRows` and stamps each live frame at creation.
- **L (Lean):** 2 lines in `Session.cpp`, 1 line in `jam_TerminalBuffer.cpp`, 1 line in `jam_TerminalHistory.cpp`, 1 `const` query (`hasLive()`) and 6 call-site checks in `jam_TextEditor`. No new classes, structs, or threads.
- **E (Explicit):** `windowChangeSignal { 28 }` names the Cygwin `SIGWINCH` constant; `Id::firstLine` explicitly carries the live frame's history start line; `hasLive()` expresses contiguity directly.
- **S (SSOT):** `TerminalBuffer::numHistoryRows` is the single monotonic history-line truth; `live` carries its own start line into `TextEditor` instead of guessing readiness from a separate lane.
- **S (Stateless):** Zero new member variables, zero boolean flags, zero shadow state.
- **E (Encapsulation):** Unidirectional data flow (`TerminalBuffer` -> `TerminalSwap` -> `TerminalHistory` -> `TextEditor`); `Session` encapsulates Cygwin PTY signaling identically to `TerminalUnixTTY`.
- **D (Deterministic):** Same output order on macOS and Windows regardless of shell throughput (`time seq` at millions of lines); active prompt redraws deterministically on pane resize.

## Risks / Open Questions

- None.
