# END terminal shell integration hooks
# Sourced by .zshenv for interactive shells.

_end_precmd() {
    if [[ -n "$END_CWD" ]]; then
        cd "$END_CWD"
        unset END_CWD
    fi
    printf '\033]7;file://%s%s\007\033]133;A\007' "$HOST" "$PWD"
}

_end_preexec() {
    printf '\033]133;C\007'
}

# Use add-zsh-hook (robust against array-clobber by frameworks)
autoload -Uz add-zsh-hook
add-zsh-hook precmd _end_precmd
add-zsh-hook preexec _end_preexec

end() {
    if [[ "$1" == "preview" ]]; then
        local file="${2:a}"
        printf '\033]1337;END;%s;%s;%s\a' "$file" "${FZF_PREVIEW_COLUMNS:-0}" "${FZF_PREVIEW_LINES:-0}"
    else
        command "$END_BINARY" "$@"
    fi
}
