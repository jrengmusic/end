# SPRINT-LOG

## Sprint 14: Fallback Advance + Zoom + Non-Destructive Reflow + Daemon Lifecycle

**Date:** 2026-04-12 / 2026-04-13
**Duration:** ~20:00

### Agents Participated
- COUNSELOR: session lead, SSOT analysis, zoom architecture (multiple iterations), daemon lifecycle design, ODE intervention
- Pathfinder: zoom call-graph trace, windowsGlyphScale consumer mapping, NF init desync root cause, AppState zoom persistence check, calcMetrics cache analysis
- Researcher: fallback advance handling across 4 reference terminals (local source), zoom handling in reference terminals, tmux daemon lifecycle patterns
- Engineer: all code implementation (~30 delegations)
- Auditor: comprehensive audit of Sprints 10-14 (BLESSED, SSOT, stale docs, coding standard)

### Files Modified (30+ total)

**Fallback advance (Phase 3 + Phase 2):**
- `modules/jreng_graphics/fonts/jreng_typeface_shaping.cpp:306-320` — force `xAdvance = primaryFace->size->metrics.max_advance >> 6` for all fallback glyphs; doxygen updated
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.cpp:449-490` — oversize rescale when bitmap.width > effectiveWidth on scalable faces; FT_Set_Char_Size restore (not FT_Set_Pixel_Sizes); vertical centering via originalBitmapTop/Rows

**SSOT windowsGlyphScale:**
- `modules/jreng_graphics/fonts/jreng_typeface.h` — `computeRenderDpi` declaration, `getBaseFontSize` declaration
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:86-111` — `computeRenderDpi` definition, `windowsGlyphScale` constant
- `modules/jreng_graphics/fonts/jreng_typeface.cpp:300,511,942` — all `FT_Set_Char_Size` sites routed through `computeRenderDpi`; `windowsGlyphScale` applied to text faces only (slots 0-3), NF/emoji/fallback use full `baseDpi * displayScale`; `cachedMetricsSize` invalidated in `setSize`; 26.6 conversion unified to `roundFloatPxTo26_6`

**Typeface init fix:**
- `Source/MainComponent.cpp:57` — pass `dpiCorrectedFontSize()` to Typeface constructor instead of raw `config.getFloat(Key::fontSize)`

**Zoom (wezterm Mode A):**
- `Source/component/TerminalDisplay.cpp:700-740` — `applyZoom` reads SSOT (Config x AppState), computes ratio from `font.calcMetrics`, resizes window proportionally; font size from `getBaseFontSize` (pre-zoom) vs SSOT (post-zoom)
- `Source/component/TerminalDisplay.h:68-70,420-432` — updated `@par Zoom` and `applyZoom` doxygen
- `Source/component/Tabs.cpp:491-533` — `Config::zoomStep` constant, `resetZoom` uses `Config::zoomMin`
- `Source/component/Tabs.h:252-276` — updated zoom method doxygen
- `Source/config/Config.h:75` — `static constexpr float zoomStep { 0.25f }`
- `Source/config/Config.h` — removed `Config::Key::windowZoom`
- `Source/config/Config.cpp` — removed `addKey(Key::windowZoom, ...)`
- `Source/config/default_end.lua` — removed `zoom` entry from window table
- `Source/AppState.cpp:79,494` — `getWindowZoom` fallback and `initDefaults` use `Config::zoomMin`
- `Source/component/PaneComponent.h:74-79` — `virtual void applyZoom(float) noexcept = 0`
- `Source/whelmed/Component.h` — no-op `applyZoom` override

**Non-destructive reflow:**
- `Source/terminal/logic/GridReflow.cpp` — `reflowPass()` replaces duplicate `countOutputRows`/`writeReflowedContent` (SSOT); `effectiveLen = flatLen` unconditionally (no hard-line truncation); `maxOffset = flatLen - 1` (no cursor clamping); `pinToBottom` removed; `emitLogicalLine` dead code deleted; file-level doxygen updated

**Screen:**
- `Source/terminal/rendering/Screen.h` — `getBaseFontSize()` accessor
- `Source/terminal/rendering/Screen.cpp` — `getBaseFontSize()` implementation; `calc()` unchanged; `setFontSize` with `calc()` restored

**Daemon lifecycle:**
- `Source/interprocess/Daemon.cpp:134-143` — removed `onAllSessionsExited` from `removeConnection`; `stop()` no longer calls `releasePlatformProcessCleanup`
- `Source/interprocess/DaemonWindows.cpp` — `JOB_OBJECT_LIMIT_BREAKAWAY_OK` on daemon job; `CREATE_BREAKAWAY_FROM_JOB` on daemon spawn
- `Source/nexus/Nexus.cpp:168` — `attachedLink->sendRemove(uuid)` in `Nexus::remove`
- `Source/Main.cpp` — GUI Job Object with `KILL_ON_JOB_CLOSE + BREAKAWAY_OK` for OpenConsole cleanup

**Keyboard protocol rename:**
- `Source/terminal/data/Keyboard.h` — `disambiguate`, `allKeys`, `progressiveKeyCodes` (removed all external terminal references)
- `Source/terminal/data/State.h`, `State.cpp`, `Identifier.h`, `Parser.h`, `ParserCSI.cpp`, `ParserVT.cpp` — doxygen updated
- `ARCHITECTURE.md`, `README.md` — protocol references updated
- `carol/SPRINT-LOG.md` — historical references updated
- `gen/gen_char_props.py` — references updated

**Cleanup:**
- Deleted `PLAN-fix-zoom.md`, `test-fallback-advance.sh`
- Dead `Display::increaseZoom/decreaseZoom/resetZoom` removed
- All stale doxygen fixed per audit

### Alignment Check
- [x] BLESSED principles followed — D (Deterministic): non-destructive reflow, zoom roundtrip preserves content; S (SSOT): single `reflowPass`, zoom from AppState, `computeRenderDpi` for all DPI sites, `Config::zoomStep` constant, no shadow copies; E (Encapsulation): `windowsGlyphScale` text-only, zoom doesn't poke grid; L (Lean): dead code removed (Display zoom triad, emitLogicalLine, Config::Key::windowZoom)
- [x] NAMES.md adhered — `computeRenderDpi`, `getBaseFontSize`, `zoomStep`, `reflowPass`, `disambiguate`, `allKeys`, `progressiveKeyCodes` all follow naming rules; `zoomFactor`/`setZoomFactor` introduced and removed within same sprint
- [x] MANIFESTO.md principles applied — no early returns; alternative tokens; brace init; const where practical

### Problems Solved
- **Fallback advance:** Proportional system fallback fonts broke monospace grid. Forced cell-width advance matching all 4 reference terminal implementations. Oversize bitmaps rescaled via FT_Set_Char_Size (not FT_Set_Pixel_Sizes) with vertical centering.
- **Zoom determinism:** Zoom in+out now preserves grid content. Wezterm Mode A: window resizes to maintain cols/rows. No grid mutation from zoom. SIGWINCH fires with same dimensions — harmless.
- **Non-destructive reflow:** Hard lines wrap on shrink, unwrap on grow. Single `reflowPass` eliminates count/write SSOT violation. `pinToBottom` removed — cursor stays content-relative.
- **windowsGlyphScale text-only:** Optical weight correction (0.90) now applies only to regular text faces. NF icons, emoji, and fallback symbols render at full DPI.
- **Typeface init desync:** Constructor received raw font size (12pt) while Screen used DPI-corrected (9.6pt). Guard skip left NF face at wrong size until first zoom. Fixed by passing `dpiCorrectedFontSize()` to constructor.
- **calcMetrics cache stale:** `setSize()` didn't invalidate cache. Post-zoom `calcMetrics` returned stale `physCellW/H`. Fixed with `cachedMetricsSize = -1` in `setSize`.
- **26.6 SSOT:** `setSize` truncated, `calcMetrics` rounded. Unified to `roundFloatPxTo26_6` at all 3 sites.
- **Daemon lifecycle:** `removeConnection` no longer kills daemon on client disconnect. `Nexus::remove` sends `killSession` IPC. Session restoration preserved.
- **OpenConsole cleanup:** GUI Job Object with `KILL_ON_JOB_CLOSE` ensures ConPTY children die on GUI exit. `CREATE_BREAKAWAY_FROM_JOB` lets daemon outlive GUI.
- **Config::Key::windowZoom removed:** Zoom is AppState-only (SSOT). `resetZoom` = `Config::zoomMin` (1.0). No config/state conflation.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 13: Arrow Glyphs + Ligature Span Fix

**Date:** 2026-04-13
**Duration:** ~02:30

### Agents Participated
- COUNSELOR: session lead, rendering pipeline trace, root-cause analysis (oversize rescale + span=1 hardcode)
- Pathfinder: codebase survey (font pipeline, rendering dispatch)
- Engineer: build_fonts.py execution, build_monolithic.py execution, GSUB/HarfBuzz verification

### Files Modified (7 total)
- `___display___/sheet_config.py` — added ARROWS list (U+2190-U+2194)
- `___display___/generate_unified_sheets.py` — added arrow row calculation, guide/cell emission blocks, updated total_rows and print output
- `___display___/svg-mono/GlyphSheet_Display-MONO-Book.svg` — height 3622→3875, 5 arrow guide blocks, 5 arrow cell groups with artwork
- `___display___/svg-mono/GlyphSheet_Display-MONO-Medium.svg` — same as Book
- `___display___/svg-mono/GlyphSheet_Display-MONO-Bold.svg` — same as Book (rgb() style convention preserved)
- `Source/terminal/rendering/ScreenRender.cpp:962` — tryLigature: span 0 → `static_cast<uint8_t>(tryLen)` so oversize rescale uses correct multi-cell width
- `Source/terminal/rendering/ScreenRender.cpp:259-261` — emitShapedGlyphsToCache: route through span-aware `getGlyph` when `span > 1` even without active constraint
### Alignment Check
- [x] BLESSED principles followed — SSOT (sheet_config.py drives generator and manual edits identically), Deterministic (HarfBuzz verified: lig_002D_003E glyph_id=107 independent of uni2192 glyph_id=102), Encapsulation (span flows through existing getGlyph API, no new surface)
- [x] NAMES.md adhered — no new names introduced
- [x] MANIFESTO.md principles applied — no early returns, positive checks, alternative tokens

### Problems Solved
- **Arrow glyphs missing from Display Mono:** OS fallback arrows rendered ugly on Windows (proportional advance, wrong style). Added 5 arrow codepoints to GlyphSheets, drew artwork in all 3 weights, rebuilt Display Mono + Monolith.
- **Ligature oversize rescale bug:** `tryLigature` passed `span=0` to `emitShapedGlyphsToCache`, which called `font.getGlyph(glyphIndex)` (single-arg overload hardcoding `span=1`). Oversize rescale in `rasterizeGlyph` computed `effectiveWidth = cellWidth * 1`, shrinking 2-cell ligature bitmaps to 1 cell. Previously invisible because OS-fallback arrows looked different; exposed when Display Mono arrows matched the rescaled ligature. Fix: pass `tryLen` as span, route through span-aware `getGlyph` overload.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 12: Daemon Lifecycle — Clean Start, Clean Kill, tmux Alignment

**Date:** 2026-04-12
**Duration:** ~02:00

### Agents Participated
- COUNSELOR: session lead, PLAN authorship, 5-step delegation, bug root-cause analysis (killSession path missing quit trigger)
- Pathfinder: daemon/nexus infrastructure survey (2 invocations — lifecycle + detailed line-level)
- Engineer: all code implementation (Steps 1–5)
- Auditor: post-implementation validation (found orphaned App::ID::connected — resolved)

### Files Modified (8 total)
- `Source/interprocess/Message.h:56` — added `killDaemon = 0x18` PDU enum value
- `Source/interprocess/Daemon.h:123-133` — declared `killAll()` public method
- `Source/interprocess/Daemon.cpp:144-165` — implemented `killAll()`: snapshot session list, iterate `nexus.remove()`, fire `onAllSessionsExited`
- `Source/interprocess/Channel.cpp:108,219-236` — added `case Message::killDaemon` → `daemon.killAll()`; fixed `killSession` handler to broadcast sessions and fire `onAllSessionsExited` when last session removed
- `Source/Main.cpp:189-295,429-436,487-640` — added `clientLock` (InterProcessLock) member; restructured `initialise()` with kill/kill-all CLI dispatch; rewrote `resolveNexusInstance()` with lock-based claim; removed `setConnected(false)` from `systemRequestedQuit()`
- `Source/AppState.h:19,33-35,155-169` — removed `setConnected`/`isConnected` declarations and doc references
- `Source/AppState.cpp:154-167,390,410,449-450` — removed `setConnected`/`isConnected` implementations; removed `connected` property from `load()`/`save()` docs
- `Source/AppIdentifier.h:12,83` — removed `App::ID::connected` identifier and schema comment
- `Source/interprocess/Link.cpp:140-156` — removed `setConnected` calls from `connectionMade()`/`connectionLost()`

### Alignment Check
- [x] BLESSED principles followed — SSOT (InterProcessLock IS the connected truth, no shadow boolean), Bound (lock has RAII lifecycle, auto-releases on crash), Lean (net code reduction — removed connected flag, added minimal killAll/killDaemon), Stateless (removed manual `connected` boolean flag from AppState), Encapsulation (daemon handles its own shutdown via killAll), Deterministic (lock acquire is atomic OS operation, no TOCTOU)
- [x] NAMES.md adhered — `killDaemon` (PDU), `killAll` (method), `clientLock` (member), `KillConn` (function-local struct) all ARCHITECT-approved
- [x] MANIFESTO.md principles applied — no early returns, positive nesting, alternative tokens, brace init throughout

### Problems Solved
- **Crash-unsafe connected flag:** `.display` XML `connected=true` persisted after crash, blocking future clients from claiming daemon. Replaced with `juce::InterProcessLock` (named by UUID) — OS auto-releases on crash. `resolveNexusInstance()` now uses `enter(0)` for non-blocking lock-based claim.
- **No explicit daemon kill command:** Added `killDaemon` PDU + `Daemon::killAll()`. CLI: `end --nexus kill <uuid>` and `end --nexus kill-all` — ephemeral fire-and-forget connections.
- **ctrl+w on last pane leaves daemon alive:** `Channel::messageReceived` killSession handler only called `nexus.remove()` which checked `Nexus::onAllSessionsExited` (nullptr in daemon mode). Fixed by adding `daemon.broadcastSessions()` + `daemon.onAllSessionsExited` check after removal — mirrors the `wireOnExit` path.
- **Dead code:** Removed all `connected` flag infrastructure (AppState, AppIdentifier, Link, Main) — ~30 lines of dead code eliminated.

### Debts Paid
- None

### Debts Deferred
- None

---

## Sprint 10: Windows Font Rendering + Box Drawing AA + Daemon Cleanup

**Date:** 2026-04-11 / 2026-04-12
**Duration:** ~16:00

### Agents Participated
- COUNSELOR: session lead, planning, multi-wave delegation, role drift correction mid-session
- Pathfinder: codebase surveys (config key layout, daemon lifecycle, rounded corner primitive search, terminal vs popup vs DWM rounded corner disambiguation, box drawing helpers, fallback glyph sizing path)
- Engineer: all code implementation across the waves listed below
- Researcher: reference terminal / JUCE rounded rectangle prior art, fallback glyph scaling prior art

### Files Modified

**Config / font routing (Ask A — `font.desktop_scale`):**
- `Source/config/Config.h:200-202` — added `Config::Key::fontDesktopScale { "font.desktop_scale" }` with windows-only semantic doc
- `Source/config/Config.h:645-647` — declared `Config::dpiCorrectedFontSize() const noexcept`
- `Source/config/Config.cpp:104` — `addKey (Key::fontDesktopScale, "false", { T::string })`
- `Source/config/Config.cpp:985-1003` — implemented `Config::dpiCorrectedFontSize()`: returns `Key::fontSize`, on Windows divides by `jreng::Typeface::getDisplayScale()` when `fontDesktopScale == "false"`
- `Source/config/default_end.lua:88-93` — `desktop_scale = "%%font_desktop_scale%%"` entry under `font` table with docs
- `Source/component/TerminalDisplay.h:554-558` — deleted stub `dpiCorrectedFontSize`
- `Source/component/TerminalDisplay.cpp:700,726` — route through `Config::getContext()->dpiCorrectedFontSize()`
- `Source/MainComponent.cpp:320` — same
- `Source/MainComponentActions.cpp:391` — same
- `Source/component/Panes.cpp:57` — same
- `Source/terminal/rendering/Screen.cpp:40` — `baseFontSize` initialised via `Config::getContext()->dpiCorrectedFontSize()` (eliminates transient pre-applyScreenSettings mismatch)
- `Source/config/Config.cpp:698-760` — BLESSED cleanup of `Config::load` lambdas (removed 3 early returns inside `sol::table::for_each`, restructured as nested positive checks with `isTableVal`/`isSpecialGroup`/`isPadding` locals)
- `Source/config/Config.cpp:847-858` — BLESSED cleanup of `patchKey` (collapsed mutable `needsQuotes` flag into `const bool isNumberType`)

**Windows glyph scale (Ask B — `windowsGlyphScale`):**
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:86-88` — `static constexpr float windowsGlyphScale { 0.90f }` inside `namespace jreng` (JUCE_WINDOWS only)
- `modules/jreng_graphics/fonts/jreng_typeface_metrics.cpp:252-257` — `#if JUCE_WINDOWS` branch multiplies final render DPI by `windowsGlyphScale`; logical metrics pass untouched, so cell dimensions stay at the unscaled size while FreeType rasterises the glyph outline slightly smaller

**Daemon cleanup (DEBT-20260411T101344):**
- `Source/interprocess/Daemon.cpp:134-145` — after `connections.remove(index)`, if `connections.isEmpty() and onAllSessionsExited != nullptr`, fire the callback; reuses the existing shell-exit quit path

**Box drawing (DEBT-20260411T083315):**
- `modules/jreng_graphics/fonts/jreng_box_drawing.h:235-260` — `lightThickness(w, embolden)` doubles base thickness when embolden is true; `heavyThickness` delegates through (effective ×4 under embolden, correct — embolden is orthogonal to light/heavy weight)
- `modules/jreng_graphics/fonts/jreng_box_drawing.h` — threaded `bool embolden` through `BoxDrawing::rasterize`, `drawLines`, `drawDashedLine`, `drawDiagonal`, `drawDoubleLines`, `drawRoundedCorner`; block elements and braille unchanged (they don't use thickness)
- `modules/jreng_graphics/fonts/jreng_box_drawing.h:680-749` — `drawRoundedCorner` replaced with 4× supersampled SDF + 4×4 box-filter downsample. `adjustedHxSs` / `adjustedHySs` derived from `drawLines`' actual line-range centres (`cxNative - lt/2 + lt * 0.5f`, times `supersampleFactor`) so the arc's attachment pole tangents the straight stroke at the same pixel column. `aaSs = supersampleFactor * 0.5f` widens the AA band in SS space. `juce::HeapBlock<uint8_t>` scratch buffer at `4w × 4h`, unconditional max-blend into `buf` on downsample.
- `modules/jreng_graphics/fonts/jreng_glyph_atlas.cpp:232` — `Atlas::getOrRasterizeBoxDrawing` passes `embolden` member to `BoxDrawing::rasterize`

### Alignment Check
- [x] BLESSED principles followed — SSOT (one `dpiCorrectedFontSize` source in Config, referenced by 5 call sites; single `windowsGlyphScale` constexpr; single `lightThickness`/`heavyThickness` source with embolden parameter), Lean (one method per concept, no speculative helpers), Explicit (no magic numbers — `windowsGlyphScale`, `supersampleFactor`, `renderDpiUnit` are named constexpr), Stateless (daemon quit gate is derived from connection list state, not a flag), Encapsulation (Box drawing, font metrics, and config are separate concerns touched through their public APIs), Bound (ScopedFaceSize experiment was reverted on failure — no lingering ownership concerns)
- [x] NAMES.md adhered — all new names ARCHITECT-approved: `Config::Key::fontDesktopScale`, `Config::dpiCorrectedFontSize`, `windowsGlyphScale`, `platformCellWidthCorrection` (superseded by `windowsGlyphScale`), `supersampleFactor`, embolden parameter on box drawing helpers
- [x] MANIFESTO.md principles applied — no early returns introduced; all new code uses positive nesting and alternative tokens; brace initialisation throughout; `const` where practical; no new helper types introduced at class scope; file-local `static` for utility symbols

### Problems Solved
- **`font.desktop_scale` (Windows-only):** Windows desktop scale slider previously ballooned the configured font size because JUCE's `getDisplayScale()` returns the OS scale. Users wanted 12pt to stay 12pt physical on any scale setting. Implemented upstream in `Config::dpiCorrectedFontSize` as an optional divide-by-scale when the key is `"false"`; default is `"false"` (persistent), user can opt into OS-scale-following by setting `"true"`. No-op on macOS/Linux.
- **`windowsGlyphScale`:** Display Mono visually felt heavier on Windows than macOS because FreeType's rasteriser at the full render DPI produced slightly wider strokes than CoreText at the equivalent size. Applied a downstream 0.90× scale to the final `FT_Set_Char_Size` render DPI only; cell metrics remain at unscaled size. Net effect: glyph bitmap is slightly smaller within the (unchanged) cell, matching macOS visual weight.
- **Embolden x2 for box drawing:** Embolden config flag previously only affected font glyphs via `FT_Outline_Embolden`. Now also doubles `lightThickness` / `heavyThickness` used by all procedural box-drawing helpers, so rounded corners and straight lines inherit the same weight adjustment as the surrounding font.
- **Rounded corner AA (DEBT-20260411T083315):** Original SDF at native resolution was visibly jaggy at small cell sizes. Applied 4× supersampling pattern: render SDF into a `4w × 4h` scratch buffer with `aa = supersampleFactor * 0.5f` (wider AA band in SS space), then box-filter down to native. Visual quality improved significantly per ARCHITECT's visual verification. Minor sub-pixel drift on even cell widths remains inherent to integer pixel grids.
- **Daemon cleanup on last close (DEBT-20260411T101344):** Daemon process previously outlived the GUI after the last terminal was closed. 1:1 daemon↔GUI lifecycle established by having `Daemon::removeConnection` trigger the existing `onAllSessionsExited` quit path when the connection list drains empty. `Main.cpp` wiring to `appState.deleteNexusFile(); quit()` unchanged.

### Failed Attempt (Not in Sprint Scope)
- **Fallback glyph advance width** (RFC-FALLBACK-ADVANCE.md): attempted to fix the "arrow too close to comma" visual bug where proportional system fallback fonts (Segoe UI Symbol) broke the monospace grid. Three iterations: (1) `xAdvance = cellAdvance` with centering xOffset — shifted further; (2) per-glyph FreeType rescale via `ScopedFaceSize` RAII, new `renderScale` field on `Glyph` and `Key` — all text rendered as dots (unconditional guard); (3) conditional guard + `shapeASCII`/`shapeHarfBuzz` renderScale init — artifacts remained, still shifted. Root cause of residual artifacts not diagnosed within the session's diagnostic budget. COUNSELOR's mental model of the shaping→rasterization interaction was incomplete; each fix iteration compounded the problem. **Full revert of all 8 touched files via `git show HEAD:<path>` readback, restoring shaping pipeline to HEAD state.** RFC-FALLBACK-ADVANCE.md stays open for a future sprint with deeper pre-investigation.

### Debts Paid
- `DEBT-20260411T101344` — daemon not killed on last terminal close; fixed via `Source/interprocess/Daemon.cpp:134-145` (`removeConnection` fires `onAllSessionsExited` on empty connection list)
- `DEBT-20260411T083315` — rounded rectangle misalignment; diminished via `modules/jreng_graphics/fonts/jreng_box_drawing.h:680-749` (4× supersample SDF, attachment-aware `adjustedHx/Hy`). Residual sub-pixel drift on even cell widths is mathematical, not algorithmic

### Debts Deferred
- None (no ARCHITECT-commanded mid-sprint deferrals)

---

## Sprint 9: Nexus Architectural Refactor + CWD Propagation Fix + Audit Cleanup

**Date:** 2026-04-11

### Agents Participated
- COUNSELOR: session lead, planning, 6-wave audit-driven fix sprint, delegation
- Pathfinder: codebase surveys (Nexus call sites, file layout, caller migration scope)
- Engineer: all code implementation (Waves 1–6 fix sprint, ~15 delegations)
- Auditor: comprehensive post-refactor audit (2 critical, 12 high, 5 medium, 4 low)
- MACHINIST: stale file deletion + .gitignore update (Wave 5)

### Files Modified (50+ total)

**Config:**
- `Source/config/Config.h` — `Key::nexus` → `Key::daemon`
- `Source/config/Config.cpp:146` — `addKey(Key::daemon, ...)`
- `Source/config/default_end.lua:59` — `daemon = "%%daemon%%"`

**Nexus (new class, replaces Nexus::Session):**
- `Source/nexus/Nexus.h` — NEW — session manager class (`jreng::Context<Nexus>`), `unordered_map<uuid, Terminal::Session>`, attach(Daemon&)/attach(Link&), three create overloads (with TTY / no TTY / routing)
- `Source/nexus/Nexus.cpp` — NEW — implementation. Client branch uses `Link::sendCreateSession` + factory (SSOT + Encapsulation fix)
- `Source/nexus/Session.h/cpp` — DELETED
- `Source/nexus/SessionFanout.cpp` — DELETED

**Interprocess (new namespace, IPC layer):**
- `Source/interprocess/Daemon.h/cpp` — moved from nexus/, namespace `Interprocess`, absorbs broadcast/subscriber registry from SessionFanout, `wireSessionCallbacks` split into `wireOnBytes`/`wireOnStateFlush`/`wireOnExit` helpers, `installPlatformProcessCleanup`/`releasePlatformProcessCleanup`
- `Source/interprocess/Daemon.mm` — moved, namespace `Interprocess`, added POSIX no-op platform cleanup
- `Source/interprocess/DaemonWindows.cpp` — NEW — extracted Windows platform helpers (hideDockIcon, spawnDaemon, Job Object setup) to parallel `Daemon.mm` (H9 resolution, Daemon.cpp back under 300 code lines)
- `Source/interprocess/Link.h/cpp` — moved from nexus/, namespace `Interprocess`, added typed `sendCreateSession(cwd, uuid, cols, rows)` method
- `Source/interprocess/Channel.h/cpp` — moved from nexus/, namespace `Interprocess`, constructor takes `Nexus&` (not `Nexus::Session&`)
- `Source/interprocess/EncoderDecoder.h/cpp` — moved and renamed from `Wire.h/cpp`
- `Source/interprocess/Message.h` — moved from nexus/

**Terminal layer:**
- `Source/terminal/logic/Session.h` — added `process`, `getStateInformation`, `setStateInformation` public API; `shouldTrackCwdFromOs` private member; deleted dead `onDrainComplete` public callback
- `Source/terminal/logic/Session.cpp` — two `create` factory overloads (with TTY / no TTY), `applyShellIntegration` sets `CHERE_INVOKING=1` unconditionally; onFlush gates getCwd on `shouldTrackCwdFromOs`
- `Source/terminal/tty/WindowsTTY.h` — added `getCwd` override declaration
- `Source/terminal/tty/WindowsTTY.cpp` — implemented `getCwd` via PEB query (NtQueryInformationProcess + ReadProcessMemory), split into 4 file-local static helpers (queryPebAddress, readProcessParametersAddress, readCurrentDirectoryUnicodeString, readAndConvertWidePath); Job Object setup moved out to DaemonWindows.cpp

**Application:**
- `Source/Main.cpp` — rewired ENDApplication: `unique_ptr<Nexus>`, `unique_ptr<Interprocess::Daemon>`, `unique_ptr<Interprocess::Link>`; daemon branch constructs+attaches; client branch constructs Link+attaches; standalone appends SESSIONS ValueTree; `shutdown()` orders teardown (link → daemon → mainWindow → nexus)
- `Source/AppState.h/cpp` — `isDaemonMode`/`setDaemonMode` (renamed from Nexus variant)
- `Source/AppIdentifier.h:79` — `App::ID::nexusMode` → `App::ID::daemonMode` (C++ identifier + XML string key)
- `Source/MainComponent.h/cpp` — include path update (`nexus/Nexus.h`)
- `Source/MainComponentActions.cpp` — `isDaemonMode` rename call sites
- `Source/component/Tabs.cpp` — `Nexus::getContext()->remove(uuid)` (2 sites), `isDaemonMode` renames (6 sites)
- `Source/component/Panes.cpp` — `Nexus::getContext()->create(...)` + `.getProcessor()` (2 sites)

**Diagnostics (temporary, removed before sprint close):**
- `Source/TeardownLog.h` — ADDED then DELETED (diagnostic logger for CWD trace + shutdown hang investigation)
- Temporary `TEARDOWN_LOG` calls in 7 files — added then removed

**Docs:**
- `ARCHITECTURE.md` — full rewrite: new module map with `interprocess/` tree, new Nexus+Interprocess section, layer separation diagram, communication contracts table, PDU kind table, data flow for all three configurations, Windows Job Object subsection
- `.gitignore` — added `# Diagnostic logs` section with `*.log` pattern
- `PLAN-nexus-refactor.md` — DELETED (all 7 steps executed)
- `teardown.log` — DELETED (30MB leftover)

### Alignment Check
- [x] BLESSED principles followed — Bound (clear ownership chain ENDApplication→Nexus→Terminal::Session), Lean (Nexus=pure container, Interprocess=IPC only, all files under 300 code lines, all functions under 30), Explicit (no mode flags, attach IS the mode; no early returns; no magic values), SSOT (one session map, one create factory with typed overloads, one Link::sendCreateSession method), Stateless (Nexus has attachment pointers but is dumb about IPC internals), Encapsulation (Terminal layer does not include Nexus or Interprocess headers; Nexus does not include Interprocess implementations), Deterministic (same create call produces same Session regardless of mode)
- [x] NAMES.md adhered — all new names ARCHITECT-approved: `Nexus`, `Interprocess::{Daemon,Link,Channel,EncoderDecoder,Message}`, `Terminal::Session::process/getStateInformation/setStateInformation`, `shouldTrackCwdFromOs`, `Link::sendCreateSession`, `Daemon::wireOnBytes/wireOnStateFlush/wireOnExit`, `Daemon::installPlatformProcessCleanup/releasePlatformProcessCleanup`, `App::ID::daemonMode`
- [x] MANIFESTO.md principles applied — zero stale `Nexus::Session` references, zero early returns, positive nesting, alternative tokens, brace init, `const` everywhere, `jassert` preconditions, no anonymous namespaces (static file-local where needed), no `namespace detail`

### Problems Solved

**Architectural rot:**
- `Nexus::Session` was a three-mode dispatcher (standalone/daemon/client) with null-check mode routing (`link != nullptr`, `daemon != nullptr`). Replaced with `Nexus` class owning sessions + `Interprocess::` transport layer; mode determined by `attach(Daemon&)` vs `attach(Link&)` vs nothing. Mode IS the attached object.
- `Terminal::Session` had two personalities (full with TTY, "remote" without TTY). Unified under single class with two `create` factory overloads.
- Nexus::Session was reaching into Terminal::Session::Processor internals for state sync. Session now exposes `process`, `getStateInformation`, `setStateInformation` — callers talk to Session, not Processor.

**CWD propagation (Explorer launch case):**
- Root cause: MSYS2 `/etc/post-install/05-home-dir.post` sources via `/etc/profile` when shell launched with `-l`; if `CHERE_INVOKING` unset AND `SHLVL<=1`, it executes `cd "$HOME"` BEFORE first prompt. END spawned shells at the intended cwd via `CreateProcessW lpCurrentDirectory`, but MSYS2 overrode it 60ms later during shell init. Vim-launched END inherited SHLVL≥2 from nested context, so the cd was skipped there — which is why "from vim works."
- Fix: `seedEnv.set("CHERE_INVOKING", "1")` unconditionally in `Terminal::Session::applyShellIntegration`. Cross-platform safe (harmless on macOS/Linux).
- Verified via diagnostic TEARDOWN_LOG sweep across the chain: Parser OSC 7 → State.setCwd → flushStrings → ValueTree → onStateFlush → stateUpdate PDU → Link handler → client ValueTree → pwdValue → getPwd → split. Chain was intact; the shell was just cd-ing away from the intended dir.

**OpenConsole.exe surviving daemon death:**
- Root cause: OpenConsole.exe is spawned internally by `conpty.dll::CreatePseudoConsole` — it's a grandchild of the daemon. If the daemon process is killed externally (taskkill, crash), destructors never run, `ClosePseudoConsole` never fires, OpenConsole.exe orphans.
- Fix: Windows Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` assigned to the daemon process in `installPlatformProcessCleanup()`. All children (including OpenConsole.exe grandchildren) killed by kernel when job handle closes — even on crash.

**BLESSED audit findings (15 total, all resolved):**
- C1 — Nexus.cpp client branch used raw `make_unique<Terminal::Session>` constructor instead of `Terminal::Session::create` factory (SSOT). Fixed by routing through factory.
- C2 — Nexus hand-rolling `createSession` PDU payload (Encapsulation). Added typed `Interprocess::Link::sendCreateSession` method.
- H1 — dead `Nexus::create(cols, rows, uuid)` overload. Resolved by C1/C2 fix which now uses it with extended signature.
- H2 — dead `Terminal::Session::onDrainComplete` public member. Removed.
- H3 — stale `Nexus::Session` doxygen references across 13+ files. Swept.
- H4 — ARCHITECTURE.md Nexus section described deleted class. Fully rewritten.
- H5 — stale `PLAN-nexus-refactor.md` at repo root. Deleted.
- H6 — `teardown.log` 30MB leftover. Deleted; `*.log` added to `.gitignore`.
- H7 — `Nexus::create(cwd, uuid, cols, rows)` > 30 lines. Shrunk to 23 via C1/C2 fix.
- H8 — `Daemon::wireSessionCallbacks` 78 lines. Split into 3 helpers.
- H9 — `Daemon.cpp` 340 code lines (>300). Extracted Windows platform code to `DaemonWindows.cpp` → 212 lines.
- H10 — `WindowsTTY::getCwd` 158 lines, 10-level nesting. Split into 4 file-local helpers, flat body.
- H11 — `App::ID::nexusMode` → `App::ID::daemonMode` (identifier + XML string key).
- H12 — 3 defensive `Nexus::getContext() != nullptr` guards in `Link.cpp` without named threat. Replaced with `jassert`.

### Debts Paid
- None (no formal DEBT.md entries existed at sprint start)

### Debts Deferred
- None explicitly pushed to DEBT.md this sprint. One pre-existing observation flagged during audit: `Source/config/Config.cpp:705,713,743` contain three early returns in the Lua loader root-iteration lambda (pre-Sprint 9 debt). Not fixed this sprint — out of scope.

## Sprint 8: BLESSED Cleanup — CWD SSOT, Config, Nexus Rename, Instance Isolation

**Date:** 2026-04-10

### Agents Participated
- COUNSELOR: session lead, planning, delegation, discussion
- Engineer: all code implementation (35+ delegations)
- Pathfinder: codebase exploration, git history, blast radius surveys
- Auditor: PLAN-nexus-cleanup validation, comprehensive final audit (22 findings, all resolved)
- Librarian: juce::var bool-vs-string behavior research

### Files Modified (35+ total)
- `modules/jreng_gui/glass/jreng_window.cpp` — middle-click drag only (removed DocumentWindow fall-through)
- `Source/nexus/Session.cpp` — openTerminal decomposed (dispatcher + openTerminalRemote + openTerminalLocal), inline PDU, remote Terminal::Session, manual encode replaced with writeString, session vocabulary
- `Source/nexus/Session.h` — openTerminal, removed dead forEachAttached, member renames (daemon/link), forward decls, session vocabulary
- `Source/nexus/SessionFanout.cpp` — broadcastSessions (was broadcastProcessorList), Channel refs, operator[] comment
- `Source/nexus/Daemon.h` (was Server.h) — class rename, folded hideDockIcon/spawnDaemon, removed lockfile wrappers
- `Source/nexus/Daemon.cpp` (was Server.cpp) — class rename, Windows spawnDaemon impl
- `Source/nexus/Daemon.mm` (new) — macOS hideDockIcon/spawnDaemon, merged macOS+Linux spawnDaemon (SSOT)
- `Source/nexus/Link.h` (was Client.h) — class rename, removed dead connectToHost/onPdu/handleUnknown, perProbeTimeoutMs constant
- `Source/nexus/Link.cpp` (was Client.cpp) — class rename, port from .nexus file, handleSessions/handleSessionKilled, removed dead fgConsumed, sendPdu delegates to encodePdu
- `Source/nexus/Channel.h` (was ServerConnection.h) — class rename, Daemon& ref, removed dead getId/id member
- `Source/nexus/Channel.cpp` (was ServerConnection.cpp) — class rename, broadcastSessions, minimal createSession PDU, sendPdu delegates to encodePdu
- `Source/nexus/Message.h` — createSession/killSession/detachSession/sessionKilled/sessions, removed dead shutdown enum
- `Source/nexus/Wire.h` — doc updates, encodePdu extracted (SSOT), removed redundant static from constexpr
- `Source/nexus/Wire.cpp` — encodePdu implementation
- `Source/nexus/NexusDaemon.h` — DELETED (folded into Daemon)
- `Source/nexus/NexusDaemon.mm` — DELETED
- `Source/nexus/NexusDaemon_win.cpp` — DELETED
- `Source/terminal/logic/Session.h` — Terminal::Session::create factory, applyShellIntegration static, remote constructor
- `Source/terminal/logic/Session.cpp` — create factory, applyShellIntegration impl, remote constructor, shellProgram write, onFlush guards
- `Source/terminal/tty/WindowsTTY.h` — getForegroundPid, getProcessName, childPid member
- `Source/terminal/tty/WindowsTTY.cpp` — getForegroundPid/getProcessName impl, spawnProcess pidOut
- `Source/config/Config.h` — Type enum (removed boolean), gpu key flattened
- `Source/config/Config.cpp` — boolean defaults as strings, getBool toString, validateAndStore, writeDefaults, patchKey
- `Source/config/WhelmedConfig.h` — Type enum unified, dead loader keys removed
- `Source/config/WhelmedConfig.cpp` — dead loader keys, scroll key names fixed, dead isBool branch
- `Source/config/default_end.lua` — gpu flattened, booleans quoted
- `Source/AppIdentifier.h` — SESSIONS/SESSION identifiers, removed instanceID
- `Source/AppState.h` — instanceUuid, connected, port, getNexusFile, getStateFile (.display), setConnected (no save), dtor trivial
- `Source/AppState.cpp` — split state files, save/load, setPort read-modify-write, dtor = default, setConnected property only
- `Source/Main.cpp` — startup scan extracted to resolveNexusInstance(), daemon UUID via CLI, systemRequestedQuit file decisions, shutdown order (nexus before mainWindow), spawnDaemon(uuid), stale comments fixed
- `Source/terminal/data/State.h` — "ServerConnection" → "Channel" in doxygen
- `ARCHITECTURE.md` — nexus/ module map added, state.xml → end.state/uuid.display
- `README.md` — state.lua → end.state (XML format)
- `PLAN-nexus-cleanup.md` — DELETED (completed)
- `Source/MainComponent.h` — removed onNexusConnected, sessionsNode member
- `Source/MainComponent.cpp` — inlined onNexusConnected into valueTreeChildAdded, implemented valueTreeChildRemoved (closeSession), SESSIONS/SESSION refs
- `Source/MainComponentActions.cpp` — save on tab mutation (nexus mode)
- `Source/component/Tabs.h` — closeSession declaration
- `Source/component/Tabs.cpp` — closeSession impl, save on mutation, removed file ops
- `Source/component/Panes.h` — doc updates
- `Source/component/Panes.cpp` — openTerminal call sites updated

### Alignment Check
- [x] BLESSED principles followed — SSOT for CWD (Terminal::State → AppState → new terminal), Bound (AppState created first destroyed last, daemon lifecycle explicit), Lean (removed wrappers, dead code, folded files), Explicit (names match reality), Encapsulation (AppState dumb storage, Main owns file I/O)
- [x] NAMES.md adhered — Daemon/Link/Channel/createSession/killSession/sessions approved by ARCHITECT
- [x] MANIFESTO.md principles applied — no early returns, positive checks, brace init, alternative tokens

### Problems Solved
- Window drag: left-click collision with text selection (Windows) — middle-click only
- Byte trails: duplicate DA/DSR response from client Processor — removed setHostWriter from display-only Processor
- CWD not propagating (Windows): shell integration env injection lost during Nexus arch shift — ported applyShellIntegration to Terminal::Session::create
- CWD overwrite: WindowsTTY::getCwd returning stale PEB data overwrote OSC 7 — removed getCwd, added onFlush guards
- CWD in nexus client: handleStateUpdate overwrote initial CWD with empty — added isNotEmpty guard
- CWD SSOT: Terminal::Session::create single entry point for all modes, daemon resolves shell/args from own config
- Config inconsistency: booleans as strings, gpu table flattened, Type enum unified
- Instance collision: per-UUID lockfile/state in nexus/ directory, startup scan with probe
- Session restoration: separated .nexus (daemon) and .display (client) ownership
- Stale .display cleanup: MainComponent owns .display lifecycle, valueTreeChildRemoved handles daemon-killed sessions
- shellProgram never written to State: fixed displayName always showing "zsh"

### Audit Sweep (22 findings, all resolved)
- Dead code: Message::shutdown, forEachAttached, Channel::getId/id, fgConsumed
- SSOT: sendPdu extracted to Wire::encodePdu, spawnDaemon merged macOS+Linux, manual encode replaced with writeString
- Stale docs: State.h, Main.cpp, MainComponent.cpp, ARCHITECTURE.md, README.md
- BLESSED L: openTerminal decomposed, initialise() scan extracted to resolveNexusInstance()
- Low: magic 200ms named, operator[] commented, redundant static removed, PLAN-nexus-cleanup.md deleted

### Technical Debt / Follow-up
- PLAN-IMAGE.md execution pending (9 steps for inline image rendering)
- WHELMED Mermaid still broken

## Sprint 7: BLESSED Audit — Production Quality

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Directed audit resolution, prioritized findings
- Auditor: Comprehensive 2-critical/5-violation/6-stale/4-improvement sweep across 30+ files
- Engineer: All fixes — early returns, SSOT extraction, doc updates, dead code removal

### Files Modified (15 total)
- `Source/terminal/logic/Processor.h` — setScrollOffsetClamped declaration
- `Source/terminal/logic/Processor.cpp` — setStateInformation early returns→positive nesting, setScrollOffsetClamped definition, removed Nexus::logLine from process(), removed Log.h include
- `Source/terminal/logic/Input.h/cpp` — deleted local setScrollOffsetClamped, routes to Processor
- `Source/component/TerminalDisplay.h/cpp` — deleted local setScrollOffsetClamped, routes to Processor
- `Source/component/Tabs.cpp` — addNewTab no-args delegates to parameterized overload
- `Source/nexus/Session.h` — deleted createClientSession declaration, added buildProcessorListPayload, stale docs fixed
- `Source/nexus/Session.cpp` — createClientSession inlined into create(), stale docs fixed
- `Source/nexus/SessionFanout.cpp` — broadcastProcessorList payload extracted to buildProcessorListPayload
- `Source/nexus/Client.cpp` — stale Loader doc references updated
- `Source/action/Action.cpp` — TODO removed
- `ARCHITECTURE.md` — filenames updated, Session description corrected, Grid snapshot section added, GlassWindow→Window
- `SPEC.md` — terminal state serialization marked Done
- `README.md` — roadmap updated, Nexus daemon mention added
- `PLAN-nexus.md` — deleted

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- 5 early returns in setStateInformation eliminated (BLESSED-E, JRENG)
- createClientSession deleted as separate method (dead pattern)
- setScrollOffsetClamped duplicated in Display + Input → single Processor method (SSOT)
- broadcastProcessorList duplicated payload → extracted helper (SSOT)
- addNewTab duplication → no-args delegates to parameterized (SSOT)
- Nexus::logLine in Processor::process hot path removed (Lean)
- 6 stale Loader doc references fixed
- ARCHITECTURE.md brought current with Sprint 4-6 architecture
- SPEC.md terminal state serialization marked implemented
- PLAN-nexus.md deleted (superseded)
- TODO in Action.cpp removed

### Technical Debt / Follow-up
- None deferred — all audit findings resolved

## Sprint 6: Terminal Owns Processor, Nexus Decoupled

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Architecture analysis, Nexus contamination discovery, staged execution direction
- Pathfinder: Grid/State internals, call site mapping, Nexus reference sweep
- Researcher: tmux reattach architecture (virtual grid redraw pattern)
- Librarian: JUCE ComponentBoundsConstrainer API
- Engineer: Serialization, Terminal::Session ownership, Nexus removal from Terminal layer, ServerConnection inline, popup fix

### Files Modified (25+ total)
- `Source/terminal/logic/Session.h` — owns Processor, getProcessor(), onStateFlush callback, uuid parameter
- `Source/terminal/logic/Session.cpp` — creates Processor in ctor, wires all 6 callbacks (onBytes, onDrainComplete, onFlush, setHostWriter, writeInput, onResize), stop() destroys Processor before TTY
- `Source/terminal/logic/Processor.h` — getStateInformation/setStateInformation, writeInput callback, onResize callback, deleted onLoadingStarted/onLoadingFinished
- `Source/terminal/logic/Processor.cpp` — serialization implementations
- `Source/terminal/logic/Grid.h/cpp` — getStateInformation/setStateInformation (dual buffer memcpy)
- `Source/terminal/logic/Input.cpp` — Nexus::Session→processor.writeInput (2 sites)
- `Source/terminal/logic/Mouse.cpp` — Nexus::Session→processor.writeInput (6 sites)
- `Source/component/TerminalDisplay.h/cpp` — removed Nexus::Session dependency, LoaderOverlay, titleBarHeight; sendInput→writeInput, sendResize→onResize
- `Source/component/Panes.h/cpp` — deleted buildTerminal/teardownTerminal/CreatedTerminal, direct Session::create/remove calls
- `Source/component/Tabs.cpp` — teardownTerminal→Session::remove, first-leaf sub-rect descent, cellsFromRect physical pixels, computeContentRect jassert
- `Source/component/Popup.h/cpp` — owns Terminal::Session directly, no Nexus, no buildTerminal
- `Source/MainComponent.h/cpp` — removed titleBarHeight from getContentRect, resize overlay via Window::isUserResizing
- `Source/MainComponentActions.cpp` — popup creates Terminal::Session directly
- `Source/nexus/Session.h/cpp` — single create() (no mode helpers), deleted processors map, deleted createLocalSession/createDaemonSession, client-side Processor wired with writeInput/onResize
- `Source/nexus/SessionFanout.cpp` — attach uses getStateInformation snapshot, terminalSessions lookups
- `Source/nexus/ServerConnection.h/cpp` — handle* methods inlined into messageReceived, unified createProcessor PDU
- `Source/nexus/Client.h/cpp` — createSession (renamed), deleted attachSession, handleStateUpdate
- `Source/nexus/Message.h` — createProcessor=0x10, stateUpdate=0x22, deleted 3 dead PDUs
- `Source/Main.cpp` — jreng::Window
- `modules/jreng_gui/glass/jreng_window.h/cpp` — renamed from GlassWindow, ComponentBoundsConstrainer inheritance
- `modules/jreng_gui/glass/jreng_modal_window.h/cpp` — GlassWindow→Window
- `modules/jreng_gui/jreng_gui.h/cpp` — updated module includes
- Deleted: `Source/nexus/Loader.h`, `Source/nexus/Loader.cpp`, `Source/nexus/Phrases.h`

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Terminal::Session owns Processor — single owner, deterministic lifecycle (B)
- Grid+State snapshot replaces raw byte history replay — eliminates dim-garble bug class
- Terminal layer decoupled from Nexus — Input, Mouse, Display use Processor callbacks
- Popup creates Terminal::Session directly — ephemeral terminal, no Nexus routing
- ServerConnection handle* methods inlined — no unnecessary helpers
- buildTerminal/teardownTerminal/CreatedTerminal deleted — wrong placement, raw pointer struct
- Unified createProcessor PDU — 3 dead PDUs deleted
- Double titleBarHeight subtraction — getContentRect and Display::resized
- cellsFromRect physical-pixel SSOT — eliminates 1-row divergence with Screen::calc
- First-leaf sub-rect descent — correct dims for split-pane restore
- Resize overlay via ComponentBoundsConstrainer — no flags, no timers
- Client-side Processor writeInput/onResize wired for IPC routing

### Technical Debt / Follow-up
- createClientSession still exists in Nexus::Session — should be eliminated (END creates its own Processor)
- Nexus::logLine referenced from Terminal::Session.cpp and Processor.cpp — logging dependency
- Stale doc comments referencing Nexus in Terminal headers
- forEachAttached template in Session.h — verify callers exist
- Session::create still has envID resolution logic that may belong elsewhere

## Sprint 5: Grid Snapshot Architecture

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Architecture analysis, tmux research, flow tracing, directed all changes
- Pathfinder: Grid/State internals discovery, destruction order analysis, dims divergence tracing
- Researcher: tmux reattach architecture research (virtual grid redraw vs raw byte replay)
- Librarian: JUCE ComponentBoundsConstrainer API research
- Engineer: Serialization implementation, daemon rewire, Loader deletion, Window rename

### Files Modified (20+ total)
- `Source/terminal/logic/Processor.h` — getStateInformation/setStateInformation declarations, deleted onLoadingStarted/onLoadingFinished
- `Source/terminal/logic/Processor.cpp` — serialization implementations delegating to Grid + State
- `Source/terminal/logic/Grid.h` — getStateInformation/setStateInformation declarations
- `Source/terminal/logic/Grid.cpp` — dual-buffer serialization (cells, graphemes, rowStates, ring metadata)
- `Source/terminal/data/Cell.h` — (read only, trivially copyable confirmed)
- `Source/nexus/Session.h` — deleted Loader includes/members, updated startLoading doc
- `Source/nexus/Session.cpp` — daemon Processor real (processWithLock in onBytes), startLoading calls setStateInformation, deleted Loader machinery, createClientSession initial cwd write
- `Source/nexus/SessionFanout.cpp` — attach sends getStateInformation snapshot, merged attachAndSync+attachConnection into attach(uuid, target, sendHistory, cols, rows)
- `Source/nexus/ServerConnection.h/cpp` — handleCreateProcessor (unified), hasSession check, attach with dims, deleted handleAttachProcessor
- `Source/nexus/Client.h/cpp` — createSession (renamed from spawnSession), deleted attachSession, handleStateUpdate for cwd/fgProcess
- `Source/nexus/Message.h` — createProcessor=0x10, stateUpdate=0x22, deleted 3 dead PDUs
- `Source/component/TerminalDisplay.h` — deleted LoaderOverlay member, titleBarHeight member
- `Source/component/TerminalDisplay.cpp` — deleted loading overlay wiring, removed titleBarHeight from resized
- `Source/component/Tabs.cpp` — Tabs::restore first-leaf sub-rect descent, cellsFromRect physical-pixel math, removed AppState fallback from computeContentRect
- `Source/component/Panes.cpp` — cellsFromRect uses physical pixels (Screen::calc SSOT)
- `Source/MainComponent.h/cpp` — removed getContentRect titleBarHeight subtraction, resize overlay via Window::isUserResizing
- `Source/Main.cpp` — jreng::GlassWindow → jreng::Window
- `modules/jreng_gui/glass/jreng_window.h/cpp` — renamed from jreng_glass_window, added ComponentBoundsConstrainer inheritance, resizeStart/resizeEnd/isUserResizing
- `modules/jreng_gui/glass/jreng_modal_window.h/cpp` — GlassWindow → Window
- `modules/jreng_gui/jreng_gui.h/cpp` — updated module includes
- Deleted: `Source/nexus/Loader.h`, `Source/nexus/Loader.cpp`, `Source/nexus/Phrases.h`

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Raw byte history replay replaced with Grid+State snapshot (eliminates dim-garble class of bugs)
- Daemon Processor real — parses all bytes, maintains live Grid+State for snapshot
- Unified createProcessor PDU — daemon decides create-vs-attach, deleted 3 dead PDUs
- cellsFromRect/Screen::calc SSOT — physical-pixel math eliminates 1-row divergence
- Double titleBarHeight subtraction in getContentRect and Display::resized
- First-leaf dims computed from actual sub-rect (split tree descent), not full content rect
- Resize overlay driven by ComponentBoundsConstrainer (no flags, no timers)
- GlassWindow → Window rename (domain naming)
- Loader thread + LoaderOverlay + loading callbacks deleted (entire subsystem)

### Technical Debt / Follow-up
- Session::create mode helpers (createClientSession/createDaemonSession/createLocalSession) to be unified into one path — byte source is configuration, not architecture
- Generic naming (daemon/client) to be purged — domain is Nexus/END
- startLoading naming is stale — now calls setStateInformation, not loading
- Daemon stub Processor concept eliminated but processors map still exists on daemon side

## Sprint 4: Nexus Parity — Loader, CWD, DSR, Shutdown

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Root cause analysis, flow tracing, directed all fixes
- Pathfinder: Byte path tracing, destruction order analysis, cwd flow discovery
- Engineer: Per-pane overlay, stateUpdate PDU, Phrases, shutdown fix

### Files Modified (10 total)
- `Source/terminal/logic/Processor.h:292-297` — added onLoadingStarted/onLoadingFinished callbacks
- `Source/nexus/Phrases.h` — new, random verb pool for loading messages
- `Source/component/TerminalDisplay.h` — added LoaderOverlay member, include
- `Source/component/TerminalDisplay.cpp` — wired onLoadingStarted/onLoadingFinished, addChildComponent, resized
- `Source/MainComponent.h` — removed global LoaderOverlay, loadingNode, getLoaderOverlay
- `Source/MainComponent.cpp` — removed LOADING ValueTree listener, overlay management, nexus-connect op
- `Source/nexus/Message.h` — added stateUpdate (0x22) PDU kind
- `Source/nexus/Session.h` — moved loaders to last member (destruction order)
- `Source/nexus/Session.cpp` — wired onFlush in createDaemonSession (cwd+fgProcess relay), flushResponses in feedBytes, loaders.clear() in dtor, initial cwd write in createClientSession
- `Source/nexus/Client.h` — declared handleStateUpdate
- `Source/nexus/Client.cpp` — implemented handleStateUpdate, added switch case

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Global LoaderOverlay replaced with per-pane overlay (Encapsulation — each pane manages its own loading state)
- Nexus cwd tracking: daemon relays getCwd via stateUpdate PDU, identical flow to standalone (BLESSED violation — standalone/nexus divergence eliminated)
- DSR responses dropped in client mode: flushResponses() never called after process() in feedBytes (nvim startup degraded)
- Shutdown crash: Loader threads accessing freed Processors due to wrong member destruction order (use-after-free)

### Technical Debt / Follow-up
- None deferred

## Sprint 3: BLESSED Audit Sweep

**Date:** 2026-04-09

### Agents Participated
- COUNSELOR: Led audit analysis, directed all passes, wrote cleanup plan
- Pathfinder: Codebase discovery, file-size scoping, direct-access sweep
- Auditor: Comprehensive BLESSED/NAMES/JRENG audit (14 dimensions)
- Engineer: Encapsulation refactor, decomposition, file extraction, rename/relocate

### Files Modified (18 total)
- `Source/terminal/logic/Processor.h` — private members, reference getters, bundled methods (processWithLock, setHostWriter)
- `Source/terminal/logic/Processor.cpp` — bundled method definitions
- `Source/terminal/logic/Input.h` — new (relocated from component/InputHandler.h, renamed InputHandler to Terminal::Input)
- `Source/terminal/logic/Input.cpp` — new (relocated from component/InputHandler.cpp, all access via getters)
- `Source/terminal/logic/Mouse.h` — new (relocated from component/MouseHandler.h, renamed MouseHandler to Terminal::Mouse)
- `Source/terminal/logic/Mouse.cpp` — new (relocated from component/MouseHandler.cpp, all access via getters)
- `Source/component/InputHandler.h` — deleted
- `Source/component/InputHandler.cpp` — deleted
- `Source/component/MouseHandler.h` — deleted
- `Source/component/MouseHandler.cpp` — deleted
- `Source/component/TerminalDisplay.h` — updated includes, member types to Terminal::Input/Mouse
- `Source/component/TerminalDisplay.cpp` — all direct Processor access replaced with getters
- `Source/component/Panes.cpp` — processor.uuid to processor.getUuid()
- `Source/component/Tabs.h` — added Tabs::restore declaration
- `Source/component/Tabs.cpp` — added Tabs::restore definition (recursive walk, no intermediate types)
- `Source/MainComponent.h` — deleted broken TabRestoreEntry method declarations
- `Source/MainComponent.cpp` — deleted anon namespace, file-scope constants, collect/replay methods; initialiseTabs calls Tabs::restore
- `Source/MainComponentActions.cpp` — new (6 register*Actions method definitions extracted)
- `Source/nexus/Session.h` — 3 private mode helpers declared, setHostWriter
- `Source/nexus/Session.cpp` — create() decomposed into createClientSession/createDaemonSession/createLocalSession, single-exit
- `Source/nexus/Client.h` — removed processorUuids shadow state, stale docs fixed
- `Source/nexus/Client.cpp` — switch dispatch with 5 handler methods, shadow state eliminated, processor->getUuid()
- `Source/nexus/Loader.h` — onFinished takes no args, loaderJoinTimeoutMs constant
- `Source/nexus/Loader.cpp` — uuid captured by value in onFinished lambda (UseAfterFree fix)
- `Source/nexus/ServerConnection.cpp` — processor.uuid to processor.getUuid()
- `Source/nexus/Message.h` — stale doc comment corrected
- `Source/AppState.h` — dead ensureProcessorsNode removed, getProcessorsNode/getLoadingNode added
- `Source/AppState.cpp` — dead method removed, accessor implementations
- `Source/Main.cpp` — nexus construction reordered after mainWindow for listener availability

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Processor encapsulation: 100+ direct member accesses routed through getters/bundled API
- UseAfterFree in Loader::onFinished lambda (const ref to destroyed Loader::uuid)
- ProcessorList arrival race (connected=true before processorList PDU round-trip)
- AppState ctor regression (load() vs initDefaults() ordering)
- MainComponent listener bootstrap (lazy processorsNode/loadingNode assignment)
- Broken MainComponent.h (TabRestoreEntry referenced but only defined in anon-ns)
- Anonymous namespace violation in MainComponent.cpp eliminated
- File-scope statics (fake VT100 fallback constants) eliminated
- InputHandler/MouseHandler naming and location (component/ to terminal/logic/)
- Session::create 5 early returns eliminated via single-exit with mode helpers
- Client::messageReceived if-else chain replaced with switch dispatch

### Technical Debt / Follow-up
- None deferred — all audit findings resolved or accepted as KEEP with BLESSED justification

## Handoff to COUNSELOR: Nexus Byte-Forward Architecture — Session Continuity + Final BLESSED Pass

**From:** COUNSELOR
**Date:** 2026-04-08
**Status:** In Progress — baseline works, residual bugs identified, env var unification still pending

### Context
Multi-day session that started mid-execution of PLAN-nexus.md (state-replication architecture) and pivoted to a byte-forward architecture after repeated BLESSED violations and functional failures exposed the state-replication approach as wrong. ARCHITECT locked the new invariant:

> **The ONLY divergence between standalone and nexus paths is the ANSI byte data source** (standalone: reader thread from local TTY; nexus client: message thread from IPC). Every other branch on `isNexusMode` or mode-aware logic is a BLESSED violation.

Late in the session ARCHITECT also demolished the "extra/override/shell/var" garbage naming in env-var handling — the right model is `Terminal::Session::env` as a plain `std::map<String, String>` with `getEnv(name)` / `setEnv(name, value)`, no "extra", no "override".

### Completed
**Byte-forward refactor (landed, built, partially working):**
- `Terminal::History` (`Source/terminal/data/History.{h,cpp}`) — fixed-capacity byte ring buffer, `append` / `snapshot`, juce::HeapBlock + CriticalSection, bounded by `terminalScrollbackLines * bytesPerLineEstimate`
- `Terminal::Session` (`Source/terminal/logic/Session.{h,cpp}`) — owns TTY + History. `onBytes` callback forwards bytes to consumer. Replaces Processor's TTY ownership.
- `Terminal::Processor` stripped to pure pipeline: State + Grid + Parser. `encodeKeyPress`/`encodeMouseEvent`/`encodeFocusEvent` const encoders. `process(bytes, len)` feeds parser.
- `Nexus::Session::create` routes internally by mode: local/daemon constructs Terminal::Session + wires onBytes; client constructs Processor only, registers with Client
- New wire messages: `Message::output` (daemon → client live bytes), `Message::history` (daemon → client snapshot on attach)
- Deleted entirely: `Nexus::StateInfo`, `Nexus::DirtyRow`, `Grid::writeDirtyRowsToStream`, `Grid::readDirtyRowsFromStream`, `Grid::writeScrollbackRow`, `Grid::setSeqno`, `Processor::getStateInformation`, `Processor::setStateInformation`, `Processor::HostedTag`, `Processor::startShell`, `Processor::writeToPty`, `Processor::addExtraEnv`, `Processor::getShellEnvVar`, `Processor::hasShellExited`, `Processor::onShellExited`, `Processor::setShellProgram`, `Processor::setWorkingDirectory`

**Daemon persistence fixes:**
- `Nexus::Session::create` now idempotent: on duplicate uuid, returns existing Processor without spawning a new shell or creating a dangling reference
- `Main.cpp` nexus client branch probes existing lockfile port via `juce::StreamingSocket` with 200ms timeout; only spawns daemon if port is dead or lockfile absent. Session continuity across GUI restarts now works.
- Daemon log stays alive across GUI close/reopen cycles

**State setter API cleanup (BLESSED-E encapsulation):**
- `Terminal::State::get()` is now const-only (non-const overload removed)
- `Terminal::State::setId(uuid)` added as proper setter — replaces three `state.get().setProperty(jreng::ID::id, ...)` violations
- Single uuid SSOT: `Processor` ctor now takes uuid explicitly, no internal generation. `Session::create` generates once, flows through ctor → member → map key → `state.setId`

**Unified tab/split restoration walker (no isNexusMode branch):**
- `MainComponent::initialiseTabs` refactored to a single code path. Walks the saved TABS tree, extracts each TAB's full pane hierarchy (splits + ratios), then rebuilds via `addNewTab` + `splitAt` + ratio restoration
- `Panes::splitAt(targetUuid, newUuid, cwd, direction, isVertical)` new public method — allows restoration to place panes at specific positions with specific uuids
- `Panes::splitImpl` now delegates to `splitAt` with `AppState::getActivePaneID()` as target and empty uuid hint
- Deleted: `findFirstPane`, the `if (isNexusMode) … else …` branch in initialiseTabs
- `Tabs::getActivePanes()` made public so the walker can reach the live Panes after `addNewTab`

**Parser response wiring (partial):**
- `Nexus::Session::create` client branch now wires `proc->parser->writeToHost = [this, uuid] (data, len) { client->sendInput(uuid, data, len); }` — DSR/DA/CPR replies flow from client parser → IPC → daemon PTY. Fixes "Did not detect DSR response" in vim/neovim. `[this, uuid]` capture with no raw pointer extraction.

**Dimension fix on history replay:**
- `Message::history` wire format now carries `cols (uint16) + rows (uint16)` after the uuid prefix
- Daemon's `Session::sendHistorySnapshot` writes its current dims from the stub Processor's grid
- Client's `Client::messageReceived` history branch parses cols/rows, calls `processor->resized(cols, rows)` before feeding history bytes to `process()` — prevents parser from replaying cursor-positioning escapes at wrong dimensions

**Other byte-forward supporting changes (from earlier in session):**
- `Nexus::Client::getContext()` removed from external visibility; Client is private to `Nexus::Session`
- Client/Server owned as `unique_ptr<...>` by Session (three-mode tag struct ctors: default/DaemonTag/ClientTag)
- `Nexus::Wire.h` consolidated binary codec helpers (SSOT)
- `Nexus::Server::getLockfile()` single source of truth for lockfile path
- `Nexus::wireMagicHeader` single source of truth for IPC magic
- `Terminal::Processor::defaultCols` / `defaultRows` single source of truth for default terminal geometry
- `Terminal::State::setDimensions` called from Processor::setStateInformation (before refactor deleted it)
- `Terminal::History::bytesPerLineEstimate` = 256 (single derivation from `terminalScrollbackLines`)
- All `juce::StringArray` replacing `std::vector<juce::String>` in nexus files
- All known silent-fail `if (ptr != nullptr)` double-guards removed; `jassert` + unconditional body
- Log file split: `end-nexus-client.log` (GUI) and `end-nexus-daemon.log` (daemon)
- JUCE FileLogger explicit `shutdownLog()` in `ENDApplication::shutdown` to avoid static-lifetime leak detector

### Remaining
**Confirmed broken / needs fix after last test run:**

1. **Rendering artifacts on nexus reconnect (PARTIALLY FIXED, needs verification):** Dimension fix on `Message::history` just landed but was NOT yet built/tested. ARCHITECT should rebuild and retest:
   - vim session persistence (should now render at correct dims)
   - btop rendering (should resolve if it was the dimension issue)
   - "Active prompt ghost / shadow" on first pane (likely dimension-related, verify)
   - "Last active tab always broken" on reconnect (likely dimension-related, verify)

2. **Env var feature (`getShellEnvVar` / `addExtraEnv`) still broken in nexus mode:** ARCHITECT's final demolition:
   - HEAD implementation reads live shell PATH via `tcgetpgrp` + `sysctl(KERN_PROCARGS2)` (macOS) / `/proc/<pid>/environ` (Linux) in `Terminal::Session::getShellEnvVar` → `UnixTTY::getEnvVar`. **Pure OS process introspection**, no shell integration, no OSC.
   - Current byte-forward refactor stubbed `Terminal::Display::getShellEnvVar` and `addExtraEnv` to no-op — feature completely broken in both modes. Popup PATH inheritance dead.
   - **ARCHITECT-locked fix direction (not yet implemented):** restore `Terminal::Session::getEnv(name)` and `Terminal::Session::setEnv(name, value)` (renamed from garbage `getShellEnvVar`/`addExtraEnv`), mirror on `Nexus::Session::getEnv(uuid, name)` / `setEnv(uuid, name, value)` for IPC routing. Non-nexus: direct call into local Terminal::Session. Nexus client: IPC round-trip (sync) to daemon.
   - Naming cleanup: no "extra", no "override", no "Shell", no "Var". `env` is a `std::map<String,String>` member, `getEnv` / `setEnv` are the accessors. Child shell's env constructed from this map at `execve` time.
   - MainComponent popup path becomes mode-agnostic: `Nexus::Session::getContext()->getEnv(activeUuid, "PATH")` + `setEnv(newUuid, "PATH", value)`. The `isNexusMode` guard at `MainComponent.cpp:577` is deleted.

3. **Deferred tab init mode branch (Option A locked, not yet implemented):** `MainComponent.cpp:75` has `if (not appState.isNexusMode()) initialiseTabs()`. Fix: always defer `initialiseTabs` until `nexus->onConnected` fires. Non-nexus `Nexus::Session` fires `onConnected` synchronously in its default ctor; client-mode `Nexus::Session` fires it on `Client::connectionMade`. Both modes subscribe the same way, `MainComponent::onNexusConnected` is the single handler.

4. **Verification after all fixes land:**
   - `grep -rn "isNexusMode" Source/` should only have legitimate hits: Main.cpp daemon-spawn branch, AppState getter/setter. NOTHING in MainComponent, Panes, Tabs, Display, Processor, State, Grid, Parser.
   - Session persistence smoke: run vim, quit, reopen → vim still there at correct dims
   - Split persistence smoke: split pane, quit, reopen → splits restored in the same layout
   - Popup PATH inheritance smoke (after env fix): custom PATH in shell A, popup from shell A inherits it

### Key Decisions
**Byte-forward architecture (ARCHITECT-locked, replacing original PLAN-nexus.md state-replication approach):**
- Daemon owns PTY + History (byte ring buffer). No Parser, State, Grid on daemon side.
- Client owns full pipeline (Processor + State + Grid + Parser + Display).
- Only the ANSI byte source differs between modes. Standalone: local TTY reader thread feeds `onBytes` → `Processor::process`. Nexus client: IPC `Message::output` → `Processor::process`. Identical sequence.
- Initial attach sends `Terminal::History::snapshot()` via `Message::history` with cols/rows. Client resizes processor, then pipes the whole byte chunk through its parser to reconstruct state.
- Scrollback preserved via daemon's bounded byte history. Single config `Config::Key::terminalScrollbackLines` drives both Grid row ring and History byte ring (multiplier `bytesPerLineEstimate = 256`).
- Rationale: JUCE's `get/setStateInformation` is one-shot save/load, not continuous push. `ValueTreeSynchroniser` exists but is not used in `juce_audio_processors` anywhere. Production terminal muxes (tmux/zellij/wezterm/mosh) all have the server owning the parser, never forward raw bytes as primary GUI path. END's byte-forward model is closest to tmux control mode + wezterm's `ClientPane` but simpler because END has one client per session, so independent client parsing is fine.

**`Nexus::Session` ownership and mode selection:**
- Three construction modes via tag struct: default (non-nexus local), `DaemonTag`, `ClientTag`. No bool flags.
- Daemon: owns `std::map<uuid, unique_ptr<Terminal::Session>>` (PTY side) + stub Processor map for uuid tracking + Server via `unique_ptr`
- Client: owns Client via `unique_ptr`, routes IPC messages. Processors owned by Panes via Display.
- Non-nexus: owns both Terminal::Session map and Processor map, wires them directly via onBytes lambda.
- `Session::create(shell, args, cwd, uuid)` is idempotent on duplicate uuid (returns existing Processor, no shell respawn).

**Display is passive reader holding `Terminal::Processor&`:**
- Reads `processor.state` and `processor.grid` directly (JUCE AudioProcessorEditor pattern).
- Subscribes as `juce::ChangeListener` — Processor inherits `juce::ChangeBroadcaster`, fires `sendChangeMessage()` on `process()` completion so UI thread coalesces and repaints.
- Display input handlers call `processor.encodeX(...)` then `Nexus::Session::getContext()->sendInput(processor.uuid, bytes, len)` — Session routes internally.

**Naming discipline (BLESSED-E Explicit):**
- `get()` on containers is `const`-only when no caller legitimately mutates through it
- `state.get().setProperty(id, …)` is a violation — add proper `State::setId(uuid)` setter
- No "extra", no "override", no "Shell", no "Var" redundant prefixes. Just `env`, `getEnv`, `setEnv`.
- Tag structs over bool flags for ctor disambiguation.
- `.get()` on smart pointers is only for true non-owning boundary (JUCE API). Never for capturing in lambdas where `this` + member dereference works.

**Layer rules:**
- `Source/terminal/` cannot include `Source/nexus/*` headers (except pure data struct `nexus/StateInfo.h` — but that's now deleted)
- `Source/nexus/` can include `Source/terminal/` freely
- `Source/component/` can include both

### Files Modified
Byte-forward refactor landed across:
- `Source/terminal/data/History.h`, `History.cpp` — new
- `Source/terminal/logic/Session.h`, `Session.cpp` — new
- `Source/terminal/logic/Processor.h`, `Processor.cpp` — stripped
- `Source/terminal/logic/Grid.h`, `Grid.cpp` — stream methods deleted
- `Source/terminal/data/State.h`, `State.cpp` — `setId` added, `get()` const-only, stale subscriber methods dead code
- `Source/nexus/Session.h`, `Session.cpp` — three-mode tag ctor, idempotent `create`, `sendHistorySnapshot` with cols/rows, local/daemon/client routing
- `Source/nexus/SessionFanout.cpp` — handleAsyncUpdate rewritten (no stateInfo)
- `Source/nexus/Server.h`, `Server.cpp` — `getLockfile()` public SSOT
- `Source/nexus/ServerConnection.h`, `ServerConnection.cpp` — attach now sends history
- `Source/nexus/Client.h`, `Client.cpp` — `output`/`history` dispatch, no stateInfo path, hostedProcessors routing
- `Source/nexus/Message.h` — `output`/`history` added, `stateInfo`/`listSessions*`/`getRenderDelta`/`renderDelta` deleted
- `Source/nexus/Wire.h` — binary codec helpers + `wireMagicHeader`
- `Source/nexus/Log.h`, `Log.cpp` — `initLog(filename)` + `shutdownLog()`, separate daemon/client logs
- `Source/nexus/StateInfo.h` — deleted
- `Source/component/Panes.h`, `Panes.cpp` — `splitAt` new public method, `buildTerminal` + `teardownTerminal` via `Nexus::Session`, `hostedProcessors` moved to Client
- `Source/component/Tabs.h` — `getActivePanes` public
- `Source/component/TerminalDisplay.h`, `TerminalDisplay.cpp` — Processor& reference, ChangeListener, `getShellEnvVar`/`addExtraEnv` stubbed to no-op (broken — see Remaining)
- `Source/component/InputHandler.h`, `InputHandler.cpp` — member renamed `session → processor`, `handleKey` restructured without `bool handled` flag
- `Source/component/MouseHandler.h`, `MouseHandler.cpp` — member renamed, accesses via `processor.state` / `processor.grid`
- `Source/component/Popup.h`, `Popup.cpp` — `onTeardown` callback removed (dead), `removePopupSession` uses `teardownTerminal`
- `Source/Main.cpp` — three startup branches, StreamingSocket probe before spawnDaemon, `Nexus::shutdownLog` in shutdown
- `Source/MainComponent.h`, `Source/MainComponent.cpp` — `onNexusConnected` deferred init, unified restoration walker, popup path still has broken env guard
- `Source/AppState.h`, `Source/AppState.cpp` — `clientMode → nexusMode` rename, `isNexusMode/setNexusMode`
- `Source/AppIdentifier.h` — `clientMode → nexusMode`
- `Source/terminal/data/Identifier.h` — subscriber identifiers (now dead)
- Various sibling files for stale comment updates and rename cascades

### Open Questions
1. **`Terminal::Session::getEnv` / `setEnv` API (ARCHITECT-locked naming, not yet implemented):** method signatures are agreed but the actual wiring hasn't been written. Needs:
   - Restore the OS-process-introspection code (`tcgetpgrp` + `sysctl(KERN_PROCARGS2)` macOS / `/proc/environ` Linux) that HEAD had in `UnixTTY::getEnvVar`. Was not moved over in the byte-forward refactor.
   - Decide on sync vs async IPC round-trip in nexus client mode. Popup construction is a user-initiated action so brief blocking on the message thread is probably acceptable UX.
   - Add new Message kinds: `Message::getEnv` (request), `Message::getEnvResponse` (reply), `Message::setEnv` (fire-and-forget).
2. **Async IPC round-trip pattern**: the existing JUCE `InterprocessConnection` is message-based. To do a synchronous-looking `getEnv(uuid, name) -> String` on the client side we'd need either (a) a blocking wait with a timeout, (b) keep the API async with a callback. Not yet decided.
3. **btop rendering**: is it downstream of the dimension fix or a separate bug? Verify after rebuild.
4. **`Processor::onShellExited`** — deleted in the byte-forward refactor. On daemon side, `Terminal::Session::onExit` lambda now handles the "shell died" lifecycle (callAsync → remove → broadcastProcessorList → onAllSessionsExited). On non-nexus, same mechanism. Verify this chain still fires correctly — no known bug but was refactored heavily.

### Next Steps
1. **ARCHITECT rebuild and test current working tree** — the dimension fix on `Message::history` and parser writeToHost wiring both just landed but were not built/tested. Share logs after test, especially daemon + client log for the reconnect scenario with vim running.
2. **If vim/btop render correctly** after step 1 → proceed to env var unification (item 2 in Remaining). If still broken → diagnose with fresh logs before any more code changes.
3. **Deferred tab init unification**: delete the `isNexusMode` guard at `MainComponent.cpp:75`. Always defer `initialiseTabs` until `nexus->onConnected` fires. Requires `Nexus::Session` default-ctor (non-nexus) to fire `onConnected` immediately from its constructor body.
4. **Env var unification**: implement `Terminal::Session::getEnv` / `setEnv` (restore OS introspection from HEAD), `Nexus::Session::getEnv(uuid, name)` / `setEnv(uuid, name, value)` with mode routing, IPC message kinds for the daemon round-trip, MainComponent popup path updated to the mode-agnostic two-line pattern.
5. **Final `isNexusMode` audit**: grep should show only Main.cpp daemon-spawn branch and AppState getter/setter. Anything else is a BLESSED violation.
6. **Split persistence verification**: the unified restoration walker just landed. Verify two-pane split survives quit/relaunch with correct layout.

---

## Handoff to COUNSELOR: Nexus Refactor — PLAN-nexus.md Revision Complete

**From:** COUNSELOR
**Date:** 2026-04-07
**Status:** Ready for Implementation — PLAN-nexus.md fully locked, all decisions made, no open items

### Context
Session consumed RFC-NEXUS.md and the previous COUNSELOR handoff (Nexus Processor/Display Architecture Fix). Ran extended ARCHITECT discussion to resolve every naming, ownership, and API design question before writing the execution plan. All discussion was in service of producing PLAN-nexus.md with zero ambiguity so @Engineer can execute step-by-step without further clarification rounds.

Key framing shift during the session: the wire payload is **processor state serialized for a proxy**, not a "delta" or "frame." This renamed the API from the BRAINSTORMER-proposed `applyDelta` (and COUNSELOR's intermediate `processFrame` misstep) to JUCE-literal `getStateInformation` / `setStateInformation`, mirroring `juce::AudioProcessor`'s exact preset save/restore API.

### Completed

**PLAN-nexus.md fully revised and written.** 16 sequential steps, all decisions locked, per-step @Auditor checklist, forbidden-token table, BLESSED alignment notes, dependency graph. Ready for @Engineer consumption.

**Pathfinder recons completed (3 sweeps):**
1. Initial rename scope + Grid grapheme API + PTY timing + Client threading
2. Grid writer API audit for proxy-mode setStateInformation correctness
3. Final sweep: State setters, AppState::isClientMode consumers, Client lifetime, lockfile protocol, per-subscriber seqno tracking today, wire serialization pattern, InputHandler/MouseHandler refs, LoaderOverlay API, Cell::LAYOUT_GRAPHEME, HeapBlock API

**Librarian recons completed (2 sweeps):**
1. JUCE AudioProcessor/Editor API naming + DAW out-of-process bridging precedent (Apple AUv3, Reaper, Bitwig, Ardour, GoF Remote Proxy)
2. JUCE `InterprocessConnectionServer` ownership contract (verified base class is ownership-opaque, contradicting BRAINSTORMER's initial reading)

**Researcher recon completed:**
- Wezterm `ClientPane` + `apply_changes_to_surface` + `GetPaneRenderChangesResponse` architecture
- Tmux architecture (confirmed unusable as precedent — tmux sends ANSI escapes directly via fd-passing, doesn't replicate grid state)

**Contract documents read and applied:**
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- JRENG-CODING-STANDARD.md (C++ coding standards)
- CAROL.md (protocol)

### Remaining
Everything. Plan is the deliverable of this session; execution is the next session's work.

Execution order per PLAN-nexus.md dependency graph:
1. Step 0 — annotation sweep (READER THREAD/MESSAGE THREAD → PRODUCER/CONSUMER CONTEXT, codebase-wide)
2. Steps 1-3 — mechanical renames (Session→Processor, Host→Session, Pdu→Message)
3. Step 7 — StateInfo type definition (before Step 6 because Step 6 Grid methods return DirtyRow)
4. Step 6 — Grid additions (writeScrollbackRow, setSeqno, collectDirtyRows) + buildDelta deletion
5. Steps 4, 5 — Bug 1 and Bug 2 fixes
6. Step 8 — Terminal::Processor two-mode (the largest architectural step)
7. Steps 9, 10 — tell-don't-ask protocol + multi-subscriber seqno
8. Steps 11a/b/c — consumer updates (Panes, Tabs+Popup, Main+MainComponent)
9. Steps 12, 13 — Bug 3 wiring + Bug 4/5/6/7 verification
10. Step 15 — InputHandler/MouseHandler cascade (can run parallel with 11)
11. Step 14 — async startup + LoaderOverlay
12. Step 16 — final cleanup + forbidden-token sweep

### Key Decisions

**Architecture (L1-L18 in PLAN-nexus.md):**
- **L1:** One `Terminal::Processor` class, two ctors (local, hosted). No Proxy/Mirror/Stub subclass. Proxy-ness is structural (constructor choice). Matches JUCE `AudioProcessor` standalone/plugin pattern.
- **L2:** `state` and `grid` are public members on Processor. Display reads `processor.state.*` / `processor.grid.*` directly. Exact 1:1 with `AudioProcessorEditor::processor` at `juce_AudioProcessorEditor.h:68`. No unnecessary getters.
- **L3:** IPC transfer methods named `getStateInformation` / `setStateInformation` — literal JUCE convention. Mode-neutral (same method on real and proxy Processor).
- **L4:** Wire payload type `Nexus::StateInfo` (replaces `Nexus::GridDelta`). Names content, not mechanism.
- **L5:** `StateInfo` uses `juce::HeapBlock<DirtyRow>` + `HeapBlock<Cell>` / `HeapBlock<Grapheme>`. Two-pass count-then-fill. No `std::vector` in wire path (project convention for trivially copyable).
- **L6:** `Grid::buildDelta` deleted. Replaced by `Grid::collectDirtyRows(sinceSeqno)`. Grid no longer reads State or Nexus types. Full StateInfo assembly moves to Processor.
- **L7:** Two new Grid APIs mandatory — `writeScrollbackRow` (no existing public API for scrollback writes, blocks proxy) and `setSeqno` (seqno is private atomic, proxy needs to align). Pathfinder confirmed both gaps in pass 2.
- **L8:** `GridDelta.cpp` deleted. Logic moves to `Processor::getStateInformation`.
- **L9:** `Pdu.h` → `Message.h`. `enum class PduKind` → `enum class Nexus::Message` (flat, no nested Kind). JUCE vocabulary, drops OSI jargon.
- **L10:** `jreng::Owner<ServerConnection>` on `Nexus::Server`. `connections.add(std::make_unique<ServerConnection>(session)).get()` one-liner. Librarian verified JUCE base is ownership-opaque — without this, current code leaks.
- **L11:** Server owns connections; Session has non-owning broadcast set `std::vector<ServerConnection*> attached` populated by `connectionMade`/`connectionLost`. Separation of ownership and broadcast intent.
- **L12:** `Client` uses `InterprocessConnection(true, ...)` — `callbacksOnMessageThread = true`. No raw-this lambdas. JUCE SafeAction handles lifetime.
- **L13:** Explicit `Processor::startShell()` lifecycle method. `resized()` becomes pure SIGWINCH. `callAsync` deferral at `Session.cpp:251-281` deleted. Fixes Bug 3 by construction.
- **L14:** Multi-subscriber (N>1 clients) in scope. Per-subscriber seqno tracking as ValueTree children under `ID::subscribers` on sidecar Processor's State. Migration from `Host::lastFanoutSeqno` (per-session watermark shared across subscribers — incorrect for N>1).
- **L15:** `LoaderOverlay.show(0, "Connecting to nexus")` during connect wait, `hide()` on `connectionMade`. Indeterminate mode renders spinner without progress bar.
- **L16:** Thread annotation rename scope: **entire codebase**. `// READER THREAD` / `// MESSAGE THREAD` → `// PRODUCER CONTEXT` / `// CONSUMER CONTEXT`. Honest across both modes.
- **L17:** `AppState::isClientMode` **kept**. Used at 4 construction points (Panes 3x, Tabs 1x) + 2 startup sites. Smaller blast radius than factory injection. ARCHITECT ruling.
- **L18:** No wire protocol backwards compatibility. Single-binary deployment.

**Naming (final):**
- `Terminal::Session` → `Terminal::Processor`
- `Nexus::Host` → `Nexus::Session`
- `Nexus::HostFanout` → `Nexus::SessionFanout`
- `Nexus::RemoteSession` → absorbed (file deleted)
- `Nexus::GridDelta` → `Nexus::StateInfo`
- `Nexus::CellRange` → `Nexus::DirtyRow`
- `Grid::buildDelta` → `Grid::collectDirtyRows`
- New: `Processor::getStateInformation`, `setStateInformation`, `startShell`
- New: `Grid::writeScrollbackRow`, `setSeqno`
- `Source/nexus/Pdu.h` → `Source/nexus/Message.h`
- `enum class PduKind` → `enum class Nexus::Message` (flat)
- `Message::renderDelta` → `Message::stateInfo`
- `spawnSession/attachSession/detachSession/sessionExited` → `spawnProcessor/attachProcessor/detachProcessor/processorExited`
- `getRenderDelta`, `listSessions`, `listSessionsResponse` → deleted (tell-don't-ask)
- New: `Message::processorList` (push on connectionMade)
- `// READER THREAD` / `// MESSAGE THREAD` → `// PRODUCER CONTEXT` / `// CONSUMER CONTEXT`

**Grid return type (L6 follow-up, locked):** `Grid::collectDirtyRows` returns `juce::HeapBlock<Nexus::DirtyRow>`. Grid.h includes `nexus/StateInfo.h`. No intermediate Terminal::ChangedRow conversion.

**Key rationale highlights:**
- **Why `getStateInformation`/`setStateInformation` and not `processFrame`:** `processBlock`-style verbs (`process`) are for *computing on* work units. The proxy doesn't compute — it *adopts* state. JUCE's `getStateInformation`/`setStateInformation` is the preset save/restore API, which is exactly what cross-process proxy sync is semantically (continuous restore instead of one-shot).
- **Why `StateInfo` is not called "Delta" or "Frame":** only 1/10 fields are incremental (dirtyRows). 9/10 are absolute snapshots (cursor, title, cwd, geometry, etc.). Naming it "Delta" privileged the transmission optimization over the semantic payload. "StateInfo" matches JUCE method names exactly.
- **Why public `state`/`grid` members:** exact replication of `AudioProcessorEditor::processor` public member pattern. No getters where JUCE has none. NAMES.md forbids dead getters.
- **Why multi-subscriber tracking in ValueTree children on State:** project idiom is "all state lives in Terminal::State as ValueTree." External maps in Session/Client would violate SSOT.
- **Why JUCE `OwnedArray` was rejected in favor of `jreng::Owner`:** jreng::Owner is the project's modern smart-pointer-based equivalent to the pre-C++11 OwnedArray. Jules Storer (JUCE) has publicly said smart pointers are preferred for new code.

### Files Modified

- `PLAN-nexus.md` — fully rewritten (file previously contained an earlier plan draft; now reflects all locked decisions)
- `carol/SPRINT-LOG.md` — this handoff entry prepended

No source code modified. All code work is delegated to @Engineer in next session per the plan's step-by-step structure.

### Open Questions
None. All decisions locked. The only open item surfaced during plan-writing (Grid return type a/b) was resolved by ARCHITECT as (a) and the plan updated accordingly.

### Next Steps

**Immediate (next COUNSELOR session):**
1. ARCHITECT activates COUNSELOR with context: read PLAN-nexus.md, carol/SPRINT-LOG.md (this entry), and optionally RFC-NEXUS.md for background.
2. COUNSELOR invokes @Pathfinder for Step 0 enumeration (every `// READER THREAD` and `// MESSAGE THREAD` annotation site codebase-wide). Pathfinder returns the full list.
3. COUNSELOR delegates Step 0 execution to @Engineer with the Pathfinder-verified site list.
4. @Auditor verifies Step 0 against the checklist (zero `// READER THREAD` / `// MESSAGE THREAD` survivors, comments-only diff).
5. Proceed to Step 1 (mechanical Terminal::Session → Terminal::Processor rename) only after Step 0 is signed off.

**Gating discipline:** execute ONE STEP AT A TIME. @Auditor signs off before next step begins. ARCHITECT may gate at any step boundary. Do not batch multiple steps in one Engineer pass — the incremental validation is the whole point of the 16-step decomposition.

**Recovery hint if a step fails:** PLAN-nexus.md step dependency graph shows which steps must be reverted together. Any failure in Step 8 (Terminal::Processor two-mode) is the highest-risk rollback — it depends on Steps 1, 2, 6, 7 being in place.

### Commit Message (for current uncommitted changes)

```
docs(nexus): lock PLAN-nexus.md execution plan for Nexus refactor

Rewrite PLAN-nexus.md with all ARCHITECT-locked decisions from extended
COUNSELOR discussion. Supersedes the earlier draft. 16 sequential steps
covering: thread-annotation sweep, Terminal::Session → Terminal::Processor
rename, Nexus::Host → Nexus::Session rename, Pdu.h → Message.h rename,
StateInfo type replacing GridDelta (HeapBlock-based), Grid API additions
(writeScrollbackRow, setSeqno, collectDirtyRows), Bug 1 ServerConnection
ownership via jreng::Owner, Bug 2 Client threading via callbacksOnMessageThread,
Terminal::Processor two-mode absorbing RemoteSession, tell-don't-ask protocol,
multi-subscriber ValueTree seqno tracking, split consumer updates, Bug 3
startShell lifecycle, async startup with LoaderOverlay, final cleanup.

API naming matches juce::AudioProcessor 1:1 (getStateInformation /
setStateInformation, public state/grid members per AudioProcessorEditor).
Wire vocabulary matches JUCE InterprocessConnection (Message, messageReceived).

Ready for Engineer execution; step-by-step with per-step Auditor checklist
and forbidden-token table for validation gating.

Pre-flight recons logged: 3 Pathfinder sweeps, 2 Librarian sweeps, 1
Researcher sweep (wezterm/tmux), covering JUCE AudioProcessor source,
InterprocessConnectionServer ownership contract, wezterm ClientPane
precedent, Terminal::State setter API, existing Grid API gaps.

Also prepend handoff entry to carol/SPRINT-LOG.md for session continuity.
```

---

## Handoff to COUNSELOR: Nexus Processor/Display Architecture Fix

**From:** BRAINSTORMER
**Date:** 2026-04-07
**Status:** Ready for Implementation — RFC complete, all decisions made

### Context
BRAINSTORMER session to diagnose why `nexus = true` produces a blank window while `nexus = false` works. Evolved into a full architectural audit and redesign. The previous implementation was poisoned by wezterm/tmux hand-rolled IPC patterns that fought JUCE's threading model. BRAINSTORMER analyzed all JUCE IPC source code, both working JUCE IPC examples, and every nexus source file. Found 7 bugs. Designed the fix architecture based on JUCE's actual AudioProcessor/AudioProcessorEditor pattern (read from source, not docs).

### Completed
- Full root cause analysis: 7 bugs identified with evidence from JUCE source
- RFC-NEXUS.md written — single authoritative document, no contradictions
- Naming correction: Terminal::Session → Terminal::Processor, Nexus::Host → Nexus::Session, RemoteSession absorbed
- Architecture: one Processor class with two construction modes (local/hosted), ChangeBroadcaster/ChangeListener pattern
- UX model defined: tmux-like session persistence, exit hierarchy, sidecar lifecycle
- All ARCHITECT decisions captured: RAII cleanup, signal/clock separation, tell-don't-ask, async startup, coalesced fan-out
- Analyzed JUCE source: InterprocessConnection, InterprocessConnectionServer, ConnectedChildProcess, AudioProcessor, AudioProcessorEditor, ChangeBroadcaster

### Remaining
- Rename sprint: Terminal::Session → Terminal::Processor across entire codebase (mechanical, first priority)
- Fix all 7 bugs per RFC architecture
- Verify Grid API has `activeVisibleGraphemeRow()` for Bug 7 fix (or add it)
- Background connect thread design for fresh-spawn case
- Shell exit mechanism: Processor sets atomic `exited` flag + `sendChangeMessage()`, Session detects in `handleAsyncUpdate()`

### Key Decisions
- **One Processor, two modes:** Local ctor (owns PTY) vs hosted ctor (shadow Grid + IPC). Display holds `Processor&` always. No proxy class. No interface. No dual-path routing.
- **ChangeBroadcaster:** Processor extends it. Display extends ChangeListener. JUCE-sanctioned, event-driven, lifetime-safe.
- **callbacksOnMessageThread = true** on both Client and ServerConnection. JUCE SafeAction protects everything.
- **Server owns connections:** `jreng::Owner<ServerConnection>`. Cleanup in connectionLost. No timer, no polling.
- **Tell, don't ask:** Sidecar pushes unsolicited. `getRenderDelta` and `listSessions` PDUs eliminated. Attach triggers full snapshot push.
- **Signal/clock separation:** ChangeBroadcaster → setSnapshotDirty (signal). VBlank → render (clock).
- **Sidecar notification:** AsyncUpdater coalesces N dirty Processors into one batched fan-out. ChangeBroadcaster per-Processor on GUI side.
- **Display RAII:** Destructor calls `processor.removeChangeListener(this)` + `processor.displayBeingDeleted(this)`.
- **Sidecar runs real Processors:** Same class, local constructor. ChangeBroadcaster with no Display listeners — Nexus::Session listens instead.
- **Rename same sprint, first priority.**

### Files Modified
- `RFC-NEXUS.md` — NEW. Complete architecture RFC. Single authority for COUNSELOR. Supersedes PLAN-mux-server.md "Remaining Work" section.

### Open Questions
- None for ARCHITECT. All decisions made.
- COUNSELOR implementation details: background connect thread lifetime management, Grid grapheme row API verification.

### Next Steps
1. COUNSELOR reads RFC-NEXUS.md (single document, no external dependencies)
2. Write PLAN for rename sprint + fix sprint (can be combined per ARCHITECT decision)
3. Rename first: Terminal::Session → Terminal::Processor, Nexus::Host → Nexus::Session, delete RemoteSession
4. Fix Bug 1 (connection ownership) + Bug 2 (callbacksOnMessageThread) — prerequisites
5. Fix Bug 3 (blank screen timing) — unblocked by 1+2
6. Fix Bugs 4+5 (InputHandler/MouseHandler) — independent
7. Fix Bug 6 (splitImpl) — two missing calls
8. Fix Bug 7 (graphemes) — after Grid API verification

### Recommended Commit Message
```
docs(nexus): RFC for Processor/Display architecture fix

Root cause analysis of blank-screen bug found 7 issues:
connection leaks, unsafe threading, missing input handlers,
deferred PTY timing, grapheme copy omission.

Redesign based on JUCE AudioProcessor/Editor pattern:
- One Processor class, two modes (local PTY / hosted IPC)
- ChangeBroadcaster/ChangeListener (JUCE-sanctioned)
- Tell-don't-ask: sidecar pushes, client declares intent
- Nexus::Session as listener-orchestrator
- Terminal::Session → Terminal::Processor rename

All JUCE IPC source audited. All architect decisions captured.
```

---


## Handoff to COUNSELOR: Nexus Mux Server

**From:** COUNSELOR
**Date:** 2026-04-06
**Status:** In Progress — client-mode terminal rendering not working

### Context
Implementing a mux server architecture for END (Ephemeral Nexus Display) that follows the PluginProcessor/PluginEditor pattern from JUCE audio plugins. Session (Processor) owns state and lives independently in a background daemon; Display (Editor) is ephemeral UI that connects as a client. Goal: sessions survive window close, relaunch reconnects.

Architecture: `end --nexus` runs headless daemon. `end` (with `nexus = true` in config) spawns daemon if needed, connects as client, shows UI. `nexus = false` reverts to single-process behavior.

### Completed
- Nexus::Host — session pool, Context<Host>, owns all Sessions
- Terminal::Component renamed to Terminal::Display across entire codebase
- Session::createDisplay() — Processor creates Editor pattern
- SeqNo + GridDelta — monotonic version tracking on Grid, delta builder
- Pdu.h — PduKind enum for message vocabulary
- JUCE InterprocessConnectionServer/Connection — replaced hand-rolled AF_UNIX IPC
- Nexus::Server — TCP localhost, port lockfile at ~/.config/end/end.port
- Nexus::ServerConnection — per-client PDU dispatch (spawn, input, resize, attach, detach, getRenderDelta)
- Nexus::Client — InterprocessConnection + Context<Client>, connects to daemon
- Nexus::RemoteSession — shadow Grid+State for client-mode Display
- Terminal::Display dual-mode — local (reads Session directly) vs client (reads RemoteSession)
- Push fan-out — AsyncUpdater on Host, onHostDataReceived fires triggerAsyncUpdate from reader thread, handleAsyncUpdate builds deltas and pushes to subscribers
- Headless daemon — end --nexus, hideDockIcon, onAllSessionsExited quits
- NexusDaemon.h/.mm/_win.cpp — posix_spawn (macOS/Linux), CreateProcess (Windows)
- nexus = true/false config key in end.lua
- AppState stores clientMode — no manual boolean flags
- Session restoration — state.xml layout matching in client mode
- Step 10 eliminated — JUCE TCP localhost is cross-platform

### Remaining
1. **Connection drops (critical):** `connectionLost()` fires unexpectedly during client operation. Root cause unknown. Currently connectionLost is an empty body (crash was fixed — callAsync with dangling `this` captured when Client was being destroyed). Need to investigate WHY the TCP connection drops between UI and daemon on localhost.
2. **Client-mode terminal blank (critical):** Delta delivery chain has issues. Push fan-out is implemented but connection instability may prevent deltas from arriving. Even when connection stays up, the terminal renders blank. Need to verify: does spawnSession create a real PTY in the daemon? Does the PTY produce output? Does handleAsyncUpdate fire? Does the delta reach the client? Does applyDelta work?
3. **5-second blocking poll (UX):** Main.cpp::initialise() blocks message thread for up to 5 seconds polling for daemon readiness. Unacceptable. Need async startup — spawn daemon, show window immediately, connect in background, attach when ready.
4. **connectionLost safe cleanup:** When connection genuinely drops (not shutdown), remoteSessions should be cleaned up. Current empty body leaks. Need safe cleanup that doesn't crash on shutdown (the callAsync + dangling this problem).

### Key Decisions
- **Processor/Editor pattern:** Session is the Processor (always alive, owns state), Display is the Editor (ephemeral, created/destroyed). Nexus::Host is the DAW Host. This mirrors JUCE audio plugin architecture exactly.
- **Naming:** END = Ephemeral Nexus Display. Nexus IS the architecture. Terminal::Display (not Component/View).
- **JUCE IPC over TCP localhost:** Replaced hand-rolled AF_UNIX with JUCE InterprocessConnectionServer/Connection. Cross-platform, battle-tested framing, managed threading. Eliminated Step 10 (Windows AF_UNIX).
- **Config-driven:** `nexus = true` (default) enables daemon. `nexus = false` single-process. User never types --nexus.
- **AppState for clientMode:** Stored in ValueTree WINDOW node. All routing uses AppState::getContext()->isClientMode(). No manual boolean passing.
- **Push not poll:** AsyncUpdater for dirty notification. Reader thread triggers, message thread builds delta and fans out. VBlank is for rendering only.
- **UUID in renderDelta payload:** Every renderDelta carries the session UUID in the payload itself. No serial matching.

### Files Modified (new + modified, 39 total)

**New files (Source/nexus/):**
- `Source/nexus/Host.h/cpp` — Nexus::Host session pool
- `Source/nexus/Server.h/cpp` — InterprocessConnectionServer wrapper
- `Source/nexus/ServerConnection.h/cpp` — per-client connection handler
- `Source/nexus/Client.h/cpp` — client-side InterprocessConnection
- `Source/nexus/RemoteSession.h/cpp` — shadow Grid+State
- `Source/nexus/GridDelta.h` — GridDelta/CellRange value types
- `Source/nexus/HostFanout.cpp` — subscriber registry, push fan-out, serializeGridDelta
- `Source/nexus/Pdu.h` — PduKind enum
- `Source/nexus/NexusDaemon.h` — hideDockIcon, spawnDaemon declarations
- `Source/nexus/NexusDaemon.mm` — macOS/Linux implementations
- `Source/nexus/NexusDaemon_win.cpp` — Windows implementation

**Renamed files:**
- `Source/component/TerminalComponent.h/cpp` → `Source/component/TerminalDisplay.h/cpp`

**New file (terminal):**
- `Source/terminal/logic/GridDelta.cpp` — Grid::buildDelta() implementation

**Modified files:**
- `Source/Main.cpp` — three launch paths (--nexus, nexus=true client, nexus=false single), daemon spawn, shutdown order
- `Source/MainComponent.h/cpp` — initialiseTabs client-mode branch
- `Source/AppState.h/cpp` — setClientMode/isClientMode
- `Source/AppIdentifier.h` — App::ID::clientMode
- `Source/component/Panes.h/cpp` — dual-mode createTerminal, closePane with Host::remove
- `Source/component/Tabs.h/cpp` — addNewTab(cwd, uuid) overload, closeActiveTab session cleanup
- `Source/component/Popup.h/cpp` — removePopupSession helper, session cleanup on dismiss
- `Source/config/Config.h/cpp` — Key::nexus, addKey, Lua reader
- `Source/config/default_end.lua` — nexus = true default
- `Source/terminal/logic/Grid.h/cpp` — seqno, rowSeqnos, markRowDirty/batchMarkDirty tracking
- `Source/terminal/logic/Session.h/cpp` — createDisplay, onHostShellExited, onHostDataReceived
- `Source/terminal/rendering/Screen.h` — comment updates
- `Source/terminal/selection/LinkSpan.h` — comment updates
- Various .h files — Terminal::Component → Terminal::Display in comments

### Open Questions
- Why does InterprocessConnection drop on localhost? Is it a JUCE bug, a timing issue, or a configuration problem (magic number mismatch, read timeout)?
- Should the daemon use a different JUCE application model? Currently uses START_JUCE_APPLICATION with no window. Does NSApplication (created by JUCE) interfere with headless operation?
- Is the probe loop (temporary Client objects connecting/disconnecting rapidly) destabilizing the daemon's ServerConnection accept/teardown cycle?

### Next Steps
1. Investigate connectionLost — add file-based logging (not DBG, since MainComponent may not exist) to trace when and why connections drop
2. Verify the daemon is functioning: does spawnSession create a PTY? Does the shell start? Does handleAsyncUpdate fire in the daemon?
3. Consider simplifying startup: instead of probe loop, have the daemon write a "ready" signal to the lockfile after Server::start() succeeds
4. Fix the 5-second blocking poll — make it async
5. Once connection is stable, verify full delta delivery chain end-to-end

### Recommended Commit Message
```
feat(nexus): mux server architecture — daemon + client mode (WIP)

Processor/Editor pattern: Session owns state (daemon), Display is
ephemeral UI (client). JUCE InterprocessConnection for IPC.

- Nexus::Host session pool with Context<Host>
- Terminal::Component → Terminal::Display rename
- Session::createDisplay() (Processor creates Editor)
- SeqNo + GridDelta for incremental state transfer
- Server/ServerConnection (JUCE IPC, TCP localhost)
- Client + RemoteSession (shadow Grid for client-mode Display)
- Push fan-out via AsyncUpdater
- end --nexus headless daemon, nexus=true/false config
- NexusDaemon cross-platform process launch
- AppState clientMode routing

WIP: client-mode terminal rendering not functional yet.
Connection stability issues under investigation.
```

## Sprint 2: Fix macOS Titlebar Flash on Window Creation

**Date:** 2026-04-06
**Duration:** ~30m

### Agents Participated
- COUNSELOR: Root cause analysis, plan, orchestration
- Pathfinder: Codebase exploration (GlassWindow init sequence, BackgroundBlur API, renderer probe timing)
- Engineer: Code changes (configureWindowChrome, sync/async separation, dead code removal)
- Auditor: Validation (BLESSED compliance, dead code detection, idempotency check)

### Files Modified (4 total)
- `modules/jreng_gui/glass/jreng_background_blur.h:24` -- Added `configureWindowChrome` declaration; removed dead `hideWindowButtons` declaration
- `modules/jreng_gui/glass/jreng_background_blur.mm:162-184` -- Added `configureWindowChrome` implementation (synchronous NSWindow title hiding, style mask, button hiding); removed duplicated title/style mutations from `applyBackgroundBlur` and `applyNSVisualEffect`; removed dead `hideWindowButtons` implementation
- `modules/jreng_gui/glass/jreng_background_blur.cpp:210-212` -- Removed dead `hideWindowButtons` Windows stub
- `modules/jreng_gui/glass/jreng_glass_window.cpp:115` -- `visibilityChanged()` calls `configureWindowChrome` synchronously before `triggerAsyncUpdate()`; removed redundant `isBlurApplied = true` from `handleAsyncUpdate()`; removed redundant `hideWindowButtons` call from `setGlass()`; updated file and class doc comments

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **macOS titlebar flash on first show**: When GlassWindow became visible, the native titlebar was briefly rendered before being hidden by the async blur handler. Root cause: titlebar hiding was bundled with blur application in `handleAsyncUpdate()`, deferred to the next message pump cycle. Only blur requires async deferral (window server needs the window fully presented for CoreGraphics blur APIs). Fix: extract NSWindow chrome mutations (setTitleVisibility, setTitlebarAppearsTransparent, FullSizeContentView mask, button hiding) into `BackgroundBlur::configureWindowChrome()`, called synchronously in `visibilityChanged()` before `triggerAsyncUpdate()`. Blur remains async.
- **SSOT violation**: Title/style mutations were duplicated in `applyBackgroundBlur()` and `applyNSVisualEffect()`. Extracted to single `configureWindowChrome()` called once synchronously.
- **Dead code**: `hideWindowButtons()` had zero callers after refactor — removed from header, macOS implementation, and Windows stub.

### Technical Debt / Follow-up
- Pre-existing early returns in `BackgroundBlur::enable()`, `applyBackgroundBlur()`, `applyNSVisualEffect()`, `enableWindowTransparency()` violate BLESSED E (Explicit). Not introduced by this sprint. New code (`configureWindowChrome`) is compliant.

---

## Sprint 1: Fix Active Prompt Misalignment After Pane Close

**Date:** 2026-04-06
**Duration:** ~4h

### Agents Participated
- COUNSELOR: Requirements analysis, root cause investigation, plan, orchestration
- Pathfinder: Codebase exploration (resize path tracing, reflow architecture, rendering pipeline, buffer struct analysis)
- Engineer: Code changes (height-only fast path, diagnostics, cleanup)
- Auditor: Validation (alternate buffer overflow, BLESSED compliance)
- Oracle: (not invoked)

### Files Modified (2 total)
- `Source/terminal/logic/GridReflow.cpp:119-170` -- Height-only resize fast path with pinToBottom branching: when only visibleRows changes (cols unchanged, both buffers large enough), skip full reflow and adjust buffer metadata in-place. pinToBottom: head unchanged, scrollbackUsed decreases, cursor moves down (viewport expands from top). Not pinToBottom: head advances by heightDelta, scrollbackUsed unchanged, cursor stays (viewport expands from bottom). Sync State::scrollbackUsed after both paths. Existing full reflow path unchanged (now `else if`). Cursor fallback in full reflow: use `totalOutputRows - 1` and preserve `cursorCol`.
- `Source/terminal/data/State.h:373-378` -- Updated `setScrollbackUsed` doc comment to reflect dual-thread usage (READER THREAD or MESSAGE THREAD).

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- **Active prompt 1-row misalignment after pane close**: When a split pane was closed and the surviving terminal reclaimed height, the active prompt rendered 1 row above its correct position. Root cause: the full reflow algorithm (designed for column-width changes that re-wrap content) produced a 1-row viewport shift when only height changed, due to the cursor fallback inflating `contentExtent` by 1 and floor-division rounding losses (107/2=52, 52*2=104!=107) compounding through double reflow (shrink+expand). Fix: bypass the full reflow for height-only changes — adjust viewport metadata directly (scrollbackUsed, allocatedVisibleRows, cursor) without touching the ring buffer content. Content layout is identical when cols are unchanged.
- **Prompt at wrong position after external window resize**: Height-only fast path initially assumed pinToBottom unconditionally (cursor + heightDelta). When the cursor was NOT at the bottom (e.g., fresh launch, Hammerspoon resize), the prompt slid to the middle instead of staying at its content position. Fix: branch on pinToBottom — when true, viewport expands from top (head stays, scrollback revealed); when false, viewport expands from bottom (head advances, cursor stays).
- **State::scrollbackUsed stale after reflow**: After full reflow, `Buffer::scrollbackUsed` was updated but `State::scrollbackUsed` (atomic) was not synced. Shell's OSC 133;A computed `promptRow = staleScrollbackUsed + cursorRow` producing wrong absolute row. Fix: sync State from Buffer after reflow.
- **SSOT violation**: Initial approach tracked OSC 133 markers (promptRow, blockTop, blockBottom) through the reflow, making Grid a second writer for values owned by the shell via parser. Removed all marker tracking — only the shell (via OSC 133) writes prompt/block markers. Grid writes only what it owns: cursor position, scrollbackUsed.

### Technical Debt / Follow-up
- None. PLAN-fix-active-prompt.md at project root can be removed (plan is superseded by the actual implementation).
