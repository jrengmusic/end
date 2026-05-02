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
    printf '\033]7;file://%s%s\a' (hostname) $PWD
    printf '\033]133;A\a'
end

function _end_preexec --on-event fish_preexec
    printf '\033]133;C\a'
end

function end
    if test "$argv[1]" = "preview"
        set -l file (realpath "$argv[2]")
        switch "$END_SKIT"
            case kitty
                printf '\033_GEND;%s\033\\' "$file"
            case iterm2
                printf '\033]1337;END;%s\a' "$file"
            case sixel
                printf '\033P0qEND;%s\033\\' "$file"
        end
    else
        command "$END_BINARY" $argv
    end
end
