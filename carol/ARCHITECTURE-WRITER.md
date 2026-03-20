# ARCHITECTURE-WRITER.md
## Instructions for Agents: How to Analyze and Document Architecture

**Purpose:** Step-by-step protocol for PRIMARY agents (COUNSELOR, SURGEON) to write or update ARCHITECTURE.md. PRIMARY agents delegate analysis tasks to secondary agents (subagents) as needed.

**Version:** 2.2.3

**File Location:** Create ARCHITECTURE.md at project root (not in carol/)

**For Agents:** Read this document when assigned architecture documentation tasks.

**Format Reference:** See `templates/ARCHITECTURE.md` for comprehensive structure and section examples.

---

## 📖 Notation Reference

**[N]** = Sprint Number (e.g., `1`, `2`, `3`...)

**File Naming Convention:**
- `[N]-[ROLE]-[OBJECTIVE].md` — Task summary files written by agents
- `[N]-COUNSELOR-[OBJECTIVE]-KICKOFF.md` — Phase kickoff plans (COUNSELOR)
- `[N]-AUDITOR-[OBJECTIVE]-AUDIT.md` — Audit reports (AUDITOR)

**Example Filenames:**
- `[N]-COUNSELOR-INITIAL-PLANNING-KICKOFF.md` — COUNSELOR's plan for sprint 1
- `[N]-ENGINEER-MODULE-SCAFFOLD.md` — ENGINEER's task in sprint 2
- `[N]-AUDITOR-QUALITY-CHECK-AUDIT.md` — AUDITOR's audit after sprint 2

---

## Role Responsibilities

### Who Writes ARCHITECTURE.md?

**PRIMARY Agents:**

**COUNSELOR:**
- Writes initial architecture sketch after SPEC is concrete
- Updates when SPEC changes significantly
- Documents new patterns after human approval

**SURGEON:**
- Updates when surgical fix reveals new pattern
- Documents edge cases discovered during fix

**Secondary Agents (invoked by PRIMARY):**

**AUDITOR:**
- Updates after audit discovers new patterns
- Documents refactoring opportunities (human decides)
- Validates architecture matches codebase

**MACHINIST:**
- Updates when polishing introduces new patterns
- Documents wiring decisions made during implementation

**NEVER assume architecture. ALWAYS consult ARCHITECT if uncertain.**

---

## Critical Rules for Writing ARCHITECTURE.md

### Rule 0: Codebase is SSOT

**ARCHITECTURE.md captures what EXISTS in code, not what SHOULD exist.**

If you find mismatch:
1. Verify codebase is correct (check SPEC.md)
2. Update ARCHITECTURE.md to match codebase
3. Note discrepancy in sprint summary

**Never modify code to match documentation.**

---

## Role-Specific Instructions

### For COUNSELOR

**When you write ARCHITECTURE.md:**
1. Read SPEC.md completely
2. Identify module boundaries from spec
3. Sketch initial layer separation
4. Propose dependency graph
5. Document design decisions from SPEC
6. **STOP HERE** - Wait for ARCHITECT approval

**You write INITIAL architecture, not final.**

**Use templates/ARCHITECTURE.md as format guide:**
- Reference section structure (Module Structure, Layer Separation, etc.)
- Follow documentation patterns shown in template
- Adapt template sections to project needs (see Template Adaptation Guide below)

**Your output format:**
```markdown
# [PROJECT] - Architecture (DRAFT)

## Proposed Module Structure
[Your analysis]

## Proposed Layer Separation
[Your suggestions]

## Open Questions for ARCHITECT
- Should [module A] depend on [module B] or vice versa?
- Where should [responsibility X] live?
- Threading model: [proposal]

**Status:** AWAITING ARCHITECT APPROVAL
```

### COUNSELOR Writing Process

1. Read SPEC.md thoroughly
2. Identify module boundaries from SPEC flows
3. Map dependencies between modules
4. Draft ARCHITECTURE.md sections (see templates/ARCHITECTURE.md for format):
   - Module Structure
   - Layer Separation Rules
   - Interface Contracts
   - Data Flow
5. Mark uncertain decisions with `[ARCHITECTURAL DECISION NEEDED]`
6. Present to human architect for approval

**NEVER assume architectural decisions. Always propose, never dictate.**

---

## Machinist Instructions

**When you update ARCHITECTURE.md:**
- You've just added error handling to scaffolded code
- You notice a repeated pattern emerging
- You see opportunity to extract pattern into map-based dispatch

**What to document:**
1. New pattern introduced (if any)
2. Error handling contract at interface boundaries
3. Validation rules added

**Process:**
1. Identify the pattern you used (e.g., "Converted 3-branch if/else to map lookup")
2. Add entry to "Design Patterns in Use" section
3. Update relevant "Interface Contracts" if error handling changed
4. Write summary to `[N]-MACHINIST-ARCHITECTURE-UPDATE.md`

**Example update:**
```markdown
## Pattern: Validation at Boundary

**Used for:** Input validation before processing

**Implementation location:** `Source/Validation/InputValidator.cpp`

**Structure:**
```cpp
// Validate at call site
if (auto error = validator.check(input))
{
    return error;  // Fail fast
}
process(input);  // Guaranteed valid
```

**Key insight:** Fail fast at boundaries, not deep in call stack.
```

---

## For AUDITOR (invoked by PRIMARY)

### Your Responsibility
After implementation phase completes and passes ARCHITECT testing, audit codebase and update ARCHITECTURE.md.

### What to Document

**Module Structure:**
- New modules added
- Module responsibility changes
- Dependency graph updates

**Patterns Discovered:**
- If you see same pattern used 3+ times, document it
- If MACHINIST converted conditionals to maps, document the pattern
- If new interface contract established, document it

**Layer Contracts:**
- New API methods between layers
- Threading rules for new components
- Data flow for new features

**Design Decisions:**
- Why this implementation over alternatives
- Trade-offs accepted
- Constraints that drove design

### Analysis Process

1. **Read these in order:**
   - SPEC.md (design intent)
   - Implemented code (what exists)
   - [N]-completion.md (your own audit notes)

2. **Look for:**
   - Repeated patterns (3+ occurrences)
   - Layer boundaries (who calls whom)
   - Data transformations (how data flows)
   - Error handling strategies (fail fast, graceful degradation)

3. **Ask yourself:**
   - "If I were onboarding a new agent, what patterns would they need to know?"
   - "What non-obvious contracts exist between modules?"
   - "What decisions were made that aren't visible in code?"

4. **Document:**
   - Add to appropriate ARCHITECTURE.md section
   - Use concrete examples from codebase (file:line references)
   - Explain WHY, not just WHAT

### Update Locations

**Common update sections:**
- "Design Patterns in Use" (new patterns)
- "Interface Contracts" (new APIs)
- "Data Flow" (new flows)
- "Key Design Decisions" (architectural choices)
- "Anti-Patterns Avoided" (things we explicitly rejected)

### Example Update

**Before:**
```markdown
## Design Patterns in Use

(Empty section, project just started)
```

**After Phase 2:**
```markdown
## Design Patterns in Use

### Pattern: Type-Erased Factory

**Used for:** Creating UI components from XML type strings

**Implementation location:** `Source/Registry/Registry.h:45`

**Structure:**
```cpp
ku::Function::Map<juce::String, std::unique_ptr<juce::Component>> make;
make.add("knob", []() { return std::make_unique<Knob>(); });
auto component = make.get("knob");  // Type-erased lookup
```

**Key insight:** Avoids switch statements, scales to 50+ component types.

**Call sites:**
- `Source/Manager/Manager.cpp:127` (makeAndStyle method)

**Related patterns:** See Registry System documentation for full details.
```

### When to Consult ARCHITECT

**Stop and consult ARCHITECT if:**
- Layer calling upward (lower layer calling higher layer)
- Circular dependency between modules
- Direct global state access (no context passing)
- More than 2 branches in conditional (should be map)

### Pattern Inconsistencies
- [ ] Same problem solved 3 different ways
- [ ] New pattern contradicts Architectural Manifesto
- [ ] Existing pattern being violated

### Missing Constraints
- [ ] Threading rules unclear for new component
- Error handling strategy not documented
- [ ] Dependency direction ambiguous

**How to consult:**
```
ARCHITECTURE DECISION NEEDED

[Describe what you found]

[List options if multiple approaches exist]

[Ask specific question about which pattern/rule to follow]
```
ARCHITECTURAL DECISION NEEDED

During audit, I found 5 places where we validate end-user input with different patterns:
1. Early return with error message (3 places)
2. Assert and crash (1 place)
3. Silent fallback to default (1 place)

Which pattern should be documented as canonical? This affects ARCHITECTURE.md.
```

---

## For SURGEON (PRIMARY agent)

### Your Responsibility
When fixing complex issues, you may discover existing architectural patterns or contracts that weren't documented. Update ARCHITECTURE.md to reflect reality.

### What to Document

**Discovered patterns:**
- Pattern you used that already exists in codebase
- Contract you had to follow that wasn't documented

**New patterns:**
- If your fix introduces a new pattern that solves the problem better
- Consult human first before introducing new pattern

### When to Update

**Document existing patterns:**
- If you spent >5 minutes figuring out how something works
- If the pattern isn't in ARCHITECTURE.md but should be

**Consult before new patterns:**
- If your fix requires architectural change
- If new pattern would be used by other parts of codebase

### Example Scenario

**Problem:** Status bar not updating when files staged

**Investigation reveals:**
```cpp
// Undocumented pattern: Event subscription
// Found in 3 other components, but not in ARCHITECTURE.md
events.Subscribe("files_staged", callback);
```

**SURGEON action:**
1. Fix the bug using existing pattern
2. Document pattern in ARCHITECTURE.md
3. Write `[N]-SURGEON-PATTERN-DISCOVERED.md`

**Architecture update:**
```markdown
## Pattern: Event Subscription

**Used for:** Loose coupling between modules

**Implementation location:** `Source/Events/EventBus.cpp`

**Structure:**
```cpp
// Components subscribe to events
events.Subscribe("event_name", callback);

// Emitters publish events
events.Emit("event_name", data);
```

**Key insight:** Discovered during bug fix. Already used by StatusBar, FileList, and SidePanel.

**Call sites:**
- `Source/StatusBar/StatusBar.cpp:89`
- `Source/FileList/FileList.cpp:45`
- `Source/SidePanel/SidePanel.cpp:112`
```

---



## COUNSELOR: Documentation Responsibility

### Your Responsibility
COUNSELOR handles all documentation tasks including:
- Writing initial ARCHITECTURE.md after SPEC is concrete
- Creating and updating SPRINT-LOG.md entries
- Compiling sprint summaries from agent task files
- Maintaining documentation consistency across the project

After ARCHITECT approves SPEC.md, create initial ARCHITECTURE.md skeleton.

### What to Write

**At planning stage, you provide:**
1. Module Structure (proposed boundaries)
2. Layer Separation Rules (how modules communicate)
3. Dependency Graph (who depends on whom)
4. Threading Model (if project is concurrent/real-time)

**What you DON'T write yet:**
- Design Patterns (those emerge during implementation)
- Interface Contracts (those get refined during coding)
- Anti-Patterns Avoided (we discover those through mistakes)

### Example Initial Architecture

```markdown
# [PROJECT] - Architecture

**Status:** DRAFT (Phase 1 not started)

## Module Structure

### Proposed Modules
1. **InputHandler** - Parse and validate end-user input
2. **CoreLogic** - Business rules and processing
3. **OutputFormatter** - Format results for display

### Proposed Dependencies
```
InputHandler → CoreLogic → OutputFormatter
(No circular dependencies)
```

## Layer Separation Rules

**InputHandler layer:**
- Validates input
- Returns errors at boundary
- Does NOT call OutputFormatter

**CoreLogic layer:**
- Pure business logic
- No I/O operations
- Returns data structures

**OutputFormatter layer:**
- Formats output
- Handles display concerns
- Does NOT modify CoreLogic data

## Threading Model

**Single-threaded:** All operations sequential (no concurrency)

---

**Note:** This is initial architecture. Will be refined after Phase 1 implementation.
```

### When to Consult ARCHITECT

**Ask before writing if:**
- Multiple architectural approaches exist (monolith vs modular)
- Performance constraints unclear (real-time vs batch processing)
- Platform-specific patterns needed (JUCE, web, CLI)

**Example:**
```
ARCHITECTURAL DECISION NEEDED

SPEC calls for "file watching and auto-refresh."

Two approaches:
1. Polling every 5 seconds (simple, higher CPU)
2. OS filesystem events (complex, lower CPU)

Which should I propose in ARCHITECTURE.md?
```

---

## Cross-Role Coordination

### COUNSELOR (PRIMARY) Delegation Flow

**COUNSELOR delegates to subagents:**

**Pathfinder** (ALWAYS FIRST)
- Searches codebase for existing patterns
- Identifies where new modules should live
- Reports findings to COUNSELOR

**Oracle** 
- Analyzes SPEC.md for architectural requirements
- Identifies design decisions needed
- Reports analysis to COUNSELOR

**Engineer**
- Creates module scaffolding based on COUNSELOR's boundaries
- Implements initial structure following Layer Rules
- Reports completion to COUNSELOR

**Auditor**
- Validates implementation matches architecture
- Documents patterns discovered during review
- Reports audit results to COUNSELOR

### SURGEON (PRIMARY) Delegation Flow

**SURGEON delegates to subagents:**

**Pathfinder** (ALWAYS FIRST)
- Locates relevant code for bug/feature
- Identifies affected modules and dependencies
- Reports findings to SURGEON

**Oracle**
- Debugs complex issues
- Analyzes root causes
- Reports analysis to SURGEON

**Machinist**
- Polishes implementation (error handling, validation)
- Applies patterns from ARCHITECTURE.md
- Reports completion to SURGEON

**Librarian**
- Researches external libraries/solutions
- Documents integration patterns
- Reports findings to SURGEON

### Information Flow

**Subagent → PRIMARY:**
- Subagent writes `[N]-[ROLE]-[OBJECTIVE].md` task file
- PRIMARY reads task file, integrates findings
- PRIMARY updates ARCHITECTURE.md with discovered patterns

**PRIMARY → ARCHITECTURE.md:**
- PRIMARY maintains architecture documentation
- Subagents consult ARCHITECTURE.md for patterns
- All agents follow documented Layer Rules and contracts

---

## Validation Checklist

**Before considering ARCHITECTURE.md complete:**

### Completeness
- [ ] All modules in codebase are documented
- [ ] All patterns used 3+ times are documented
- [ ] All layer boundaries have contracts
- [ ] All threading rules are explicit

### Accuracy
- [ ] ARCHITECTURE.md matches current codebase (not outdated)
- [ ] Code examples compile and run
- [ ] File paths are correct (not hypothetical)
- [ ] Line numbers are current (or removed if code changed)

### Clarity
- [ ] New agent could onboard from this document
- [ ] WHY is explained, not just WHAT
- [ ] Concrete examples for every pattern
- [ ] Glossary defines project-specific terms

### Consistency
- [ ] Naming matches Coding Standards
- [ ] Patterns align with Architectural Manifesto
- [ ] No contradictions between sections

---

## Update Triggers

**ARCHITECTURE.md should be updated when:**

**By PRIMARY agents (COUNSELOR, SURGEON):**
1. **New module added** (COUNSELOR)
2. **Design decision made** (Human architect, documented by COUNSELOR)
3. **Threading rules change** (Human architect, documented by COUNSELOR)
4. **Pattern discovered during fix** (SURGEON)
5. **SPEC changes** (COUNSELOR updates affected sections)

**By subagents (invoked by PRIMARY):**
6. **Pattern emerges** (3+ uses of same approach) (AUDITOR reports to PRIMARY)
7. **Interface contract established** (MACHINIST reports to SURGEON, AUDITOR reports to COUNSELOR)
8. **Anti-pattern avoided** (AUDITOR reports to PRIMARY)

---

## Red Flags: When to Stop and Consult

**Immediately consult human architect if:**

### Architectural Violations
- [ ] Layer calling upward (lower layer calling higher layer)
- [ ] Circular dependency between modules
- [ ] Direct global state access (no context passing)
- [ ] More than 2 branches in conditional (should be map)

### Pattern Inconsistencies
- [ ] Same problem solved 3 different ways
- [ ] New pattern contradicts Architectural Manifesto
- [ ] Existing pattern being violated

### Missing Constraints
- [ ] Threading rules unclear for new component
- [ ] Error handling strategy not documented
- [ ] Dependency direction ambiguous

**How to consult:**
```
ARCHITECTURE DECISION NEEDED

[Describe what you found]

[List options if multiple approaches exist]

[Ask specific question about which pattern/rule to follow]
```

---

## Success Criteria

**ARCHITECTURE.md is successful when:**

1. **New agents onboard faster** - Read doc, understand system in <30 min
2. **Fewer pattern violations** - Agents follow established patterns
3. **Clearer code reviews** - AUDITOR can reference documented patterns
4. **Reduced rework** - Agents build correctly first time
5. **Better fixes** - SURGEON understands contracts before making changes

**Measure success by:**
- Reduction in "How does X work?" questions
- Decrease in pattern-violating code
- Faster phase completion times
- Fewer RESET contexts for SURGEON

---

## Maintenance Rules

### Continuous Updates
- Update ARCHITECTURE.md in same sprint as code changes
- Don't defer documentation to "later" (it never happens)
- Small, incremental updates better than big rewrites

### Version Alignment
- ARCHITECTURE.md version tracks codebase version
- If code is v1.2.3, doc should be v1.2.3
- Breaking changes increment major version

### Dead Code Removal
- When module removed, remove from ARCHITECTURE.md
- When pattern abandoned, move to "Deprecated Patterns" section
- Keep history in git, not in document

---

## Anti-Patterns in Documentation

### ❌ Vague Descriptions
```markdown
// BAD
## Module: DataHandler
Handles data.
```

```markdown
// GOOD
## Module: DataHandler
Validates end-user input, transforms to internal format, passes to CoreLogic.
Located: `Source/Input/DataHandler.cpp`
Depends on: Validator, Transformer
```

### ❌ Outdated Examples
```markdown
// BAD (code changed, doc didn't)
```cpp
// Example from old version (doesn't compile anymore)
```

```markdown
// GOOD
```cpp
// Current implementation as of v1.2.3
// Source/Registry/Registry.cpp:45
auto component = registry.make.get(type);
```

### ❌ Missing WHY
```markdown
// BAD
We use map-based dispatch.
```

```markdown
// GOOD
We use map-based dispatch because Architectural Manifesto requires
complex conditionals (>1 branch) to be O(1) lookups. This prevents
switch statement sprawl and makes adding new types trivial.
```

---

## Template Adaptation Guide

**Reference templates/ARCHITECTURE.md for complete structure. Adapt by:**

### For Small Projects
- Combine sections (Module Structure + Dependency Graph in one)
- Skip Threading Model if single-threaded
- Reduce glossary if terminology is standard

### For Large Projects
- Split ARCHITECTURE.md by domain (UI/, Core/, Data/)
- Add "Module Interactions" diagrams
- Expand Threading Model with detailed rules per subsystem

### For Framework Projects
- Add "Extension Points" section (how to add plugins/modules)
- Document versioning strategy (API compatibility)
- Include migration guides between versions

### For Real-Time Projects
- Expand Threading Model with latency budgets
- Add "Performance Constraints" section
- Document lock-free patterns in detail

---

**End of Instructions**

Remember:
- ARCHITECTURE.md reflects **what exists**, not what's planned
- Update incrementally as patterns emerge
- Consult human for architectural decisions
- Code is SSOT, doc is map

**JRENG!**
