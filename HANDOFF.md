# Handoff to COUNSELOR: Pane Layer — Working Skeleton, ARCHITECT Codes the Data Structure

**From:** COUNSELOR
**Date:** 2026-07-09
**Status:** In Progress — working skeleton landed, ARCHITECT implements the pane data structure himself

## Context

The session-manager-mux sprint hit the pane layer and burned four rewrites (SCRATCH pane∕bar pair → EDGE event-driven → skeletons → working skeleton). Root cause named by ARCHITECT and confirmed: **layer crossings** — Session (engine) walked the UI tree, views authored model state, bars authored pane geometry, multiple writers per truth — plus repeated protocol failures on my side (constraints lost in delegation prose, outputs never verified against instructions before reporting, invented patterns/names). ARCHITECT has taken the pane data-structure implementation himself; the codebase is restored to a clean working skeleton he can build on.

## Completed

- **Working skeleton (runs)**: newTab opens a tab with a TerminalView stub (plain `juce::Component`, paints its uuid); tab create/switch/close/titles work; splitHorizontal/splitVertical are registered no-ops; closePane keeps its count guard; pane navigation no-ops safely.
- **Session decoupled (engine/UI split enforced)**: owns TerminalProcessors keyed by the terminal's OWN uuid; `newTerminal (uuid)` = try_emplace only; `removeTerminal (uuid)` = erase only; zero tree walking; zero pane vocabulary in signatures, bodies, or params; focusedPane reaction removed.
- **ENDView**: `Tabs tabs` plain value member (agent-invented `adoptTabs` helper deleted); tabs full window rect; WINDOW `paneManager` member + `components` pool dormant; `createDockPane` empty sequence-comment stub.
- **jam pane layer = dormant skeletons** (files kept, nothing deleted): `jam_PaneManager.h` is the ratified spec header (see Key Decisions); `.cpp` stub bodies; `jam::PaneComponent` dumb adopt component (focus self-report only); `jam::PaneResizerBar` dumb drag self-reporter (writes only its EDGE row's `jam::ID::position`).
- **jam landed for real** (earlier this session): `jam::Model` per-instance parameter identity (groupId = TYPE#id, jam_Model.cpp:233-240); `updateAdapterConnections` now restores rebound adapters (rebind-without-restore fixed); `jam::LookAndFeel::Custom::getPaneResizerBarSize()` virtual (default 8; ENDLookAndFeel overrides, `override` keyword fixed at ENDLookAndFeel.h:229); `X (edge, "edge")` identifier row; `jam::map::Axis` and `jam::map::Edge` bimaps both dead.

## Remaining

- ARCHITECT codes the pane data structure himself. COUNSELOR holds, reads on demand, answers with file:line.
- After his structure lands: real terminal rebuild (TerminalProcessor from scratch, jam::Model::Listener only — long-standing sprint goal), then SMX Step 5 (SessionList — task #40), Step 7 (serialization one-path — #42), Step 8 (daemon — #43).
- `ID::focusedPane` param still registered by Panes ctor (Panes.cpp:14) — nothing writes it now (TerminalView no longer self-reports focus); Tabs::currentTabChanged falls back to `panes->get().grabKeyboardFocus()` — works single-pane.
- Tab titles resolve empty (terminal state no longer grafted anywhere; getTitle's cwd/process lookup finds nothing) — inherent to stub scope.

## Key Decisions (ratified this session — bind all future pane work)

1. **META-MVP mapping** (ARCHITECTURE.md:933): PaneManager = Processor/Orchestrator, its ValueTree = Model, PaneComponent = dumb told-object. NO component watches trees — no valueTreeChildAdded/Removed on any juce::Component, container or leaf. PaneManager is the only VT listener in the layer.
2. **1D edge-scalar geometry** (Blender ScrVert isomorphism, researched at source — tmux layout tree rejected): only stored geometry = EDGE rows (IDtype::edge: `jam::ID::id` plain, `jam::ID::value` + `jam::ID::position` registered Parameter<int>); PANE rows carry four registered Parameter<int64_t> edge references (`jam::ID::left/top/right/bottom` = EDGE ids). Rects derived via `juce::Rectangle::leftTopRightBottom`. Adjacency = shared reference. Axis derived (vertical iff referenced via left/right). Boundary edges stored uniform (four real EDGE rows).
3. **ONE-AUTHOR LAW**: PaneManager sole author of rows/references/values (split/remove/drag-reaction/rescale). Bar's only write = `jam::ID::position` on its own EDGE row; PaneManager clamps → writes `value` → tells container to relayout (Orchestrator TELL, downstream).
4. **Bars = PaneManager state/machinery** — owned by PaneManager, one per split-born edge, never pane children, never END-visible pools.
5. **Remove tie-rule**: seam qualifies iff removed pane is sole-sided on it; best Blender alignment score (min/max perpendicular-extent ratio) wins; rewire survivors' references to the far edge.
6. **`jam::ID::id` = the one plain-property exception** (identity, not value — parameter identity derives FROM it). Everything else registered (ALL-PARAMETER).
7. **Session = engine**: terminals keyed by own uuid, no pane/tab vocabulary, no tree walking. Placement/association is a UI-layer verb.
8. **Verbs complete transitions**: state → component → layout in one deterministic call stack. No reactive relayout lanes (the HashMap::at crash class = pool observed lagging the tree mid-verb).
9. **splitHorizontal → jam::ID::bottom, splitVertical → jam::ID::right is CANON** ("horizontal" names the divider; display.lua:282-283 doc comment was the wrong artifact and was corrected; keys.lua untouched).
10. **Workflow law**: working stubs only (app always runs); skeleton-first (spec = code skeleton on disk, agents fill bodies only); NO doxygen until structure is tested and ratified; no new names/patterns ever (Function::Map/registerEvents/Bimap/HashMap are the codebase's dispatch canon); COUNSELOR verifies every produced file line-by-line against the verbatim constraint list before reporting anything.

## Files Modified (this session's surviving surface)

- `Source/end/Session.cpp/.h` — pane vocabulary stripped, tree ops deleted, params renamed to `uuid`
- `Source/end/Panes.h/.cpp` — working stub container (pool of TerminalView, full-rect single pane), PaneManager member dormant
- `Source/terminal/TerminalView.h/.cpp` — dumb stub: `explicit TerminalView (jam::UUID)`, paints uuid
- `Source/end/ENDView.h/.cpp` — Tabs value member, adoptTabs deleted, dormant pane machinery
- `Source/end/ActionRegistration.cpp` — splitH/V no-ops; createDockPane sequence-comment stub; newPane uses `panes->add()` returns
- `Source/Bimap.h` — Edge/Axis aggregate members removed; `Source/lookAndFeel/ENDLookAndFeel.h:229` — `override`
- jam: `jam_gui/layout/jam_PaneManager.h` (spec header) `.cpp` (stubs), `jam_PaneComponent.h/.cpp`, `jam_PaneResizerBar.h/.cpp`, `jam_gui.h/.cpp`, `jam_data_structures/model/jam_Model.cpp`, `jam_look_and_feel/jam_LookAndFeelCustom.h`, `jam_core/identifier/jam_IdentifierLayout.h`

## Open Questions

- Session.h doxygen: 12 lines still cite deleted flows/symbols (focusedPane reaction, PANE grafting, jam::PaneManager, closePane) — flagged, ARCHITECT disposition pending.
- Panes.h doc comments describe the pre-stub paneManager flow — stale, same disposition.
- `uuid` params shadow Session's `uuid` member (harmless; `-Wshadow` would flag).
- jam::Model has NO parameter/adapter removal surface — rows deleted with registered parameters orphan their adapters in `Model::adapters` for the Model's lifetime (jam_Model.cpp:196-199 never touches adapters). Exercised by any future remove(); pre-existing gap.
- `Source/sidebar/SidebarComponent.h:4` — ARCHITECT's own new work derives `jam::PaneComponent`; never touched by agents; future pane changes must account for it.

## Next Steps

- HOLD. ARCHITECT is implementing the pane data structure himself ("so you can understand how PROFESSIONAL CODE CORRECT DATA STRUCTURE").
- When he returns direction: read his code first as ground truth, then resume per his scope. Protocol record: 3+ failures this session — constraint loss in delegation prose, unverified outputs, invented names (adoptTabs treated as canon, Slot tables, getContentBounds). The verification gate (Key Decision 10) is non-negotiable.
