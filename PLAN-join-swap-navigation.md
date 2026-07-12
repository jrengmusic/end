# PLAN: Pane Join + Swap + Navigation — Binary-Space Graph Verbs

**RFC:** none — objective from ARCHITECT prompt; semantics ratified in chat this session against Blender source (`~/Documents/Poems/dev/blender/`)
**Date:** 2026-07-12
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM — header-preferred, 300/30/3

## Context

The binary-space substrate is complete (PLAN-binary-space.md, Sprint 71): flat EDGE rows whose head/tail name a SPACE (PANE or EDGE uuid), recursive-descent layout from the derived root. Navigation actions and keybindings are fully wired END-side (`pane_left/right/up/down` → `TabView::focusPane`, ENDActions.cpp:175-205, keys.lua:100-109) but dead at `TabView::findNearestPane` (TabView.cpp:56-60, returns nullptr). Join and swap do not exist anywhere — no verbs, no actions, no bindings.

**Ratified decisions (locked this session):**

1. **Blender is the reference** — semantics read from source, not priors:
   - JOIN: initiator survives, absorbs the neighbor across a shared wall (`screen_area_join_aligned`, screen_edit.cc:419; eligibility `area_getorientation`, screen_edit.cc:246-256).
   - SWAP: content exchanges, geometry untouched (`ED_area_swapspace`, area.cc:2756).
2. **Join eligibility: any full-shared-wall** (ARCHITECT-ruled) — initiator's whole wall == neighbor's whole wall; not restricted to graph siblings.
3. **Join mechanism: collapse + rotation** (ARCHITECT-ratified) — absorbed pane collapses out (existing childRemoved graph collapse), then the wall EDGE and the initiator-side child EDGE rotate (swap nesting). Sibling case degenerates to plain collapse. Worked examples below are the acceptance semantics.
4. **Nearest-pane resolution: graph walk** (ARCHITECT-ruled) — zero pixel reads. Ascend to the bounding EDGE of the direction's orientation with focused on the near side; descend the far side — parallel cuts take the wall-near child, perpendicular cuts take the child whose span contains focused's centre. Spans/centres derived from proportions on a unit rect (same cut math as layout's `splits` map).
5. **Actions: `join_left/right/up/down`, `swap_left/right/up/down`** (ARCHITECT-ruled). Modal keys: `ctrl+h/j/k/l` = join, `shift+h/j/k/l` = swap; `h/j/k/l` stays focus navigation.
6. Direction tokens reuse the existing `ID::paneLeft/paneRight/paneUp/paneDown` identifiers — no new direction vocabulary.
7. Corner join gesture, popup menu, glassmorphism stay queued — not this sprint.

## Design Contract

### Direction ↔ graph mapping (shared by navigation, swap, join)

Ascend from pane uuid via `getParent` (existing SSOT lookup, jam_MatrixComponent.cpp:18):
- **left** → nearest VERTICAL ancestor EDGE with current in TAIL (wall on pane's left)
- **right** → nearest VERTICAL ancestor with current in HEAD
- **up** → nearest HORIZONTAL ancestor with current in TAIL
- **down** → nearest HORIZONTAL ancestor with current in HEAD

(Same low/high orientation logic as `reducePane`'s ancestor walk, jam_MatrixComponent.cpp:79-94.)

Descend the wall's far slot:
- PANE row → found.
- EDGE same orientation as wall → wall-near child (right/down: head; left/up: tail).
- EDGE perpendicular → child whose normalized span contains focused's centre along that axis.

Normalized geometry (spans, centres) computed by pure descent over the graph on a unit rect using the existing `splits` Function::Map — no component bounds, no pixels.

### jam::MatrixComponent — new public verbs

- **`getNeighbor (jam::UUID pane, const juce::Identifier& direction) → jam::UUID`** — the walk above. `UUID::none()` when no wall exists in that direction (pane at container boundary).
- **`swap (jam::UUID a, jam::UUID b)`** — exchange the two PANE uuids in their referencing EDGE slots (same-edge sibling case: swap head/tail values). Property writes + `layout()`. Components untouched — content moves, geometry slots stay; keyboard focus rides the focused component (focus contract untouched).
- **`join (jam::UUID pane, const juce::Identifier& direction) → jam::UUID`** — resolve target via the walk; validate full-shared-wall (pane's normalized span along the wall's cut line == target's span); ineligible or no wall → return `none()`, no mutation (result return, no bail-out guard). Eligible → capture wall edge, `remove (target)` (existing hook chain performs the collapse), then if the wall survived (non-sibling case) rotate the wall EDGE with the pane-side child EDGE, `layout()`, return the absorbed uuid.
- Private rotation primitive — `rotate (wall, child)` (Names section).

**Worked examples (acceptance semantics, proportions preserved):**

2×2 — A absorbs C (down neighbor):
```
before: x{head=y,tail=z} horiz · y{head=A,tail=B} vert · z{head=C,tail=D} vert  (y.p == z.p)
collapse C:  z dies, D inherits → x{head=y, tail=D}
rotate y↔x:  y{head=A, tail=x} · x{head=B, tail=D}
after:  A full-height left column · right column B over D
```
Column stack — A absorbs B (right neighbor):
```
before: x{head=y,tail=z} vert · y{head=A,tail=Q} horiz · z{head=B,tail=C} horiz  (y.p == z.p)
collapse B:  z dies, C inherits → x{head=y, tail=C}
rotate y↔x:  y{head=A, tail=x} · x{head=Q, tail=C}
after:  A full-width top row · bottom row Q | C
```
Sibling case: collapse only, no rotation (wall == target's parent, dies in the collapse).

### END surfaces

- **`TabView::childRemoved` override (new):** strips the target's PANE state row, then delegates to `MatrixComponent::childRemoved`. `TabView::remove` collapses to plain `MatrixComponent::remove` — close and join ride ONE removal path (SSOT).
- **`TabView::focusPane`:** rewritten on `getNeighbor (getFocusedChild(), direction)`; found → `get (target).toFront (true)` (existing focus pattern, TabView.cpp:69). `findNearestPane` and `findFocusedPane` DELETED (state `getFocusedChild()` is the SSOT — hasKeyboardFocus iteration is a second copy).
- **`TabView::join (direction) → jam::UUID`** and **`TabView::swap (direction)`** — thin wrappers over the matrix verbs keyed on `getFocusedChild()`.
- **`ENDActions.cpp`** — eight new registrations mirroring the `paneLeft` family shape (ENDActions.cpp:175-205):
  - `swap_*`: `tabView->swap (ID::pane<Dir>)`.
  - `join_*`: `const auto target { tabView->join (ID::pane<Dir>) }`; valid → `nexus.getActiveSession().removeTerminal (target)`. **Note (visible decision):** terminal engine retirement follows the view surgery here (join must resolve+validate before anything dies), inverting closePane's engine-first order — engines persist independently of ephemeral views (ARCHITECTURE.md session layer), so order is safe; symmetry deviation stated for the record.
  - Last-pane/no-neighbor cases are no-ops via `none()` result — no guards.
- **`Source/Identifier.h`** — X-macro entries: `joinLeft/joinRight/joinUp/joinDown`, `swapLeft/swapRight/swapUp/swapDown` (`join_left` … `swap_down`).
- **`Source/config/lua/keys.lua`** — modal block: `join_left = ctrl+h` family, `swap_left = shift+h` family (exact key-string format per the existing keys.lua parser convention — Engineer reads the parser before writing).

### Deletion ledger (delete first)

- `TabView::findNearestPane` (TabView.h:38, TabView.cpp:56-60) — stub, superseded by `getNeighbor`.
- `TabView::findFocusedPane` (TabView.h:36, TabView.cpp:47-54) — second copy of state truth.
- `MatrixComponent::find` stub (jam_MatrixComponent.h:55, .cpp:129) — consumerless nullptr stub; Engineer verifies zero call sites, then deletes.

## Validation Gate

Each step validated by @Auditor against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and this plan before the next. ARCHITECT builds and runs — agents never build. Discrepancy between the ratified mechanism and a deeper-nesting case → STOP, discuss.

## Steps

### Step 1: jam graph verbs
**Scope:** `jam_gui/layout/jam_MatrixComponent.h/.cpp`.
**Action:** `getNeighbor` (ascend/descend walk, unit-rect spans via `splits`), `swap`, `join` (eligibility + collapse + rotation, worked examples as spec), private rotation primitive. Delete `find` stub (after call-site verification).
**Validation:** @Auditor — walk matches the direction mapping table; join reproduces both worked examples on paper; zero pixel/bounds reads in graph logic; no new vocabulary beyond ratified names; no manual focus.

### Step 2: END wiring
**Scope:** `Source/end/TabView.h/.cpp`, `Source/action/ENDActions.cpp`, `Source/Identifier.h`, `Source/config/lua/keys.lua`.
**Action:** `childRemoved` override + `remove` unification; `focusPane` on `getNeighbor`; `join`/`swap` wrappers; eight actions; identifiers; modal bindings. Deletion ledger executed.
**Validation:** @Auditor — one removal path; stubs gone; action shape mirrors the existing paneLeft family; keys parse per existing convention.

### Step 3: Audit clean sweep
@Auditor over all touched files; every finding resolved or ARCHITECT-ruled.

### Step 4: Doxygen (dedicated delegation, LAST)
New verbs on MatrixComponent + TabView surfaces. Zero warnings.

### Step 5: Docs sync
ARCHITECTURE.md (matrix verb surface), SPEC.md (navigation/join/swap functional lines), CLAUDE.md current state. Sprint log on ARCHITECT command only.

## Names for ratification

- `getNeighbor` / `swap` / `join` — matrix verb names (getNeighbor mirrors existing getParent/getFocusedChild shape; swap/join are the ARCHITECT-named operations).
- Private rotation primitive: `rotate (wall, child)`.
- `joinLeft…swapDown` identifier spellings as listed.

## BLESSED Alignment

- **B:** no new ownership — verbs mutate the existing graph; components stay bound to rows.
- **L:** two TabView stubs + matrix find stub deleted; close/join unified onto one removal path.
- **E:** direction mapping and eligibility are explicit graph statements; no inferred adjacency, no pixel probing.
- **S (SSOT):** seam graph remains the single truth — neighbor, eligibility, spans all derived; findFocusedPane second-copy eliminated.
- **S (Stateless):** no cached adjacency, no navigation state; every query walks fresh.
- **E (Encaps):** panes never poked — navigation focuses via toFront (widget claims), join/swap end at layout().
- **D:** same graph + same direction → same neighbor, same surgery, same pixels.

## Risks / Open Questions

- Deep-nesting join (initiator several levels below the wall with equal spans): ratified mechanism covers the worked depths; if a reachable case requires iterated rotation, STOP mid-execution and present the trace (Validation Gate).
- keys.lua modifier-string format for modal bindings (`ctrl+h` vs parser-specific spelling) — Engineer reads the parser; no format invented.
