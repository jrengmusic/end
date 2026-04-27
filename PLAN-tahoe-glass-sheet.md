# PLAN: Configurable Window Blur Backend

**RFC:** BRAINSTORMER session 2026-04-26 (consumed — this is the plan)
**Date:** 2026-04-26
**BLESSED Compliance:** verified
**Language Constraints:** C++ / JUCE — no overrides (LANGUAGE.md)

## Overview

User-configurable window blur backend via single lua config key. All platforms, all backends exposed. Fallback chain: user chosen → auto → solid (disable).

## Config Surface

Single key in `window` table:

```lua
window = {
    blur_type = "auto",  -- default
    blur_radius = 32,
    opacity = 0.75,
}
```

### Valid `blur_type` Values

| Value | Platform | Backend |
|---|---|---|
| `cgs` | macOS | CGS private SPI, `blur_radius` applies |
| `glass:regular` | macOS 26+ | NSGlassEffectView Regular |
| `glass:clear` | macOS 26+ | NSGlassEffectView Clear |
| `material:window` | macOS | NSVisualEffectView WindowBackground |
| `material:fullscreen` | macOS | NSVisualEffectView FullScreenUI |
| `acrylic` | Windows 11 | accentEnableAcrylicBlurBehind |
| `mica` | Windows 11 22H2+ | DwmSetWindowAttribute SYSTEMBACKDROP_TYPE=2 |
| `blur` | Windows 10 | accentEnableBlurBehind + DwmEnableBlurBehindWindow legacy fallback |
| `auto` | both | macOS → `cgs`, Windows 11 → `acrylic`, Windows 10 → `blur` |

### Fallback Chain

user chosen → auto → solid (disable blur, opaque window)

"User chosen" fails (e.g. `glass:regular` on pre-Tahoe, `mica` on Win10) → falls through to `auto`. `auto` fails (e.g. CGS symbols missing) → solid.

Three tiers, explicit, no silent magic.

## Language / Framework Constraints

C++ / JUCE reference implementation. All BLESSED enforced. ObjC++ (.mm) for macOS-specific AppKit interop. `@available(macOS 26.0, *)` for NSGlassEffectView runtime dispatch.

## Locked Decisions

1. **All three macOS backends kept.** CGS, NSVisualEffect, NSGlassEffect — user-selectable.
2. **NSVisualEffect materials curated to 2:** WindowBackground, FullScreenUI.
3. **NSGlassEffect styles: 2:** Regular, Clear.
4. **Windows backends: 3:** acrylic (Win11), mica (Win11 22H2+), blur (Win10).
5. **`auto` default:** macOS → CGS. Windows 11 → acrylic. Windows 10 → blur.
6. **Fallback is explicit:** user chosen → auto → solid. No silent substitution within tiers.
7. **Single lua key `blur_type`** — colon-delimited for variants.
8. **Sheet + GL deferred.** NSSheet modal presentation is a separate sprint.

## Validation Gate

Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- ~/.carol/JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: Expand BackgroundBlur Type Enum + Dispatch

**Scope:** `jam/jam_style/background_blur/jam_background_blur.h`, `jam_background_blur.mm`, `jam_background_blur.cpp`

**Action:**

*Header (jam_background_blur.h):*
- macOS `Type` enum: `cgs`, `glassRegular`, `glassClear`, `materialWindow`, `materialFullscreen`
- Add `static bool isGlassEffectAvailable()` — runtime `@available(macOS 26.0, *)` check, cached
- Add private `applyNSGlassEffect` declaration
- Windows `Type` enum: `acrylic`, `mica`, `blur`

*macOS Implementation (jam_background_blur.mm):*
- `isGlassEffectAvailable()` — `static const bool` via `@available(macOS 26.0, *)`
- `enable()` dispatch: switch on all 5 macOS types
- `applyNSGlassEffect(component, colour, style)` — new, handles Regular/Clear via `NSGlassEffectViewStyle`
- `applyNSVisualEffect()` — parameterize material (WindowBackground or FullScreenUI)
- `disable()` cleanup: remove NSGlassEffectView + NSVisualEffectView subviews. Zero CGS radius.

*Windows Implementation (jam_background_blur.cpp):*
- `enable()` dispatch: switch on `acrylic`, `mica`, `blur`
- `applyMica()` — new, uses `DwmSetWindowAttribute` with `DWMWA_SYSTEMBACKDROP_TYPE = 2`
- Existing `applyDwmGlass()` splits into acrylic path (Win11) and blur path (Win10), no longer auto-branching on `isWindows10()` — the caller decides

**Validation:**
- All enum values map 1:1 to config strings
- No early returns, braces on new line, `not`/`and`/`or`, brace init

### Step 2: Lua Config Parsing + BlurStyle Resolution

**Scope:** `Source/lua/Engine.h`, `Source/lua/EngineParseDisplay.cpp`, `Source/config/default_display.lua`

**Action:**
- `Engine::Display::Window` — add `juce::String blurType { "auto" }`
- `parseDisplayWindow()` — parse `blur_type` string from lua table
- `default_display.lua` — add `blur_type = "auto"` with comment listing all valid values
- Add free function or static method to resolve `blur_type` string → `BackgroundBlur::Type` with fallback chain:
  1. Parse user string → platform Type
  2. Invalid/unavailable for platform → resolve `auto`
  3. `auto` fails → return sentinel meaning "solid/disable"

**Validation:**
- `blur_type` key parsed, default "auto"
- Resolution function handles all strings, all platforms, all fallback

### Step 3: Wire Config to Callsites

**Scope:** `Source/Main.cpp`, `Source/component/LookAndFeelMenu.cpp`, `Source/component/ModalWindow.cpp`

**Action:**
- `Main.cpp` — resolve `blurType` string to `BackgroundBlur::Type`, pass to `setGlass()` or `BackgroundBlur::enable()` directly
- All surfaces (main window, popup menus, modal windows) use the resolved type
- Hot-reload path (lua config change) re-resolves and re-applies

**Validation:**
- All three surfaces respect user config
- Hot-reload works
- Fallback chain tested: invalid value → auto → solid

### Step 4: Verify All Surfaces + Fallback

**Scope:** verification only — no code changes expected

**Action:**
- Verify each `blur_type` value on its target platform
- Verify fallback: wrong-platform value → auto → solid
- Verify `disable()` cleans up all subview types (NSGlassEffectView, NSVisualEffectView)
- Verify Windows: acrylic on Win11, mica on Win11 22H2+, blur on Win10
- Verify `blur_radius` only applies to `cgs` (macOS) — ignored by other backends

**Validation:**
- Every config value produces correct visual result on correct platform
- Every wrong-platform value falls through correctly
- No orphaned subviews after disable

---

## Dependency Chain

```
Step 1 (BackgroundBlur enum + dispatch)
  → Step 2 (Lua config + resolution)
    → Step 3 (Wire callsites)
      → Step 4 (Verify all surfaces)
```

Four steps, linear.

## BLESSED Alignment

- **B (Bound):** Subviews owned by NSWindow hierarchy. Lifecycle matches window.
- **L (Lean):** One config key, one resolution function, one dispatch point.
- **E (Explicit):** All backends user-selectable. Fallback chain documented. No silent magic.
- **S (SSOT):** BackgroundBlur::enable() single dispatch for all blur. blur_type string is SSOT for user intent.
- **S (Stateless):** BackgroundBlur remains static utility. Resolution is pure function.
- **E (Encapsulation):** All platform dispatch internal to BackgroundBlur .mm/.cpp. Callers pass Type enum.
- **D (Deterministic):** Same config + same OS = same backend. Fallback chain is fixed order.

## Risks

1. **Mica requires Win11 22H2+.** `DwmSetWindowAttribute` with `DWMWA_SYSTEMBACKDROP_TYPE` may fail on older Win11 builds. Fallback handles it.
2. **NSGlassEffectView requires macOS 26.0.** `@available` runtime check. Fallback handles it.
3. **CGS private SPI.** Always was private, works through macOS 26. If Apple kills it, user switches config. No auto-fallback within the CGS path — it either works or falls to auto.

## Deferred

- NSSheet modal presentation (fork kuassa showSheet/endSheet into jam)
- GL renderer on sheet path
- Terminal::Popup + Action List swap to sheet
