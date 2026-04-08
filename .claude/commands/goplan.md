---
description: Produce BLESSED-compliant incremental execution plan (consumes RFC.md if one exists)
---

## Plan-Go Protocol

**Invocation:**
- `/goplan` — consume RFC.md if present, otherwise use ARCHITECT's prompt
- `/goplan no RFC` (or `no-rfc`, `skip rfc`) — explicit override: DO NOT read any RFC file, objective comes from ARCHITECT's prompt only

1. **Read RFC** at project root (`RFC.md` or `RFC-[objective].md`) — OPTIONAL. Skip entirely if ARCHITECT passed "no RFC". If present and not overridden, consume it. If absent, proceed using ARCHITECT's prompt as the objective source. Never invent an RFC, never block on a missing one.
2. **Read MANIFESTO.md** (BLESSED principles)
3. **Read LANGUAGE.md** — language-specific BLESSED adaptations and framework constraints
4. **Read SPEC.md** if it exists — plan must align with spec
5. **Invoke @Pathfinder** — discover existing patterns, architecture, naming conventions
6. **Write PLAN-[objective].md** at project root:

### Plan Format

```markdown
# PLAN: [Objective]

**RFC:** [RFC filename, or "none — objective from ARCHITECT prompt"]
**Date:** YYYY-MM-DD
**BLESSED Compliance:** verified
**Language Constraints:** [language/framework from LANGUAGE.md, e.g. "Go / Bubbletea"]

## Overview
[1-3 sentences — what this plan achieves]

## Language / Framework Constraints
[Relevant LANGUAGE.md adaptations that affect this plan — BLESSED overrides, framework limitations, accepted violations]

## Validation Gate
Each step MUST be validated before proceeding to the next.
Validation = @Auditor confirms step output complies with ALL documented contracts:
- MANIFESTO.md (BLESSED principles)
- NAMES.md (naming philosophy)
- JRENG-CODING-STANDARD.md (C++ coding standards)
- The locked PLAN decisions agreed with ARCHITECT (no deviation, no scope drift)

## Steps

### Step 1: [Title]
**Scope:** [files/modules affected]
**Action:** [precise instruction for @Engineer]
**Validation:** [what @Auditor checks — must cover MANIFESTO.md, NAMES.md, JRENG-CODING-STANDARD.md, and locked PLAN decisions]

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
- Objective name in filename derived from RFC title when RFC exists, otherwise from ARCHITECT's stated objective (kebab-case, e.g. `PLAN-session-management.md`)
- Delegate to @Engineer for execution, @Auditor for validation — COUNSELOR tracks and orchestrates
