# END terminal shell integration hooks
# Autoloaded by .zshenv, then unfunctioned.

end-integration() {
    _end_precmd() { printf '\033]7;file://%s%s\007\033]133;A\007' "$HOST" "$PWD" }
    _end_preexec() { printf '\033]133;C\007' }

    precmd_functions+=(_end_precmd)
    preexec_functions+=(_end_preexec)
}
