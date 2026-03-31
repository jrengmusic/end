#!/bin/bash
# CAROL Context Rot Meter — Claude Code status line

data=$(cat)

# CAROL version from SSOT (bin/carol)
carol_version=$(grep '^VERSION=' "$(dirname "$0")/carol" 2>/dev/null | cut -d'"' -f2)
carol_version=${carol_version:-"?"}

# Parse context %, model name, agent role, and rate limit
parsed=$(python3 -c "
import sys, json, time
d = json.load(sys.stdin)
cw = d.get('context_window', {}) or {}
pct = int(cw.get('used_percentage', 0) or 0)
model = (d.get('model', {}) or {}).get('display_name', '') or ''
agent = (d.get('agent', {}) or {}).get('name', '') or ''
rl = (d.get('rate_limits', {}) or {}).get('five_hour', {}) or {}
rl_pct = int(rl.get('used_percentage', 0) or 0)
resets_at = int(rl.get('resets_at', 0) or 0)
remaining = ''
if resets_at > 0:
    delta = max(0, resets_at - int(time.time()))
    h, m = delta // 3600, (delta % 3600) // 60
    remaining = f'{h}h{m:02d}m' if h else f'{m}m'
print(pct)
print(model)
print(agent.upper())
print(rl_pct)
print(remaining)
" <<< "$data" 2>/dev/null)

pct=$(sed -n '1p' <<< "$parsed")
model=$(sed -n '2p' <<< "$parsed")
agent=$(sed -n '3p' <<< "$parsed")
rl_pct=$(sed -n '4p' <<< "$parsed")
rl_remaining=$(sed -n '5p' <<< "$parsed")

pct=${pct:-0}
model=${model:-""}
agent=${agent:-""}
rl_pct=${rl_pct:-0}
rl_remaining=${rl_remaining:-""}

# Scale to 0-80% range (CC compacts at ~80%, so 80% = full bar)
# Clamp at 100 to handle any edge cases
[ "$pct" -gt 100 ] && pct=100
scaled=$((pct * 100 / 80))
[ "$scaled" -gt 100 ] && scaled=100

# Palette from StyleSheet.xml
reset="\033[0m"
bold="\033[1m"
bg_dark="\033[48;5;236m"           # dark grey
bg_gap="\033[48;5;233m"            # darker gap
dim_color="\033[38;2;51;83;91m"   # mediterranea
label_color="\033[38;2;105;157;170m"  # tranquiliTeal

# Color: 4 hard thresholds
# 0-24%: deep teal | 25-49%: rich amber | 50-74%: warm orange | 75%+: preciousPersimmon
if   [ "$scaled" -ge 75 ]; then color="\033[38;2;252;112;76m"; ctx_emoji="🥵"
elif [ "$scaled" -ge 50 ]; then color="\033[38;2;200;120;50m"; ctx_emoji="😟"
elif [ "$scaled" -ge 25 ]; then color="\033[38;2;0;150;160m"; ctx_emoji="😐"
else                            color="\033[38;2;51;83;91m"; ctx_emoji="😊"
fi

# Continuous bar builder
# Usage: build_bar <segments> <filled_count> <color_escape>
build_bar() {
    local segments=$1 filled=$2 clr=$3
    local bar_out="${bg_dark}"
    for ((i=0; i<segments; i++)); do
        if [ "$i" -lt "$filled" ]; then
            bar_out="${bar_out}${clr}${bold}█"
        else
            bar_out="${bar_out} "
        fi
    done
    bar_out="${bar_out}${reset}"
    echo -n "$bar_out"
}

# Context bar — 20 segments
CTX_SEGMENTS=15
ctx_filled=$((scaled * CTX_SEGMENTS / 100))
bar=$(build_bar $CTX_SEGMENTS $ctx_filled "$color")

# Role badge: block bg color with contrast fg
role_label=""
if [ -n "$agent" ]; then
    case "$agent" in
        COUNSELOR) role_bg="\033[48;2;0;200;216m\033[38;2;9;13;18m" ;;   # blueBikini bg, bunker fg
        SURGEON)   role_bg="\033[48;2;252;112;76m\033[38;2;9;13;18m" ;;  # preciousPersimmon bg, bunker fg
        *)         role_bg="\033[48;2;78;140;147m\033[38;2;9;13;18m" ;;  # paradiso bg, bunker fg
    esac
    role_label="  ${role_bg}${bold} ${agent} ${reset}"
fi

# Rate limit bar — 10 segments (same gradient)
rl_label=""
if [ "$rl_pct" -gt 0 ]; then
    if   [ "$rl_pct" -ge 75 ]; then rl_color="\033[38;2;252;112;76m"
    elif [ "$rl_pct" -ge 50 ]; then rl_color="\033[38;2;200;120;50m"
    elif [ "$rl_pct" -ge 25 ]; then rl_color="\033[38;2;0;150;160m"
    else                            rl_color="\033[38;2;51;83;91m"
    fi
    RL_SEGMENTS=15
    rl_filled=$((rl_pct * RL_SEGMENTS / 100))
    rl_bar=$(build_bar $RL_SEGMENTS $rl_filled "$rl_color")
    rl_reset_label=""
    [ -n "$rl_remaining" ] && rl_reset_label=" ${dim_color}${rl_remaining}${reset}"
    rl_label="  ${dim_color}⚡${reset}${rl_bar}${rl_reset_label}"
fi

printf "${label_color}◈ CAROL v${carol_version}${reset}${role_label}  ${dim_color}${model}${reset}  ${ctx_emoji}${bar}${rl_label}\n"
