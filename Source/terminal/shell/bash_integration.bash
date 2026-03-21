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
_end_precmd() { printf '\033]133;A\007'; }
PS0=$'\033]133;C\007'
PROMPT_COMMAND="_end_precmd${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
