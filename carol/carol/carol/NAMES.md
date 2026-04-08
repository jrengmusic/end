# Philosophical Framework for Naming in Code

## Preamble

These rules set boundaries to reduce cognitive load, improve readability, lower debugging complexity, and prevent unnecessary technical debt.

They form a holistic approach to naming—meant to guide consistent reasoning, not to cover every edge case or be applied selectively for convenience.

They are tools, not laws: reliable in most situations, but not universally applicable. When a rare case genuinely requires breaking a rule, it should be a deliberate exception, not an accident.

---

## Rule 0 — Use English

**Principle:**  
All names in code must be written in English.

**Rationale:**  
Using a single language removes unnecessary context-switching. Almost all programming tools, libraries, and documentation are written in English, so using English names keeps the codebase uniform and easier to read. It also ensures that everyone reading the code interprets the same meaning without translation effort.

---

## Rule 1 — Word classes must match their role

**Principle:**  
Use nouns for things, verbs for actions. The grammatical form of the name must reflect what the construct does in the program.

**Rationale:**  
Developers write code, and every line of code becomes a potential liability: it may break, it may hide bugs, and someone will eventually need to understand it. The only protection we have against that future uncertainty is readability. If you cannot read a piece of code clearly, you will not understand it. If you cannot understand it, you will not be able to fix it.

Using nouns for classes, structs, and variables—and verbs for functions—is aligned with the way we naturally think and communicate. It makes code read more like a structured sentence rather than a puzzle. This reduces mental effort and makes the intent of each construct immediately obvious.

**Examples:**

Classes and variables use nouns:
```cpp
class Manager;
class ProcessorChain;
struct Descriptor;
int panelHeight;
juce::File presetDirectory;
```

Functions use verbs:
```cpp
void buildInterface();
void applyImages();
bool isValidParameterID();
juce::String getProductName();
```

Booleans prefix verbs before nouns or use adjectives:
```cpp
bool isUsingWhitespace;
bool isPresetDirty;
bool shouldPromptLicense;
bool isEvaluating;
```

**Singular vs Plural:**
```cpp
struct Descriptor;
// Singular. An object structure that contains information describing data,
// used at compile-time or runtime.

using Descriptors = std::unordered_map<juce::String, Descriptor>;
// Plural. A container that holds multiple Descriptor objects indexed by ID.
```

---

## Rule 2 — Construct expression without data type statement

**Principle:**  
A name must express its role without explicitly using the data type itself.

**Rationale:**  
Type information is already enforced by the compiler and visible in modern IDEs. Encoding types into names adds noise without adding meaning. The name should describe *what* the construct represents in the domain, not *how* it's stored in memory. This keeps names focused on intent and prevents names from becoming outdated when types change during refactoring.

**Examples:**

**Bad:**
```cpp
int filesInt;
// filesInt tells the reader the type, not the meaning.

class FileMgrClass;
// FileMgrClass redundantly names the construct instead of expressing its purpose.

juce::StringArray choicesArray;
// "Array" suffix duplicates type information already visible in IDE.

std::unique_ptr<juce::XmlElement> xmlPtr;
// "Ptr" suffix is redundant—the type already indicates it's a pointer.
```

**Good:**
```cpp
int panelHeight;
// panelHeight clearly communicates it represents a vertical measurement—
// readers naturally expect this to be an integer without being told.

class Manager;
// Manager conveys the idea of a class responsible for coordinating operations,
// without needing to append "Class" or encode its type.

juce::StringArray choices;
// "choices" describes the semantic content, not the container type.

std::unique_ptr<juce::XmlElement> layout;
// "layout" describes what the data represents in the domain.
```

---

## Rule 3 — Semantic over literal

**Principle:**  
Semantic naming involves choosing names that clearly and accurately convey the purpose and meaning of the data they hold, rather than just their type or how they were obtained.

**Rationale:**  
Code is an expression of ideas. A name should clearly communicate its purpose, intention, or role within its specific context and scope. Semantic names prevent bugs by revealing intent—literal names hide meaning and force readers to reverse-engineer what code does. When you name things by what they *mean* rather than what they *are*, you make the invisible visible.

**Examples:**

**Bad:**
```cpp
juce::File getUserSettings(juce::XmlElement* defaultSettings = nullptr);
// "getUserSettings" suggests retrieval only, but the function actually 
// creates the file if missing and writes default values.

const juce::File getFactoryDefaultPresetsDirectory(const juce::String& versionString);
// Function creates directories if they don't exist—"get" implies pure retrieval.

std::unique_ptr<juce::XmlElement> xml;
// "xml" describes the format/container, not what the data represents.
```

**Good:**
```cpp
juce::File getOrCreateUserSettings(juce::XmlElement* defaultSettings = nullptr);
// Name reveals the function creates the file when needed, not just retrieves it.

const juce::File getOrCreateFactoryPresetsDirectory(const juce::String& versionString);
// "getOrCreate" accurately describes the side effect of directory creation.

std::unique_ptr<juce::XmlElement> layout;
// "layout" describes the semantic meaning—a UI layout structure.

std::unique_ptr<juce::XmlElement> presetData;
// "presetData" tells you what the content represents, not just that it's XML.
```

**Key practices:**

- **Describe the content, not the container:** A variable named `layout` is more semantic than `xmlElement` or `xmlData`, as it tells you what the XML represents.

- **Be specific and precise:** Instead of `getUserSettings()`, use `getOrCreateUserSettings()` when the function has side effects. This prevents bugs where callers assume the function is read-only.

- **Boolean variables:** Name boolean variables to reflect the condition they represent, such as `isPresetDirty`, `shouldPromptLicense`, or `isEvaluating`.

---

## Rule 4 — Clarity over brevity

**Principle:**  
Longer descriptive names are unambiguous and self-documented, potentially reducing unnecessary comments.

**Rationale:**  
Compilers don't care how long or short your variables and functions are named. If everything is legal and sane, it will work. But when it doesn't, humans need to read, review, and debug. Short names might be easier to type, but adding extra comments to explain them just adds noise and pollutes the codebase.

**Examples:**

**Bad:**
```cpp
// Unclear scope and purpose
void build(juce::Component* view, 
           Descriptors& descriptors,
           Registry& registry);

// Vague about what's being set
void setValue(const juce::String& newValue);

// Hidden side effect—function also validates and may throw
juce::File getSettings();
```

**Good:**
```cpp
// Explicit about the complete operation
void buildAndAttachComponents(juce::Component* parentView,
                              Descriptors& componentDescriptors,
                              Registry& componentRegistry,
                              Model& dataModel);

// Clear about which scale value
void setUIscale(const juce::String& newScaleValue);

// Reveals the side effect in the name
juce::File getOrCreateUserSettings(juce::XmlElement* defaultSettings = nullptr);

// Parameters clarify their domain context
const juce::File getFactoryDefaultPresetsDirectory(const juce::String& versionString) const noexcept;
const juce::File getUserPresetsDirectory() const noexcept;
```

**Real consequence:**
```cpp
// BAD: Hides critical behavior
void loadPreset(const juce::File& file);
// Reader assumes this is safe to call anytime.
// Actually silently fails if preset is dirty!

// GOOD: Name reveals the precondition
void loadPresetIfNotDirty(const juce::File& file);
// Or better, be explicit in implementation:
if (isPresetDirty()) return; // Now readers see the guard
```

---

## When to Bend the Rules

These rules are tools, not laws. There are legitimate cases where bending them produces clearer code:

**Domain-specific context:**
```cpp
class Model : public juce::AudioProcessorValueTreeState
{
    // "Model" bends Rule 2 (it's technically an APVTS wrapper)
    // BUT: Within an MVC architecture, "Model" is semantically correct
    // for its role, even though the type is 40+ characters long.
    // Context matters—inside your namespace and architecture,
    // this name is more meaningful than "AudioProcessorValueTreeState".
};
```

**Established conventions:**
```cpp
// Loop counters in small scopes
for (int i = 0; i < items.size(); ++i)
{
    process(items[i]);
}

// Standard abbreviations when universally understood
juce::XmlElement* xml;  // Not "xmlDocument"—context is clear
juce::String id;        // Not "identifier"—too verbose for ubiquitous use
const juce::String& url; // Domain abbreviation, widely understood
```

**Scope-dependent brevity:**
```cpp
// Short scope: brevity acceptable
auto isValid = [&](const auto& x) { return x.isNotEmpty(); };

// Class member: clarity required
class Manager
{
    bool isUsingWhitespace;  // Not "flag" or "b"
};
```

**When bending a rule, ask:**
- Does this name's meaning remain clear within its scope?
- Would a longer name add noise without adding clarity?
- Is this convention universally understood by the team?

If you can't answer "yes" to all three, follow the rules.

---

## Rule 5 — Consistency

**Principle:**  
Applying the above rules holistically across the codebase will guarantee identical self-documented patterns linguistically and semantically. When the same concept appears in different parts of the code, it should use the same naming convention. If you use `get` for retrieval in one place, don't switch to `fetch` or `retrieve` elsewhere. If collections are plural, keep them plural throughout.

**Rationale:**  
A consistent codebase is easier to read, understand, and maintain. Consistency creates predictability—developers can form reliable mental models about how the code works. When patterns repeat uniformly, new team members onboard faster, bugs become easier to spot, and refactoring becomes safer. Inconsistency forces readers to question whether differences in naming indicate differences in behavior, adding unnecessary cognitive load. Consistency transforms a collection of files into a cohesive system.

**Examples:**

**Inconsistent patterns:**
```cpp
// Mixing verb forms for similar operations
juce::String getProductName() const noexcept;
juce::String fetchVersionString() const noexcept;
juce::String retrieveProductWebsite() const noexcept;

// Inconsistent parameter naming
void applyImages(juce::Component* view, Descriptors& descriptors);
void applyRules(juce::Component* comp, Descriptors& desc);
void attachTo(juce::Component* parent, Descriptors& componentDescriptors);

// Mixed naming for boolean checks
bool isPresetDirty() const noexcept;
bool shouldPromptLicense() const noexcept;
bool checkIfEvaluating() const noexcept;  // Inconsistent with is/should pattern
```

**Consistent patterns:**
```cpp
// Uniform verb form for getters
juce::String getProductName() const noexcept;
juce::String getVersionString() const noexcept;
juce::String getProductWebsite() const noexcept;

// Consistent parameter naming throughout Manager
void applyImages(juce::Component* view, Descriptors& descriptors);
void applyRules(juce::Component* view, Descriptors& descriptors);
void attachTo(juce::Component* view, Descriptors& descriptors);

// Consistent boolean naming pattern
bool isPresetDirty() const noexcept;
bool isEvaluating() const noexcept;
bool shouldPromptLicense() const noexcept;
```