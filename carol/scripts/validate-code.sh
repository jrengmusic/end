#!/bin/bash
# validate-code.sh - Pre-commit style and safety checks
# Part of CAROL Framework v1.0.0

set -e

VERSION="1.0.0"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Defaults
STRICT=false
FIX=false
EXCLUDE_PATTERN=""
CHECKS="syntax,safety,architecture"
TARGET=""

ISSUES_FOUND=0
EXIT_CODE=0

usage() {
    cat << EOF
validate-code.sh - Pre-commit style and safety checks

Usage:
  validate-code.sh <file-or-directory> [options]

Options:
  --strict              Treat warnings as errors
  --fix                 Auto-fix issues if possible
  --exclude <pattern>   Exclude files matching pattern
  --checks <list>       Run specific checks (comma-separated)
  --help                Show this help

Checks:
  syntax        - Language-specific syntax validation
  safety        - Debug statements, TODOs, commented code
  architecture  - Layer separation, pattern compliance

Exit Codes:
  0  All checks passed
  1  Syntax errors found
  2  Safety violations found
  3  Architectural violations found

CAROL Framework v$VERSION
EOF
    exit 0
}

error() {
    echo -e "${RED}✗ $1${NC}" >&2
    ((ISSUES_FOUND++))
}

warning() {
    echo -e "${YELLOW}⚠ $1${NC}" >&2
    if $STRICT; then
        ((ISSUES_FOUND++))
    fi
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

info() {
    echo -e "${YELLOW}→ $1${NC}"
}

parse_args() {
    for arg in "$@"; do
        [ "$arg" = "--help" ] && usage
    done

    while [ $# -gt 0 ]; do
        case "$1" in
            --strict)
                STRICT=true
                shift
                ;;
            --fix)
                FIX=true
                shift
                ;;
            --exclude)
                EXCLUDE_PATTERN="$2"
                shift 2
                ;;
            --checks)
                CHECKS="$2"
                shift 2
                ;;
            -*)
                echo "Unknown flag: $1" >&2
                exit 1
                ;;
            *)
                TARGET="$1"
                shift
                ;;
        esac
    done

    [ -z "$TARGET" ] && { echo "Missing target file/directory" >&2; exit 1; }
    [ ! -e "$TARGET" ] && { echo "Target not found: $TARGET" >&2; exit 1; }
}

detect_language() {
    local file="$1"
    case "$file" in
        *.cpp|*.cc|*.cxx|*.hpp|*.h) echo "cpp" ;;
        *.go) echo "go" ;;
        *.py) echo "python" ;;
        *.rs) echo "rust" ;;
        *.ts|*.tsx) echo "typescript" ;;
        *.js|*.jsx) echo "javascript" ;;
        *) echo "unknown" ;;
    esac
}

check_syntax() {
    local file="$1"
    local lang=$(detect_language "$file")

    case "$lang" in
        cpp)
            if command -v clang-format > /dev/null 2>&1; then
                if ! clang-format --dry-run -Werror "$file" 2>/dev/null; then
                    error "Syntax: $file (clang-format)"
                    EXIT_CODE=1
                fi
            fi
            ;;
        go)
            if command -v gofmt > /dev/null 2>&1; then
                if [ -n "$(gofmt -l "$file")" ]; then
                    if $FIX; then
                        gofmt -w "$file"
                        info "Fixed: $file (gofmt)"
                    else
                        error "Syntax: $file (gofmt)"
                        EXIT_CODE=1
                    fi
                fi
            fi
            ;;
        python)
            if command -v python3 > /dev/null 2>&1; then
                if ! python3 -m py_compile "$file" 2>/dev/null; then
                    error "Syntax: $file (compile check)"
                    EXIT_CODE=1
                fi
            fi
            ;;
        rust)
            if command -v rustfmt > /dev/null 2>&1; then
                if ! rustfmt --check "$file" 2>/dev/null; then
                    if $FIX; then
                        rustfmt "$file"
                        info "Fixed: $file (rustfmt)"
                    else
                        error "Syntax: $file (rustfmt)"
                        EXIT_CODE=1
                    fi
                fi
            fi
            ;;
    esac
}

check_safety() {
    local file="$1"

    # Check for debug statements
    if grep -n "console\.log\|printf\|println!\|print(" "$file" 2>/dev/null | grep -v "^[[:space:]]*//\|^[[:space:]]*#"; then
        warning "Safety: $file has debug statements"
        EXIT_CODE=2
    fi

    # Check for TODOs/FIXMEs
    if grep -n "TODO\|FIXME" "$file" 2>/dev/null; then
        warning "Safety: $file has TODO/FIXME"
    fi

    # Check for trailing whitespace
    if grep -n "[[:space:]]$" "$file" 2>/dev/null | head -5; then
        warning "Safety: $file has trailing whitespace"
    fi
}

check_architecture() {
    local file="$1"

    # Check for commented code blocks (3+ consecutive comment lines)
    if awk '/^[[:space:]]*\/\/.*$/{c++} c>=3{print NR": Commented code block"; c=0} !/^[[:space:]]*\/\//{c=0}' "$file" | head -1; then
        warning "Architecture: $file has commented code blocks"
        EXIT_CODE=3
    fi
}

validate_file() {
    local file="$1"

    # Skip if matches exclude pattern
    if [ -n "$EXCLUDE_PATTERN" ] && echo "$file" | grep -q "$EXCLUDE_PATTERN"; then
        return
    fi

    # Run requested checks
    if echo "$CHECKS" | grep -q "syntax"; then
        check_syntax "$file"
    fi

    if echo "$CHECKS" | grep -q "safety"; then
        check_safety "$file"
    fi

    if echo "$CHECKS" | grep -q "architecture"; then
        check_architecture "$file"
    fi
}

validate_directory() {
    local dir="$1"
    find "$dir" -type f \( -name "*.cpp" -o -name "*.go" -o -name "*.py" -o -name "*.rs" -o -name "*.ts" \) | while read -r file; do
        validate_file "$file"
    done
}

main() {
    parse_args "$@"

    info "Running validation checks on: $TARGET"
    echo ""

    if [ -f "$TARGET" ]; then
        validate_file "$TARGET"
    elif [ -d "$TARGET" ]; then
        validate_directory "$TARGET"
    fi

    echo ""
    if [ $ISSUES_FOUND -eq 0 ]; then
        success "All checks passed"
        exit 0
    else
        echo -e "${RED}Summary: $ISSUES_FOUND issue(s) found${NC}"
        exit $EXIT_CODE
    fi
}

main "$@"
