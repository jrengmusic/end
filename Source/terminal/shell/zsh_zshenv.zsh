# END terminal shell integration - do not edit
# Restores original ZDOTDIR, sources real .zshenv, then loads hooks.

{
    # Restore original ZDOTDIR
    if [[ -n "$END_ORIG_ZDOTDIR" ]]; then
        export ZDOTDIR="$END_ORIG_ZDOTDIR"
        unset END_ORIG_ZDOTDIR
    else
        unset ZDOTDIR
    fi

    # Source the real .zshenv
    if [[ -f "${ZDOTDIR:-$HOME}/.zshenv" ]]; then
        source "${ZDOTDIR:-$HOME}/.zshenv"
    fi
} always {
    # Load integration hooks for interactive shells
    if [[ -o interactive ]]; then
        source "${${(%):-%x}:h}/end-integration"
    fi
}
