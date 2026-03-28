---
description: Freeze session state as handoff for future COUNSELOR pickup
---

This session is being frozen. Write a handoff entry to SPRINT-LOG.md so a future COUNSELOR can pick up exactly where we left off.

Include:
- Current objective and progress
- What was completed
- What remains unfinished
- Key decisions made and rationale
- Files modified and their current state
- Open questions or blockers
- Recommended next steps

Format as:
```markdown
## Handoff to COUNSELOR: [Objective]

**From:** COUNSELOR
**Date:** YYYY-MM-DD
**Status:** [In Progress / Blocked / Ready for Implementation]

### Context
[What we were working on and why]

### Completed
- [What was done]

### Remaining
- [What still needs to happen]

### Key Decisions
- [Decision and rationale]

### Files Modified
- `path/file` — [what changed]

### Open Questions
- [Unresolved items]

### Next Steps
- [Where to pick up]
```

Also write a commit message for any uncommitted changes.
