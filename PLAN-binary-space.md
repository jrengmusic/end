# PLAN: Binary-Space Pane Substrate — PaneEdge Graph

**RFC:** none — objective from ARCHITECT's rulings, discussed and ratified in chat this session
**Date:** 2026-07-12
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM — header-only preferred ~300 LOC (LANGUAGE.md); 300/30/3 unchanged

## Context

The seam-truth substrate landed (flat EDGE rows, pane-pair head/tail, tree-order replay layout, PANE side-prop inverse index) but removal exposed the model's gap: a closing pane's edges have no principled resolution, and per-pair seams on one wall cannot move independently (a pane is rigid — its wall is one line). ARCHITECT re-derived head/tail: not a pane-pair relationship but the relationship of SPACE occupied by splits relative to the MatrixComponent bounds — a binary space graph, held flat by reference.

**Ratified model (locked this session):**

1. **head/tail name a SPACE:** each is a UUID naming either a PANE or another EDGE. An edge's UUID *is* its space (union of everything under it). Rows stay flat siblings; nesting is pure metadata through the UUID graph.
2. **Layout = recursive descent.** Root space = `getLocalBounds()`; at each EDGE: bar bounds = region, cut at proportions, head/tail each either a pane (`setBounds`) or an edge (recurse). One path — split, removal, drag, container resize all end here.
3. **Root is derived** — the one row UUID no EDGE row references as head/tail. Zero new state.
4. **Removal is a pure graph op** — the pane's parent EDGE dies; the sibling space inherits the slot in the grandparent's head/tail. Sibling space is by construction the exact complement of the reclaimed pane. No orphan iteration, no adjacency scan.
5. **PANE side props die** (`top=`/`left=`/`right=`/`bottom=` stamping deleted). A pane's bounding seam is derived: nearest ancestor EDGE of that orientation with the pane on the far side.
6. **Proportions stays** on the EDGE row — drag writes it, layout consumes it (owner listener on `jam::ID::proportions` fires `resized()`, existing machinery jam_OwnerComponent.h:152-168).
7. **Focus contract (CONTRACT-level, ratified):** pane focus is self-report obeying juce::Component focus machinery; parents listen and track; NO orchestrator ever assigns focus manually. Removal relies on JUCE OOTB: `removeChildComponent` gives focus away and parent `grabKeyboardFocus()` → traverser dispatches (juce_Component.cpp:1523-1535, :3004-3010). Future widget panes (Terminal, Whelmed, SURF) opt in via `wantsKeyboardFocus` + `toFront (true)`.
8. **Rename:** `PaneResizerBar` → `PaneEdge`. EDGE UUID == seam UUID == component identity (`componentID`).
9. Closure: split divides one leaf in two, removal collapses one node — every reachable layout stays binary-representable; invalid tilings unrepresentable.

## Design Contract

### State schema

```
MATRIX row {id, focus, bounds, focusedPane}                       — OwnerComponent row, untouched
├─ PANE rows {id, focus, bounds}                                  — side props DELETED
└─ EDGE rows {id, focus, bounds, head, tail, orientation, proportions}
                                                                  — head/tail: UUID of PANE or EDGE
```

Example (A | (B / C)):
```
EDGE x  head=A  tail=y      vertical
EDGE y  head=B  tail=C      horizontal
```

### jam::PaneEdge (`jam_gui/layout/jam_PaneEdge.h`, header-only; `jam_PaneResizerBar.h` deleted)

Current PaneResizerBar minus the side-prop machinery:
- ctor: registers head/tail (`Parameter<jam::UUID>`), orientation (`ParameterText`), proportions (`Parameter<float>`) on own EDGE row; `setWantsKeyboardFocus (false)` / `setRepaintsOnMouseActivity (true)` / `setAlwaysOnTop (true)`. **Sibling stamping deleted** (jam_PaneResizerBar.h:27-33). head = low-side space (left/top), tail = high-side.
- dtor: **empty** — side-prop clearing deleted (jam_PaneResizerBar.h:36-49). Row removal stays the explicit remove verb in MatrixComponent (established contract).
- `getSeam()` / `hitTest` / `mouseDown` / `mouseDrag` / `paint` / `resized` / accessors — carried unchanged. Drag writes own `jam::ID::proportions`; layout fires via owner listener.

### jam::MatrixComponent (`.h/.cpp` rewrite)

- **`split (edge, position)`** — binary graph op:
  1. `pane = getFocusedChild()`; mint `newPane`, `add (newPane, createChild (newPane))`.
  2. Construct PaneEdge: members ordered by `newLeads` (existing rule, jam_MatrixComponent.cpp:30), head/tail = {pane, newPane}.
  3. Slot rewrite: the EDGE row whose head or tail == pane's id (if any — root pane has none) now names the new edge's id instead.
  4. Side-prop inheritance loop (jam_MatrixComponent.cpp:25-28) **deleted**. `resized()`.
- **`childRemoved (uuid)`** — graph collapse:
  1. Parent edge = EDGE row with head or tail == uuid. None → uuid was root, nothing structural (last pane; tab closes at END level, existing behavior).
  2. Sibling = parent edge's other member. Grandparent slot naming parent edge's id → rewritten to sibling's id (root case: no grandparent, sibling becomes derived root).
  3. Destroy the parent PaneEdge (bars entry) + remove its EDGE row explicitly. `resized()`.
  4. **Zero focus code** — JUCE machinery dispatches (Design Contract §7).
- **`reducePane (pane, axis, step)` / `expandPane`** — graph walk replaces side props:
  - Bounding seam per side = walk ancestors from pane (parent = EDGE naming current id): nearest vertical ancestor with current in tail → left seam; in head → right seam (top/bottom mirror for height).
  - Distributed step preserved: count found seams, `shiftSeam (edgeRow, ±step/count)` — centre-shrink semantics unchanged.
  - `shiftSeam` retargeted: takes the found EDGE row directly (side-prop lookup jam_MatrixComponent.cpp:61-64 deleted); minShare clamp from bar geometry kept.
- **`layout()`** — recursive descent, replaces replay (jam_MatrixComponent.cpp:100-133):
  - Derive root: the one row id unreferenced by any EDGE head/tail.
  - Descend (private `layout (jam::UUID, juce::Rectangle<int>)` overload — no new name): EDGE row → bar `setBounds (region)`, cut via existing `splits` Function::Map (left/top keyed on orientation, explicit template args preserved), recurse head with cut, tail with remainder; PANE row → `get (uuid).setBounds (region)`.
- `bars` OwnedArray stays (owner only, never iterated for logic); `find()` stub untouched this plan.

### END surfaces

- `ENDLookAndFeel.cpp` drawResizerBar (:292+): cast retargets `jam::PaneEdge&`. LnF virtual names and config/theme keys (`drawResizerBar`, `getPaneResizerBarSize`, `pane.resize_bar_thickness`, `resizer_bar` SVG) — see Names for ratification.
- `TabView`, `ENDActions`, keybindings, `display.lua` — signatures unchanged, no edits expected beyond the LnF cast and any `PaneResizerBar` type references.

### Deletion ledger (delete first)

- `jam_PaneResizerBar.h` — replaced by `jam_PaneEdge.h`; `jam_gui.h` include updated.
- PaneResizerBar sibling stamping + dtor clearing (:27-33, :36-49).
- MatrixComponent split inheritance loop (.cpp:25-28), shiftSeam side-prop lookup (.cpp:61-64), replay layout (.cpp:100-133 iteration structure).
- `jam::Position::getLowPropertyId` / `getHighPropertyId` (jam_Position.h:63-73) **if** the rewrite leaves them consumerless — Engineer verifies all call sites before deleting.

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and this plan before the next. ARCHITECT builds and runs — agents never build.

## Steps

### Step 1: Substrate rewrite (jam)
**Scope:** `jam_gui/layout/jam_PaneEdge.h` (new), `jam_PaneResizerBar.h` (deleted), `jam_MatrixComponent.h/.cpp`, `jam_gui.h`, `jam_Position.h` (dead API check).
**Action:** execute Design Contract in full — delete first. Space-graph split, graph-collapse removal, ancestor-walk reduce/expand, recursive-descent layout, derived root. No manual focus anywhere.
**Validation:** @Auditor — head/tail as space refs only; no side props written anywhere; no manual focus; layout single path; no new vocabulary beyond ratified names; deletion ledger executed.

### Step 2: END retarget
**Scope:** `ENDLookAndFeel.cpp/h`, any END `PaneResizerBar` reference.
**Action:** retarget cast to `jam::PaneEdge`; apply whichever LnF/API renames ARCHITECT ratifies.
**Validation:** @Auditor — zero stale `PaneResizerBar` references END-wide.

### Step 3: Audit clean sweep + DIAG sweep
@Auditor over all touched files; every finding resolved or ARCHITECT-ruled. DIAG lines swept (jam_OwnerComponent.h:155-156 debug::Log).

### Step 4: Doxygen (dedicated delegation, LAST)
PaneEdge, MatrixComponent, OwnerComponent, OwnedComponent, PaneComponent + touched END surfaces. Zero warnings.

### Step 5: Docs sync
ARCHITECTURE.md, SPEC.md, CLAUDE.md reflect the binary-space contract. Sprint log on ARCHITECT command only.

### Queued behind the substrate (carried, ratifications intact)
- join + swap + pane navigation · corner join gesture · popup menu + glassmorphism.
- Engineer Case-3 flags awaiting disposition: ENDActions closePane focused-pane read (:112-133), TabView.cpp:102 getComponentID parse, split loop unused `name` binding, find/childRemoved unused params (childRemoved resolves in Step 1).

## Names for ratification

- `jam::PaneEdge` (class + file) — ARCHITECT's rename.
- LnF virtuals: keep `drawResizerBar` / `getPaneResizerBarSize`, or rename `drawPaneEdge` / `getPaneEdgeSize` (+ END overrides). Config/theme keys (`pane.resize_bar_thickness`, `resizer_bar` SVG) follow the same ruling.
- Private `layout (jam::UUID, juce::Rectangle<int>)` descent overload — reuses the existing verb, no new word.

## BLESSED Alignment

- **B:** PaneEdge RAII-bound to MatrixComponent (`bars`); one owner per row; focus lifecycle owned by JUCE machinery.
- **L:** side-prop stamping/clearing, inheritance loop, replay bookkeeping all deleted; one descent path.
- **E:** graph is explicit metadata — head/tail state every space by name; no inferred adjacency.
- **S (SSOT):** seam graph stored once; cells, root, bounding seams, focus heir all derived. Side-prop second copy eliminated.
- **S (Stateless):** components hold no layout members; MatrixComponent tracks nothing between ops.
- **E (Encaps):** PaneEdge owns its row, panes self-report, owner listens — no orchestrator poking (focus included).
- **D:** same graph + same proportions → same descent → same pixels.

## Risks / Open Questions

- LnF/config naming ruling (above) — only open point; does not block Step 1.
