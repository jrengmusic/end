# PLAN: Windows Focus Orchestration and Headless Cygwin PTY

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM (`~/.carol/LANGUAGE.md` — C++ / JUCE)

## Overview
This plan fixes two Windows defects by contract:
1. `TerminalConsole::createProcess` spawns `eve-cygwinPTY.exe` headless (`CREATE_NO_WINDOW` when `reference == nullptr`), so no Windows Terminal opens or steals focus.
2. `VSTPluginWindow` declares one cross-platform `juce::Component` focus contract (`setWantsKeyboardFocus (true)` and `focusGained`), and `EditorView::createProcessorEditor` hands focus to a newly mounted focused editor, so END's `juce::Component` hierarchy orchestrates focus identically on macOS, Windows, and Linux (`DEBT-20261121T004125`).

## Language / Framework Constraints
- **C++ / JUCE (`LANGUAGE.md`):** One object, one translation unit. Non-template bodies stay in `.cpp` (`CODING.md` *TEMPLATE, CONSTEXPR, AND TRANSLATION UNIT PLACEMENT*).
- **JUCE patches (`user-modules-info.md:1412`):** `juce-plugin-window-focus-hook.patch` is an existing kernel patch applied after `juce-plugin-transparency-hook.patch` in the cast-managed JUCE cache. Inside JUCE module code (`juce_VSTPluginFormat.cpp`), match JUCE's surrounding C++ style (`!`, `&&`, `||`, `nullptr`).
- **JAM / END code:** Follow `CODING.md` CRITICAL RULES verbatim (`not`, `and`, `or`, brace initialization, no bail-out guards, positive nesting, no pre-audit comments).

## Framework Inventory

### 1. API

| Need | Module | API | Source | Step |
|---|---|---|---|---|
| Spawn child process with optional console reference | `jam_terminal` | `jam::TerminalConsole::createProcess (HANDLE reference, ...)` | `jam/jam_terminal/tty/native/jam_TerminalConsole_windows.h:72-74` | Step 1 |
| Headless child process flag on Windows | Win32 SDK | `CREATE_NO_WINDOW` | `jam/jam_subprocess/native/jam_StdioClient_windows.cpp:158` | Step 1 |
| Accept keyboard focus on JUCE Component | `juce_gui_basics` | `juce::Component::setWantsKeyboardFocus (bool)` | `juce_VSTPluginFormat.cpp:90`, `juce_Component.cpp:2996` | Step 2 |
| React to Component focus gain | `juce_gui_basics` | `juce::Component::focusGained (FocusChangeType)` | `juce_VSTPluginFormat.cpp:638` | Step 2 |
| Query hosted VST2 child HWND | `juce_audio_processors` | `VSTPluginWindow::getPluginHWND()` | `juce_VSTPluginFormat.cpp:714-717` | Step 2 |
| Transfer Win32 keyboard focus to HWND | Win32 SDK | `SetFocus (HWND)` | `juce_Windowing_windows.cpp:2303` | Step 2 |
| Focus embedded X11 plugin component | `juce_gui_extra` | `juce::XEmbedComponent::grabKeyboardFocus()` (inherited from `juce::Component`, `withWantsKeyboardFocus (true)`) | `juce_VSTPluginFormat.cpp:749-752`, `juce_XEmbedComponent.h:189` | Step 2 |
| Hand keyboard focus to newly created editor | `juce_gui_basics` | `juce::Component::isShowing()`, `juce::Component::grabKeyboardFocus()` | `juce_Component.cpp:3021-3034` | Step 3 |
| Pane focus state in ValueTree | `jam_gui` | `Id::focus` on `OwnedComponent::state` | `jam/jam_gui/layout/jam_OwnedComponent.cpp:36`, `end/Source/ui/EditorView.cpp:259` | Step 3 |

### 2. Pattern usage

| Pattern | Exemplar | Use sites | Step |
|---|---|---|---|
| Headless `CreateProcessW` with `CREATE_NO_WINDOW` | `jam/jam_subprocess/native/jam_StdioClient_windows.cpp:158` | `jam/jam_terminal/tty/native/jam_TerminalConsole_windows.cpp:222` (missing flag when `reference == nullptr`) | Step 1 |
| `TerminalConsole::createProcess` callers | `jam/jam_terminal/tty/native/jam_TerminalConsole_windows.cpp:353` (`reference != nullptr`) | `jam/jam_terminal/tty/native/jam_TerminalCygwinTTY_windows.cpp:90` (`reference == nullptr`) | Step 1 |
| `VSTPluginWindow` focus handoff to native view | `jam/patch/juce-plugin-window-focus-hook.patch:17-21` (macOS) | `juce_VSTPluginFormat.cpp:638-642` (macOS only; missing Windows and Linux) | Step 2 |
| Focus orchestration via `KeyboardFocusTraverser` | `end/Source/ui/SessionView.cpp:74-82` (`focusPane`) | `SessionView.cpp:70`, `SessionView.cpp:111`, `EVEView.cpp:74-81` | Step 2, Step 3 |
| Guard `grabKeyboardFocus()` with `isShowing()` and `Id::focus` | `eve/Source/EVEView.cpp:83-87` (`parentHierarchyChanged`) | `end/Source/ui/EditorView.cpp:265-275` (`createProcessorEditor`) | Step 3 |

### 3. Names

| Family | Members | Intent | Step |
|---|---|---|---|
| Win32 process creation flags | `creationFlags` (local `DWORD`) | Holds the `dwCreationFlags` mask for `CreateProcessW` | Step 1 |
| JUCE `Component` focus virtuals | `setWantsKeyboardFocus`, `focusGained`, `grabKeyboardFocus`, `isShowing` | Standard JUCE focus lifecycle methods (`juce_Component.cpp:2955-3034`) | Step 2, Step 3 |
| `VSTPluginWindow` native handle getters | `getPluginHWND()` (`juce_VSTPluginFormat.cpp:714`), `cocoaWrapper->getView()` (`:640`), `xembedComponent` (`:749`) | Existing platform child-view handles inside `VSTPluginWindow` | Step 2 |
| `jam::OwnedComponent` state properties | `Id::focus` (`jam_OwnedComponent.cpp:36`) | Single source of truth for pane focus state | Step 3 |

### 4. Synthesis

| Rule | Established form | Exemplar |
|---|---|---|
| "Aggregate (brace) initialization is ALWAYS preferred" | `const DWORD creationFlags { ... };` | `jam_TerminalConsole_windows.cpp:193-196` |
| "Use C++ alternative tokens: `not`, `and`, `or`" | `if (isShowing() and static_cast<bool> (state.getProperty (Id::focus)))` | `eve/Source/EVEView.cpp:85` |
| "No bail-out guards — ALWAYS use nested positive checks" | `if (isShowing() and ...) editor->grabKeyboardFocus();` | `end/Source/ui/EditorView.cpp:251-261` |
| "Use the framework API fully" | Route focus through `juce::Component` hierarchy (`setWantsKeyboardFocus`, `focusGained`, `grabKeyboardFocus`), never parallel OS focus hacks in END | `end/Source/ui/SessionView.cpp:74-82` |
| "Code Hygiene — no comments pre-audit" | Write code only in Steps 1–3; update doxygen in Step 4 post-audit | `CAROL.md` Code Hygiene |

### 5. Absence

| Need | Query | Result |
|---|---|---|
| `HWNDComponent` built-in focus forwarding to child HWND | Read `juce_HWNDComponent_windows.cpp:1-184` in full | Zero occurrences of `focusGained`, `SetFocus`, or `setWantsKeyboardFocus`; JUCE provides no automatic focus handoff in `HWNDComponent`, so `VSTPluginWindow::focusGained` is the single seam matching macOS `NSViewComponent`. |
| Existing `CREATE_NO_WINDOW` in `TerminalConsole::createProcess` | Read `jam_TerminalConsole_windows.cpp:189-234` in full | Absent; `CreateProcessW` at `:222` passes only `EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT`. |

## Validation Gate
Each step is validated by COUNSELOR before proceeding to the next — against
MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions
(no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering
the whole sprint — never per step.

## Steps

### Step 1: Make `TerminalConsole::createProcess` headless when `reference == nullptr`
**Scope:** `D:\Poems\dev\jam\jam_terminal\tty\native\jam_TerminalConsole_windows.cpp`
**Action:**
In `TerminalConsole::createProcess` (`jam_TerminalConsole_windows.cpp:222`), compute:
```cpp
const DWORD creationFlags { EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | (reference == nullptr ? CREATE_NO_WINDOW : 0ul) };
```
Pass `creationFlags` as the `dwCreationFlags` argument of `CreateProcessW`. Change nothing else. Add no comments.
**Inventory:**
- API: `jam::TerminalConsole::createProcess` (`jam_TerminalConsole_windows.h:72-74`), `CREATE_NO_WINDOW` (`jam_StdioClient_windows.cpp:158`).
- Pattern: `StdioClient::start` (`jam_StdioClient_windows.cpp:158`).
- Name family: local `DWORD creationFlags`.
- Synthesis: brace initialization, no pre-audit comments.
**Validation:**
COUNSELOR reads `jam_TerminalConsole_windows.cpp:189-234`, confirms `CREATE_NO_WINDOW` is set if and only if `reference == nullptr` (preserving `TerminalConsole::spawnProcess` ConDrv behavior at `:353`), verifies CODING.md formatting and brace initialization, and confirms zero comments added.

### Step 2: Unify `VSTPluginWindow` focus contract across all platforms in `juce-plugin-window-focus-hook.patch`
**Scope:**
- `D:\Poems\dev\jam\patch\juce-plugin-window-focus-hook.patch`
- `D:\Poems\dev\jam\user-modules-info.md` (line 1412 description cell only)
**Action:**
1. Update the `juce_VSTPluginFormat.cpp` hunks in `D:\Poems\dev\jam\patch\juce-plugin-window-focus-hook.patch` (base: unpatched `D:\Poems\JUCE\modules\juce_audio_processors\format_types\juce_VSTPluginFormat.cpp` after `juce-plugin-transparency-hook.patch`):
   - Remove `setWantsKeyboardFocus (true);` from inside `#elif JUCE_MAC`.
   - Place `setWantsKeyboardFocus (true);` once, unconditionally, in `VSTPluginWindow`'s constructor right beside `setOpaque (false);` and `setVisible (true);`.
   - Remove `focusGained (FocusChangeType) override` from inside `#if JUCE_MAC`.
   - Place ONE `void focusGained (FocusChangeType) override` member on `VSTPluginWindow` (outside `#if JUCE_MAC`, after the `#if JUCE_MAC ... #endif` block or in the public section of `VSTPluginWindow`), dispatching only the native OS focus handoff inside it:
     ```cpp
     void focusGained (FocusChangeType) override
     {
        #if JUCE_MAC
         if (auto* pluginView = [[(NSView*) cocoaWrapper->getView() subviews] firstObject])
             [[pluginView window] makeFirstResponder: pluginView];
        #elif JUCE_WINDOWS
         if (auto* pluginHWND = getPluginHWND())
             SetFocus (pluginHWND);
        #elif JUCE_LINUX || JUCE_BSD
         xembedComponent.grabKeyboardFocus();
        #endif
     }
     ```
   - Keep the `juce_AudioUnitPluginFormat.mm` hunks unchanged.
2. Update the description cell of `juce-plugin-window-focus-hook.patch` at `D:\Poems\dev\jam\user-modules-info.md:1412` to reflect cross-platform VST2 focus handoff (macOS first responder, Windows `SetFocus`, Linux `XEmbedComponent`) and macOS AU first responder.
**Inventory:**
- API: `setWantsKeyboardFocus`, `focusGained`, `VSTPluginWindow::getPluginHWND()`, `SetFocus`, `xembedComponent.grabKeyboardFocus()`.
- Pattern: `juce-plugin-window-focus-hook.patch:1-50`, `juce_VSTPluginFormat.cpp:103-110,631-643,714-717,749-752`.
- Name family: JUCE `Component` focus virtuals and `VSTPluginWindow` native handles.
- Synthesis: no platform-split `setWantsKeyboardFocus`; one `focusGained` override on `VSTPluginWindow`.
**Validation:**
COUNSELOR reads `D:\Poems\dev\jam\patch\juce-plugin-window-focus-hook.patch` in full and `D:\Poems\dev\jam\user-modules-info.md:1410-1415`, verifies line context matches unpatched `D:\Poems\JUCE\modules\juce_audio_processors\format_types\juce_VSTPluginFormat.cpp` after `juce-plugin-transparency-hook.patch`, confirms `setWantsKeyboardFocus (true)` is unconditional, and confirms one `focusGained` override covers macOS, Windows, and Linux cleanly.

### Step 3: Hand focus to newly created focused editor in `EditorView::createProcessorEditor`
**Scope:** `D:\Poems\dev\end\Source\ui\EditorView.cpp`
**Action:**
In `EditorView::createProcessorEditor()` (`EditorView.cpp:265-275`), after `addAndMakeVisible (editor.get());` and `dropTarget.toFront (false);`, add:
```cpp
if (isShowing() and static_cast<bool> (state.getProperty (Id::focus)))
    editor->grabKeyboardFocus();
```
Change nothing else. Add no comments.
**Inventory:**
- API: `juce::Component::isShowing()`, `juce::Component::grabKeyboardFocus()`, `Id::focus`.
- Pattern: `eve/Source/EVEView.cpp:83-87` (`isShowing()` + `Id::focus` check before `grabKeyboardFocus()`), `end/Source/ui/SessionView.cpp:74-82`.
- Name family: JUCE `Component` focus virtuals, `Id::focus`.
- Synthesis: `and` alternative token, positive check, no bail-out guard, no pre-audit comments.
**Validation:**
COUNSELOR reads `D:\Poems\dev\end\Source\ui\EditorView.cpp:245-285`, verifies CODING.md rules (`and`, positive check, zero comments), and confirms `editor->grabKeyboardFocus()` runs only when `isShowing()` and `Id::focus` are true, matching `juce_Component.cpp:3033`.

### Step 4: Audit and Post-Audit Doxygen Pass
**Scope:**
- `@Auditor` once across Steps 1–3 (`jam_TerminalConsole_windows.cpp`, `juce-plugin-window-focus-hook.patch`, `user-modules-info.md`, `EditorView.cpp`).
- `@Pathfinder` doxygen pass on `jam_TerminalConsole_windows.h:54-75` (`createProcess` comment: document `CREATE_NO_WINDOW` when `reference` is null) and `EditorView.h` (`createProcessorEditor` / class brief note if needed), verified by COUNSELOR against the code.
**Action:**
Run `@Auditor` once; resolve any validated DCF findings; run `@Pathfinder` doxygen pass; stop for ARCHITECT to run `cast cast/spell.md` in END, build, and test at runtime.
**Inventory:** All rows from Steps 1–3.
**Validation:** COUNSELOR validates every Auditor finding per Design by Contract Filter and reads every Pathfinder doxygen edit against the implementation.

## BLESSED Alignment
- **B (Bound):** Process creation flags live at `TerminalConsole::createProcess` (the process owner). Focus ownership lives at END's `juce::Component` hierarchy (`SessionView` → `TabView` → `EditorView` → `VSTPluginWindow`), which hands native focus to the hosted plugin's window.
- **L (Lean):** One flag expression in `createProcess`, one unconditional `setWantsKeyboardFocus (true)` and one `focusGained` override on `VSTPluginWindow`, two lines in `EditorView::createProcessorEditor`. No new classes, helpers, or state.
- **E (Explicit):** `CREATE_NO_WINDOW` is explicit when `reference == nullptr`. `isShowing() and static_cast<bool> (state.getProperty (Id::focus))` states the exact precondition for grabbing focus.
- **S (SSOT):** END's `juce::Component` focus hierarchy + `Id::focus` ValueTree state remain the single source of truth across macOS, Windows, and Linux.
- **S (Stateless):** Zero new member variables, zero boolean flags, zero shadow focus state.
- **E (Encapsulation):** END never reaches into Win32 `HWND`s; `VSTPluginWindow` encapsulates the platform focus handoff behind `juce::Component::focusGained`.
- **D (Deterministic):** Same click, split, tab switch, or window activation produces identical focus behavior across platforms.

## Risks / Open Questions
None.
