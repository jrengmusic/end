# END terminal shell integration hooks
# Sourced by .zshenv for interactive shells.

_end_precmd() {
    if [[ -n "$END_CWD" ]]; then
        cd "$END_CWD"
        unset END_CWD
    fi
    printf '\033]7;file://%s%s\007\033]133;A\007' "$HOST" "$PWD"
}
_end_preexec() { printf '\033]133;C\007' }

precmd_functions+=(_end_precmd)
preexec_functions+=(_end_preexec)
