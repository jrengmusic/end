#!/bin/bash
# restore-backup.sh - Restore files from .bak backups
# Part of CAROL Framework v1.0.0

set -e

VERSION="1.0.0"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

DRY_RUN=false
FORCE=false

usage() {
    cat << EOF
restore-backup.sh - Restore files from .bak backups

Usage:
  restore-backup.sh <backup-file> [options]
  restore-backup.sh --all [directory] [options]

Arguments:
  <backup-file>       Path to .bak file to restore
  <directory>         Directory containing .bak files (use with --all)

Options:
  --dry-run           Preview restoration without modifying files
  --force             Overwrite existing files without confirmation
  --help              Show this help message

Examples:
  # Restore single backup
  restore-backup.sh src/file.cpp.bak

  # Preview restoration
  restore-backup.sh --dry-run src/file.cpp.bak

  # Restore all .bak files in directory
  restore-backup.sh --all src/

Exit Codes:
  0  Success
  1  Backup file not found
  2  Invalid backup file format
  3  Restore failed (file exists, use --force)
  4  Validation failed

CAROL Framework v$VERSION
EOF
}

restore_single_backup() {
    local backup_file="$1"
    local original_file

    if [[ ! -f "$backup_file" ]]; then
        echo -e "${RED}Error: Backup file not found: $backup_file${NC}"
        return 1
    fi

    if [[ ! "$backup_file" =~ \.bak$ ]]; then
        echo -e "${RED}Error: Not a .bak file: $backup_file${NC}"
        return 2
    fi

    original_file="${backup_file%.bak}"

    if [[ -f "$original_file" && "$FORCE" == "false" ]]; then
        echo -e "${RED}Error: File already exists: $original_file${NC}"
        echo "Use --force to overwrite or remove the file first."
        return 3
    fi

    if [[ "$DRY_RUN" == "true" ]]; then
        echo -e "${YELLOW}[DRY-RUN]${NC} Would restore: $backup_file -> $original_file"
        return 0
    fi

    if mv "$backup_file" "$original_file"; then
        echo -e "${GREEN}✓${NC} Restored: $backup_file -> $original_file"
        return 0
    else
        echo -e "${RED}Error: Failed to restore $backup_file${NC}"
        return 4
    fi
}

restore_all_backups() {
    local directory="${1:-.}"
    local count=0
    local errors=0

    if [[ ! -d "$directory" ]]; then
        echo -e "${RED}Error: Directory not found: $directory${NC}"
        return 1
    fi

    while IFS= read -r backup_file; do
        if restore_single_backup "$backup_file"; then
            ((count++))
        else
            ((errors++))
        fi
    done < <(find "$directory" -maxdepth 1 -name "*.bak" -type f 2>/dev/null)

    echo ""
    echo "Restored: $count files"
    if [[ $errors -gt 0 ]]; then
        echo -e "${RED}Errors: $errors files${NC}"
    fi
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --force)
            FORCE=true
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        --all)
            shift
            RESTORE_ALL=true
            DIRECTORY="${1:-.}"
            shift
            ;;
        -*)
            echo -e "${RED}Error: Unknown option: $1${NC}"
            usage
            exit 1
            ;;
        *)
            BACKUP_FILES+=("$1")
            shift
            ;;
    esac
done

if [[ "${RESTORE_ALL:-false}" == "true" ]]; then
    restore_all_backups "$DIRECTORY"
elif [[ ${#BACKUP_FILES[@]} -gt 0 ]]; then
    for backup in "${BACKUP_FILES[@]}"; do
        restore_single_backup "$backup" || true
    done
else
    echo -e "${RED}Error: No backup files specified${NC}"
    usage
    exit 1
fi

exit 0
