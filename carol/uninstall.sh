#!/bin/bash
# uninstall.sh - CAROL Uninstaller
#
# Usage:
#   Direct:  ./uninstall.sh
#   Curl:    curl -fsSL https://raw.githubusercontent.com/jrengmusic/carol/main/uninstall.sh | bash

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Default installation location
CAROL_INSTALL_DIR="${CAROL_INSTALL_DIR:-$HOME/.carol}"

error() {
    echo -e "${RED}Error: $1${NC}" >&2
    exit 1
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

info() {
    echo -e "${YELLOW}→ $1${NC}"
}

notice() {
    echo -e "${BLUE}ℹ $1${NC}"
}

warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Remove CAROL directory
remove_carol_dir() {
    if [ ! -d "$CAROL_INSTALL_DIR" ]; then
        info "CAROL not found at $CAROL_INSTALL_DIR"
        return 0
    fi

    info "Found CAROL at: $CAROL_INSTALL_DIR"
    read -p "Remove this directory? (y/N) " -r response

    if [[ "$response" =~ ^[Yy]$ ]]; then
        rm -rf "$CAROL_INSTALL_DIR"
        success "Removed $CAROL_INSTALL_DIR"
    else
        info "Skipped removal of $CAROL_INSTALL_DIR"
    fi
}

# Remove from shell RC files
remove_from_path() {
    local files_updated=0

    # Check bash files
    for rc_file in "$HOME/.bashrc" "$HOME/.bash_profile"; do
        if [ -f "$rc_file" ] && grep -q "CAROL Framework" "$rc_file"; then
            info "Removing CAROL from $rc_file"

            # Create backup
            cp "$rc_file" "$rc_file.bak"

            # Remove CAROL section (from marker to export line)
            if [[ "$OSTYPE" == "darwin"* ]]; then
                sed -i '' '/# CAROL Framework/,/export PATH.*carol/d' "$rc_file"
            else
                sed -i '/# CAROL Framework/,/export PATH.*carol/d' "$rc_file"
            fi

            success "Cleaned $rc_file (backup: $rc_file.bak)"
            ((files_updated++))
        fi
    done

    # Check zsh files
    for rc_file in "$HOME/.zshrc" "$HOME/.zprofile"; do
        if [ -f "$rc_file" ] && grep -q "CAROL Framework" "$rc_file"; then
            info "Removing CAROL from $rc_file"

            # Create backup
            cp "$rc_file" "$rc_file.bak"

            # Remove CAROL section
            if [[ "$OSTYPE" == "darwin"* ]]; then
                sed -i '' '/# CAROL Framework/,/export PATH.*carol/d' "$rc_file"
            else
                sed -i '/# CAROL Framework/,/export PATH.*carol/d' "$rc_file"
            fi

            success "Cleaned $rc_file (backup: $rc_file.bak)"
            ((files_updated++))
        fi
    done

    if [ $files_updated -eq 0 ]; then
        info "CAROL not found in any shell configuration files"
    fi
}

# Check for active projects using CAROL
check_active_projects() {
    info "Checking for projects using CAROL..."

    # Find .carol directories (excluding the CAROL installation itself)
    local carol_projects=$(find ~ -maxdepth 4 -name ".carol" -type d 2>/dev/null | grep -v "^$CAROL_INSTALL_DIR" | head -5)

    if [ -n "$carol_projects" ]; then
        warning "Found projects using CAROL:"
        echo "$carol_projects"
        echo ""
        warning "After uninstall, these projects will still have .carol/ directories"
        warning "Symlink mode projects will have broken symlinks in:"
        echo "  - .carol/ (documentation symlinks)"
        echo "  - .opencode/agents/ (role definition symlinks)"
        echo ""
        notice "Portable mode projects will continue to work independently"
        echo ""
    fi
}

# Main uninstall
main() {
    echo ""
    info "CAROL Uninstaller"
    echo ""

    # Check for active projects first
    check_active_projects

    # Confirm uninstall
    read -p "Proceed with uninstall? (y/N) " -r response
    echo ""

    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        info "Uninstall cancelled"
        exit 0
    fi

    # Remove CAROL directory
    remove_carol_dir

    # Remove from PATH
    remove_from_path

    echo ""
    success "Uninstall complete!"
    echo ""
    info "Next steps:"
    echo "  1. Reload your shell:"
    echo "     source ~/.bashrc  (bash)"
    echo "     source ~/.zshrc   (zsh)"
    echo "  2. Or open a new terminal"
    echo ""
    notice "To clean up project integrations manually:"
    echo "  rm -rf /path/to/project/.carol"
    echo "  rm -rf /path/to/project/.opencode/agents"
    echo ""
    info "To reinstall later:"
    echo "  curl -fsSL https://raw.githubusercontent.com/jrengmusic/carol/main/install.sh | bash"
    echo ""
}

main "$@"
