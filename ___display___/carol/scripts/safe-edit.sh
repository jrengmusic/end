#!/bin/bash
# safe-edit.sh - Safe text replacement with backup and validation
# Part of CAROL Framework v1.0.0

set -e

# Version
VERSION="1.0.0"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
DRY_RUN=false
USE_REGEX=false
BACKUP_SUFFIX=".bak"
VALIDATE_CMD=""

# Usage function
usage() {
    cat << EOF
safe-edit.sh - Safe text replacement with backup and validation

Usage:
  safe-edit.sh <file> <pattern> <replacement> [options]

Arguments:
  <file>           Path to file (absolute or relative)
  <pattern>        Text to find (literal string or regex if --regex flag used)
  <replacement>    Replacement text

Options:
  --dry-run        Preview changes without modifying file
  --regex          Treat pattern as regex (default: literal)
  --backup <suffix> Custom backup suffix (default: .bak)
  --validate <cmd> Run validation command after edit
  --help           Show this help message

Exit Codes:
  0  Success
  1  File not found
  2  Backup failed
  3  Pattern not found
  4  Validation failed

Examples:
  # Simple text replacement
  safe-edit.sh src/main.cpp "old_function" "new_function"

  # Dry-run first
  safe-edit.sh --dry-run src/main.cpp "float value" "double value"

  # With validation (compile check)
  safe-edit.sh src/main.cpp "old" "new" --validate "make build"

  # Regex replacement
  safe-edit.sh --regex src/config.hpp "version.*=.*" "version = 2.0"

CAROL Framework v$VERSION
EOF
    exit 0
}

# Error handling
error() {
    echo -e "${RED}Error: $1${NC}" >&2
    exit "$2"
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

info() {
    echo -e "${YELLOW}→ $1${NC}"
}

# Parse arguments
parse_args() {
    # Check for help flag first
    for arg in "$@"; do
        if [ "$arg" = "--help" ] || [ "$arg" = "-h" ]; then
            usage
        fi
    done

    # Parse positional arguments
    FILE=""
    PATTERN=""
    REPLACEMENT=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --regex)
                USE_REGEX=true
                shift
                ;;
            --backup)
                BACKUP_SUFFIX="$2"
                shift 2
                ;;
            --validate)
                VALIDATE_CMD="$2"
                shift 2
                ;;
            -*)
                error "Unknown flag: $1" 1
                ;;
            *)
                if [ -z "$FILE" ]; then
                    FILE="$1"
                elif [ -z "$PATTERN" ]; then
                    PATTERN="$1"
                elif [ -z "$REPLACEMENT" ]; then
                    REPLACEMENT="$1"
                else
                    error "Too many arguments" 1
                fi
                shift
                ;;
        esac
    done

    # Validate required arguments
    if [ -z "$FILE" ] || [ -z "$PATTERN" ] || [ -z "$REPLACEMENT" ]; then
        error "Missing required arguments. Use --help for usage." 1
    fi
}

# Check if file exists
check_file() {
    if [ ! -f "$FILE" ]; then
        error "File not found: $FILE" 1
    fi

    if [ ! -r "$FILE" ]; then
        error "File not readable: $FILE" 1
    fi

    if [ ! -w "$FILE" ] && [ "$DRY_RUN" = false ]; then
        error "File not writable: $FILE" 1
    fi
}

# Check if pattern exists in file
check_pattern() {
    if $USE_REGEX; then
        if ! grep -E -q "$PATTERN" "$FILE"; then
            error "Pattern not found in file: $PATTERN" 3
        fi
    else
        if ! grep -F -q "$PATTERN" "$FILE"; then
            error "Pattern not found in file: $PATTERN" 3
        fi
    fi
}

# Create backup
create_backup() {
    if [ "$DRY_RUN" = true ]; then
        return 0
    fi

    local backup_file="$FILE$BACKUP_SUFFIX"

    if ! cp "$FILE" "$backup_file"; then
        error "Failed to create backup: $backup_file" 2
    fi

    info "Backup created: $backup_file"
}

# Perform replacement
perform_replacement() {
    local temp_file=$(mktemp)

    # Get original file permissions
    local perms=$(stat -f "%A" "$FILE" 2>/dev/null || stat -c "%a" "$FILE" 2>/dev/null)

    if $USE_REGEX; then
        # Regex replacement
        sed -E "s/$PATTERN/$REPLACEMENT/g" "$FILE" > "$temp_file"
    else
        # Literal replacement (escape special chars for sed)
        # Escape pattern: all sed special chars
        local escaped_pattern=$(printf '%s\n' "$PATTERN" | sed 's/[]\/$*.^[]/\\&/g')
        # Escape replacement: backslash and ampersand
        local escaped_replacement=$(printf '%s\n' "$REPLACEMENT" | sed 's/[\/&]/\\&/g')

        sed "s/$escaped_pattern/$escaped_replacement/g" "$FILE" > "$temp_file"
    fi

    # Show diff in dry-run mode
    if $DRY_RUN; then
        info "Dry-run preview:"
        diff -u "$FILE" "$temp_file" || true
        rm "$temp_file"
        return 0
    fi

    # Atomic write: move temp file to original
    if ! mv "$temp_file" "$FILE"; then
        rm "$temp_file"
        error "Failed to write file" 2
    fi

    # Restore permissions
    chmod "$perms" "$FILE"

    success "File updated: $FILE"
}

# Run validation command
run_validation() {
    if [ -z "$VALIDATE_CMD" ]; then
        return 0
    fi

    info "Running validation: $VALIDATE_CMD"

    if ! eval "$VALIDATE_CMD"; then
        error "Validation failed" 4
    fi

    success "Validation passed"
}

# Main function
main() {
    parse_args "$@"
    check_file
    check_pattern

    if [ "$DRY_RUN" = false ]; then
        create_backup
    fi

    perform_replacement

    if [ "$DRY_RUN" = false ]; then
        run_validation
    fi

    exit 0
}

# Run main
main "$@"
