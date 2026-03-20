# PATTERNS-WRITER.md - Pattern Discovery and Documentation Guide

**Version:** 2.2.3
**Purpose:** How agents discover, validate, and document patterns in codebases
**Audience:** MACHINIST, SURGEON (pattern discovery during work), COUNSELOR (documentation compilation)

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

## Table of Contents

1. [When to Document Patterns](#when-to-document-patterns)
2. [Pattern Discovery Process](#pattern-discovery-process)
3. [Pattern Validation](#pattern-validation)
4. [Documentation Templates](#documentation-templates)
5. [Integration with ARCHITECTURE.md](#integration-with-architecturemd)
6. [Role-Specific Guidelines](#role-specific-guidelines)

---

## When to Document Patterns

### Rule of Three

**Document a pattern when you see it 3+ times in the codebase.**

**Example:**
```cpp
// Occurrence 1 (src/processor.cpp)
if (!buffer) {
    throw std::invalid_argument("buffer cannot be null");
}

// Occurrence 2 (src/handler.cpp)
if (!data) {
    throw std::invalid_argument("data cannot be null");
}

// Occurrence 3 (src/loader.cpp)
if (!config) {
    throw std::invalid_argument("config cannot be null");
}
```

**Action:** Document as "Null Check Pattern" in ARCHITECTURE.md

### When NOT to Document

- **Single occurrence:** Not a pattern yet
- **Two occurrences:** Wait for third
- **Standard library usage:** Don't document `std::vector`, `std::map` usage
- **Language idioms:** Don't document `RAII`, `defer`, `with` statements
- **Framework conventions:** Don't document React hooks, JUCE timer callbacks (unless project-specific customization)

### Exception: Critical Patterns

**Document immediately if:**
- Safety-critical (prevents crashes, data loss)
- Performance-critical (affects real-time guarantees)
- Architectural constraint (layer separation, SSOT violation)
- Subtle bug source (documented failures)

**Example:**
```cpp
// DSP class - MUST be trivially copyable
class AudioProcessor {
    static_assert(std::is_trivially_copyable_v<AudioProcessor>,
                  "AudioProcessor must be trivially copyable for lock-free usage");
    // ...
};
```

**Action:** Document immediately in ARCHITECTURE.md "Real-Time Safety Patterns"

---

## Pattern Discovery Process

### Step 1: Recognize Pattern

**While working, notice:**
- Same code structure repeated
- Similar function signatures
- Consistent naming conventions
- Repeated error handling
- Common data flow

**MACHINIST:** Notice while adding error handling
**SURGEON:** Notice while fixing bugs
**ENGINEER:** Notice while generating code
**AUDITOR:** Notice during audits

### Step 2: Search for All Occurrences

**Use tools systematically:**

```bash
# Search for similar code patterns
# Example: Find all null checks
grep -r "if (!.*) {" src/

# Find similar function names
grep -r "validate.*(" src/

# Find similar class structures
grep -r "class.*Processor" src/
```

**Document findings:**
```
Pattern: Null Check with exception
Occurrences:
  - src/processor.cpp:42
  - src/handler.cpp:89
  - src/loader.cpp:15
  - tests/test_utils.cpp:33
Total: 4 instances
```

### Step 3: Extract Common Structure

**Identify:**
- What varies? (variable names, types)
- What's constant? (structure, intent)
- Why this pattern? (rationale)
- What breaks if violated? (consequences)

**Example:**

**Variations:**
- Variable names: `buffer`, `data`, `config`, `user`
- Types: `Buffer*`, `Data*`, `Config*`, `User*`

**Constants:**
- Structure: `if (!var) { throw std::invalid_argument("..."); }`
- Intent: Fail-fast on null pointer
- Rationale: Prevent segfaults, clear error messages
- Consequence: Crash without error if omitted

### Step 4: Validate Against Architectural Manifesto

**Check alignment with LIFESTAR + LOVE:**

```
[ ] Lean: Is pattern minimal?
    - No unnecessary complexity
    - Simplest solution that works

[ ] Explicit: Is intent clear?
    - Obvious why pattern exists
    - Clear what it prevents

[ ] SSOT: Is pattern reusable?
    - Works across similar contexts
    - Not duplicating other patterns

[ ] Fail-Fast (LOVE): Does it catch errors early?
    - Prevents late failures
    - Clear error messages

[ ] Validates (LOVE): Does it enforce correctness?
    - Type safety, bounds checking
    - Architectural constraints
```

**If pattern violates principles:**
- Document as **anti-pattern**
- Propose refactoring
- Note in [N]-[ROLE]-[OBJECTIVE].md

### Step 5: Document Pattern

**Write in ARCHITECTURE.md** (see templates below)

---

## Pattern Validation

### Validation Checklist

Before documenting pattern in ARCHITECTURE.md:

```
[ ] Rule of Three satisfied (3+ occurrences)
    OR critical pattern (safety/performance)

[ ] All occurrences found and listed (file:line)

[ ] Common structure extracted

[ ] Variations identified

[ ] Rationale documented (WHY this pattern)

[ ] Consequences documented (WHAT happens if violated)

[ ] Aligned with Architectural Manifesto

[ ] Examples provided (real code from project)

[ ] Cross-references added (related patterns, specs)
```

### Anti-Pattern Validation

If documenting anti-pattern (bad pattern to avoid):

```
[ ] Documented failure case (why it's bad)

[ ] Alternative pattern provided (correct approach)

[ ] Migration path described (how to fix)

[ ] Grep pattern provided (find all instances)
```

---

## Documentation Templates

### Template 1: Design Pattern

**Use for:** Repeating code structures, architectural patterns

```markdown
### [Pattern Name]

**Category:** [Design | Safety | Performance | Architecture]

**Intent:** [Why this pattern exists]

**Structure:**
```[language]
[Code template showing pattern structure]
```

**Occurrences:**
- `[file:line]` - [Brief context]
- `[file:line]` - [Brief context]
- `[file:line]` - [Brief context]

**Rationale:**
[Explain WHY we use this pattern]

**Consequences if Violated:**
[What breaks if pattern not followed]

**Related Patterns:**
- [Link to related pattern]

**Examples:**

```[language]
// Good: Following pattern
[Real code example from codebase]

// Bad: Violating pattern
[Anti-example showing what NOT to do]
```

**Validation:**
[How to verify pattern is followed]
```

**Example - Null Check Pattern:**

```markdown
### Null Check with Exception

**Category:** Safety

**Intent:** Fail-fast on null pointer arguments to prevent segfaults

**Structure:**
```cpp
if (!ptr) {
    throw std::invalid_argument("ptr cannot be null");
}
```

**Occurrences:**
- `src/processor.cpp:42` - Audio buffer validation
- `src/handler.cpp:89` - Event data validation
- `src/loader.cpp:15` - Config validation
- `tests/test_utils.cpp:33` - Test setup validation

**Rationale:**
- Prevents silent segfaults
- Clear error messages for debugging
- Fail-fast principle (catch errors at boundary)
- Enforces contract (non-null parameters)

**Consequences if Violated:**
- Segmentation fault (crash without error)
- Difficult debugging (crash location != cause)
- Production instability

**Related Patterns:**
- Bounds Check Pattern
- Input Validation Pattern

**Examples:**

```cpp
// Good: Following pattern
void processBuffer(AudioBuffer* buffer) {
    if (!buffer) {
        throw std::invalid_argument("buffer cannot be null");
    }
    // Process buffer...
}

// Bad: No validation (violates pattern)
void processBuffer(AudioBuffer* buffer) {
    buffer->process();  // Segfault if buffer is null!
}

// Bad: Silent return (violates fail-fast)
void processBuffer(AudioBuffer* buffer) {
    if (!buffer) return;  // Silent failure, hard to debug
    buffer->process();
}
```

**Validation:**
```bash
# Find all null checks
grep -r "if (!.*) {" src/ | grep "throw std::invalid_argument"

# Find functions missing null checks (manual review)
grep -r "void.*\*" src/ | grep -v "if (!"
```
```

---

### Template 2: Anti-Pattern

**Use for:** Patterns to AVOID

```markdown
### [Anti-Pattern Name]

**Category:** Anti-Pattern

**Problem:** [What makes this bad]

**Bad Example:**
```[language]
[Code showing anti-pattern]
```

**Why It's Bad:**
- [Reason 1]
- [Reason 2]
- [Documented failure if applicable]

**Correct Pattern:**
```[language]
[Code showing correct approach]
```

**Migration:**
[How to fix existing code]

**Detection:**
```bash
[Grep pattern to find instances]
```

**Occurrences:** (if any found in codebase)
- `[file:line]` - [Context]
```

**Example - Silent Null Return:**

```markdown
### Silent Null Return (Anti-Pattern)

**Category:** Anti-Pattern

**Problem:** Returning early on null without error makes debugging difficult

**Bad Example:**
```cpp
void processData(Data* data) {
    if (!data) return;  // Silent failure
    data->process();
}
```

**Why It's Bad:**
- Violates fail-fast principle
- Caller doesn't know operation failed
- Hard to debug (no error message)
- Inconsistent with codebase (see Null Check Pattern)

**Correct Pattern:**
```cpp
void processData(Data* data) {
    if (!data) {
        throw std::invalid_argument("data cannot be null");
    }
    data->process();
}
```

**Migration:**
1. Search for silent null returns:
   ```bash
   grep -r "if (!.*) return;" src/
   ```
2. Replace with exception throw
3. Verify tests still pass
4. Update callers if needed (handle exception)

**Detection:**
```bash
# Find all silent null returns
grep -r "if (!.*) return;" src/
```

**Occurrences:**
- None found (good!)
```

---

### Template 3: Performance Pattern

**Use for:** Performance-critical patterns (hot paths, real-time code)

```markdown
### [Performance Pattern Name]

**Category:** Performance

**Intent:** [Performance goal]

**Critical Path:** [Where this matters: audio callback, render loop, etc.]

**Pattern:**
```[language]
[Code template]
```

**Performance Impact:**
- [Metric: latency, throughput, allocations]

**Benchmarks:**
```
[Benchmark results showing impact]
```

**Occurrences:**
- `[file:line]` - [Context]

**Rationale:**
[Why performance matters here]

**Violations to Avoid:**
- [Anti-pattern 1]
- [Anti-pattern 2]

**Validation:**
```bash
[How to verify pattern followed]
```
```

**Example - Lock-Free DSP:**

```markdown
### Trivially Copyable DSP Classes

**Category:** Performance

**Intent:** Enable lock-free sharing between audio and UI threads

**Critical Path:** Audio callback (real-time thread)

**Pattern:**
```cpp
class AudioProcessor {
    static_assert(std::is_trivially_copyable_v<AudioProcessor>,
                  "AudioProcessor must be trivially copyable");

    // Only trivially copyable members
    float gain;
    double frequency;
    int32_t mode;

    // No: std::vector, std::string, virtual functions
};
```

**Performance Impact:**
- Zero allocation in audio thread
- Lock-free parameter updates
- <1μs copy overhead

**Benchmarks:**
```
Trivially copyable:     0.8μs (lock-free copy)
std::vector member:     45μs (heap allocation)
Mutex lock:            120μs (context switch)
```

**Occurrences:**
- `src/dsp/Processor.hpp:15` - Main audio processor
- `src/dsp/Filter.hpp:22` - Filter state
- `src/dsp/Envelope.hpp:18` - Envelope generator

**Rationale:**
- Real-time audio cannot block (no locks)
- Allocations cause latency spikes
- Trivially copyable enables atomic<T> usage

**Violations to Avoid:**
- ❌ Virtual functions (vtable pointer not trivially copyable)
- ❌ `std::vector`, `std::string` (heap allocation)
- ❌ Custom copy constructor (breaks trivial copyability)
- ❌ Pointers to dynamically allocated memory

**Validation:**
```bash
# Check all DSP classes have static_assert
grep -r "class.*Processor" src/dsp/ | while read line; do
    file=$(echo "$line" | cut -d: -f1)
    grep "static_assert.*trivially_copyable" "$file" || echo "Missing: $file"
done
```
```

---

## Integration with ARCHITECTURE.md

### Where to Document Patterns

**ARCHITECTURE.md Structure:**

```markdown
# Project Architecture

## Design Patterns in Use

### [Pattern Category]

#### [Pattern Name]
[Use template from above]

#### [Pattern Name]
[Use template from above]

### [Another Category]

...

## Anti-Patterns to Avoid

### [Anti-Pattern Name]
[Use anti-pattern template]

## Performance Patterns

### [Performance Pattern Name]
[Use performance template]
```

### Cross-Referencing

**Link patterns to:**
- **SPEC.md:** Requirements that necessitate pattern
- **SPRINT-LOG.md:** Decisions about pattern adoption
- **PATTERNS.md:** Meta-patterns (debugging, validation)
- **Other patterns:** Related or conflicting patterns

**Example:**
```markdown
### Null Check Pattern

...

**Related Specifications:**
- See `SPEC.md` Section 3.2 (Error Handling Requirements)

**History:**
- See `SPRINT-LOG.md` Sprint 5 (Decision to use exceptions over error codes)

**Related Patterns:**
- Bounds Check Pattern (similar validation approach)
- Input Validation Pattern (higher-level validation)

**Meta-Patterns:**
- See `PATTERNS.md` Debug Methodology (Fail-Fast Checklist)
```

---

## Role-Specific Guidelines

### MACHINIST: Pattern Discovery While Polishing

**When adding error handling, validation, safety checks:**

1. **Before adding check:**
   ```bash
   # Search if similar check exists
   grep -r "if (!.*)" src/
   ```

2. **If found 3+ times:**
   - Extract pattern
   - Document in ARCHITECTURE.md
   - Apply consistently
   - Note in [N]-MACHINIST-*.md

3. **If creating new pattern:**
   - Document intent (why needed)
   - Add validation (how to verify)
   - Propose to ARCHITECT for approval

**Example Sprint:**

```markdown
# SPRINT-15-MACHINIST-NULL-CHECKS.md

## Task
Add null checks to handler functions

## Pattern Discovered
Found 4 instances of null check with exception:
- src/processor.cpp:42
- src/handler.cpp:89
- src/loader.cpp:15
- tests/test_utils.cpp:33

## Action Taken
1. Documented "Null Check Pattern" in ARCHITECTURE.md
2. Applied pattern to 3 additional functions consistently
3. Added validation script to detect missing checks

## Files Modified
- ARCHITECTURE.md (pattern documentation)
- src/validator.cpp (added null checks)
- src/exporter.cpp (added null checks)
- src/importer.cpp (added null checks)
```

---

### SURGEON: Pattern Discovery While Fixing

**When fixing bugs, notice patterns that could prevent similar bugs:**

1. **After identifying root cause:**
   - Was this bug preventable with pattern?
   - Do similar bugs exist elsewhere?
   - Should we document pattern?

2. **Search for similar bugs:**
   ```bash
   # Example: Float precision bug
   grep -r "float.*0\.999" src/
   ```

3. **If pattern found:**
   - Document in ARCHITECTURE.md
   - Note in [N]-SURGEON-PATTERN-DISCOVERED.md
   - Propose systematic fix to ARCHITECT

**Example Sprint:**

```markdown
# SPRINT-22-SURGEON-PATTERN-DISCOVERED.md

## Bug Fixed
Float/double precision loss in smoothing factor

## Root Cause
`float smoothingFactor = 0.999;` loses precision
Should be: `double smoothingFactor = 0.999;`

## Pattern Discovered
Found 12 instances of numeric literals assigned to float:
- src/processor.cpp:42 (fixed)
- src/filter.cpp:89 (potential bug)
- src/envelope.cpp:15 (potential bug)
- ... (9 more)

## Recommendation
1. Document "Numeric Precision Pattern" in ARCHITECTURE.md
2. Use double by default for constants >0.9
3. Add static_assert for precision-critical values
4. Run validation across codebase

## Files Modified
- src/processor.cpp (bug fix)
- ARCHITECTURE.md (pattern documentation)
```

---

### AUDITOR: Pattern Validation During Audit

**When auditing code:**

1. **Check pattern compliance:**
   ```bash
   # Load patterns from ARCHITECTURE.md
   # Verify each occurrence follows pattern
   ```

2. **Report violations:**
   - File:line reference
   - Which pattern violated
   - Severity (critical, major, minor)

3. **Suggest fixes:**
   - Which role should fix (MACHINIST, SURGEON)
   - Priority (immediate, next sprint)

**Example Audit Report:**

```markdown
# 3-AUDITOR-PATTERNS.md

## Pattern Compliance Audit

### Null Check Pattern
**Status:** 85% compliant

**Violations Found:**
- `src/importer.cpp:45` - Missing null check (Major)
- `src/exporter.cpp:78` - Silent return instead of exception (Major)

**Recommendation:**
- Assign MACHINIST to add missing checks
- Priority: Before next commit

### Trivially Copyable DSP
**Status:** 100% compliant

**Validated:**
- All DSP classes have static_assert
- No violations found

### Summary
- 2 violations found
- 1 pattern fully compliant
- Recommend: Fix violations before commit
```

---

### COUNSELOR: Pattern Compilation

**When compiling sprint summaries:**

1. **Track pattern evolution:**
   - When pattern discovered (sprint)
   - Who documented (role)
   - Files affected

2. **Update SPRINT-LOG.md:**
   ```markdown
   ## Sprint 22 - SURGEON
   **Pattern Discovered:** Numeric Precision Pattern
   **Documented in:** ARCHITECTURE.md
   **Impact:** 12 potential bugs identified
   ```

3. **Create pattern index:**
   ```markdown
   ## Pattern Index
   - Null Check Pattern (Sprint 15, MACHINIST)
   - Numeric Precision Pattern (Sprint 22, SURGEON)
   - Trivially Copyable DSP (Sprint 8, COUNSELOR)
   ```

---

## Validation Against Architectural Manifesto

Every pattern must align with **LIFESTAR + LOVE:**

### Checklist

```
LIFESTAR:
[ ] Lean: Pattern is minimal, no unnecessary complexity
[ ] Immutable: Pattern produces deterministic results
[ ] Findable: Pattern documented in obvious location (ARCHITECTURE.md)
[ ] Explicit: Intent and rationale clear
[ ] SSOT: Pattern defined once, referenced everywhere
[ ] Testable: Validation script can verify compliance
[ ] Accessible: Works across languages/frameworks (or language-specific noted)
[ ] Reviewable: Clear examples, occurrences listed

LOVE:
[ ] Listens (Fail-Fast): Pattern catches errors early
[ ] Optimizes: Pattern prevents wasted effort/bugs
[ ] Validates: Pattern enforces correctness
[ ] Empathizes: Pattern makes code easier to understand/maintain
```

**If pattern fails checklist:**
- Refine pattern
- Document as anti-pattern
- Propose alternative

---

## Examples from Real Projects

### Example 1: JUCE Audio Project

**Pattern Discovered:** RAII for Audio Resources

```markdown
### RAII Audio Resource Pattern

**Category:** Safety

**Intent:** Ensure audio resources released even on exception

**Structure:**
```cpp
class AudioResourceGuard {
    AudioDevice* device;
public:
    AudioResourceGuard(AudioDevice* d) : device(d) { device->acquire(); }
    ~AudioResourceGuard() { device->release(); }

    // Delete copy, allow move
    AudioResourceGuard(const AudioResourceGuard&) = delete;
    AudioResourceGuard(AudioResourceGuard&& other) : device(other.device) {
        other.device = nullptr;
    }
};
```

**Occurrences:**
- `src/audio/DeviceManager.cpp:89`
- `src/audio/FilePlayer.cpp:42`
- `src/audio/Recorder.cpp:33`

**Rationale:**
- Audio device must be released (prevent resource leak)
- Exceptions can bypass manual release()
- RAII guarantees cleanup in all paths

**Consequences if Violated:**
- Audio device stuck in "busy" state
- Cannot reopen device without restart
- Memory leak
```

---

### Example 2: Go Microservice

**Pattern Discovered:** Context Propagation

```markdown
### Context Propagation Pattern

**Category:** Architecture

**Intent:** Propagate cancellation and timeouts through call chain

**Structure:**
```go
func handleRequest(ctx context.Context, req *Request) error {
    // Pass context to all downstream calls
    data, err := fetchData(ctx, req.ID)
    if err != nil {
        return err
    }

    return processData(ctx, data)
}
```

**Occurrences:**
- `handlers/user.go:42`
- `handlers/order.go:89`
- `services/payment.go:15`

**Rationale:**
- Enables request timeout enforcement
- Allows graceful shutdown
- Prevents goroutine leaks

**Consequences if Violated:**
- Goroutines run indefinitely
- Cannot enforce deadlines
- Memory leaks on cancellation
```

---

## Cross-References

- **CAROL.md:** Role definitions
- **PATTERNS.md:** Meta-patterns (debugging, validation)
- **ARCHITECTURE-WRITER.md:** How to write ARCHITECTURE.md
- **SCRIPTS.md:** Validation scripts for pattern compliance

---

## Version History

- **1.0.0** (2026-01-16): Initial release
  - Pattern discovery process
  - Validation checklist
  - Documentation templates (design, anti-pattern, performance)
  - Role-specific guidelines
  - Integration with ARCHITECTURE.md

---

**CAROL Framework**
Created by JRENG
https://github.com/jrengmusic/carol
