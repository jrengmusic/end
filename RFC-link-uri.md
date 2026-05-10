# RFC — Link URI: Native Terminal File Browser

Date: 2026-05-10
Status: Ready for COUNSELOR handoff

## Problem Statement

END's hyperlink system is half-implemented. The infrastructure exists — `LinkDetector`, `LinkSpan`, `LinkManager`, hit-test, dispatch, hint mode, keyboard/mouse routing — but all three scan methods are stubbed (`TODO Step 7: migrate to Screen`). The 16.5 MB `linkUriTable[65536]` is dead weight removed by the State refactor (RFC-state-refactor.md). OSC 8 registration path writes URIs that nobody reads.

The goal: implement link scanning so that every terminal output becomes interactive. URLs open in browsers. Files open in editors or preview. Directories navigate — `ls` in END becomes a native simplified finder/explorer. The infrastructure is built; the scan is the missing piece.

## Research Summary

### Existing Infrastructure (Complete, Working)

**LinkDetector** (`Source/terminal/selection/LinkDetector.h`) — stateless classifier. `classify(token)` returns `url` (pattern match: `https://`, `http://`, `ftp://`) or `file` (extension match: built-in set + config-driven via `lua::Engine::getContext()->isClickableExtension(ext)`). Header-only.

**LinkSpan** (`Source/terminal/selection/LinkSpan.h`) — data type: `row`, `col`, `labelCol`, `length`, `uri` (`juce::String`), `type` (`LinkDetector::LinkType`), `hintLabel[2]`. Holds everything needed for hit-test, rendering, and dispatch.

**LinkManager** (`Source/terminal/selection/LinkManager.h/cpp`) — owns `clickableLinks` and `hintLinks` vectors. Hit-test (`hitTest(row, col)`, `hitTestHint(char)`), dispatch, pagination (`buildPages`, `advanceHintPage`), hint clearing. All MESSAGE thread. All working.

**Dispatch** (`LinkManager.cpp:142`) — URL → `juce::URL::launchInDefaultBrowser()`. File → handler lookup: `"whelmed"` → markdown callback, `"image"` → image preview callback, else → build editor command and write to PTY. Complete.

**Mouse routing** (`Mouse.cpp:66`) — `handleDown()` calls `hitTest`, dispatches on match, sets cursor on hover. Complete.

**Keyboard hint mode** (`Input.cpp:278`) — openFile modal: Escape exits, Space advances page, a-z triggers `hitTestHint` + dispatch. Complete.

**Hint rendering** — `Screen::buildSnapshot()` reads `LinkManager::getActiveHintsData()` for overlay labels. ColourIds defined. Complete.

### Previous Working Implementation (commit `036d4d7`)

**scanHeuristicTokens:**
- Iterates visible rows, reads cells from Grid
- Skips codepoints ≤ 0x20, accumulates token string until whitespace
- `LinkDetector::classify(token)` → url or file
- File links gated by OSC 133 output block (between C and D markers)
- URL links allowed anywhere (no output block gate)
- File paths resolved: absolute passthrough, relative resolved against cwd
- URI stored as `"file://" + fullPath` for files, raw URI for URLs

**scanCellNativeLinks:**
- Same row iteration, fetches cell + linkId sidecar per row
- Groups consecutive cells with same non-zero linkId into spans
- URI resolved via `state.getLinkUri(id)`
- Same output block gate for non-URL OSC 8 links

**assignHintLabels:**
- Walks each span's cell characters, finds first unused lowercase alpha
- Stores in `span.hintLabel[0]`, records `span.labelCol`
- Collision tracking via `std::unordered_set<char>`

**scanViewport:**
- Computes visible base from scrollback offset
- Calls `scanHeuristicTokens` then `scanCellNativeLinks` sequentially
- Both append to same `spans` vector

**Scan trigger:**
- `valueTreePropertyChanged` on output block bottom node — fires once when command completes (OSC 133 D)
- Only runs when `state.hasOutputBlock()` is true
- `scanForHints()` triggered externally on hint mode entry — full viewport scan

### What Changed (Why Scan Is Stubbed)

Screen was refactored from custom renderer to `juce::Component`. Grid access patterns changed. Scan methods lost their Grid accessor compatibility. The scan logic itself is correct — it just needs to read cells through Screen's current API instead of direct Grid accessors.

## Principles and Rationale

### Two Link Sources, Two Paths

```
OSC 8 (READER thread):
  Parser → Video → events → Processor → State (Atom<const char*> + TextBuffer)
    → flush → ValueTree → Screen reads

Heuristic (MESSAGE thread):
  Screen scans visible cells → LinkDetector::classify()
    → juce::File resolution → LinkSpan → LinkManager
```

OSC 8 crosses threads — requires State atomic transport. Heuristic scan is born and consumed on MESSAGE thread — no State involvement, no atomic, no TextBuffer. `LinkSpan` holds `juce::String uri` directly.

Both paths converge at LinkManager for hit-test, hints, and dispatch.

### BLESSED Compliance

**B (Bound):** Screen owns LinkDetector and LinkManager as widget utilities. Same level as ScreenSelection. Clear ownership.

**L (Lean):** No new classes needed for scan. Three existing stub methods filled in. `LinkDetector::classify()` gains one enum value (`directory`). `dispatch()` gains one action (cd + ls).

**E (Explicit):** Output block gate is explicit — files only within OSC 133 C-D markers. URLs always. Directory click action config-driven (`links.list_directory`). Path resolution via `juce::File(cwd).getChildFile(token)` — no manual path separator handling.

**S (SSOT):** LinkManager is the SSOT for active links. State is SSOT for OSC 8 URIs (cross-thread). cwd from State ValueTree (post-flush).

**S (Stateless):** LinkDetector is stateless (pure classifier). Screen scans — doesn't remember previous scans. LinkManager stores results but is rebuilt on each scan.

**E (Encapsulation):** Screen owns the scan concern. LinkManager owns the interaction concern. LinkDetector is a pure utility. No layer violations. Parser/Video know nothing about links — OSC 8 flows through events.

**D (Deterministic):** Same cells + same cwd + same output block = same links. Always.

## Scaffold

### Ownership

```
Screen owns:
  LinkDetector    — classifies tokens (URL, file, directory)
  LinkManager     — interaction (hit-test, dispatch, hints, pagination)
  ScreenSelection — selection anchor/end, contains() hit-test
```

All MESSAGE thread. All widget utilities serving Screen.

### LinkDetector Changes

```cpp
// Current
enum class LinkType { none, file, url };

// New
enum class LinkType { none, file, directory, url };

// New method
static LinkType classifyPath (const juce::File& resolved) noexcept
{
    if (resolved.isDirectory())
        return LinkType::directory;

    if (resolved.existsAsFile())
        return LinkType::file;

    return LinkType::none;
}
```

`classify(token)` remains for URL pattern matching. `classifyPath(resolved)` added for filesystem-resolved tokens. Two-step: pattern first (no I/O), filesystem second (only within output block).

### LinkManager::dispatch() — Directory Action

```cpp
// In dispatch(), after url and file cases:
if (span.type == LinkDetector::LinkType::directory)
{
    const juce::String path { span.uri.fromFirstOccurrenceOf ("file://", false, false) };

    writeToPty ("cd " + path + "\n");

    if (lua::Engine::getContext()->nexus.links.listDirectory)
        writeToPty ("ls\n");
}
```

Two separate writes to PTY. Shell-agnostic — no `&&`, no `;`. Every shell processes newline-terminated commands independently.

### Config

```lua
-- end.lua
nexus = {
    links = {
        list_directory = true,  -- cd + ls on directory click (default true)
    }
}
```

Parsed by `lua::Engine` into `nexus.links.listDirectory` bool.

### Scan Implementation (Filling the Stubs)

**scanHeuristicTokens** — same algorithm as commit `036d4d7`:
1. Iterate visible rows via Screen's cell access API
2. Skip codepoints ≤ 0x20, accumulate token until whitespace
3. `LinkDetector::classify(token)` for URL pattern match → `LinkType::url` (no output block gate)
4. If not URL and within output block: resolve `juce::File(cwd).getChildFile(token)` → `classifyPath()` → `file` or `directory`
5. Store `"file://" + resolvedPath` as URI in LinkSpan

**scanCellNativeLinks** — OSC 8 links from State ValueTree:
1. Iterate visible rows
2. Read link properties from State ValueTree (OSC 8 URIs flushed from `Atom<const char*>`)
3. Group consecutive linked cells into spans
4. Same output block gate for non-URL OSC 8 links

**assignHintLabels** — same algorithm as commit `036d4d7`:
1. Walk each span's cell characters
2. Find first unused lowercase alpha
3. Store in `hintLabel[0]`, record `labelCol`
4. `std::unordered_set<char>` for collision tracking

**scanViewport** — unchanged entry point:
1. Compute visible base
2. Call `scanHeuristicTokens` then `scanCellNativeLinks`
3. Both append to same `spans` vector

### Scan Trigger

Same unidirectional data flow as everything else:

```
READER: Parser processes OSC 133 D → sets outputBlockBottom on State (atomic)
MESSAGE: State flush → ValueTree property change
         → LinkManager::valueTreePropertyChanged fires
         → scan()
```

One scan per command completion. Not per frame. Negligible cost.

`scanForHints()` triggered on hint mode entry — full viewport scan regardless of output block.

### OSC 8 in New State Architecture

With the State refactor (RFC-state-refactor.md):

- `linkUriTable[65536]` eliminated
- OSC 8 URIs stored as dynamic `Atom<const char*>` entries in State's AnyMap
- `TextBuffer` provides double-buffered slots allocated on demand per `registerLinkUri()` call
- `registerLinkUri()` adds a TextBuffer slot + AnyMap entry, returns linkId
- Flush writes URI string to ValueTree LINK child node
- Screen reads LINK children from ValueTree for `scanCellNativeLinks`

Dynamic allocation on MESSAGE thread (flush creates TextBuffer slot if absent). READER thread writes to existing slots only — zero allocation on hot path.

### Data Flow Summary

```
OSC 8:
  READER: Parser → Video::handleOsc8() → events → Processor
    → State::registerLinkUri() → TextBuffer slot + Atom<const char*>
    → Video::setActiveLinkId() → cells stamped with linkId
  MESSAGE: State flush → ValueTree LINK children
    → Screen::scanCellNativeLinks() reads LINK nodes
    → LinkSpan → LinkManager

Heuristic:
  MESSAGE: OSC 133 D → State flush → valueTreePropertyChanged
    → Screen::scan() → scanHeuristicTokens()
      → iterate visible cells → accumulate tokens
      → URL pattern → LinkType::url
      → juce::File(cwd).getChildFile(token)
        → .isDirectory() → LinkType::directory
        → .existsAsFile() → LinkType::file
      → LinkSpan → LinkManager

Interaction (both paths):
  Mouse: click → LinkManager::hitTest() → dispatch()
  Keyboard: openFile modal → hint labels → hitTestHint() → dispatch()

Dispatch:
  url       → juce::URL::launchInDefaultBrowser()
  file      → handler: whelmed / image / editor command to PTY
  directory → "cd <path>\n" + optionally "ls\n" to PTY
```

## BLESSED Compliance Checklist

- [x] Bounds — Screen owns LinkDetector and LinkManager. LinkSpan lifetime scoped to scan cycle. TextBuffer slots owned by Session. Clear ownership chain.
- [x] Lean — Three stub methods filled in. One enum value added. One dispatch case added. One config key added. No new classes.
- [x] Explicit — Output block gate explicit. URL vs file vs directory classification explicit. Config-driven directory action. `juce::File` path resolution — no manual path handling.
- [x] SSOT — LinkManager is SSOT for active links. State ValueTree is SSOT for OSC 8 URIs. cwd from State ValueTree.
- [x] Stateless — LinkDetector is pure. Scan rebuilds links from scratch. No incremental state.
- [x] Encapsulation — Screen scans, LinkManager interacts, LinkDetector classifies. Parser/Video know nothing about links. OSC 8 flows through events.
- [x] Deterministic — Same cells + same cwd + same output block = same links.

## Open Questions

1. **Cell linkId sidecar** — the old `scanCellNativeLinks` read `uint16_t` linkId per cell via Grid's `HeapBlock<uint16_t> linkIds` parallel array. With the Cell/Grid refactors, does `jam::Cell` carry a linkId field, or does the sidecar need to be re-added to Grid? This determines whether OSC 8 links can be cell-stamped.

2. **Scrollback links** — the old scan only covered visible rows. Should heuristic scan also cover scrollback visible in the viewport (when user scrolls up)? The output block gate won't apply to old output, but URLs should still be clickable in scrollback.

3. **Path quoting** — `cd <path>\n` breaks on paths with spaces. Should dispatch quote the path (`cd "<path>"\n`)? Quoting syntax varies by shell (`"` for bash/zsh, `'` for fish edge cases). Or use `cd $'<escaped>'\n` which is POSIX?

4. **Extension config surface** — `isClickableExtension()` is already config-driven. Should `isDirectory` detection also be toggleable (some users may not want every directory token to become a link)?

## Handoff Notes

- This RFC depends on RFC-state-refactor.md for OSC 8 URI storage (dynamic `Atom<const char*>` + TextBuffer replacing `linkUriTable[65536]`).
- This RFC depends on PLAN-video-terminal.md for the OSC 8 event routing (Video → events → Processor → State).
- This RFC depends on RFC-text-editor.md for Screen's cell access API (scan reads cells through Screen, not direct Grid accessors).
- The scan implementation is a direct port of commit `036d4d7` logic, adapted to Screen's cell access API. No new algorithms.
- `LinkDetector`, `LinkSpan`, `LinkManager`, dispatch, hit-test, hint mode, mouse/keyboard routing — all complete and working. Only the three scan methods need implementation.
- The "finder" experience emerges from universal scanning + directory classification + cd dispatch. No special `ls` detection. Works for `eza`, `find`, `tree`, `git status`, `rg`, any command that outputs paths.
- Directory navigation sends simple newline-terminated commands to PTY. Shell-agnostic. No `&&`, no shell-specific syntax.
