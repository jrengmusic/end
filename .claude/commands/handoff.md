---
description: Freeze session state as handoff for future COUNSELOR pickup
---

This session is being frozen. Do both, no approval gate between them:

**1. Write commit message in CHAT ONLY** for any uncommitted changes.
- Follow the repository's existing commit style (check recent `git log`).
- Output the message directly in chat as a code block so ARCHITECT can copy it.
- Do NOT write the commit message into SPRINT-LOG.md or any other file.
- Do NOT run `git commit` yourself — ARCHITECT commits manually.

**2. Append handoff entry to carol/SPRINT-LOG.md** so a future COUNSELOR can pick up exactly where we left off. The handoff entry does NOT contain the commit message.

Format:
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
