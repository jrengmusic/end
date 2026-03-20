# PATTERNS.md - LLM Meta-Patterns for CAROL Agents

**Version:** 2.2.3
**Purpose:** Systematic approaches to prevent cognitive overload, scope creep, and autonomous mistakes
**Audience:** All CAROL agents (2 PRIMARY + 8 Secondary roles)

---

## 📖 Notation Reference

**[N]** = Sprint Number (e.g., `1`, `2`, `3`...)

**File Naming Convention:**
- `[N]-AUDITOR-[OBJECTIVE]-AUDIT.md` — Audit reports (AUDITOR)

**Example Filenames:**
- `[N]-AUDITOR-QUALITY-CHECK-AUDIT.md` — AUDITOR's audit after sprint 2

---

## Table of Contents

1. [Core Principles](#core-principles)
2. [Problem Decomposition Framework](#problem-decomposition-framework)
3. [Debug Methodology - Fail-Fast Checklist](#debug-methodology---fail-fast-checklist)
4. [Tool Selection Decision Tree](#tool-selection-decision-tree)
5. [Self-Validation Checklist](#self-validation-checklist)
6. [Role-Specific Patterns](#role-specific-patterns)
7. [Common Anti-Patterns to Avoid](#common-anti-patterns-to-avoid)

---

## Core Principles

CAROL aligns with **LIFESTAR + LOVE** from Architectural Manifesto:

### LIFESTAR
- **Lean:** Simple solutions, minimal changes
- **Immutable:** Deterministic, predictable behavior
- **Findable:** Clear documentation, obvious locations
- **Explicit:** No hidden assumptions, ask if unsure
- **SSOT:** Single Source of Truth - check existing patterns first
- **Testable:** Validate before committing
- **Accessible:** Works across languages/frameworks
- **Reviewable:** Clear attribution, trackable changes

### LOVE
- **Listens (Fail-Fast):** Check simple bugs BEFORE complex theories
- **Optimizes:** Prevent wasted cycles, avoid overthinking
- **Validates:** Self-check before responding
- **Empathizes:** Human-centric, ask clarifying questions

---

## Problem Decomposition Framework

**When to use:** COUNSELOR planning, ENGINEER breaking down tasks, SURGEON analyzing complex bugs

### Step 1: Identify SSOT (Single Source of Truth)

**Before creating anything new, search for existing solutions:**

```
1. Check project documentation:
   - Read SPEC.md (requirements)
   - Read ARCHITECTURE.md (existing patterns)
   - Read SPRINT-LOG.md (previous decisions)

2. Search codebase for similar functionality:
   - Use Grep to find similar class names
   - Use Grep to find similar function signatures
   - Use Glob to find related files

3. If found existing solution:
   - Reuse pattern (don't reinvent)
   - Extend if needed (don't duplicate)
   - Document in SPRINT-LOG.md
```

**Anti-pattern:** Creating new solution without checking if it exists

### Step 2: Break Complex → Atomic Tasks

**Follow Lean principle: smallest possible change that works**

```
Bad (monolithic):
  - "Implement user authentication"

Good (atomic):
  1. Add User model with credentials
  2. Add password hashing function
  3. Add login validation logic
  4. Add sprint management
  5. Add logout handler
```

**Rule:** If task description has "and", break into separate tasks

### Step 3: Check Architectural Constraints

**Before implementing, verify constraints:**

```
1. Layer separation:
   - Audio/DSP layer: no UI calls
   - UI layer: no direct audio processing
   - Model layer: owns state, not controllers

2. Language-specific constraints:
   - C++: RAII, no manual memory management
   - Go: goroutine safety, context propagation
   - Python: type hints, immutable defaults

3. Project-specific constraints:
   - Read ARCHITECTURE.md for design patterns
   - Check SPEC.md for requirements
```

### Step 4: Estimate Scope

**Prevent scope creep with explicit estimation:**

```
Count:
  - Files to modify: _____
  - Lines of code (rough): _____
  - New files to create: _____
  - Dependencies to add: _____

If numbers exceed expectations:
  - Ask ARCHITECT if scope is correct
  - Propose smaller iteration
  - Document why larger scope needed
```

**Example:**
```
Task: "Add error handling to API"
Files: 12 API handlers
Lines: ~200 (15-20 per file)
New files: 0
Dependencies: 0

→ Reasonable scope, proceed
```

---

## Debug Methodology - Fail-Fast Checklist

**Purpose:** Prevent 50+ message waste on simple bugs (documented failure pattern)

**Rule:** Check simple bugs BEFORE complex theories

### The Fail-Fast Order (MANDATORY)

**When investigating ANY bug, check in this exact order:**

#### 1. Type Mismatches (30% of bugs)

```
Common culprits:
  - float vs double
  - int vs size_t vs uint32_t
  - signed vs unsigned
  - implicit conversions
  - precision loss in calculations

Example (real bug from LESSONS_LEARNED):
  float smoothingFactor = 0.999;  // Precision loss!
  double smoothingFactor = 0.999; // Fixed
```

**Action:** Search for numeric types in relevant code, check precision

#### 2. Construction/Initialization Order (25% of bugs)

```
Common culprits:
  - Member initialization order (C++)
  - Registration before object ready
  - Callback installed before state initialized
  - Static initialization order fiasco

Example:
  class Processor {
      Engine engine;
      Config config;  // Used by engine constructor!
      // Bug: config initialized AFTER engine
  };
```

**Action:** Check constructor member init lists, registration timing

#### 3. Simple Logic Errors (20% of bugs)

```
Common culprits:
  - Off-by-one errors
  - Wrong comparison operator (< vs <=)
  - Inverted boolean logic
  - Copy-paste errors (wrong variable name)
  - Typos in function/method names
```

**Action:** Read code line-by-line, verify logic matches intent

#### 4. Null/Undefined Checks (15% of bugs)

```
Common culprits:
  - Dereferencing nullptr
  - Accessing uninitialized variable
  - Using optional/pointer before checking
  - Race condition (value changed between check and use)
```

**Action:** Trace variable lifetime, verify initialization

#### 5. THEN Complex Theories (10% of bugs)

```
Only after checking 1-4:
  - Threading/concurrency issues
  - Performance/optimization bugs
  - Compiler bugs
  - Library bugs
  - Hardware issues
```

**Anti-pattern:** Starting with "probably a race condition" without checking types

### Debug Workflow

```
1. Read ARCHITECT bug report
2. Read RESET context (for SURGEON)
3. Check Fail-Fast Checklist (1-4) FIRST
4. Use minimal reproduction (no full codebase read)
5. Verify fix with ARCHITECT
6. Document in SPRINT-LOG.md (on "log sprint")
```

**Never:** Read entire codebase, theorize for 50+ messages, assume complex cause

---

## Tool Selection Decision Tree

**Purpose:** Prevent cognitive overload by using right tool for task

### Decision Tree

```
Task: Need to find/analyze code
├─ Know exact file path?
│  ├─ YES → Use Read tool
│  └─ NO → Continue
├─ Know file name pattern?
│  ├─ YES → Use Glob (e.g., "**/*.cpp", "src/**/Auth*.ts")
│  └─ NO → Continue
├─ Know specific code pattern/string?
│  ├─ YES → Use Grep (search file contents)
│  └─ NO → Continue
├─ Open-ended exploration? (uncertain, need multiple rounds)
│  └─ YES → Use Task tool (Explore agent)

Task: Need to edit code
├─ Simple text replacement?
│  ├─ YES → Use Edit tool
│  └─ NO → Continue
├─ Multi-file changes, uncertain scope?
│  ├─ YES → Use scripts from SCRIPTS.md (when available)
│  └─ NO → Ask ARCHITECT for approach

Task: Need to run command
├─ File operation (read/write/search)?
│  ├─ YES → Use specialized tool (Read/Edit/Grep/Glob)
│  └─ NO → Use Bash tool

Task: Need to plan/design
├─ Requirements unclear?
│  └─ YES → Ask ARCHITECT questions (COUNSELOR role)
```

### Tool Usage Patterns

**Read Tool:**
```
Use for:
  - Reading specific known files
  - Reviewing code before editing
  - Checking existing implementation

Don't use for:
  - Searching across codebase
  - Finding files by pattern
```

**Grep Tool:**
```
Use for:
  - Finding code patterns (function names, class names)
  - Searching for specific strings
  - Locating usage of API/function

Patterns:
  - `output_mode: "files_with_matches"` → find files
  - `output_mode: "content"` → see actual code
  - Use `-C 3` for context lines
```

**Glob Tool:**
```
Use for:
  - Finding files by name pattern
  - Listing files in directory structure
  - Discovering related files

Patterns:
  - `**/*.ext` → all files with extension
  - `src/**/Test*.cpp` → test files in src/
```

**Task Tool (Explore agent):**
```
Use for:
  - Uncertain scope (don't know where to start)
  - Multi-round exploration needed
  - Complex questions about codebase structure

Example:
  "Where are errors from the client handled?"
  → Task tool explores, finds multiple locations, summarizes
```

**Anti-pattern:** Using Bash tool for file reading (`cat`, `grep`, `find`) instead of specialized tools

---

## Defensive Programming Pattern: Guard Clauses

**Purpose:** Enforce fail-fast error handling without silent failures

### Rule
**Use positive preconditions with nested blocks. Execute ONLY when valid.**
**NEVER use negative checks with early returns (silent failures).**

### The Pattern

```cpp
// ✅ CORRECT: Guard Clause Pattern
void loadPreset(const juce::File& file)
{
    // Positive precondition - only continue when valid
    if (file.existsAsFile())
    {
        // Nested scope - operations happen ONLY when precondition passes
        if (auto xml = juce::parseXML(file))
            applyPreset(xml);
    }
}

// ❌ BAD: Negative check with silent return
void loadPreset(const juce::File& file)
{
    auto xml = juce::parseXML(file);
    if (xml == nullptr)  // Silent failure!
        return;  // Caller doesn't know what happened
    applyPreset(xml);
}
```

### Key Differences

| Aspect | ❌ Anti-Pattern | ✅ Correct Pattern |
|---------|----------------|-------------------|
| **Check type** | Negative: `if (xml == nullptr)` | Positive: `if (file.existsAsFile())` |
| **Scope** | Early return (exits silently) | Nested block (only executes when valid) |
| **Failure mode** | Silent (caller doesn't know) | Explicit (either works or doesn't enter) |
| **Intent** | "Avoid if null" | "Only proceed when valid" |

### When to Use Guard Clauses

```cpp
// Example 1: File operations
if (file.existsAsFile())
{
    auto data = file.loadFileAsString();
    process(data);
}

// Example 2: Array access
if (index >= 0 && index < array.size())
{
    auto value = array[index];
    process(value);
}

// Example 3: Pointer dereference
if (ptr != nullptr)
{
    ptr->process();
}
```

### Anti-Pattern: NEVER Use These Patterns

**❌ Pattern 1: Negative check + throw (UNNECESSARY)**
```cpp
void loadPreset(const juce::File& file)
{
    auto xml = juce::parseXML(file);
    if (xml == nullptr)
        throw runtime_error("Failed to parse XML");  // NO! Unnecessary!
    applyPreset(xml);
}
```
**Why wrong:** If xml is nullptr, program crashes or undefined behavior IS defined behavior. Don't wrap it in exception.

**❌ Pattern 2: Negative check + bool return (HELL NO! ABSOLUTE GARBAGE)**
```cpp
bool loadPreset(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;  // NO!
    
    auto xml = juce::parseXML(file);
    if (!xml)
        return false;  // NO!
    
    applyPreset(xml);
    return true;
}
```
**Why wrong:** Negative checks push complexity to caller. Caller must remember to check return value. Caller will forget.

### Enforcement

**Guard Clause Pattern: How to add validation**

**Use positive preconditions with nested blocks. Execute ONLY when valid.**

This is HOW validation should be added, NOT WHO adds it or WHEN.

**ENGINEER must NOT add validation** - That's MACHINIST's job

**SURGEON applies when fixing bugs** - Use guard clauses to prevent similar failures

---

## Self-Validation Checklist

**Purpose:** Prevent autonomous mistakes (documented failure: SPRINT-32 git disasters)

**MANDATORY: Run before responding to ARCHITECT**

### Pre-Response Checklist

```
[ ] Did I check SSOT for existing solution?
    - Read SPEC.md, ARCHITECTURE.md, SPRINT-LOG.md
    - Searched codebase with Grep/Glob
    - Not reinventing existing pattern

[ ] Did I verify against architectural constraints?
    - Layer separation maintained
    - No violations of project principles
    - Consistent with existing patterns

[ ] Did I check construction/initialization order?
    - Member variables initialized in correct order
    - No use-before-init bugs
    - Registration happens after object ready

[ ] Did I validate against SPEC.md requirements?
    - Feature matches specification
    - No features added beyond spec
    - Edge cases documented

[ ] Am I scope creeping?
    - Only doing what was asked
    - Not adding "improvements" unasked
    - Not refactoring unrelated code

[ ] Did I ask when uncertain?
    - No assumptions about requirements
    - No guessing at ARCHITECT intent
    - Clarified ambiguities

[ ] For ENGINEER: Am I generating literally?
    - Following kickoff document exactly
    - Not adding optimizations
    - Not making architectural decisions

[ ] For SURGEON: Am I minimal?
    - Smallest possible fix
    - Not refactoring entire module
    - Only touching broken code

[ ] For MACHINIST: Am I preserving behavior?
    - Only adding safety/validation
    - Not changing logic
    - Not optimizing prematurely
```

### Git Operations Checklist (CRITICAL)

```
NEVER run git commands without asking ARCHITECT:
  - git merge, git rebase
  - git reset --hard
  - git push (especially --force)
  - git commit --amend
  - git checkout (destructive)

ALWAYS ask before:
  - Creating commits
  - Changing branches
  - Modifying git history
  - Pushing to remote

Exception: Read-only git commands OK
  - git status
  - git log
  - git diff
  - git show
```

**Documented failure:** SPRINT-32 - ran `git merge --abort` without asking, destroyed work

---

## Role-Specific Patterns

### COUNSELOR Patterns

**Core behavior:** Ask questions, gather requirements, write specs

**Pattern: Systematic Questioning**
```
1. Read existing documentation first:
   - SPRINT-LOG.md (previous decisions)
   - ARCHITECTURE.md (existing patterns)
   - SPEC.md (current requirements)

2. Ask clarifying questions:
   - What problem are we solving?
   - Who are the users?
   - What are the constraints?
   - What are the success criteria?
   - What should we NOT do? (scope boundary)

3. Write SPEC.md:
   - Clear requirements
   - Explicit non-requirements
   - Success criteria
   - Examples/mockups

4. Write ARCHITECTURE.md (if needed):
   - Design patterns
   - Module structure
   - Data flow
   - Integration points

5. Create kickoff document for ENGINEER:
   - Concrete, literal instructions
   - File structure
   - Code examples
   - Style guidelines
```

**Anti-pattern:** Writing code, making architectural decisions without questions

### ENGINEER Patterns

**Core behavior:** Generate code exactly as specified

**Pattern: Literal Implementation**
```
1. Read kickoff document carefully
   - Understand EVERY requirement
   - Note examples/templates
   - Identify file structure

2. Check SSOT for existing patterns:
   - Read ARCHITECTURE.md
   - Search for similar code (Grep)
   - Reuse existing patterns

3. Generate code literally:
   - Follow examples exactly
   - Use same naming conventions
   - Match existing style
   - NO optimization
   - NO extra features
   - NO architectural changes

4. Document what was created:
   - Update SPRINT-LOG.md (on "log sprint")
   - List files created
   - Note any clarifications needed
```

**Anti-pattern:** Adding error handling, optimization, extra features

**Example (Good):**
```
Kickoff: "Create User struct with name, email fields"
Output:
  struct User {
      std::string name;
      std::string email;
  };
```

**Example (Bad - scope creep):**
```
Kickoff: "Create User struct with name, email fields"
Output:
  struct User {
      std::string name;
      std::string email;
      std::optional<std::string> phone;  // NOT ASKED FOR

      // Validation (NOT ASKED FOR)
      bool isValid() const {
          return !name.empty() && email.find('@') != std::string::npos;
      }
  };
```

### MACHINIST Patterns

**Core behavior:** Add robustness without changing behavior

**Pattern: Defensive Programming**
```
1. Read existing code
2. Identify missing safety checks:
   - Null/nullptr checks
   - Bounds validation
   - Error handling
   - Input validation
   - Resource cleanup (RAII)

3. Add fundamentals only:
   - Fail-fast assertions
   - Early returns for invalid input
   - Logging (non-intrusive)
   - Clear error messages

4. Verify behavior unchanged:
   - Run tests
   - Compare output
   - Check performance

5. NO cleverness:
   - No optimizations
   - No refactoring
   - No architectural changes
```

**Anti-pattern:** Refactoring code, optimizing, changing logic

**Example (Good):**
```
Before:
  void process(User* user) {
      user->update();
  }

After:
  void process(User* user) {
      if (!user) {
          throw std::invalid_argument("user cannot be null");
      }
      user->update();
  }
```

**Example (Bad - changing behavior):**
```
Before:
  void process(User* user) {
      user->update();
  }

After:
  void process(User* user) {
      if (!user) return;  // Silently ignoring! Behavior changed!

      // Optimized update (NOT ASKED FOR)
      if (user->needsUpdate()) {
          user->update();
      }
  }
```

### AUDITOR Patterns

**Core behavior:** Audit code, report findings

**Pattern: Systematic Audit**
```
1. Read audit phase requirements
2. Check against SPEC.md:
   - All requirements implemented?
   - No extra features added?
   - Edge cases handled?

3. Check against ARCHITECTURE.md:
   - Design patterns followed?
   - Layer separation maintained?
   - Naming conventions consistent?

4. Check style:
   - Language-specific conventions
   - Project-specific guidelines
   - Consistent formatting

5. Check safety:
   - Error handling present
   - Input validation
   - Resource management (RAII)
   - No obvious bugs

6. Write findings report:
   - [N]-[AREA]-findings.md
   - List violations
   - Reference line numbers (file:line)
   - Severity (critical, major, minor)
   - DO NOT FIX (report only)

7. If critical issues found:
   - Block commit
   - Notify ARCHITECT
   - Suggest which role should fix (SURGEON, MACHINIST)
```

**Anti-pattern:** Fixing code, making changes, implementing features

### SURGEON Patterns

**Core behavior:** Fix bugs with minimal changes

**Pattern: Surgical Fix**
```
1. Read RESET context:
   - What broke?
   - When did it break?
   - What changed?
   - Error messages/logs

2. Use Debug Methodology (Fail-Fast Checklist):
   - Check types FIRST
   - Check construction order SECOND
   - Check simple logic THIRD
   - Only then complex theories

3. Locate exact failure point:
   - Use minimal reproduction
   - Don't read entire codebase
   - Focus on call stack

4. Implement minimal fix:
   - Change only broken code
   - No refactoring
   - No optimizations
   - No "while I'm here" changes

5. Verify fix:
   - Test reproduction case
   - Verify no regressions
   - Document in SPRINT-LOG.md (on "log sprint")

6. If discovering undocumented pattern:
   - Note in ARCHITECTURE.md
   - Create PATTERNS-WRITER entry
   - Reference call sites (file:line)
```

**Anti-pattern:** Refactoring entire module, reading entire codebase, theorizing without checking simple bugs

**Example (Good):**
```
Bug: Crash on division
Fix: Add zero check before division (1 line)
```

**Example (Bad - scope creep):**
```
Bug: Crash on division
Fix:
  - Refactor calculation into separate class
  - Add comprehensive error handling
  - Optimize algorithm
  - Add logging
  - Update 10 other files "for consistency"
```

---

## Common Anti-Patterns to Avoid

### 1. The Eager Helper (Scope Creep)

**Symptom:** "While I'm here, let me also..."

**Example:**
```
ARCHITECT: "Add validation to login function"
Agent: "I'll add validation, AND refactor the auth module, AND add logging, AND..."
```

**Fix:** Only do what was asked. Nothing more.

### 2. The Overthinker (Cognitive Overload)

**Symptom:** 50+ messages theorizing about complex causes

**Example:**
```
Bug: Function returns wrong value
Agent: *reads entire codebase*
Agent: "This could be a threading issue... or compiler optimization... or..."
ARCHITECT: "It's a typo in variable name"
```

**Fix:** Use Debug Methodology Fail-Fast Checklist. Check simple first.

### 3. The Architect (Premature Optimization)

**Symptom:** Solving future problems, not current problem

**Example:**
```
ARCHITECT: "Create simple config parser"
Agent: "I'll design an extensible plugin system with XML/JSON/YAML support..."
```

**Fix:** Solve current problem with simplest solution.

### 4. The Assumption Engine (Not Asking)

**Symptom:** Guessing ARCHITECT intent instead of asking

**Example:**
```
ARCHITECT: "Add caching"
Agent: *implements Redis without asking*
ARCHITECT: "I meant in-memory cache"
```

**Fix:** Ask clarifying questions before implementing.

### 5. The Autonomous Actor (Git Disasters)

**Symptom:** Running destructive commands without permission

**Example:**
```
Agent: *runs git merge --abort*
Agent: *destroys ARCHITECT's work*
```

**Fix:** NEVER run destructive git commands. Always ask.

### 6. The SSOT Ignorer (Reinventing)

**Symptom:** Creating new solution without checking existing

**Example:**
```
Agent: "I'll create a new logging system"
*Project already has logging in src/utils/logger.cpp*
```

**Fix:** Check ARCHITECTURE.md, search codebase before creating.

### 7. The Tool Misuser (Wrong Tool for Job)

**Symptom:** Using Bash `cat` instead of Read tool

**Example:**
```
Agent: *uses `cat file.cpp` via Bash*
Better: *uses Read tool*
```

**Fix:** Use Tool Selection Decision Tree.

---

## Integration with CAROL Workflow

### Workflow Summary

```
1. ARCHITECT assigns role:
   "Read carol/CAROL.md. You are COUNSELOR, register in SPRINT-LOG.md"

2. Agent reads role definition + PATTERNS.md

3. Agent follows role-specific patterns:
   - COUNSELOR: Systematic Questioning
   - ENGINEER: Literal Implementation
   - MACHINIST: Defensive Programming
   - AUDITOR: Systematic Audit
   - SURGEON: Surgical Fix
   - (2 PRIMARY + 8 Secondary roles total)

4. Agent uses meta-patterns:
   - Problem Decomposition (planning)
   - Debug Methodology (fixing)
   - Tool Selection (efficiency)
   - Self-Validation (before responding)

5. On "log sprint" command:
   - Update SPRINT-LOG.md with work summary
   - No intermediate files created
```

### Cross-References

- **CAROL.md:** Role definitions and constraints
- **SPEC-WRITER.md:** COUNSELOR conversation guide
- **ARCHITECTURE-WRITER.md:** Multi-role architecture documentation
- **SCRIPTS.md:** Code editing automation (coming soon)
- **PATTERNS-WRITER.md:** Pattern discovery guide (coming soon)

---

## Version History

- **2.1.0** (2026-02-02): Use `carol/` with hidden flag instead of `.carol/` (LLM tools ignore dot-directories)
- **2.0.0** (2026-01-30): CAROL v2.0.0 release
  - Removed JOURNALIST role (consolidated into SPRINT-LOG workflow)
  - Updated from 6 roles to 2 PRIMARY + 8 Secondary model
  - Removed [N]-[ROLE]-[OBJECTIVE].md intermediate files
  - Documentation now only in SPRINT-LOG.md on "log sprint"
  - Streamlined workflow patterns

- **1.0.0** (2026-01-16): Initial release
  - Problem Decomposition Framework
  - Debug Methodology (Fail-Fast Checklist)
  - Tool Selection Decision Tree
  - Self-Validation Checklist
  - Role-Specific Patterns (6 roles)
  - Common Anti-Patterns

---

**CAROL Framework**
Created by JRENG
https://github.com/jrengmusic/carol
