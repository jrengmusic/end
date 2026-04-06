---
description: Consume RFC.md and produce BLESSED-compliant incremental execution plan
---

## Plan-Go Protocol

1. **Read RFC** at project root (`RFC.md` or `RFC-[objective].md`) — if missing, STOP and report
2. **Read MANIFESTO.md** (BLESSED principles)
3. **Read LANGUAGE.md** — language-specific BLESSED adaptations and framework constraints
4. **Read SPEC.md** if it exists — plan must align with spec
5. **Invoke @Pathfinder** — discover existing patterns, architecture, naming conventions
6. **Write PLAN-[objective].md** at project root:

### Plan Format

```markdown
# PLAN: [Objective]

**RFC:** [RFC filename]
**Date:** YYYY-MM-DD
**BLESSED Compliance:** verified
**Language Constraints:** [language/framework from LANGUAGE.md, e.g. "Go / Bubbletea"]

## Overview
[1-3 sentences — what this plan achieves]

## Language / Framework Constraints
[Relevant LANGUAGE.md adaptations that affect this plan — BLESSED overrides, framework limitations, accepted violations]

## Validation Gate
Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output is correct and BLESSED-compliant.

## Steps

### Step 1: [Title]
**Scope:** [files/modules affected]
**Action:** [precise instruction for @Engineer]
**Validation:** [what @Auditor checks before step is complete]

### Step 2: [Title]
...

## BLESSED Alignment
- [How each BLESSED principle is satisfied]
- [Where LANGUAGE.md overrides apply and why]

## Risks / Open Questions
- [Anything that needs ARCHITECT decision]
```

7. **Present the plan** to ARCHITECT for approval — do not begin execution

### Rules
- Steps must be small and incremental — never choke the engineer
- Each step must have explicit validation criteria
- Objective name in filename derived from RFC title (kebab-case, e.g. `PLAN-session-management.md`)
- Delegate to @Engineer for execution, @Auditor for validation — COUNSELOR tracks and orchestrates
