# PLAN: SKiT Full Implementation

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-05-02
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE

## Overview

Wire all three SKiT protocols (Sixel, Kitty, iTerm2) through the existing decoders to Overlay. Reposition Overlay from side-by-side split to cell-coordinate overlay on top of Screen. Advertise capabilities so third-party programs auto-detect. Remove `nexus.image.protocol` config — detection is automatic, shell integration uses a single fixed envelope.

## Two Overlay Modes

1. **Native** (`END;filepath`) — user config nexus drives appearance. Border per config. Sizing from enriched protocol bounds or config fallback.
2. **SKiT conform** (`onImageDecoded`) — Overlay matches protocol-specified cell position and dimensions. No border. Input-transparent.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Overlay reposition — remove side-by-side split
**Scope:** `TerminalDisplay.cpp` resized(), `TerminalDisplay.h`
**Action:** In `resized()`, stop calling `contentArea.removeFromRight()`. Screen always gets full content area. Overlay positioned at cell coordinates on top of Screen via `setBounds`. Two positioning paths:
- SKiT conform: gridCol × cellWidth, gridRow × cellHeight, cellCols × cellWidth, cellRows × cellHeight
- Native: trigger row × cellHeight, width/height from config fractions (existing)
Add `overlayTriggerCol` member. Add `overlayConform` bool to distinguish modes.
**Validation:** Screen occupies full content area regardless of overlay state. Overlay paints on top.

### Step 2: Overlay input transparency
**Scope:** `TerminalDisplayPreview.cpp` activatePreview()
**Action:** Call `overlay->setInterceptsMouseClicks(false, false)` after creation. All keyboard and mouse events pass through to terminal beneath.
**Validation:** With overlay active, keyboard input reaches PTY. fzf can be dismissed with Esc.

### Step 3: SKiT conform — no border
**Scope:** `TerminalDisplayPreview.cpp` activatePreview()
**Action:** When `overlayConform == true`: `setShowBorder(false)`, `setPadding(0)`. When false: use config as before.
**Validation:** SKiT-sourced overlay has no border/padding. Native overlay has border per config.

### Step 4: Wire onImageDecoded → Overlay
**Scope:** `TerminalDisplay.cpp` initialise(), `TerminalDisplayPreview.cpp`
**Action:** In `initialise()`, wire `processor.getParser().onImageDecoded` callback. The callback receives: `(pixels, delays, frameCount, widthPx, heightPx, gridRow, gridCol, cellCols, cellRows, isPreview)`. READER thread fires directly — use `juce::MessageManager::callAsync` with moved HeapBlocks (same ownership-transfer pattern as 6835980's `ImageAtlas::submitDecoded`, no SpinLock, no FIFO). MESSAGE thread lambda:
- Convert RGBA8 pixels to `vector<juce::Image>` via `imageFromRGBA` (already exists)
- Store gridCol, gridRow, cellCols, cellRows for positioning
- Call activatePreview in SKiT conform mode
**Validation:** Decoded SKiT image appears at protocol-specified cell position. No border.

### Step 5: Enrich END; native protocol with bounds
**Scope:** `ParserDCS.cpp`, `ParserOSCExt.cpp`, `Parser.h`, shell integration scripts
**Action:** Extend `END;` payload format: `END;filepath[;cols;lines]` (optional bounds). When present, cols/lines specify overlay cell dimensions. Shell integration passes `$FZF_PREVIEW_COLUMNS` and `$FZF_PREVIEW_LINES` when available. `handleSkitFilepath` parses optional bounds. `onPreviewFile` signature adds optional cols/lines (default 0 = use config).
**Validation:** fzf preview script emits bounds. Overlay covers fzf's preview pane exactly.

### Step 6: Update shell integration — single envelope
**Scope:** `zsh_end_integration.zsh`, `bash_integration.bash`, `end-shell-integration.fish`, `powershell_integration.ps1`
**Action:** Replace `case "$END_SKIT"` dispatch with single `printf '\033]1337;END;%s;%s;%s\a' "$file" "${FZF_PREVIEW_COLUMNS:-0}" "${FZF_PREVIEW_LINES:-0}"`. OSC 1337 always — parser already handles it. Remove `$END_SKIT` dependency.
**Validation:** Shell integration works without `END_SKIT` env var. Single code path.

### Step 7: Remove nexus.image.protocol config
**Scope:** `default_nexus.lua`, `EngineParseConfig.cpp`, `Engine.h`, `Session.cpp`
**Action:** Remove `image.protocol` key from default_nexus.lua. Remove parsing in EngineParseConfig.cpp. Remove struct field. Remove `END_SKIT` env var seeding in Session.cpp.
**Validation:** No `protocol` reference in config pipeline. No `END_SKIT` in env.

### Step 8: Capability advertisement verification
**Scope:** `ParserCSI.cpp` (DA), `KittyDecoder.cpp` (query), `UnixTTY.cpp`/`WindowsTTY.cpp` (env)
**Action:** Verify existing:
- DA1 responds `\x1b[?62;4c` (4 = Sixel) — already done
- Kitty `_Gi=...a=q` query returns `_Gi=<id>;ok` — already done
- `TERM_PROGRAM=END` set — already done
If any missing, add. Document in ARCHITECTURE.md.
**Validation:** `timg`, `viu`, `ranger` detect END as capable terminal.

### Step 9: Verify dismiss lifecycle
**Scope:** `TerminalDisplay.cpp` onVBlank(), shell integration scripts
**Action:** Dismiss is shell-driven: `end preview ""` emits empty filepath → handleSkitFilepath("") → onPreviewFile("") → consumePendingPreview → dismissPreview(). Verify this path works for both native and SKiT conform modes. Also verify existing dismiss triggers: Escape key (Input.cpp), mouse click (Mouse.cpp), process exit (TerminalDisplay.cpp). No ED erase wiring needed.
**Validation:** fzf text preview switch dismisses overlay. fzf exit dismisses overlay. No stale overlay persists.

## BLESSED Alignment
- **B (Bound):** Overlay lifetime owned by Display. Created on demand, destroyed on dismiss. RAII via unique_ptr.
- **L (Lean):** No atlas, no FIFO, no staging. One juce::Image, one paint call per frame.
- **E (Explicit):** Two explicit modes (native/conform). No magic detection of CLI/TUI layout — bounds passed explicitly.
- **S (SSOT):** Cell→pixel conversion via jam::tui::Metrics. Config from lua::Engine reference.
- **S (Stateless):** Overlay is dumb — Display tells it what to paint, where to paint.
- **E (Encapsulation):** Overlay knows nothing about protocols. Display mediates.
- **D (Deterministic):** Same protocol input → same overlay position and image.

## Decisions (Locked)

1. **onImageDecoded thread safety** — `callAsync` with moved HeapBlocks. READER fires callback, lambda moves pixel ownership to MESSAGE thread. Same pattern as 6835980's `ImageAtlas::submitDecoded` — no SpinLock, no FIFO. Conversion to juce::Image happens on MESSAGE thread.
2. **Dismiss signal** — empty filepath via shell integration (`end preview ""`), not ED erase. ED 2/3 is unreliable for image dismissal. The shell script controls lifecycle: `end preview <file>` = show, `end preview ""` = dismiss. Already implemented in handleSkitFilepath → onPreviewFile.
3. **Single overlay** — no multiple simultaneous image case. `unique_ptr<Overlay>` is correct.
4. **Reference implementation** — commit 6835980 is the proven baseline for all SKiT wiring. No new patterns, no FIFO, no atlas.
