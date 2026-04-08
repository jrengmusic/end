---
description: Write commit message (in chat) and log sprint to carol/SPRINT-LOG.md
---

Do both, no approval gate between them:

**1. Write commit message in CHAT ONLY.**
- Draft a commit message for all current changes, following the repository's existing commit style (check recent `git log`).
- Output the message directly in chat as a code block so ARCHITECT can copy it.
- Do NOT write the commit message into SPRINT-LOG.md or any other file.
- Do NOT run `git commit` yourself — ARCHITECT commits manually.

**2. Append sprint entry to carol/SPRINT-LOG.md** using the format defined in CAROL.md (Sprint [N]: [Objective], Date, Duration, Agents Participated, Files Modified, Alignment Check, Problems Solved, Technical Debt). The sprint entry does NOT contain the commit message.
