#!/bin/bash
# safe-insert.sh - Insert code at line number with validation
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
BACKUP_SUFFIX=".bak"
INDENT_SPACES=0
VALIDATE_CMD=""

# Usage function
usage() {
    cat << EOF
safe-insert.sh - Insert code at line number with validation

Usage:
  safe-insert.sh <file> <line> <code-file> [options]

Arguments:
  <file>        Target file path
  <line>        Line number to insert at (1-indexed)
  <code-file>   File containing code to insert (or use '-' for stdin)

Options:
  --dry-run         Preview insertion without modifying file
  --backup <suffix> Custom backup suffix (default: .bak)
  --indent <spaces> Auto-indent inserted code
  --validate <cmd>  Run validation command after insertion
  --help            Show this help message

Exit Codes:
  0  Success
  1  File/code-file not found
  2  Line number out of range
  3  Validation failed

Examples:
  # Insert code from file
  safe-insert.sh src/handler.cpp 42 /tmp/error-check.cpp

  # Insert with auto-indent (4 spaces)
  safe-insert.sh src/handler.cpp 42 /tmp/code.cpp --indent 4

  # Insert from stdin
  echo "if (!ptr) return;" | safe-insert.sh src/handler.cpp 42 -

  # Dry-run preview
  safe-insert.sh --dry-run src/handler.cpp 42 /tmp/code.cpp

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
    LINE=""
    CODE_FILE=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --backup)
                BACKUP_SUFFIX="$2"
                shift 2
                ;;
            --indent)
                INDENT_SPACES="$2"
                shift 2
                ;;
            --validate)
                VALIDATE_CMD="$2"
                shift 2
                ;;
            -)
                # Stdin indicator, treat as positional argument
                if [ -z "$FILE" ]; then
                    FILE="$1"
                elif [ -z "$LINE" ]; then
                    LINE="$1"
                elif [ -z "$CODE_FILE" ]; then
                    CODE_FILE="$1"
                else
                    error "Too many arguments" 1
                fi
                shift
                ;;
            -*)
                error "Unknown flag: $1" 1
                ;;
            *)
                if [ -z "$FILE" ]; then
                    FILE="$1"
                elif [ -z "$LINE" ]; then
                    LINE="$1"
                elif [ -z "$CODE_FILE" ]; then
                    CODE_FILE="$1"
                else
                    error "Too many arguments" 1
                fi
                shift
                ;;
        esac
    done

    # Validate required arguments
    if [ -z "$FILE" ] || [ -z "$LINE" ] || [ -z "$CODE_FILE" ]; then
        error "Missing required arguments. Use --help for usage." 1
    fi
}

# Check if file exists and is writable
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

# Validate line number
validate_line_number() {
    # Check if line is a valid integer
    if ! [[ "$LINE" =~ ^[0-9]+$ ]]; then
        error "Line number must be a positive integer: $LINE" 2
    fi

    # Line numbers are 1-indexed, 0 is invalid
    if [ "$LINE" -eq 0 ]; then
        error "Line number must be >= 1: $LINE" 2
    fi

    # Check if line is within range (allow inserting at end+1)
    local file_lines=$(wc -l < "$FILE" | tr -d ' ')
    local max_line=$((file_lines + 1))

    if [ "$LINE" -gt "$max_line" ]; then
        error "Line number out of range (max: $max_line): $LINE" 2
    fi
}

# Read code to insert
read_code_to_insert() {
    local temp_code=$(mktemp)

    if [ "$CODE_FILE" = "-" ]; then
        # Read from stdin
        cat > "$temp_code"
    else
        # Read from file
        if [ ! -f "$CODE_FILE" ]; then
            rm -f "$temp_code"
            error "Code file not found: $CODE_FILE" 1
        fi
        cat "$CODE_FILE" > "$temp_code"
    fi

    # Apply indentation if requested
    if [ "$INDENT_SPACES" -gt 0 ]; then
        local indent=$(printf '%*s' "$INDENT_SPACES" '')
        sed "s/^/$indent/" "$temp_code" > "$temp_code.indented"
        mv "$temp_code.indented" "$temp_code"
    fi

    echo "$temp_code"
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

# Perform insertion
perform_insertion() {
    local code_temp=$(read_code_to_insert)
    local output_temp=$(mktemp)

    # Get original file permissions
    local perms=$(stat -f "%A" "$FILE" 2>/dev/null || stat -c "%a" "$FILE" 2>/dev/null)

    # Insert code at specified line
    # Lines before insertion point
    if [ "$LINE" -gt 1 ]; then
        head -n $((LINE - 1)) "$FILE" > "$output_temp"
    else
        # Inserting at line 1, no lines before
        : > "$output_temp"
    fi

    # Insert code
    cat "$code_temp" >> "$output_temp"

    # Lines after insertion point
    tail -n +$LINE "$FILE" >> "$output_temp"

    # Clean up code temp file
    rm -f "$code_temp"

    # Show diff in dry-run mode
    if $DRY_RUN; then
        info "Dry-run preview (inserting at line $LINE):"
        info "Context (5 lines before/after):"
        echo ""

        # Show context
        local start_line=$((LINE - 5))
        if [ "$start_line" -lt 1 ]; then
            start_line=1
        fi
        local end_line=$((LINE + 5))

        # Show lines with line numbers
        sed -n "${start_line},${end_line}p" "$FILE" | cat -n

        echo ""
        info "Code to be inserted:"
        cat "$(read_code_to_insert)"
        rm -f "$(read_code_to_insert)"

        rm "$output_temp"
        return 0
    fi

    # Atomic write: move temp file to original
    if ! mv "$output_temp" "$FILE"; then
        rm "$output_temp"
        error "Failed to write file" 2
    fi

    # Restore permissions
    chmod "$perms" "$FILE"

    success "Code inserted at line $LINE in $FILE"
}

# Run validation command
run_validation() {
    if [ -z "$VALIDATE_CMD" ]; then
        return 0
    fi

    info "Running validation: $VALIDATE_CMD"

    if ! eval "$VALIDATE_CMD"; then
        error "Validation failed" 3
    fi

    success "Validation passed"
}

# Main function
main() {
    parse_args "$@"
    check_file
    validate_line_number

    if [ "$DRY_RUN" = false ]; then
        create_backup
    fi

    perform_insertion

    if [ "$DRY_RUN" = false ]; then
        run_validation
    fi

    exit 0
}

# Run main
main "$@"
