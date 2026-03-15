# SCRIPTS.md - Code Editing Automation Catalog

**Version:** 2.2.2
**Purpose:** Language-agnostic scripts and automation for safe code editing workflows
**Location:** `carol/scripts/` (symlinked from CAROL SSOT)

---

## Table of Contents

1. [Overview](#overview)
2. [Safe Edit Wrappers](#safe-edit-wrappers)
3. [Pattern Generators](#pattern-generators)
4. [Validation Tools](#validation-tools)
5. [AST Tools](#ast-tools)
6. [Usage Examples](#usage-examples)
7. [Safety Guarantees](#safety-guarantees)

---

## Overview

All scripts follow CAROL principles:
- **Lean:** One script, one purpose
- **Explicit:** Clear inputs/outputs
- **Fail-Fast:** Validation before execution
- **SSOT:** Reusable across projects
- **Testable:** Dry-run mode available

**General Usage Pattern:**
```bash
# Dry-run (preview changes, no modifications)
./carol/scripts/script-name.sh --dry-run [args]

# Execute (make actual changes)
./carol/scripts/script-name.sh [args]

# Help
./carol/scripts/script-name.sh --help
```

---

## Safe Edit Wrappers

### `safe-edit.sh` - Validated Text Replacement

**Purpose:** Replace text in files with automatic backup and validation

**Usage:**
```bash
./carol/scripts/safe-edit.sh <file> <pattern> <replacement> [options]
```

**Arguments:**
- `<file>`: Path to file (absolute or relative)
- `<pattern>`: Text to find (literal string or regex if --regex flag used)
- `<replacement>`: Replacement text
- `[options]`:
  - `--dry-run`: Preview changes without modifying file
  - `--regex`: Treat pattern as regex (default: literal)
  - `--backup <suffix>`: Custom backup suffix (default: `.bak`)
  - `--validate <command>`: Run validation command after edit

**Safety Features:**
- Creates backup before modification (`.bak` extension)
- Validates file exists and is writable
- Dry-run shows diff preview
- Optional syntax validation (e.g., compile check)
- Atomic operation (writes to temp, then moves)

**Examples:**
```bash
# Simple text replacement
./carol/scripts/safe-edit.sh src/main.cpp "old_function" "new_function"

# Dry-run first
./carol/scripts/safe-edit.sh --dry-run src/main.cpp "float value" "double value"

# With validation (compile check)
./carol/scripts/safe-edit.sh src/main.cpp "old" "new" --validate "make build"

# Regex replacement
./carol/scripts/safe-edit.sh --regex src/config.hpp "version.*=.*" "version = 2.0"
```

**Exit Codes:**
- `0`: Success
- `1`: File not found
- `2`: Backup failed
- `3`: Pattern not found
- `4`: Validation failed

---

### `safe-insert.sh` - Insert Code at Line Number

**Purpose:** Insert code block at specific line number with validation

**Usage:**
```bash
./carol/scripts/safe-insert.sh <file> <line> <code-file> [options]
```

**Arguments:**
- `<file>`: Target file path
- `<line>`: Line number to insert at (1-indexed)
- `<code-file>`: File containing code to insert (or use `-` for stdin)
- `[options]`:
  - `--dry-run`: Preview insertion
  - `--backup <suffix>`: Custom backup suffix
  - `--indent <spaces>`: Auto-indent inserted code
  - `--validate <command>`: Post-insertion validation

**Safety Features:**
- Validates line number in range
- Preserves existing indentation
- Creates backup
- Dry-run shows context (5 lines before/after)
- Atomic write

**Examples:**
```bash
# Insert code from file
./carol/scripts/safe-insert.sh src/handler.cpp 42 /tmp/error-check.cpp

# Insert with auto-indent (4 spaces)
./carol/scripts/safe-insert.sh src/handler.cpp 42 /tmp/code.cpp --indent 4

# Insert from stdin
echo "if (!ptr) return;" | ./carol/scripts/safe-insert.sh src/handler.cpp 42 -

# Dry-run preview
./carol/scripts/safe-insert.sh --dry-run src/handler.cpp 42 /tmp/code.cpp
```

**Exit Codes:**
- `0`: Success
- `1`: File/code-file not found
- `2`: Line number out of range
- `3`: Validation failed

---

## Pattern Generators

### `generate-error-handler.sh` - Multi-Language Error Handling

**Purpose:** Generate language-specific error handling boilerplate

**Usage:**
```bash
./carol/scripts/generate-error-handler.sh <language> <type> [options]
```

**Supported Languages:**
- `cpp` (C++)
- `go`
- `python`
- `rust`
- `typescript`

**Error Types:**
- `null-check`: Null/nil pointer validation
- `bounds-check`: Array/vector bounds validation
- `file-error`: File operation error handling
- `network-error`: Network operation error handling
- `parse-error`: Parsing/deserialization error handling

**Options:**
- `--function <name>`: Generate for specific function
- `--return-type <type>`: Specify return type
- `--var <name>`: Variable name to check
- `--output <file>`: Write to file (default: stdout)

**Examples:**

**C++ Null Check:**
```bash
./carol/scripts/generate-error-handler.sh cpp null-check --var user --function processUser
```
Output:
```cpp
if (!user) {
    throw std::invalid_argument("user cannot be null");
}
```

**Go Bounds Check:**
```bash
./carol/scripts/generate-error-handler.sh go bounds-check --var items --function processItems
```
Output:
```go
if index < 0 || index >= len(items) {
    return fmt.Errorf("index out of bounds: %d", index)
}
```

**Python File Error:**
```bash
./carol/scripts/generate-error-handler.sh python file-error --var path --function loadConfig
```
Output:
```python
try:
    with open(path, 'r') as f:
        config = json.load(f)
except FileNotFoundError:
    raise FileNotFoundError(f"Config file not found: {path}")
except json.JSONDecodeError as e:
    raise ValueError(f"Invalid JSON in {path}: {e}")
```

**Rust Network Error:**
```bash
./carol/scripts/generate-error-handler.sh rust network-error --function fetchData
```
Output:
```rust
match response {
    Ok(data) => data,
    Err(e) => return Err(format!("Network request failed: {}", e)),
}
```

---

### `generate-validation.sh` - Input Validation Code

**Purpose:** Generate input validation boilerplate

**Usage:**
```bash
./carol/scripts/generate-validation.sh <language> <validation-type> [options]
```

**Validation Types:**
- `range`: Numeric range check
- `string-empty`: Empty string check
- `email`: Email format validation
- `enum`: Enum/const value validation
- `custom`: Custom predicate

**Options:**
- `--var <name>`: Variable name
- `--min <value>`: Minimum value (range)
- `--max <value>`: Maximum value (range)
- `--values <list>`: Allowed values (enum, comma-separated)
- `--predicate <expr>`: Custom validation expression

**Examples:**

**C++ Range Validation:**
```bash
./carol/scripts/generate-validation.sh cpp range --var volume --min 0.0 --max 1.0
```
Output:
```cpp
if (volume < 0.0 || volume > 1.0) {
    throw std::out_of_range("volume must be between 0.0 and 1.0");
}
```

**Go String Empty Check:**
```bash
./carol/scripts/generate-validation.sh go string-empty --var username
```
Output:
```go
if username == "" {
    return errors.New("username cannot be empty")
}
```

**Python Enum Validation:**
```bash
./carol/scripts/generate-validation.sh python enum --var status --values "pending,active,done"
```
Output:
```python
allowed_values = {"pending", "active", "done"}
if status not in allowed_values:
    raise ValueError(f"Invalid status: {status}. Must be one of {allowed_values}")
```

---

## Validation Tools

### `validate-code.sh` - Pre-Commit Style and Safety Checks

**Purpose:** Run comprehensive validation before committing code

**Usage:**
```bash
./carol/scripts/validate-code.sh <file-or-directory> [options]
```

**Checks Performed:**
1. **Syntax Validation:**
   - C/C++: `clang-format --dry-run`, compile check
   - Go: `go fmt`, `go vet`
   - Python: `black --check`, `pylint`
   - Rust: `rustfmt --check`, `cargo check`
   - TypeScript: `prettier --check`, `tsc --noEmit`

2. **Safety Checks:**
   - No TODO/FIXME in committed code (warning)
   - No debug print statements (console.log, printf, etc.)
   - No commented-out code blocks
   - No trailing whitespace

3. **Architectural Checks:**
   - Layer separation (audio/UI, model/controller)
   - No raw pointers in DSP code (C++)
   - No allocations in hot paths (language-specific)

**Options:**
- `--strict`: Treat warnings as errors
- `--fix`: Auto-fix issues if possible
- `--exclude <pattern>`: Exclude files matching pattern
- `--checks <list>`: Run specific checks only (comma-separated)

**Examples:**
```bash
# Validate single file
./carol/scripts/validate-code.sh src/processor.cpp

# Validate directory
./carol/scripts/validate-code.sh src/

# Strict mode (fail on warnings)
./carol/scripts/validate-code.sh --strict src/

# Auto-fix formatting
./carol/scripts/validate-code.sh --fix src/

# Run specific checks
./carol/scripts/validate-code.sh --checks "syntax,safety" src/
```

**Exit Codes:**
- `0`: All checks passed
- `1`: Syntax errors found
- `2`: Safety violations found
- `3`: Architectural violations found

**Output Format:**
```
[✓] Syntax validation: PASS
[✗] Safety checks: FAIL
    - src/processor.cpp:42: Debug statement found: printf("debug")
    - src/handler.cpp:89: Commented code block detected
[✓] Architectural checks: PASS

Summary: 2 issues found
```

---

## AST Tools

### `rename-symbol.sh` - AST-Based Symbol Renaming

**Purpose:** Safely rename functions, classes, variables using AST parsing

**Usage:**
```bash
./carol/scripts/rename-symbol.sh <language> <old-name> <new-name> <scope> [options]
```

**Supported Languages:**
- `cpp` (uses clang AST)
- `go` (uses go/ast)
- `python` (uses ast module)
- `rust` (uses syn/proc-macro2)
- `typescript` (uses TypeScript compiler API)

**Scopes:**
- `file:<path>`: Single file
- `dir:<path>`: Directory recursively
- `project`: Entire project (from cwd)

**Options:**
- `--dry-run`: Preview renames
- `--backup`: Create backups (default: true)
- `--type <symbol-type>`: Rename only specific type (function, class, variable)

**Safety Features:**
- AST-aware (only renames symbols, not strings/comments)
- Scope-aware (respects variable shadowing)
- Cross-file renaming (updates all references)
- Preview mode shows all changes

**Examples:**

**Rename C++ Function:**
```bash
./carol/scripts/rename-symbol.sh cpp "processAudio" "processBuffer" "project"
```
Renames all occurrences of `processAudio` function in project.

**Rename Go Variable (Single File):**
```bash
./carol/scripts/rename-symbol.sh go "oldVar" "newVar" "file:handler.go"
```

**Rename Python Class (Directory):**
```bash
./carol/scripts/rename-symbol.sh python "OldClass" "NewClass" "dir:src/"
```

**Dry-Run Preview:**
```bash
./carol/scripts/rename-symbol.sh --dry-run cpp "getValue" "fetchValue" "project"
```
Output:
```
Preview: Renaming 'getValue' → 'fetchValue'
  src/processor.cpp:42: float getValue() { ... }
  src/processor.cpp:89: return getValue();
  src/handler.cpp:15: auto val = getValue();
  tests/test_processor.cpp:33: EXPECT_EQ(getValue(), 1.0);

Total: 4 occurrences in 3 files
```

**Exit Codes:**
- `0`: Success
- `1`: Symbol not found
- `2`: AST parsing failed
- `3`: Ambiguous symbol (multiple definitions)

---

## Usage Examples

### Common Workflows

#### Workflow 1: Safe Refactoring (MACHINIST)

```bash
# 1. Add null check to function
cat > /tmp/null-check.cpp << 'EOF'
if (!buffer) {
    throw std::invalid_argument("buffer cannot be null");
}
EOF

# 2. Insert at line 42 (dry-run first)
./carol/scripts/safe-insert.sh --dry-run src/processor.cpp 42 /tmp/null-check.cpp

# 3. Execute
./carol/scripts/safe-insert.sh src/processor.cpp 42 /tmp/null-check.cpp

# 4. Validate
./carol/scripts/validate-code.sh src/processor.cpp

# 5. Commit if validated
git add src/processor.cpp
git commit -m "Add null check to processor"
```

#### Workflow 2: Type Fix (SURGEON)

```bash
# 1. Replace float with double (dry-run)
./carol/scripts/safe-edit.sh --dry-run src/processor.cpp "float smoothing" "double smoothing"

# 2. Execute with validation
./carol/scripts/safe-edit.sh src/processor.cpp "float smoothing" "double smoothing" --validate "make build"

# 3. Check all files validated
./carol/scripts/validate-code.sh --strict src/
```

#### Workflow 3: Scaffolding (ENGINEER)

```bash
# 1. Generate error handler
./carol/scripts/generate-error-handler.sh cpp null-check --var user > /tmp/check.cpp

# 2. Insert into function
./carol/scripts/safe-insert.sh src/handler.cpp 15 /tmp/check.cpp --indent 4

# 3. Validate result
./carol/scripts/validate-code.sh src/handler.cpp
```

#### Workflow 4: Pre-Commit Audit (AUDITOR)

```bash
# 1. Validate all modified files
git diff --name-only | while read file; do
    ./carol/scripts/validate-code.sh --strict "$file"
done

# 2. If validation passes, commit
if [ $? -eq 0 ]; then
    git commit -m "Your commit message"
else
    echo "Validation failed, fix issues before committing"
fi
```

---

## Safety Guarantees

### All Scripts Provide:

1. **Backup Creation:**
   - Original file saved with `.bak` extension
   - Timestamp-based backups available (`--backup-timestamp`)
   - Restore via: `mv file.bak file`

2. **Dry-Run Mode:**
   - Preview changes before execution
   - No modifications made
   - Shows diff/context

3. **Atomic Operations:**
   - Write to temporary file first
   - Validate/check temporary file
   - Atomic rename to target
   - On failure, original unchanged

4. **Validation:**
   - Syntax checking (compile/lint)
   - Optional user-defined validation
   - Rollback on validation failure

5. **Error Handling:**
   - Clear error messages
   - Exit codes for scripting
   - Partial failure handling (multi-file ops)

6. **Idempotency:**
   - Running twice produces same result
   - Safe to retry on failure

### Failure Recovery

**If script fails mid-operation:**
```bash
# Check for backup
ls *.bak

# Restore single backup
./carol/scripts/restore-backup.sh file.bak

# Restore all .bak files in directory
./carol/scripts/restore-backup.sh --all src/

# Or manual restore
mv file.bak file
```

**If validation fails:**
```bash
# Original file preserved
# Check error message for cause
# Fix issue manually
# Re-run script
```

---

## Integration with CAROL Roles

### Role-Specific Script Usage

**COUNSELOR:**
- No script usage (planning only)
- May reference scripts in kickoff documents

**ENGINEER:**
- `generate-error-handler.sh` for boilerplate
- `generate-validation.sh` for input checks
- `safe-insert.sh` for adding generated code

**MACHINIST:**
- `safe-edit.sh` for defensive edits
- `safe-insert.sh` for adding checks
- `generate-error-handler.sh` for safety

**AUDITOR:**
- `validate-code.sh` for pre-commit audit
- Must validate ALL scripts used by other roles

**SURGEON:**
- `safe-edit.sh` for surgical fixes
- `rename-symbol.sh` for refactoring (if needed)
- Minimal script usage (prefer direct edits)



## Contributing Scripts

### Guidelines for New Scripts

1. **One purpose:** Script does ONE thing well
2. **Explicit interface:** Clear inputs/outputs documented
3. **Fail-fast:** Validate inputs before execution
4. **Dry-run:** Always provide `--dry-run` option
5. **Backup:** Create backups before modifications
6. **Atomic:** Use temp files, atomic renames
7. **Tested:** TDD approach, comprehensive tests
8. **Documented:** Examples, exit codes, safety guarantees

### Script Template

```bash
#!/bin/bash
# script-name.sh - Brief description
# Usage: script-name.sh <required> [optional]

set -euo pipefail  # Fail-fast

VERSION="1.0.0"
DRY_RUN=false
BACKUP_SUFFIX=".bak"

usage() {
    cat << EOF
Usage: $0 <file> [options]

Options:
  --dry-run         Preview changes
  --backup <suffix> Backup suffix (default: .bak)
  --help            Show this help

Exit codes:
  0  Success
  1  Error
EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run) DRY_RUN=true; shift ;;
        --help) usage; exit 0 ;;
        *) break ;;
    esac
done

# Validation
[[ $# -lt 1 ]] && { usage; exit 1; }

# Main logic
# ...

# Exit with status
exit 0
```

---

## Cross-References

- **PATTERNS.md:** Tool Selection Decision Tree, Self-Validation Checklist
- **CAROL.md:** Role constraints, behavioral rules
- **ARCHITECTURE-WRITER.md:** Architecture documentation guide
- **PATTERNS-WRITER.md:** Pattern discovery guide (coming soon)

---

## Version History

- **1.0.0** (2026-01-16): Initial release
  - Safe edit wrappers (safe-edit, safe-insert)
  - Pattern generators (error handlers, validation)
  - Validation tools (validate-code)
  - AST tools (rename-symbol)
  - Usage examples and safety guarantees

---

**CAROL Framework**
Created by JRENG
https://github.com/jrengmusic/carol
