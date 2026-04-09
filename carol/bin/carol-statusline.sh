#!/bin/bash
# CAROL Context Rot Meter — Claude Code status line

data=$(cat)

# CAROL version from SSOT (/VERSION file at repo root)
carol_version=$(tr -d '[:space:]' < "$(dirname "$0")/../VERSION" 2>/dev/null)
carol_version=${carol_version:-"?"}

# Parse context %, model name, agent role, and rate limit
parsed=$(python3 -c "
import sys, json, time
d = json.load(sys.stdin)
cw = d.get('context_window', {}) or {}
pct = int(cw.get('used_percentage', 0) or 0)
model = (d.get('model', {}) or {}).get('display_name', '') or ''
model = model.replace(' (1M context)', '').strip()
agent = (d.get('agent', {}) or {}).get('name', '') or ''
rls = d.get('rate_limits', {}) or {}
def fmt(window):
    w = rls.get(window, {}) or {}
    p = int(w.get('used_percentage', 0) or 0)
    r = int(w.get('resets_at', 0) or 0)
    rem = ''
    if r > 0:
        delta = max(0, r - int(time.time()))
        days, rem_s = delta // 86400, delta % 86400
        h, m = rem_s // 3600, (rem_s % 3600) // 60
        if days: rem = f'{days}d{h:02d}h'
        elif h:  rem = f'{h}h{m:02d}m'
        else:    rem = f'{m}m'
    return p, rem
rl_pct, remaining = fmt('five_hour')
wk_pct, wk_remaining = fmt('seven_day')
print(pct)
print(model)
print(agent.upper())
print(rl_pct)
print(remaining)
print(wk_pct)
print(wk_remaining)
" <<< "$data" 2>/dev/null)

pct=$(sed -n '1p' <<< "$parsed")
model=$(sed -n '2p' <<< "$parsed")
agent=$(sed -n '3p' <<< "$parsed")
rl_pct=$(sed -n '4p' <<< "$parsed")
rl_remaining=$(sed -n '5p' <<< "$parsed")
wk_pct=$(sed -n '6p' <<< "$parsed")
wk_remaining=$(sed -n '7p' <<< "$parsed")

pct=${pct:-0}
model=${model:-""}
agent=${agent:-""}
rl_pct=${rl_pct:-0}
rl_remaining=${rl_remaining:-""}
wk_pct=${wk_pct:-0}
wk_remaining=${wk_remaining:-""}

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
        BRAINSTORMER) role_bg="\033[48;2;217;119;41m\033[38;2;9;13;18m" ;;   # claude orange bg, bunker fg
        COUNSELOR)    role_bg="\033[48;2;0;200;216m\033[38;2;9;13;18m" ;;   # blueBikini bg, bunker fg
        SURGEON)      role_bg="\033[48;2;252;112;76m\033[38;2;9;13;18m" ;;  # preciousPersimmon bg, bunker fg
        *)            role_bg="\033[48;2;78;140;147m\033[38;2;9;13;18m" ;;  # paradiso bg, bunker fg
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
    rl_label="  ${dim_color}🕔${reset}${rl_bar}${rl_reset_label}"
fi

# Weekly (7-day) bar
wk_label=""
if [ "$wk_pct" -gt 0 ]; then
    if   [ "$wk_pct" -ge 75 ]; then wk_color="\033[38;2;252;112;76m"
    elif [ "$wk_pct" -ge 50 ]; then wk_color="\033[38;2;200;120;50m"
    elif [ "$wk_pct" -ge 25 ]; then wk_color="\033[38;2;0;150;160m"
    else                            wk_color="\033[38;2;51;83;91m"
    fi
    WK_SEGMENTS=15
    wk_filled=$((wk_pct * WK_SEGMENTS / 100))
    wk_bar=$(build_bar $WK_SEGMENTS $wk_filled "$wk_color")
    wk_reset_label=""
    [ -n "$wk_remaining" ] && wk_reset_label=" ${dim_color}${wk_remaining}${reset}"
    wk_label="  ${dim_color}📅${reset}${wk_bar}${wk_reset_label}"
fi

printf "${label_color}◈ CAROL v${carol_version}${reset}${role_label}  ${dim_color}${model}${reset}  ${ctx_emoji}${bar}${rl_label}${wk_label}\n"
