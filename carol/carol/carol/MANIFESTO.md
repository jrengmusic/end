# ARCHITECTURAL MANIFESTO
## BLESSED — The Contract

**For:** ARCHITECT and CAROL agents.
**Version:** 0.1 — April 2026

---

## Core Mantra

**NEVER OVERDO IT.**

This document is the single source of truth for architectural decisions, code design, and problem-solving. It is a contract — not a guideline. All code is evaluated against it.

**Language-specific compliance:** See `LANGUAGE.md` for per-language overrides and framework adaptations. LANGUAGE.md adapts how principles are enforced — it does not override what they mean.

---

## BLESSED

```
B — Bound
L — Lean
E — Explicit
S — Single Source of Truth
S — Stateless
E — Encapsulation
D — Deterministic
```

---

## B — Bound

**Clear ownership. Deterministic lifecycle. RAII enforced. Nothing floats free.**

Every resource, object, and dependency has exactly one owner. Lifetime is scoped and unambiguous. Acquisition is initialization, release is destruction. No raw owning pointers. No manual cleanup. No ambiguous destruction order.

If you need a `SafePointer` or a lifetime guard, ownership was wrong to begin with. Fix the ownership — the guard disappears.

**Contracts:**
- Every object has one clear owner
- Lifecycle is traceable from construction to destruction
- Threads are bound to their context — audio thread owns audio, UI thread owns UI, never crossed
- No object outlives its owner
- No resource exists without an owner

**Violation signature:** *"I'm not sure what outlives what here."* — that is a B violation. Clarify ownership, RAII handles the rest.

**The Guard Rule:** Every defensive guard must name its specific threat. If you cannot answer *"what specific scenario does this defend against?"* — the guard is garbage. Remove it and fix the ownership instead.

---

## L — Lean

**300/30/3. Quantity enforced. God objects forbidden.**

These are smell detectors, not arbitrary limits. Crossing them means stop and ask why. The answer is almost always wrong decomposition.

- **300 lines per file** — file is too large, too many responsibilities. Split the object.
- **30 lines per function** — function is doing more than one thing. Split the function.
- **3 branches max** — more than 3 `if/else` or `switch` cases means you are encoding a decision table in imperative code. Replace with direct lookup.

The 3-branch rule is the sharpest. A lookup is clearer in intent, O(1), and adding a case becomes data — not a code change. This directly enforces **S** (SSOT).

Lean is about **quantity**. Balanced decomposition — distributing responsibilities correctly — is how you achieve Lean. Descriptor holds data. Manager holds logic. View holds composition. You decompose to keep each piece within bounds.

**Violation signature:** Anything a reasonable reader would call a god object. Any function that needs a scroll to read. Any chain of conditions that needs a mental truth table.

---

## E — Explicit

**No magic. Semantic names. Clarity over brevity. Fail fast. All parameters visible.**

Code is read far more than it is written. Every name, every parameter, every condition must declare its intent without requiring the reader to infer it.

**Contracts:**
- No magic values — every constant is named, intent lives in the name
- Semantic names — `gainReductionDb` not `gr`, `isProcessingActive` not `flag`
- Clarity over brevity — never sacrifice readability for a shorter name
- All parameters visible in the function signature — nothing pulled from implicit context, no hidden globals, explicit capture lists in lambdas (`[this, value]` not `[&]`)
- No early returns — one exit point, positive nested checks, happy path reads top to bottom
- Prefer `jassert` over silent fail — invalid state is loud, never swallowed
- Fail fast, debug early — catch violations at the entry point, never let corrupt state propagate

**On early returns:** Early returns hide intent. They scatter exit points and force the reader to track every possible escape mentally. Positive nesting makes the full execution path visible in a single read. A function that exits early has lied about its contract.

```cpp
// WRONG — hides conditions, silent exits
void process (const Sample& input)
{
    if (! input.isValid()) return;
    if (! isInitialized()) return;
    if (isBypassed()) return;
    doWork (input);
}

// CORRECT — full contract visible, one exit
void process (const Sample& input)
{
    jassert (input.isValid());

    if (input.isValid())
    {
        if (isInitialized())
        {
            if (! isBypassed())
                doWork (input);
        }
    }
}
```

**Violation signature:** A name that requires context to understand. A function that exits somewhere you didn't expect. A value that appeared without being passed in. A failure that produced no signal.

---

## S — Single Source of Truth

**Declare once. Reference everywhere. DRY.**

Every concept, piece of logic, and data structure is defined in exactly one place. All other parts of the system reference that single definition. Duplication is not just inefficient — it creates divergence.

**The primary violation is shadow state** — the same truth existing in two places that can drift:

```cpp
bool isActive;       // mirrors what Model already knows
int currentIndex;    // duplicates what the container owns
float cachedGain;    // shadows the APVTS parameter
```

Shadow state feels helpful until the two copies disagree. It is almost always a symptom of not trusting the Model (**S2** violation) or ambiguous ownership (**B** violation).

**Contracts:**
- Check if functionality already exists before creating new implementations
- Extract repeated patterns into reusable functions, classes, or modules
- Use named constants, configuration, or schema — never hardcode values
- If the same logic appears more than twice, it must be abstracted
- A second copy of any truth is a bug waiting to happen

**Violation signature:** Two variables that represent the same thing. Logic duplicated across two files. A hardcoded value that appears more than once.

---

## S — Stateless

**Objects are dumb workers. Transient state only. Machinery holds nothing persistent.**

Objects execute what they are told. They do not hold opinions about the system, remember what they did last, or track their own history for the orchestrator's benefit.

**Contracts:**
- Transient state only — calculation buffers, intermediate values, anything that lives and dies within a single operation
- No machinery state — an object does not track its own history for the caller
- Almost never a getter — if the orchestrator needs to ask, the design is wrong
- Orchestrator tells, never tracks — Control says *"process this"*, not *"are you ready? what was your last state? ok now process"*
- State belongs exclusively to the Model. View and machinery are pure.

DSP processor parameters *look* like state but are not machinery state — they are **calculation inputs**, always a deterministic reflection of APVTS downward. APVTS is the one truth. Processor values are a synced working copy for performance.

**The violation pattern:**

```cpp
// WRONG — orchestrator tracking object state
if (processor.isReady() && ! processor.hasProcessed() && processor.inputValid())
    processor.process();

// CORRECT — object manages itself, orchestrator tells
processor.process (input);
```

The booleans are the smell. They mean the orchestrator is doing the object's thinking for it.

**Violation signature:** A boolean flag that tracks what a subordinate object is doing. A getter called by the orchestrator to make a decision. State that lives outside the Model.

---

## E — Encapsulation

**Clear single responsibility. No poking internals. Tell, don't ask. Unidirectional layer flow.**

Objects are ignorant by design. They know nothing about the world outside their one job. The orchestrator tells — it never inspects an object's state to make decisions on its behalf.

**The Four Rules:**

1. **One responsibility** — an object is either POD, pure functional, or both. It has one job and is ignorant of the system around it.
2. **Private by default** — expose only when there is a proven external caller. No getter without a proven need. Dead getters are dead code.
3. **Objects manage their own state** — callers do not track flags on behalf of objects. If the object knows whether it is initialized, the caller does not also track that.
4. **Tell, don't ask** — the caller says *"do this"*. The caller never says *"are you in state X? then I will do Y for you."*

**Layer topology is strictly unidirectional.** Lower layers never know about higher layers. No `#include "HigherLayer.h"` in a lower layer. No reverse dependencies. Communication flows through explicit APIs only.

**Concrete example — ProcessorChain:**

```
PluginProcessor   →   owns ProcessorChain
ProcessorChain    →   owns DSP Processors, listens to parameterChanged, tells processors to calculate
DSP Processors    →   dumb, calculate on tell, store only calculation inputs
APVTS             →   the actual state machine, single source of truth
```

ProcessorChain listens to `parameterChanged`, tells each processor to recalculate, and replaces samples on `processBlock`. Each DSP processor is dumb — it stores parameter values as calculation inputs only, always synced top-down from APVTS. No processor ever asks ProcessorChain anything. No ProcessorChain ever asks PluginProcessor anything.

**On established patterns — for agents and junior devs:**
If the architecture uses listeners, use listeners. If parameters flow through APVTS, do not invent a parallel channel. A manual boolean flag, a manual callback, or a helper invented where a listener pattern already exists is not a solution — it is a symptom of not reading the architecture. Find the established pattern. Extend it. A new pattern where one already exists is always wrong.

```cpp
// WRONG — invented state, parallel channel, orchestrator poking
class Component
{
    bool shuttingDown { false };

    ~Component()
    {
        shuttingDown = true;
        session.shutdown();
    }

    void resized()
    {
        if (! shuttingDown)
            if (session.isRunning())
                session.resize (cols, rows);
    }
};

// CORRECT — tell only, object manages itself
class Component
{
    ~Component() = default;

    void resized()
    {
        session.resized (cols, rows);
    }
};
```

**Violation signature:** A getter called by a caller to make a decision for the object. An object that knows about another object's domain. A layer that includes a header from a layer above it. A manual boolean where a listener already exists.

---

## D — Deterministic

**D is not a principle you implement. It is what you get when you follow BLESSE correctly.**

The same input must always yield the same output throughout the entire data flow and processing chain. If it does not — a principle above was violated. Find it.

Non-determinism is always a symptom, never a root cause.

| Symptom | Likely violation |
|---|---|
| Hidden state producing different results | **S** (Stateless) |
| Shadow state that drifted | **S** (SSOT) |
| Something mutated outside its owner | **B** (Bound) |
| Implicit dependency not visible | **E** (Explicit) |
| Object doing more than its job | **E** (Encapsulation) |

**D is the debug protocol:**
1. Output is non-deterministic → start here
2. Trace the data flow
3. Find which of BLESSE was violated
4. Fix the principle violation — not the symptom

**D is the health metric of the architecture.** BLESSE is the law. D is the verdict.

Enforcement:
- `jassert` at boundaries catches violations early
- Unit tests prove it formally — same input, bit-identical output
- Non-determinism in production means something floated free

---

## Anti-Patterns

| Anti-Pattern | Violation |
|---|---|
| God object | **L** |
| Manual boolean to track subordinate state | **S** (Stateless) + **E** (Encapsulation) |
| Shadow state / duplicate truth | **S** (SSOT) + **B** |
| Early return | **E** (Explicit) |
| Silent fail | **E** (Explicit) |
| Getter without proven caller | **E** (Encapsulation) |
| Caller tracking state the object already represents | **S** (Stateless) + **E** (Encapsulation) |
| Layer violation (`#include "HigherLayer.h"`) | **E** (Encapsulation) + **B** |
| Defensive guard with no named threat | **B** |
| Magic value / unnamed constant | **E** (Explicit) + **S** (SSOT) |
| Hardcoded value appearing more than once | **S** (SSOT) |
| Inventing a new pattern where one exists | **E** (Encapsulation) |
| Raw owning pointer / manual cleanup | **B** |
| Cross-thread direct call | **B** |

---

*This document is the contract. All code, designs, and solutions must be evaluated against BLESSED.*

*Rock 'n Roll!*
**JRENG!**

---
*Version 0.1 — April 2026*
