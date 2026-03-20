# JRENG! CODING STANDARDS

**Purpose:** This document defines coding standards for LLM agents to follow when generating C++ code. These rules ensure consistency, readability, and maintainability.

---

## CORE PRINCIPLE: DRY (Don't Repeat Yourself)
**Never duplicate code.** Extract common logic into functions, use inheritance, templates, or other abstractions to eliminate repetition.

---

## FORMATTING

### Braces
Always place opening braces on a new line:
```cpp
// WRONG
if (x == 0) {
    foobar();
}

// CORRECT
if (x == 0)
{
    foobar();
}
```

### Spacing

**Operators:**
```cpp
x = 1 + y - 2 * z / 3;  // Space around binary operators
```

**Special operators:**
- Use C++ alternative tokens: `not`, `and`, `or` — NEVER `!`, `&&`, `||`
- `~` preceded by space, not followed: `foo = ~ bar`
- `++` and `--` no spaces: `++i`, `i++`

**Function calls:**
```cpp
foo (x, y);     // Space after comma only
foo (123);      // Space after function name
foo();          // Space after function name
foo[1];         // No space before brackets
```

**Control statements:**
- Blank line before `if`, `for`, `while`, `do` when preceded by another statement
- Blank line after closing brace (unless next line is also a closing brace)

**One-line statements allowed:**
```cpp
if (x == 1) return "one";
if (x == 2) return "two";
```

**Lambdas:**
```cpp
auto myLambda = [] { return 123; };
auto myLambda = [this, &x] (int z) -> float { return x + z; };

auto longerLambda = [] (int x, int y) -> int
{
    // multiple lines
};
```

---

## POINTERS AND REFERENCES

**Asterisk/ampersand placement:**
```cpp
SomeObject* myObject = getAPointer();   // Correct: sticks to type
SomeObject& myObject = getAReference(); // Correct: sticks to type
```

**Multiple pointers:**
```cpp
// WRONG
SomeObject* p1, *p2;

// CORRECT
SomeObject* p1;
SomeObject* p2;
```

**Const placement:**
```cpp
const Thing& t;  // CORRECT: const before type
Thing const& t;  // WRONG
```

---

## TYPE DECLARATIONS

**Templates:**
```cpp
vector<int>
template <typename Type1, typename Type2>  // Use "typename", descriptive names
```

**Line continuation:**
```cpp
// CORRECT: operator at start of continuation line
auto xyz = foo + bar
         + func (123)
         - def + 4321;

// CORRECT: method chaining
auto t = AffineTransform::translation (x, y)
                         .scaled (2.0f)
                         .rotated (0.5f);
```

---

## COMMENTS

**Prefer `//` over `/* */`** for easier block commenting during debugging.

**Alignment:**
```cpp
// CORRECT
/* This is correct
 */

/** This is also correct
 */

// WRONG
/* This is wrong!
*/
```

**Spacing:**
```cpp
// yes!
//no!
```

---

## LITERALS AND CONSTANTS

**Numeric literals:**
```cpp
0xabcdef  // Hex lowercase
0.0       // Double
0.0f      // Float

// AVOID
0.        // No
.1        // No
.1f       // No
```

**Variable initialization:**
Prefer brace initialization over `=`:
```cpp
// CORRECT
int myInteger { 9 };
double sampleRate { 48000.0 };
float gain { 0.5f };

// AVOID
int myInteger = 9;
double sampleRate = 48000.0;
```

**String construction:**
```cpp
String w ("World");              // Best
auto w = "Hello " + w;           // Use implicit conversion
```

---

## NAMING CONVENTIONS

- **Variables:** `myVariableName` (camelCase)
- **Classes:** `MyClassName` (PascalCase)
- **Use descriptive names,** not type-based names
- **Avoid `JUCE_` prefix** (reserved for JUCE library)
- **Enums:** Use `enum class`
```cpp
enum class MyEnum
{
    enumValue1 = 0,
    enumValue2 = 1
};
```
- **Template parameters:** Use descriptive names, not `T`

---

## CONST AND CONSTEXPR

- **Make everything `const` that can be `const`**
- **Use `constexpr` wherever possible** for compile-time evaluation
- **Local variables:** Only mark `const` if it improves readability in longer blocks
- **Function parameters:** Default to `const` for references

---

## OVERRIDE AND VIRTUAL

```cpp
void myFunction() override;        // CORRECT: use override
void myFunction() virtual;         // WRONG: never with override
```

**Always use `noexcept`** where applicable (can improve performance up to 10x).

---

## POINTERS AND NULLPTR

**Null checks:**
```cpp
if (myPointer != nullptr)  // CORRECT: explicit
if (myPointer)             // WRONG: implicit cast

if (myPointer == nullptr)  // CORRECT: explicit
if (! myPointer)           // WRONG: implicit
```

**Use modern casts:**
```cpp
static_cast<float> (x)     // For trivial casts
reinterpret_cast           // For data reinterpretation
```

**Integer types:**
```cpp
int64_t      // Prefer standard or juce::int64
long long    // Avoid
```

---

## MEMORY MANAGEMENT

**NEVER use raw `new` or `delete`** unless absolutely necessary.

**Prefer:**
1. Stack allocation
2. `std::unique_ptr` or `juce::ScopedPointer`
3. `juce::HeapBlock` for arrays
4. Reference passing over pointer passing

**NEVER:**
- Use `new[]` or `malloc` for C++ arrays
- Use `malloc` or `calloc` at all

**Zero initialization:**
```cpp
MyStruct s = {};           // First choice
zerostruct (s);            // If needed
zeromem (&s, sizeof (s));  // Last resort
memset (&s, 0, sizeof (s)) // AVOID
```

---

## OWNERSHIP

**Pass ownership with `std::unique_ptr`**

**Smart pointers:**
- Prefer `std::unique_ptr` over `juce::ScopedPointer` (newer code)
- JUCE codebase may still use `juce::ScopedPointer` (legacy)

---

## STRING PASSING

Choose based on use case:

```cpp
void foo (const String&);  // General purpose, read-only
void foo (String);         // Will copy anyway
void foo (String&&);       // Move semantics, modify/store
void foo (StringRef);      // Only need basic string operations
```

**Rules:**
- If function will store or move: `String` or `String&&`
- If only basic operations needed: `StringRef` (avoids String object creation)
- Otherwise: `const String&`

---

## CLASS DESIGN

**Use `struct` for data-only types** (saves `public:` line)

**Inheritance format:**
```cpp
class Thing : public Foo,
              private Bar
{
```

**Non-copyable classes:**
```cpp
Foo (const Foo&) = delete;
Foo& operator= (const Foo&) = delete;

// Or use macro
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyClass)
```

**Constructors:**
- Mark single-argument constructors `explicit` unless implicit conversion is intended
- Consider what implicit conversions you allow

---

## CONTROL FLOW

**No `else` after `return`:**
```cpp
// WRONG
if (foobar())
    return doSomething();
else
    doSomethingElse();

// CORRECT
if (foobar())
    return doSomething();
    
doSomethingElse();
```

**Narrow pointer scope:**
```cpp
// WRONG: wide scope increases null-pointer risk
auto* f = getFoo();
if (f != nullptr)
    f->doSomething();
// ...lots of code...
f->doSomething();  // Potential null dereference

// CORRECT: narrow scope
if (auto* f = getFoo())
    f->doSomething();
// f out of scope, impossible to misuse
```

---

## MISCELLANEOUS

**NULL:**
```cpp
nullptr  // CORRECT
NULL     // WRONG
0        // WRONG
0L       // VERY WRONG
```

**String literals:**
- Use plain C++ string literals: `"Hello"`
- For Unicode: use `CharPointer_UTF8` or Projucer tool

**Macros:**
- `#undef` after use when possible

**Increment operators:**
```cpp
for (int i = 0; i < 10; ++i)  // Pre-increment preferred
```

---

## PASS BY VALUE VS REFERENCE

**Pass by value for small types:**
- `Point`, `Time`, `RelativeTime`, `Colour`
- `Identifier`, `ModifierKeys`, `JustificationType`
- `Range`, `PixelRGB`, `PixelARGB`, `Rectangle`

**Pass by `const` reference for:**
- Large objects (`Array`, `String`, complex types)
- When copy constructor is expensive

**Reason:** Pass-by-value enables better compiler optimizations for small types.

---

## STANDARD LIBRARY

**Math functions:**
```cpp
std::abs, std::sqrt, std::sin, std::cos, std::pow  // CORRECT
fabs, sqrtf, powf                                  // WRONG
```

**Integer types:**
```cpp
int8, uint8, int16, uint16, int32, uint32, int64, uint64  // JUCE types
std::uint32_t, std::int64_t                               // Also acceptable
unsigned int                                              // Explicit
unsigned                                                  // AVOID alone
```

**Indexes:**
- JUCE uses `int` for array indices (not `size_t`)
- Aware of mismatch with STL conventions

---

## CONTAINERS AND ARRAY ACCESS

**ALWAYS use `.at()` for indexed access to containers:**

```cpp
// CORRECT: Bounds-checked access (Fail Fast principle)
std::array<int, 4> arr {1, 2, 3, 4};
auto value = arr.at (2);           // Throws std::out_of_range if invalid

std::vector<int> vec {1, 2, 3};
auto value = vec.at (1);           // Bounds-checked

// WRONG: Unchecked access (Undefined Behavior risk)
auto value = arr[2];               // UB if index invalid
auto value = vec[1];               // UB if index invalid
```

**Rationale:**
- ✅ **Fail Fast:** Invalid index throws immediately with stack trace
- ✅ **Semantic Correctness:** Signals "runtime index, validate it"
- ✅ **Zero UB Risk:** Prevents silent memory corruption
- ✅ **Debuggability:** Exception provides exact location of error
- ✅ **Performance:** Negligible cost (~1 branch, perfect prediction for small containers)

**Exception:** Only use `[]` when:
- Index is a compile-time constant AND you've verified it's in bounds
- You're in a proven hot path where profiling shows `.at()` is the bottleneck (rare!)
- In such cases, add a comment justifying the choice

**Range-based loops:**
Prefer range-for when you don't need the index:
```cpp
// BEST: No index needed, inherently safe
for (auto& element : container)
    element.process();

// ACCEPTABLE: Need index, use .at()
for (size_t i = 0; i < container.size(); ++i)
    container.at (i).process();

// AVOID: Unchecked access
for (size_t i = 0; i < container.size(); ++i)
    container[i].process();  // UB risk if size changes
```

---

## AUTO KEYWORD

**Use `auto` when type is obvious from RHS:**
```cpp
auto x = 0.0f;                    // OK: clearly float
auto x = 0.0;                     // OK: clearly double
auto x = thisReturnsABool();      // OK: function name indicates type
auto someResult = getResult();    // OK: expression

// AVOID when ambiguous:
auto x = 0;                       // Bad: could be int, unsigned, int64, etc.

// Explicit when helpful:
for (int i = 0; i < someNumber; ++i)  // Clear: signed int
bool someCondition = false;           // Clear: bool
```

---

## CRITICAL RULES (MANDATORY)

- **Aggregate (brace) initialization is ALWAYS preferred** over copy assignment: `int x { 0 };` not `int x = 0;`
- **No early return patterns.** Use `assert`, `static_assert` only when necessary. Use nested positive checks instead.
- **ALWAYS use nested positive checks:** `if (valid) { if (ready) { doWork(); } }` — NEVER `if (not valid) return;`
- **ALWAYS use `.at()` for container access** — NEVER `[]`. Fail Fast principle: invalid index throws immediately.
- **Use C++ alternative tokens:** `not`, `and`, `or` — NEVER `!`, `&&`, `||`

---

## SUMMARY CHECKLIST

✓ DRY: Never repeat code
✓ Braces on new line
✓ Space around operators
✓ `*` and `&` stick to type
✓ `const` before type
✓ Use `override`, never with `virtual`
✓ Add `noexcept` where possible
✓ Explicit null checks with `nullptr`
✓ Avoid raw `new`/`delete`
✓ Use smart pointers
✓ Pre-increment in loops
✓ No `else` after `return`
✓ Narrow pointer scope with if-init
✓ `enum class` over plain `enum`
✓ `explicit` single-arg constructors
✓ Pass small types by value
✓ Use `std::` math functions
✓ Aggregate (brace) initialization always
✓ No early returns — nested positive checks
✓ **ALWAYS use `.at()` for container access** (Fail Fast principle)
✓ **ALWAYS use `not`, `and`, `or`** alternative tokens

---

**Note:** These standards prioritize readability and safety. When in doubt, prefer explicitness over brevity.
