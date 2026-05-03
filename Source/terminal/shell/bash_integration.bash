# END terminal shell integration for bash - do not edit

# Exit POSIX mode (we were launched with --posix to force $ENV sourcing)
if [[ -n "$END_BASH_INJECT" ]]; then
    unset END_BASH_INJECT

    # Restore HISTFILE (POSIX mode defaults to ~/.sh_history)
    if [[ -n "$END_BASH_UNEXPORT_HISTFILE" ]]; then
        export HISTFILE="$HOME/.bash_history"
        unset END_BASH_UNEXPORT_HISTFILE
    fi

    # Turn off POSIX mode
    set +o posix

    # Source the real startup files
    if [[ -z "$END_BASH_NORC" ]]; then
        if [[ -n "$END_BASH_RCFILE" ]]; then
            source "$END_BASH_RCFILE"
            unset END_BASH_RCFILE
        elif [[ -f "$HOME/.bashrc" ]]; then
            source "$HOME/.bashrc"
        fi
    fi
    unset END_BASH_NORC

    # Unset ENV so child shells start normally
    unset ENV
fi

# Install hooks
_end_precmd() { printf '\033]7;file://%s%s\007\033]133;A\007' "$HOSTNAME" "$PWD"; }
PS0=$'\033]133;C\007'
PROMPT_COMMAND="_end_precmd${PROMPT_COMMAND:+;$PROMPT_COMMAND}"

if [[ -n "$END_CWD" ]]; then
    cd "$END_CWD"
    unset END_CWD
fi

end() {
    if [[ "$1" == "preview" ]]; then
        local file
        file="$(realpath "$2")"
        printf '\033]1337;END;%s;%s;%s\a' "$file" "${FZF_PREVIEW_COLUMNS:-0}" "${FZF_PREVIEW_LINES:-0}"
    else
        command "$END_BINARY" "$@"
    fi
}
