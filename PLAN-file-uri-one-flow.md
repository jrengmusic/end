# PLAN: file-uri-one-flow

**RFC:** none — objective from ARCHITECT prompt: *"use juce API to its fullest extent, fix any terminal command that use it crossplatform, SSOT"*; *"UNIDIRECTIONAL, ONE FLOW"*
**Date:** 2026-11-12
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM; zsh, bash, PowerShell shell integration scripts

## Overview
A `file://` URI has one form, JUCE's form `file:///<absolute path>`, written by every producer and read by one reader, `juce::URL::getLocalFile()`. The hand-written reader (`getUriPath` + percent-decode) is deleted. This fixes the markdown-link crash (`file:///D%3A/...` kept its slash, `jam_TerminalProcessor.cpp:1179`) on both platforms.

## Language / Framework Constraints
- The terminal processor callback runs on EVE's clock thread (`EVETerminalProcessor::audioDeviceStopped`, `EVETerminalProcessor.cpp:412`). MANIFESTO Deadline threads: no allocation there. So the processor stores the OSC 7 URI bytes as received. `juce::URL` is built only on the message thread, at the consumer.
- ARCHITECT ruling (eve/carol/SPRINT-LOG.md:43): *"`native` implementation always at the very first INPUT to read and very last OUTPUT"*; *"whatever juce implemented as `native` dont fight it"*.

## Dependency & API Inventory
- JUCE writer: `URL::URL (File)` → `file:///` + escaped segments (`juce_URL.cpp:157-189`). Used at `jam_AnsiDocument.cpp:328`.
- JUCE reader: `URL::getLocalFile()` (`juce_URL.h:139`, `juce_URL.cpp:399-442`). The first segment after `file:///` is the domain (`findStartOfNetLocation`, `juce_URL.cpp:284-292`): `D:` on Windows, `Users` on macOS. A URI with a host (`file://HOST/...`) gives a UNC path on Windows (`:426`, `:437-438`) and `/HOST/...` on macOS (`:428`). So no producer writes a host.
- JUCE `URL::isLocalFile()` (`juce_URL.cpp:394-397`), `URL::getFileName()` (`:404-407`), `URL::removeEscapeChars` (`juce_URL.h:677`, public static).
- JUCE splits `#` (anchor) and `?` (parameters) out of the URL text (`juce_URL.cpp:193-200`, `:201`). So the shell encoders escape `#` and `?`.
- Producers of `file://` URIs: `zsh_integration.zsh:15`, `bash_integration.bash:44`, `powershell_integration.ps1:15` (OSC 7), `jam_AnsiDocument.cpp:328` (EVE links, JUCE writer).
- Encoders: `eve/Source/shell/eve:4-15` (`_eve_encode`, bash), `zsh_integration.zsh:27-38`, `powershell_integration.ps1:37-40`.
- Readers today: OSC 7 lambda `jam_TerminalProcessor.cpp:1219-1225` (getUriPath + decode); `EVEView::getUrlFile` `EVEView.cpp:579-593`; `cwd` consumers `jam_TextEditor.cpp:1040`, `EVETerminalProcessor.cpp:440-441`, `end/Source/ui/SessionView.cpp:133`. Pass-through only (no change): `EditorView.cpp:19`, `:27`, `ENDView.cpp:374`, `TabView.cpp:26`, `EVEProcessor.cpp:191`, `jam_TextEditor.cpp:933-937`.
- `getUriPath` callers: `jam_TerminalProcessor.cpp:1221`, `EVEView.cpp:586` (text search; LSP returns no call sites for it). `applyToDecodedText` stays: callers `:1266`, `:1295`, `:1305`.

## Validation Gate
Each step is validated by COUNSELOR before proceeding to the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering the whole sprint — never per step.

## Steps

### Step 0: Plan file
**Scope:** `end/PLAN-file-uri-one-flow.md`
**Action:** Copy this plan to the project root.
**Validation:** File content equals this plan.

### Step 1: Shell producers write JUCE's form
**Scope:** `eve/Source/shell/zsh_integration.zsh`, `bash_integration.bash`, `powershell_integration.ps1`, `eve`
**Action:**
- OSC 7 writes `file://` followed by the absolute path that starts with `/`, with no host: zsh `printf '\033]7;file://%s\007...' "$(_eve_encode "$(_eve_cwd)")"`; bash the same; PowerShell `file:///$(_eve_encode ($PWD.Path -replace '\\','/'))`.
- Each `_eve_encode` also escapes `#` and `?` (zsh and bash `case` arm `'%'|';'|'#'|'?'|[[:cntrl:]]`; PowerShell class `[%;#?\x00-\x1F\x7F]`).
**Validation:** Each OSC 7 line has no host argument. MSYS form `/D:/...` (`_eve_cwd`, `cygpath -m`) and macOS `$PWD` both give `file:///...`. Encoders escape exactly `% ; # ?` and control bytes.

### Step 2: jam processor stores the URI; getUriPath deleted
**Scope:** `jam/jam_terminal/processor/jam_TerminalProcessor.{h,cpp}`
**Action:**
- OSC 7 lambda (`.cpp:1219-1225`): `events.get (Id::cwd, body.data(), static_cast<int> (body.size()));` — the URI as received, no decode.
- Delete `getUriPath` declaration and doc block (`.h:129` and its block) and definition (`.cpp:1170-1184`).
**Validation:** No allocation added on the processor thread. `getUriPath` has no remaining reference (text search over jam, eve/Source, end/Source). `applyToDecodedText` unchanged.

### Step 3: Message-thread readers use JUCE
**Scope:** `jam/jam_gui/widgets/jam_TextEditor.cpp`, `eve/Source/EVEView.cpp`, `eve/Source/EVETerminalProcessor.cpp`, `end/Source/ui/SessionView.cpp`
**Action:**
- `jam_TextEditor.cpp:1040`: `return juce::URL { row.isValid() ? row.getProperty (Id::value).toString() : terminal.getProperty (Id::cwd).toString() }.getLocalFile();`
- `EVEView::getUrlFile` (`EVEView.cpp:579-593`): body becomes `return juce::URL { getTerminalState().getProperty (Id::url).toString() }.getLocalFile();`
- `EVETerminalProcessor.cpp:440-441`: `const juce::URL cwd { state.state.getProperty (Id::cwd).toString() };` and `const auto startDirectory { cwd.isLocalFile() ? cwd.getLocalFile() : ShellIntegration::getStartDirectory() };`
- `SessionView.cpp:133`: `const juce::String directoryName { juce::URL::removeEscapeChars (juce::URL { pluginState.getProperty (Id::cwd).toString() }.getFileName()) };`
**Validation:** No hand-written URI or path parse remains. No new name. No guard added. Every `juce::File` built from `cwd` or `url` comes from `getLocalFile()`.

## BLESSED Alignment
- **S (SSOT):** one URI form, one reader (`juce::URL::getLocalFile`). The second reader (`getUriPath`) is deleted.
- **E (Encapsulation), one flow:** shell → OSC 7 URI → processor stores bytes → Model `cwd` → message-thread consumer materialises `juce::File` through JUCE. No transform in transit.
- **B / Deadline threads:** the clock thread does no new work.
- **E (Explicit):** no new names, no guards, no magic values.

## Risks / Open Questions
- None.

## Verification
1. ARCHITECT builds EVE and END.
2. Windows, zsh: `ls` in `eve/test`, click `markdown.md`. The markdown opens. No assert at `juce_File.cpp:162`.
3. New pane and pane title show the shell's folder name.
4. macOS: the same steps.
