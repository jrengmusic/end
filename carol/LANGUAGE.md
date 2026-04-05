# LANGUAGE ADDENDUM
## BLESSED Compliance per Language

**For:** ARCHITECT and CAROL agents.
**Version:** 0.1 — April 2026

**Referenced by:** MANIFESTO.md

---

## Purpose

MANIFESTO.md defines BLESSED as a language-agnostic contract. This document defines how BLESSED maps to each language's idioms, constraints, and frameworks. Where a language's design makes a BLESSED principle unenforceable or counterproductive, this document declares the accepted adaptation.

**Hierarchy:** MANIFESTO.md is the law. LANGUAGE.md is the jurisdiction. A principle not overridden here is enforced as written in MANIFESTO.md.

---

## C++ / JUCE

**Reference implementation.** MANIFESTO.md was written for C++/JUCE. No overrides. All principles enforced as written.

---

## Go

### Go Language Constraints

Go enforces design choices that conflict with BLESSED at the structural level:

- **No RAII.** No destructors, no deterministic cleanup. `defer` is the closest analog.
- **No struct-private.** Visibility is package-scoped (`exported` / `unexported`), not struct-scoped. All code in the same package sees all fields.
- **Error handling via early return.** `if err != nil { return err }` is the canonical pattern. The compiler, linters, and standard library all assume it.
- **No generics-driven type safety** (limited generics since 1.18, but not expressive enough for BLESSED-level type contracts).
- **Embedding promotes fields.** Struct embedding flattens access — there is no "accessor-only" boundary within a package.

---

### B — Bound (Adapted)

MANIFESTO says: *RAII enforced. Acquisition is initialization, release is destruction.*

**Go override:** RAII does not exist. Ownership is enforced through convention, not compiler.

**Go contracts:**
- Every resource has one clear owner — the struct that creates it is responsible for cleanup
- `defer` is the cleanup mechanism — acquire and defer release in the same function scope
- `context.Context` is the lifecycle boundary for goroutines — no goroutine outlives its context
- No goroutine without a cancellation path — leaked goroutines are B violations
- Channel ownership is explicit — one goroutine owns the write end, one owns the read end, documented in the struct

**Violation signature:** A goroutine with no cancellation. A channel with ambiguous ownership. A resource closed in a different scope than it was opened.

---

### L — Lean (No Override)

300/30/3 applies unchanged. Go's culture prefers short functions and small files. No conflict.

---

### E — Explicit (Adapted)

MANIFESTO says: *No early returns. One exit point. Positive nested checks.*

**Go override:** Early returns are permitted for error handling and precondition guards. The MANIFESTO's rationale (hidden exits, scattered return points) is addressed differently in Go — the `if err != nil { return }` pattern is explicit, not hidden. It fails fast and fails loud.

**Go contracts:**
- **Error guard returns are permitted** — `if err != nil { return ..., fmt.Errorf("context: %w", err) }` is compliant
- **Precondition guard returns are permitted** — `if input == nil { return ..., errors.New("input required") }` is compliant
- **Business logic uses positive nesting** — once guards pass, the happy path reads top to bottom without further returns
- **Every error return includes context** — bare `return err` is a violation. Wrap with `fmt.Errorf` or meaningful message.
- **No silent swallowing** — `_ = SomeFunc()` on an error-returning function is a violation unless the discard is documented with a comment naming the specific reason

**The boundary:** Guards at the top, happy path below. A return statement inside business logic (not error/precondition) is still a violation.

```go
// COMPLIANT — guards at top, happy path below
func process(input *Sample) (*Result, error) {
    if input == nil {
        return nil, errors.New("process: nil input")
    }

    result, err := transform(input)
    if err != nil {
        return nil, fmt.Errorf("process: transform failed: %w", err)
    }

    enriched := enrich(result)
    return enriched, nil
}

// VIOLATION — return inside business logic
func process(input *Sample) (*Result, error) {
    result, err := transform(input)
    if err != nil {
        return nil, fmt.Errorf("process: transform failed: %w", err)
    }

    if result.Score > threshold {
        return result, nil          // <-- early exit inside business logic
    }

    enriched := enrich(result)
    return enriched, nil
}
```

All other E contracts (semantic names, no magic values, fail fast, all parameters visible) apply unchanged.

---

### S — Single Source of Truth (No Override)

Applies unchanged. Go has no language-level excuse for shadow state or duplication.

---

### S — Stateless (No Override)

Applies unchanged. Particularly relevant in Go where goroutines tempt shared mutable state.

---

### E — Encapsulation (Adapted)

MANIFESTO says: *Private by default. No poking internals.*

**Go override:** Go's visibility is package-scoped. Within a package, all fields are accessible. Struct-level accessors within the same package are ceremony — they provide zero protection and add dead code.

**Go contracts:**
- **Encapsulation boundary is the package, not the struct** — within a package, direct field access is the API
- **Accessors exist only at package boundaries** — exported methods on exported types for external callers
- **Within-package accessors are dead code** — remove them. If `UIState.GetMode()` is only called from the same package, delete it and use `uiState.mode` directly
- **Embedding is intentional field promotion** — if you embed a struct, its fields are part of the parent's API within the package. Do not fight this with accessors.
- **Tell, don't ask still applies** — even with direct field access, the orchestrator tells objects to act, not inspect-then-decide

**Violation signature:** An accessor method used only within its own package. An embedded struct whose fields are accessed through both accessor and direct access (pick one). A package that exports fields it should not.

---

### D — Deterministic (No Override)

Emergent property. Applies unchanged.

---

### Anti-Patterns (Go-Specific Additions)

| Anti-Pattern | Violation |
|---|---|
| Bare `return err` without context | **E** (Explicit) |
| `_ = Func()` without documented reason | **E** (Explicit) |
| Goroutine without context cancellation | **B** (Bound) |
| Channel with ambiguous writer/reader | **B** (Bound) |
| Same-package accessor (getter/setter) | **E** (Encapsulation) — dead code |
| `interface{}` / `any` where a concrete type exists | **E** (Explicit) |
| Package-level `var` mutable state | **S** (Stateless) + **B** (Bound) |
| `init()` with side effects | **E** (Explicit) — hidden initialization |

---

### Framework: Bubbletea / Elm Architecture

**Accepted constraint:** The Elm Architecture (TEA) mandates a single root Model that receives all messages via `Update()` and renders all UI via `View()`. This is an inherent L violation (god object) that cannot be resolved without abandoning the framework.

**Mitigation contracts:**
- **State clusters** — decompose the root model into embedded state structs by domain (`UIState`, `NavigationState`, `OperationState`). Each cluster owns its domain. The root model is a composition, not a monolith.
- **File decomposition** — the root model's `Update()` and `View()` dispatch to handler functions in separate files. Each file owns one concern. The 300-line limit applies per file, not to the logical model.
- **Handler maps over switch chains** — message routing and action dispatch use `map[Type]Handler` lookups, not `switch` chains. This enforces SSOT (adding a case is data, not code) and respects the 3-branch limit.
- **View is pure** — `View()` reads state, produces a string, mutates nothing. This is TEA's contract and aligns with Stateless.
- **`tea.Cmd` is the only side effect channel** — no goroutines spawned from `Update()` directly. All async work returns via `tea.Cmd` and results arrive as `tea.Msg`.
- **Nested models are composition, not isolation** — embedding child models is organizational. The parent still routes messages. Accept this as framework cost, not an architecture failure.

**Not a violation:** The root model struct exceeding 300 lines of fields is accepted when fields are organized into state clusters. The 300-line limit applies to logic files, not to the struct definition file.

---

*This document is a companion to MANIFESTO.md. Language-specific overrides declared here take precedence over MANIFESTO.md examples, but not over MANIFESTO.md principles.*

*Rock 'n Roll!*
**JRENG!**

---
*Version 0.1 — April 2026*
