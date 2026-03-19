#!/bin/bash
# switch-subagent-model.sh - Toggle subagent roles between models
# Part of CAROL Framework v1.0.0
#
# Works in both contexts:
#   - Local project: operates on .opencode/agents/ in cwd
#   - Global SSOT:   operates on $CAROL_ROOT/roles/ (with --global)

set -e

SUBAGENTS=("librarian" "pathfinder" "validator" "researcher" "auditor" "engineer" "machinist" "oracle")

MODEL_SONNET="anthropic/claude-sonnet-4-6"
MODEL_MINIMAX="minimax-coding-plan/MiniMax-M2.5"

# Resolve CAROL_ROOT (same logic as carol CLI)
resolve_carol_root() {
    local source="${BASH_SOURCE[0]}"
    while [ -h "$source" ]; do
        local dir="$(cd -P "$(dirname "$source")" && pwd)"
        source="$(readlink "$source")"
        [[ $source != /* ]] && source="$dir/$source"
    done
    echo "$(cd -P "$(dirname "$source")/.." && pwd)"
}

get_roles_dir() {
    if [ "$GLOBAL" = true ]; then
        local root
        root="$(resolve_carol_root)"
        echo "$root/roles"
    else
        echo ".opencode/agents"
    fi
}

usage() {
    echo "Usage: $(basename "$0") [--global] <sonnet|minimax|status>"
    echo ""
    echo "  sonnet   Switch subagents to $MODEL_SONNET"
    echo "  minimax  Switch subagents to $MODEL_MINIMAX"
    echo "  status   Show current model for each subagent"
    echo ""
    echo "  --global Operate on CAROL SSOT (~/.carol/roles) instead of local project"
    echo ""
    echo "By default operates on .opencode/agents/ in the current directory."
    exit 1
}

show_status() {
    local roles_dir
    roles_dir="$(get_roles_dir)"
    echo "  [$roles_dir]"
    for role in "${SUBAGENTS[@]}"; do
        local file="$roles_dir/$role.md"
        if [ -f "$file" ]; then
            local model
            model=$(grep '^model:' "$file" | head -1 | sed 's/model: *//')
            echo "  $role -> $model"
        fi
    done
}

switch_model() {
    local target="$1"
    local roles_dir
    roles_dir="$(get_roles_dir)"

    if [ ! -d "$roles_dir" ]; then
        echo "Error: $roles_dir not found. Run 'carol init' first." >&2
        exit 1
    fi

    local count=0
    for role in "${SUBAGENTS[@]}"; do
        local file="$roles_dir/$role.md"
        if [ -f "$file" ]; then
            if [[ "$OSTYPE" == "darwin"* ]]; then
                sed -i '' "s|^model: .*|model: $target|" "$file"
            else
                sed -i "s|^model: .*|model: $target|" "$file"
            fi
            count=$((count + 1))
        fi
    done
    echo "Switched $count subagents to: $target"
    show_status
}

# Parse flags
GLOBAL=false
COMMAND=""

while [ $# -gt 0 ]; do
    case "$1" in
        --global) GLOBAL=true; shift ;;
        sonnet|minimax|status) COMMAND="$1"; shift ;;
        *) usage ;;
    esac
done

case "$COMMAND" in
    sonnet)  switch_model "$MODEL_SONNET" ;;
    minimax) switch_model "$MODEL_MINIMAX" ;;
    status)  show_status ;;
    *)       usage ;;
esac
