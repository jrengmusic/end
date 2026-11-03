# PLAN: Settings Once Per Scope (jam PluginEditorLayout memo)

**Parent:** `PLAN-instant-plugin-loading.md`, Amendment row 1 ("Settings once per scope").
**Date:** 2026-11-03
**Status:** approved by ARCHITECT (2026-11-03).
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM. One object, one translation unit (CODING.md). No comment or doxygen before the audit (CAROL.md Code Hygiene).

## Context

Each EVE pane builds its settings three times on the message thread. ARCHITECT's Release runtime log (`~/Desktop/EVE VST.ode`, one split) shows:

| Call | Site | ms |
|---|---|---|
| processor constructor | `EVEProcessor.cpp:17` | 249.8 |
| editor constructor, first build | `EVEView.cpp:11` | 219.9 |
| editor constructor, second build | `EVEView.cpp:11` | 230.8 |

`newPane` blocks for 764-840 ms per split (`~/.config/end/end.ode`). Settings take about 700 ms of that block. Every call parses the same binary default and the same user file. Thus every call after the first builds again what the first call made (MANIFESTO **E**, Complete at Creation).

## ARCHITECT rulings (this session)

- Design: *"Memo, events as now (Recommended)"*.
- Event contract: *"NO MANUAL STATE MANAGEMENT ALLOWED. MUST BE FULLY EVENT DRIVEN AS NOW"*.
- Scope: *"JFS with KANJUT should not break. no runtime changes, possibly no code change either. only at framework details implementation synced"*.
- Order: *"jam first then JFS with KANJUT … you dont build JFS, your build permission only for profiling. RUNTIME build still need ARCHITECT supervision"*.
- Pattern: *"all pattern exists, only need to be followed VERBATIM"*.
- Related projects: END, EVE, jam, JFS, KANJUT. WHELMED is excluded (`end/CLAUDE.md:16-17`).

## Facts (read this session)

- **One entry point serves both lanes.** `PluginEditorLayout::getOrCreateSettings()` dispatches through the member pointer `settings` (`jam_PluginEditorLayout.cpp:13-17`, `.h:121`, `.h:317`). The markdown lane (EVE) and the XML lane (JFS) both pass through it. KANJUT has the same entry (`kuassa_PluginEditorLayout.cpp:13-16`).
- **Callers.** EVE: `EVEProcessor.cpp:17`, `EVEView.cpp:11`, `jam_PluginEditor.cpp:428`. JFS: `FilterStripProcessor.cpp:34`, `FilterStripView.cpp:22`, and `kuassa_PluginEditor.cpp:358` through `FilterStripView.cpp:29`.
- **The output depends on one input.** The binary default and the rules are fixed per binary (`.h:117-122`). The only input that varies is the user settings file (`jam_PluginEditorLayout.cpp:57-58`). Thus the same file text always gives the same tree (MANIFESTO **D**).
- **Consumers only read the returned tree.**
  - `populateTree` reads it (`jam_PluginEditorLayout.cpp:70-78`, `:643-650`).
  - `StyleManager::registerStyle` stores the handle as `config` (`jam_StyleManager.cpp:21-23`). No `StyleManager` member writes `config` (whole file read).
  - `EVEView::initialiseTheme` passes it to `styleManager.create` (`EVEView.cpp:80-86`).
  - Thus the tree is published data and is immutable after creation (MANIFESTO **E**, *"Published data is immutable"*).
- **Sibling pattern, quoted verbatim.** `MarkdownDocument::getOrCreate` (`jam_MarkdownDocument.cpp:5-21`) does these steps:
  1. It finds the key in a shared registry and returns the entry.
  2. Otherwise it creates the entry.
  3. It adds the entry with `try_emplace` and returns it.

  The registry is `SharedDocuments` (`jam_Document.h:1235-1239`). `PluginEditorLayout` holds it as the nested type `LayoutDocuments` (`.h:253`) through `SharedInstance<LayoutDocuments> documents` (`.h:320`). `StyleManager::getOrCreateTypeface` is the same memo keyed by `juce::String` (`jam_StyleManager.cpp`, `typefaces`).
- **Key type.** `jam::HashMap<juce::String, …>` hashes the UTF-8 bytes with wyhash (`jam_HashMap.h:464-480`). A lookup is a dispatch by key, not a text branch (CODING.md, *No `==`/`!=` on strings*).
- **Scope.** `SharedInstance` joins one object for each binary × process. The last holder destroys it (`jam_SharedInstance.h:1-3`, `:97-101`). A host that isolates instances gets one memo per instance, and the behaviour stays correct.
- **Events do not change.** Each editor keeps its `settingsWatcher` and its `fileChanged` (`jam_PluginEditor.cpp:221-223`, `:421-438`). The watcher delivers on the message thread (`jam_Listener.cpp:66-98`). A changed file has new text, so the first `fileChanged` misses the memo and parses. Each later `fileChanged` for the same text hits the memo.

## New names (NAMES.md Rule 5: from established families, no ratification)

| Name | Kind | Family |
|---|---|---|
| `LayoutSettings` | nested struct of `PluginEditorLayout` | `LayoutDocuments` (`jam_PluginEditorLayout.h:253`) |
| `layoutSettings` | member `SharedInstance<LayoutSettings>` | type-named members `parameterManager`, `styleManager`, `commandManager` (`EVEProcessor.h:97`, `:114`; `jam_PluginEditor.h:240`) |

The family member name `settings` exists already (`.h:317`, the member pointer). Thus the member takes the type-named form.

## Steps

### Step 1: jam memo (Engineer)

**Scope:** `jam/jam_plugin_bootstrap/layout/jam_PluginEditorLayout.h`, `jam_PluginEditorLayout.cpp`. No other file.

**Action:**
1. `.h`, after `LayoutDocuments` (`:305`), add:
   ```cpp
   struct LayoutSettings : jam::HashMap<juce::String, juce::ValueTree>
   {
   };
   ```
2. `.h`, after `documents` (`:320`), add:
   ```cpp
   SharedInstance<LayoutSettings> layoutSettings { std::in_place };
   ```
   This is the shape of `uiScaleMap` (`.h:308`).
3. `.cpp`, `getOrCreateSettings()` (`:13-17`). Keep the `Timer` line (`:15`), which is instrumentation of the parent plan. The body becomes the `MarkdownDocument::getOrCreate` shape:
   ```cpp
   juce::ValueTree PluginEditorLayout::getOrCreateSettings() const
   {
       const debug::Log::Timer timer { "jam settings" };
       const auto userText { ParameterManager::getInstance()->getUserSettings().loadFileAsString() };

       if (const auto found { layoutSettings->find (userText) }; found != layoutSettings->end())
       {
           const auto& [settingsText, settingsTree] { *found };
           return settingsTree;
       }

       const auto settingsTree { (this->*settings)() };
       layoutSettings->try_emplace (userText, settingsTree);
       return settingsTree;
   }
   ```

The Engineer prompt restates the following:
- Design by Contract, per CODING.md CRITICAL RULES.
- The MANIFESTO **E** data-flow contract, verbatim.
- Doxygen first.
- No comment and no doxygen.
- No git.
- No build.
- These two files only, and the names verbatim from this plan.

**Validation (COUNSELOR):**
1. Read both files at the changed lines.
2. Check that the body is the `MarkdownDocument::getOrCreate` shape (`jam_MarkdownDocument.cpp:5-21`).
3. Check that no caller, no lane template (`.cpp:19-68`) and no `PluginEditor` code changes.

### Step 2: Profile measurement (Pathfinder, scout)

**Action:** run `scout /Users/jreng/Documents/Poems/dev/end/benchmark/pane.md`, then `…/split.md`, one at a time. Before each run, set its `baseline` to the current report. The `## toolchain` rows cast jam and build EVE and END Profile.

**Validation:**
- Each report has `buildType` Release and `dropped` 0.
- The `paneEditor` and `newPane` rows show a change with a small `probability`.

### Step 3: Runtime measurement (ARCHITECT)

ARCHITECT builds END Release with `<leader>br`. ARCHITECT opens 4 splits and closes each.

COUNSELOR reads `~/Desktop/EVE VST.ode` and `~/.config/end/end.ode`:
- Only a memo miss shows a full `jam settings` time.
- Every hit shows only the file read.

COUNSELOR reports the before and after table: `jam settings`, `EVE view`, `newPane`.

### Step 4: KANJUT sync (ARCHITECT)

ARCHITECT applies the same two-file change to `kuassa_PluginEditorLayout.h` and `.cpp` (`:13-16`).
- JFS source does not change.
- JFS runtime behaviour does not change: each JFS call returns the same tree for the same file text.
- Agents do not edit KANJUT and do not build JFS.

### Step 5: Return to the parent plan

The parent `PLAN-instant-plugin-loading.md` continues with Step 11, the remaining amendment rows. The audit (parent Step 13) and the doxygen pass (parent Step 14) cover this change. The doxygen pass also corrects the class text *"The editor calls getOrCreateSettings() once, in its constructor"* (`jam_PluginEditorLayout.h:37-38`).

## BLESSED Alignment

- **B:** `SharedInstance` owns the memo. The last holder destroys it. `PluginEditorLayout` holds it, as it holds `documents`.
- **L:** one nested type and one member. The function body has one branch.
- **E:** the input (the user file text) is the key. The memo is the existing registry shape.
- **S (SSOT):** one tree for each file text in each scope. No consumer builds it again.
- **S (Stateless):** no flag and no invalidation. A new file text is a new key.
- **E (Encapsulation):** callers, `PluginEditor` and the lanes do not change. Events stay as they are.
- **D:** the same text gives the same tree.

## Risks / Open Questions

- None open. ARCHITECT's ruling *"Memo, events as now"* closes the design.
- Fact: the memo keeps one entry for each file text that the scope parses. The scope ends with its last holder (`jam_SharedInstance.h:97-101`).
