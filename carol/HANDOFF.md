# HANDOFF — END blank-rendering sprint, 2026-09-12

**Rule for the next session:** read this file, then read the cited code before any
statement. Claims below carry file:line read in the prior session. Do not re-open
ELIMINATED items. Do not trust anything without its citation.

---

## 1. ROOT CAUSE — FOUND, NOT YET FIXED

### The defect
Rendering is nondeterministic: historically "sometimes rendered, corner-only
outline, tabs without flex"; currently first frame renders, then blank. Window
glass stays. Component tree and Vulkan present lane are healthy throughout.

### The mechanism (all read, cited)
1. Every END visual is a multi-subpath SVG flex path. Multi-subpath routes
   through `fillComplexPath` (branch test: subpath count —
   jam_VulkanLowLevelGraphicsContextPath.cpp:96-99, :250-254). Single-subpath
   paths, rects, images, glyphs go straight to the scene and are unaffected.
2. `fillComplexPath` uses ONE shared winding-scratch image per Graphics:
   CLEAR-op pass on the scratch (accumulate + cover record, tex=0), resolve,
   then a composite quad into the scene sampling the resolve (tex = scratch
   bindless index) — Path.cpp:358-387, :394-510, :519-595.
3. The lane has a Write→Read barrier ONLY (resolve write → composite sample,
   Path.cpp:534-538). There is NO Read→Write dependency between composite N's
   fragment-shader READ of the scratch and path N+1's scratch pass, which
   CLEARS and rewrites the same image (Path.cpp:408 `endRenderPass` →
   :419 `beginRenderPass (scratch…)`, nothing between). The render pass has no
   explicit SubpassDependency array; jam's own comment states implicit
   dependencies are insufficient for this image family
   (jam_VulkanGraphics.cpp:1505-1511).
4. END draws SIX complex paths per frame (5 tab-bar flex + full-pane outline —
   proven by the record dump, see §3). Six WAR races per frame on one image.
   GPU scheduling decides per path per frame whether the composite samples its
   own coverage or the next path's clear/garbage. Undefined content is drawn
   full-bounds over the region (the pane outline's bounds = the whole pane),
   which also erased the magenta test fill.

### The fix awaiting ARCHITECT's go (NOT executed)
- Read→Write execution/memory dependency before each scratch pass begin, using
  the existing `recordImageMemoryBarrier` SSOT (same helper and shape as
  Path.cpp:534-538). Ordering-only — cannot change output of already-correct
  schedules; structurally cannot affect frames with fewer than two complex
  paths (plugin_bootstrap flow).
- Secondary finding, same lane, ruling needed: `getOrCreateWindingScratch`
  rewrites a bindless descriptor slot mid-recording on capacity growth
  (jam_VulkanGraphics.cpp:1928-1940) — the pattern the codebase itself forbids
  for non-update-after-bind sets (its own set-2 comment,
  jam_VulkanGraphics.cpp:1406-1409). Not proven to be a live defect this
  session; the 4↔5 texture alternation in the log is explained by deferred
  release + reassign during growth and settles once capacity stops growing.

---

## 2. LANDED THIS SESSION (verified on disk, ARCHITECT-built)

| Change | Files | Status |
|---|---|---|
| prepareWindow delegates to jam::Window::setStyle (StyleTheme shape, jam_StyleTheme.cpp:317-320); direct StyleWindow::apply/BackgroundBlur::enable/setButtons calls deleted; buttons stay on events lane only (end/EventRegistration.cpp:91-97) | END ENDLookAndFeel.cpp:291-301 | done |
| ENDWindow deleted; Main.cpp:42-45 constructs jam::Window directly with the two ConfigModel reads; Main.h member is std::unique_ptr<jam::Window>; project-info.md source rows removed (:350, :352 pre-edit numbering) | END Source/end/, Main.h, Main.cpp, project-info.md | done, cast+ninja regen by ARCHITECT |

Decisions ratified this session (do not re-open): delete ENDWindow; sparse tab
SVG bank is intended (unfocused tabs paint no SVG).

---

## 3. EVIDENCE BASE (instrumented runs, ~/.config/end/END.ode)

- Record dump decode: every frame = pairs of records with identical geometry —
  cover (tex=0, drawn INTO scratch, never the scene) + composite (tex=4/5 =
  scratch bindless slot, drawn into scene). 12 records = 5 tab paths + pane
  outline. Removing the magenta test fill dropped 13→12 — confirms decode.
- Present lane healthy: granted extent tracks 2× logical exactly (up to
  1280x3292), zero zero-extent skips, zero non-proceed acquires, presents
  succeed while screen is blank.
- Narrowed frames legitimately record only what intersects the dirty rect;
  full frames record everything. Recording is CORRECT in all frames — the
  trampling is GPU-side, post-submit.

## 4. ELIMINATED (read end-to-end; do not revisit)

1. Window growth loop — the window is resized to full height by ARCHITECT's
   hammerspoon. External, intentional, a non-issue. Never the defect.
2. JUCE border feedback — with native title bar + windowButtons=false every
   border is zero (juce_ResizableWindow.cpp:168-169,
   juce_DocumentWindow.cpp:271, :279; jam_Window.cpp:131-132 gated off).
3. Swapchain/extent clamp/maxImageExtent — healthy per logs; clamp never hit.
4. Scene persistence LOAD/STORE ops — MSAA color CLEAR→STORE / LOAD→STORE,
   non-transient (jam_VulkanGraphicsSetupRenderPass.cpp:21-22, :85-86;
   SetupSceneTarget.cpp:63-70). Correct.
5. createSceneTarget resets sceneContentValid=false (SetupSceneTarget.cpp:61);
   endFrame sets it true on submit (jam_VulkanGraphics.cpp:741). Correct.
6. Cached-component-image hook — zero setBufferedToImage in END, factory never
   installed. Dead lane for this defect.
7. Null-shader background — paints nothing BY DESIGN
   (jam_VulkanShaderComponent.h:299-303); config `background` value cell is
   empty (~/.config/end/display.md:103). Shader library installed at
   ~/.config/end/shaders/ (singularity, weird, sirenian-dawn, ether, auroras,
   plasma, end). Restoring = one cell edit; ARCHITECT rules which/whether.
8. Stale incremental builds — a clean build (Builds/ deleted) rendered once;
   the WAR race explains the nondeterminism across builds. Build-graph hygiene
   (define flips not invalidating dependents; patched JUCE in $TMPDIR gated by
   .patch-stamp, CMakeLists.txt:116-117) remains a MACHINIST-lane observation,
   not this defect.

## 5. INSTRUMENTATION IN TREE (ODE — remove same sprint, after defect closes)

All `jam::debug::Log::write`, greppable by site-name prefix:
- END ENDLookAndFeel.cpp — "prepareWindow:" (in prepareWindow).
- END ENDView.cpp:57-59 — "ENDView::resized:" (pre-existing, prior sprint).
- jam jam_gui/layout/jam_TabbedComponent.cpp — "TabbedComponent::layout" ×2
  (pre-existing, prior sprint).
- jam jam_gui/windows/jam_Window.cpp — "Window::lookAndFeelChanged:",
  "Window::visibilityChanged:".
- jam jam_gui/windows/jam_StyleWindow.mm — "StyleWindow::apply pre/post:".
- jam jam_gui/desktop/native/jam_BackgroundBlur_mac.mm —
  "BackgroundBlur::enable:", "setVisualFX pre/post", "setGlassFX pre/post".
- jam jam_vulkan/context/jam_VulkanGraphics.cpp — "VulkanGraphics::resize:"
  (entry + granted), "beginFrame: zero extent skip", "beginFrame: acquire
  disposition", "endFrame: records=…" + per-record dump (<20 records),
  "beginSceneRenderPass: load=".
- jam jam_vulkan/engine/jam_VulkanEngine.h — "createContext: llgc …",
  "createContext: narrowed dirty=… / full record" (narrowing else-branch log).

## 6. DEBT / OPEN ITEMS

- DEBT-20260912T080000 (ValueTree reparent assert): fixed prior session,
  awaiting sprint-log receipt.
- DEBT-20260713T230500 (plugin editor steals focus): untouched.
- PLAN file: ~/.claude/plans/immutable-wondering-key.md (LookAndFeel window
  style fix — Steps 1-2 done, Step 3 verification superseded by root-cause
  hunt, Step 4 conditional untriggered, Step 5 = instrumentation drain).
- Auditor has NOT run this sprint.

## 7. VIOLATIONS ON RECORD (this session)

1. Repeated uncited claims from stale context/corpus — worst: calling a log
   record "magenta" after ARCHITECT had deleted the magenta test fill.
   ARCHITECT's standing order: citation-or-read per sentence; no assumptions;
   silence unless productive.
2. Growth-loop theorizing consumed multiple turns before ARCHITECT supplied
   the hammerspoon fact — raising non-issues is a violation, not diligence.
