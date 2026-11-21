# DEBT.md

**Purpose:** Inter-sprint ledger of debts — bugs, nitpicks, friction observed during usage. Drained
by sprints via `/pay` (COUNSELOR planning) and `/log` (hygiene drain). **JRENG = paid in full, cash.
No triage.**

**Format:** Each entry uses **O / D / E** articulation — Observation, Divergence, Expectation. IDs
are UTC timestamps (`DEBT-YYYYMMDDTHHMMSS`). Newest entries at top. Add via `carol debt add`.

**Lifecycle:** Created lazily on first `carol debt add`. Entries appended via interactive prompt.
Entries removed by `carol debt clear <id>` (called by `/log` hygiene step after SPRINT-LOG receipt
is written). Survives `carol reset` — debts persist across protocol resets.

---

## DEBT-20261121T004129

**Observation:** In EVE, while time seq prints, the active prompt already shows before the seq output has finished on screen.
**Divergence:** The prompt shows before the last lines of the command output, so the drawn order differs from the order that the shell wrote. The cause is not traced. Facts: process() decodes the TTY output on the clock context (EVETerminalProcessor.cpp:574-584). The grid reaches the view through the swap (writePendingData, :323-328, called from handleAsyncUpdate, :330-338). Terminal state rows, numPrompts among them, reach the Model through state.storeValue (:116-117) and the flush under the message manager lock (:54-59), a second lane.
**Expectation:** The prompt appears after the last line of the command output, in the order that the shell wrote them.

---

## DEBT-20261121T004127

**Observation:** Creating a new pane erases the active prompt of the last focused pane. The prompt stays erased until the next input in that pane.
**Divergence:** A new pane resizes the existing panes. Each Id::winsize change calls EVETerminalProcessor::prepare (EVEProcessor.cpp:218-219), which stops, opens and starts the clock (EVETerminalProcessor.cpp:370, :392, :397). The clock start runs prepare (winsize, history) (:432-433, :536-572), which calls TerminalProcessor::reset (:564). reset erases the whole screen and moves the cursor to 0,0 when a prompt is active and the grid size changed (jam_TerminalProcessor.cpp:21-22, :37-40). Then ttyResize.submit sends the new size to the shell (EVETerminalProcessor.cpp:570).
**Expectation:** A resize keeps the active prompt of every pane on screen.

---

## DEBT-20261121T004125

**Observation:** Sometimes END stops taking input in a pane: keys and clicks do nothing. Only a tab close or a new tab gives input back.
**Divergence:** Input stays lost until a tab closes or a new tab opens. The cause is not traced. Input paths: EVE grabs keyboard focus from the terminal focus row (EVEView.cpp:83-87 parentHierarchyChanged, :191-197 Id::focus event). END writes the focus row on the attached terminal (EditorView.cpp:259) and on each pane focus change (Session.cpp:74-88). EVE forwards keys and mouse events to END through ENAX (jam_ENAXClientExtensions.cpp:35-39). END sends a key to CommandManager::invokeKey with the window row (Nexus.cpp:6-11) and a mouse event to the peer under the position (Nexus.cpp:13-23). juce-plugin-window-focus-hook.patch changes the VST2 and AU host windows (END CLAUDE.md, ENAX Plugin Format).
**Expectation:** Keys and clicks always reach the focused pane, with no tab close or new tab.

---



