# [PROJECT NAME] - Architecture

**Purpose:** Single source of truth for project structure, patterns, and contracts.

**Status:** [DRAFT | STABLE | EVOLVING]

**Last Updated:** [YYYY-MM-DD]

**Version:** [Semantic version matching codebase]

---

## Document Scope

**What this document contains:**
- Proven patterns currently in use
- Module boundaries and responsibilities
- Interface contracts and call sites
- Dependency rules and direction
- Data flow and state management
- Key design decisions and rationale

**What this document does NOT contain:**
- Implementation details (see inline docs)
- API reference (see language-specific docs)
- Build instructions (see README.md)
- Coding standards (see Architectural Manifesto)

**SSOT Priority:** Codebase > ARCHITECTURE.md > SPEC.md

If this document conflicts with codebase, codebase is correct. Update this document.

---

## Project Overview

### Purpose
[One paragraph: What does this project do?]

### Architecture Philosophy
[Design principles specific to THIS project]

### Technology Stack
- **Language:** [e.g., C++17, Go 1.21, Python 3.11]
- **Framework:** [e.g., JUCE, Bubble Tea, Flask, React]
- **Build System:** [e.g., CMake, Go modules, npm]
- **Platform:** [e.g., Cross-platform, Web, Desktop, Mobile]

---

## Module Structure

### Module Map

```
[Visual representation of module hierarchy]
```

### Module Inventory

| Module | Location | Responsibility | Dependencies |
|--------|----------|----------------|--------------|
| [Name] | `path/to/module/` | [Single responsibility] | [Depends on] |

---

## Layer Separation Rules

**[PROJECT] follows strict layer separation. NO POKING.**

### Layer Hierarchy

```
[Define your layers]
```

### Communication Contracts

**Layer [X] → Layer [Y]:**
- **Allowed:** [What's permitted]
- **Forbidden:** [What's prohibited]
- **Example:** [Concrete usage]

### Violation Detection

**Red flags indicating layer violations:**
- [Specific anti-patterns for this project]

**If you see these, STOP and consult human architect.**

---

## Design Patterns in Use

### Pattern: [Pattern Name]

**Used for:** [Problem this pattern solves]

**Implementation location:** `path/to/implementation`

**Structure:**
```
[Minimal example showing the pattern]
```

**Key insight:** [Why this pattern, not alternatives]

---

## Interface Contracts

### Contract: [Interface Name]

**Between:** [Module A] ↔ [Module B]

**Call site:** `path/to/callsite:line`

**Contract signature:**
```
[Function/method signature in project's language]
```

**Preconditions:**
- [What must be true before calling]

**Postconditions:**
- [What is guaranteed after call]

**Error handling:**
- [How errors are signaled]

**Concurrency:**
- [Thread safety guarantees]

---

## Data Flow

### [Flow Name]

**Trigger:** [What initiates this flow]

**Path:**
```
[Start] → [Step 1] → [Step 2] → [End]
```

**Data transformations:**
- [How data changes at each step]

**Failure modes:**
- [What can go wrong]

---

## State Management

### State Storage

**Where state lives:**
- [Component/module that owns state]
- [Persistence mechanism if any]

**State mutations:**
- [Who can modify state]
- [How changes propagate]

**Concurrency rules:**
- [Thread safety strategy]

---

## Dependency Graph

### Module Dependencies

```
[Visual dependency graph]
```

### Dependency Rules

**Allowed:**
- [Permitted dependency patterns]

**Forbidden:**
- [Prohibited dependency patterns]

**Validation:**
```bash
[Command to detect violations]
```

---

## File Structure

### Directory Layout

```
project-root/
├── [primary source directory]/
├── [test directory]/
├── [documentation directory]/
└── [build artifacts directory]/
```

### Naming Conventions

- **[Language construct]:** [Convention]

---

## Key Design Decisions

### Decision: [Decision Title]

**Date:** [YYYY-MM-DD]

**Context:** [What problem existed]

**Options considered:**
1. [Option A] - [Pros/cons]
2. [Option B] - [Pros/cons]

**Decision:** [Chosen option]

**Rationale:** [Why this choice]

**Consequences:**
- [Impact of this decision]

---

## Anti-Patterns Avoided

### ❌ Anti-Pattern: [Name]

**What it looks like:**
```
[Minimal example]
```

**Why it's bad:** [Explanation]

**Correct pattern:**
```
[Correct implementation]
```

---

## Extension Points

### Adding New [Component Type]

**Steps:**
1. [Step-by-step guide]

**Files to modify:**
- [List of files]

**Patterns to follow:**
- [Reference to documented patterns]

---

## Testing Strategy

### Test Categories

| Category | Location | Purpose |
|----------|----------|---------|
| [Type] | `path/` | [What's tested] |

### Key Test Patterns

- [Pattern 1]
- [Pattern 2]

---

## Known Limitations

### Limitation: [Description]

**Impact:** [Who/what is affected]

**Workaround:** [Temporary solution]

**Future plan:** [Planned resolution]

---

## Glossary

### Project-Specific Terms

| Term | Definition |
|------|------------|
| [Term] | [Clear definition] |

---

## References

### Internal Documents
- [SPEC.md](SPEC.md)
- [Architectural Manifesto](docs/Architectural-Manifesto.md)

### External Resources
- [Framework documentation links]

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | YYYY-MM-DD | COUNSELOR | Initial architecture sketch |

---

**End of Architecture Document**

*This document reflects the codebase as implemented. If code diverges, update this document.*

**JRENG!**
