# END terminal shell integration for fish - do not edit

# Restore original XDG_DATA_DIRS
if set -q END_FISH_XDG_DATA_DIR
    if set -q XDG_DATA_DIRS
        set -gx XDG_DATA_DIRS (string replace -r "^$END_FISH_XDG_DATA_DIR:?" "" $XDG_DATA_DIRS)
        if test -z "$XDG_DATA_DIRS"
            set -e XDG_DATA_DIRS
        end
    end
    set -e END_FISH_XDG_DATA_DIR
end

# Install hooks
function _end_precmd --on-event fish_prompt
    printf '\033]133;A\a'
end

function _end_preexec --on-event fish_preexec
    printf '\033]133;C\a'
end
