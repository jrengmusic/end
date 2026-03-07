#!/bin/bash
# rename-symbol.sh - Safe symbol renaming with word-boundary matching
# Part of CAROL Framework v1.0.0
# Note: Uses regex word boundaries. For full AST-based renaming, use language-specific tools.

set -e

VERSION="1.0.0"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Defaults
DRY_RUN=false
BACKUP=true
OLD_NAME=""
NEW_NAME=""
SCOPE=""
SYMBOL_TYPE=""

usage() {
    cat << EOF
rename-symbol.sh - Safe symbol renaming with word-boundary matching

Usage:
  rename-symbol.sh <old-name> <new-name> <scope> [options]

Scopes:
  file:<path>   Single file
  dir:<path>    Directory recursively
  project       Entire project (from cwd)

Options:
  --dry-run         Preview renames without modifying
  --no-backup       Skip creating backups
  --type <type>     Rename only specific type (function, class, variable)
  --help            Show this help

Examples:
  # Rename in single file
  rename-symbol.sh oldFunction newFunction file:src/handler.cpp

  # Rename in directory
  rename-symbol.sh UserModel UserEntity dir:src/models

  # Rename in entire project
  rename-symbol.sh processData handleData project

  # Dry-run preview
  rename-symbol.sh --dry-run oldVar newVar dir:src

Note: This tool uses word-boundary regex matching. For full AST-based
renaming, use language-specific tools (clang-rename, gopls, etc.)

CAROL Framework v$VERSION
EOF
    exit 0
}

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

parse_args() {
    for arg in "$@"; do
        [ "$arg" = "--help" ] && usage
    done

    while [ $# -gt 0 ]; do
        case "$1" in
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --no-backup)
                BACKUP=false
                shift
                ;;
            --type)
                SYMBOL_TYPE="$2"
                shift 2
                ;;
            -*)
                error "Unknown flag: $1" 1
                ;;
            *)
                if [ -z "$OLD_NAME" ]; then
                    OLD_NAME="$1"
                elif [ -z "$NEW_NAME" ]; then
                    NEW_NAME="$1"
                elif [ -z "$SCOPE" ]; then
                    SCOPE="$1"
                else
                    error "Too many arguments" 1
                fi
                shift
                ;;
        esac
    done

    [ -z "$OLD_NAME" ] || [ -z "$NEW_NAME" ] || [ -z "$SCOPE" ] && error "Missing required arguments" 1
    [ "$OLD_NAME" = "$NEW_NAME" ] && error "Old and new names are identical" 1
}

get_files() {
    local scope_type="${SCOPE%%:*}"
    local scope_path="${SCOPE#*:}"

    case "$scope_type" in
        file)
            if [ ! -f "$scope_path" ]; then
                error "File not found: $scope_path" 1
            fi
            echo "$scope_path"
            ;;
        dir)
            if [ ! -d "$scope_path" ]; then
                error "Directory not found: $scope_path" 1
            fi
            find "$scope_path" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.go" -o -name "*.py" -o -name "*.rs" -o -name "*.ts" -o -name "*.js" \)
            ;;
        project)
            find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.go" -o -name "*.py" -o -name "*.rs" -o -name "*.ts" -o -name "*.js" \)
            ;;
        *)
            error "Invalid scope format. Use file:<path>, dir:<path>, or project" 1
            ;;
    esac
}

rename_in_file() {
    local file="$1"
    local temp_file=$(mktemp)

    # Check if symbol exists in file
    if ! grep -wq "$OLD_NAME" "$file"; then
        rm -f "$temp_file"
        return 0
    fi

    # Perform rename with word boundaries
    sed "s/\b$OLD_NAME\b/$NEW_NAME/g" "$file" > "$temp_file"

    if $DRY_RUN; then
        local changes=$(diff -u "$file" "$temp_file" | grep "^[+-]" | grep -v "^[+-][+-][+-]" | wc -l)
        if [ "$changes" -gt 0 ]; then
            info "Would rename in: $file"
            diff -u "$file" "$temp_file" || true
        fi
        rm -f "$temp_file"
    else
        # Create backup
        if $BACKUP; then
            cp "$file" "$file.bak"
        fi

        # Get original permissions
        local perms=$(stat -f "%A" "$file" 2>/dev/null || stat -c "%a" "$file" 2>/dev/null)

        # Atomic write
        mv "$temp_file" "$file"
        chmod "$perms" "$file"

        success "Renamed in: $file"
    fi
}

main() {
    parse_args "$@"

    info "Searching for symbol: $OLD_NAME"
    info "Scope: $SCOPE"

    if $DRY_RUN; then
        info "DRY-RUN MODE (no changes will be made)"
    fi

    echo ""

    local files=$(get_files)
    local file_count=0
    local renamed_count=0

    while IFS= read -r file; do
        [ -z "$file" ] && continue
        ((file_count++))

        if grep -wq "$OLD_NAME" "$file"; then
            rename_in_file "$file"
            ((renamed_count++))
        fi
    done <<< "$files"

    echo ""
    info "Searched $file_count files"

    if $DRY_RUN; then
        info "Would rename in $renamed_count files"
    else
        success "Renamed in $renamed_count files"
    fi

    exit 0
}

main "$@"
