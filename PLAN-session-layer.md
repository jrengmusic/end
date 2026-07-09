# PLAN: Session Layer — Ratified Topology Implementation

**RFC:** none — objective from ARCHITECT prompt + ratified discussion (captured in SPEC.md §2 / ARCHITECTURE.md "Ratified Target Topology", 2026-07-07)
**Date:** 2026-07-07
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE / JAM — single-header preferred ~300 LOC, 30/3 unchanged, RAII enforced

## Context

The ratified topology is captured in docs but not code. Today: Nexus (bare singleton) maps pane-UUID → terminal::Session 1:1 (Nexus.h:55); GUI creates/destroys state (Panes.cpp:22/:78); terminal::Session bundles document+model+Processor+Resizer (Session.h:253-276); end::Model is Application-owned (Main.h:45); terminal::Model is free-standing; the TABS/PANES topology is GUI-authored and **dies with the View** (jam::Model::Attachment dtor detaches state, jam_Model.h:117-118). Target (ARCHITECT-ruled, full inversion this sprint): `Nexus{end::Model, Owner<end::Session>} → end::Session{Owner<terminal::Processor>}`; Processor absorbs the per-terminal bundle (PluginProcessor-exact) and self-drains; Session authors the persistent SESSION subtree; Views are pure projections adopting existing subtrees. This unlocks daemon/persistence/session-manager (Phase 15) without further topology change.

## Overview

Six steps: two JAM projection seams, then END restructure inside-out — identifiers, Processor absorption (delete terminal::Session first), end::Session + Nexus ownership, GUI projection inversion, bootstrap + doc sync.

## Validation Gate

Each step MUST be validated before proceeding.
Validation = @Auditor confirms compliance with MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/JRENG-CODING-STANDARD.md, and the locked PLAN decisions. ARCHITECT builds (`ninja`) — agents never run builds.

## Steps

### Step 1: JAM — projection seams (adopt + tree-level attach)
**Scope:** `jam_data_structures/model/jam_Model.h` (+ its cpp for ctor bodies), `jam_gui/layout/jam_PaneManager.h/.cpp`, doxygen sync
**Action (@Engineer):**
- `jam::Model::Attachment` gains a **structural tree-level ctor** `Attachment (juce::ValueTree parentState, juce::ValueTree childState)` — appendChild on construction, removeChild on destruction, **no Component walk, no atomic-group registration/removal** (atomics belong to whichever jam::Model owns the subtree's parameters; read the existing dtor implementation and branch structural-only). Existing Component ctor untouched.
- `jam::Model::Component` gains an **adopt ctor** `Component (Model& model, juce::ValueTree existingState)` — state IS the passed tree (ref-counted adoption, no fresh node). Existing TYPE ctor untouched.
- `jam::PaneManager` gains an **adopt ctor** taking an existing PANES tree (state = passed tree; existing empty-root ctor untouched). Topology ops (addLeaf/split/remove) and layout() already operate on `state` — no other change.
**Validation (@Auditor):** no new names beyond ratified overloads; no behavior change to existing ctor paths; structural attachment registers/removes zero atomics; JRENG header discipline; doxygen zero-warning.

### Step 2: END — session identifiers
**Scope:** `Source/Identifier.h`
**Action (@Engineer):** Add `IDtype::sessions` ("sessions") and `ID::activeSession` ("active_session") following the existing X-macro canon (IDENTIFIER_* block placement per established pattern; SESSION child tag reuses existing `jam::IDtype::session` canon — no new per-session tag).
**Validation (@Auditor):** X-macro/Bimap canon followed; snake_case wire strings consistent with neighbors; no stray identifiers.

### Step 3: END — Processor absorbs terminal::Session (delete-first)
**Scope:** `rm Source/terminal/Session.h`; `Source/terminal/Processor.h/.cpp`, `Source/terminal/View.h/.cpp`, `Source/terminal/Input.h/.cpp`, `Source/terminal/Mouse.h/.cpp`, `Source/terminal/EventRegistration.cpp`, `Source/Nexus.h` (interim), `Source/end/Panes.cpp` + `ActionRegistration.cpp` (type re-point only)
**Action (@Engineer):**
- Delete `Source/terminal/Session.h` FIRST (`rm`). Compiler errors are ground truth for the remaining moves.
- `terminal::Processor` absorbs (PluginProcessor-exact): `Model model` (owned member — ctor loses `Model&` param), `jam::TextModel document { 2 }`, `started` flag + parameterless idempotent `start()` (owner-reads shell.program/args/scrollback_lines from config::Model, gated on own positive winsize — verbatim semantics from Session.h:99-118), `drain()` + `liveFirstLine()` (Session.h:134-171), `jam::Resizer` + `wireResizer()` (Session.h:213-250, declared last), `getDocument()`, `getModel()`, and the ValueTree listener currently in Session (winsize → resizer, Session.h:183-194).
- **Self-drain:** Processor's ValueTree dispatch adds `jam::ID::screenDirty` → `drain()`. Processor registers its VT listener at construction — before any View attaches — so drain fires before View's calc (registration-order invariant; document it at the registration site).
- `terminal::View`/`Input`/`Mouse` re-point `Session&` → `Processor&` (writeInput, getModel, getDocument, start). View's screenDirty event handler keeps ONLY `codeView->calc()`.
- `Nexus` interim: map stores `std::unique_ptr<terminal::Processor>`; `Panes.cpp`/`ActionRegistration.cpp` type re-point only (flow inversion is Step 5). Tree stays green.
**Validation (@Auditor):** Session.h gone, zero references remain; no drain call from reader thread; started-flag semantics preserved verbatim; member declaration order (tty last, resizer per destructor contract); no bail-out guards introduced; BLESSED B (one owner per resource).

### Step 4: END — end::Session + Nexus ownership
**Scope:** NEW `Source/end/Session.h`; `Source/Nexus.h`, `Source/Main.h/.cpp` (Application member move)
**Action (@Engineer):**
- NEW `end::Session` (Source/end/Session.h): owns `jam::Owner<terminal::Processor>`, its `jam::UUID`, and **authors its persistent subtree** in end::Model: builds `SESSION(jam::IDtype::session, uuid property)` → `TABS → TAB → PANES(PaneManager tree) → PANE(uuid)` via an owned `jam::PaneManager` (topology authority). Holds Step-1 tree-level Attachments: SESSION under SESSIONS (dies with Session), each Processor's `terminal::Model` root tree under its PANE (dies with the Processor entry). Verbs: `createTerminal()`, `removeTerminal (uuid)`, `splitPane (uuid, direction)`, `addTab()`, `removeTab (uuid)`, `get (paneUuid)` → `terminal::Processor&`. Per-pane VIEW state node is Session-authored (persistent UI state), including `focusedPane` relocation into the SESSION subtree.
- `Nexus` rework: owns `end::Model model` (moves OUT of end::Application, Main.h:45 — Instance access unchanged for consumers) + `Owner<end::Session>`/UUID map. Bootstraps the `SESSIONS (IDtype::sessions)` child and `ID::activeSession` parameter. Verbs: `createSession()`, `getSession (uuid)`, `removeSession (uuid)`, active-session resolution for action call sites.
- Application: member order adjusted (Nexus constructs end::Model); no other Application behavior change.
**Validation (@Auditor):** Session tree complete with zero Views (constructible headless); every attachment RAII-held with named owner; no GUI include in Nexus/end::Session (gui-less contract — jam::PaneManager is jam_gui module but tree-only usage; flag if its header drags Component deps); NAMES verbs (create/remove/split/add/get canon); no getters without proven caller.

### Step 5: END — GUI projection inversion (state-first)
**Scope:** `Source/end/View.h/.cpp`, `Source/end/Tabs.h/.cpp`, `Source/end/Panes.h/.cpp`, `Source/end/EventRegistration.cpp`, `Source/end/ActionRegistration.cpp`, `Source/terminal/View.h/.cpp` (ctor source)
**Action (@Engineer):**
- **Views adopt, never author; Session authors, owns RAII.** Views may WRITE properties via Model API as today (Direction B); they never create/remove tree children.
- end::View resolves `ID::activeSession` → adopts that SESSION subtree (Step-1 adopt ctor) as its Model::Component anchor. Tabs adopts the session's TABS tree; each Panes adopts its TAB's PANES tree (PaneManager adopt ctor — layout math over the Session-authored tree); terminal::View adopts its PANE/VIEW node.
- Creation flow inverts: `Panes::addPaneView` no longer calls Nexus::create (Panes.cpp:22) — GUI actions (newTab/closeTab/splitHorizontal/splitVertical/closePane) route to Nexus/end::Session verbs; Tabs/Panes react to `valueTreeChildAdded/Removed` on adopted subtrees, constructing/destroying tab and pane GUI (terminal::View built with the pane's uuid + `session.get(paneUuid)` Processor).
- Pane resizer drag writes split weights as tree properties through the adopted tree (property write, not structure).
- ActionRegistration: zoom/focus actions resolve active session via Nexus, then `get(paneUuid).getModel()`.
- GUI-held topology Attachments die — remove the Panes/Tabs attachment authorship for structure; ephemeral-only GUI state (if any remains) stays app-level.
**Validation (@Auditor):** zero tree-structure mutation from any juce::Component; View destruction leaves the SESSION subtree byte-identical; no manual boolean orchestration; event-driven canon (existing listener patterns extended, no parallel channels); locked-plan trace for every re-routed action.

### Step 6: END — bootstrap + doc sync
**Scope:** `Source/Main.h/.cpp`, `ARCHITECTURE.md`, `SPEC.md` (current-state refs), `CLAUDE.md` (layer line)
**Action (@Engineer):**
- Startup (state-first): after config load, Nexus ensures ≥1 Session exists (first Session with one tab/pane/terminal) and `activeSession` set; end::View then constructs and projects it. Window close leaves Sessions intact (standalone teardown order documented).
- Doc sync: ARCHITECTURE.md "Ratified Target Topology (NOT YET IMPLEMENTED)" block rewritten as current-state description (ownership diagram, tree, drain, adoption contract); stale current-code labels removed; SPEC.md §2 current markers; CLAUDE.md layer order line updated.
**Validation (@Auditor):** app boots from state (no GUI-created state anywhere); docs match code exactly (descriptive contract); zero-debt: no deferred residue from Steps 1-5.

## BLESSED Alignment
- **B** — every resource one owner: Nexus→Session→Processor chain; RAII attachments; no SafePointers.
- **L** — steps decompose by responsibility; end::Session single-responsibility (state authority); no god objects; YAGNI: no daemon/IPC code this sprint, only the topology it needs.
- **E (Explicit)** — verbs named for exact semantics; no magic tags (Identifier canon); fail-fast asserts at ownership boundaries.
- **S (SSOT)** — the SESSION subtree is the one truth; Views project it; shadow GUI state eliminated (topology no longer duplicated between PaneManager tree and Nexus map).
- **S (Stateless)** — Views zero cached state (Sprint-65 contract extended to Tabs/Panes projection); Processor state = calculation inputs synced from Model.
- **E (Encapsulation)** — tell-don't-ask verbs; unidirectional layers (Nexus/Session never include GUI headers); established listener patterns extended, no new channels.
- **D** — restoration determinism: same tree → same GUI, by construction.

## Risks / Open Questions
- **Drain-before-calc ordering** rests on VT listener registration order (Processor registers at construction, View later). Deterministic; documented invariant at both registration sites.
- **focusedPane relocation** into the SESSION subtree (session UI state) — plan decision; veto if it should stay app-level.
- **jam::PaneManager in gui-less end::Session** — tree-only usage; if its header drags Component dependencies into the Session include graph, Engineer flags and we split topology ops from layout at that point (discrepancy → STOP per CAROL).
- Conformance acceptance unchanged: 84/84 + ARCHITECT visual pass after Step 6.

## Verification
ARCHITECT builds and runs (`ninja` via Builds/ — ARCHITECT only). Acceptance: app boots to interactive terminal; split/newTab/closeTab/closePane function through verbs; killing the Window and reopening (standalone restart path) rebuilds identical layout from the tree; 84/84 conformance; ARCHITECT visual pass.
