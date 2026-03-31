#!/bin/bash
# CAROL preToolUse hook — blocks forbidden operations

input=$(cat)
tool_name=$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('tool_name',''))" 2>/dev/null)
command=$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('tool_input',{}).get('command',''))" 2>/dev/null)

if [[ "$tool_name" == "Bash" ]]; then
    if echo "$command" | grep -qE '^git reset --hard'; then
        echo "CAROL: forbidden — git reset --hard requires ARCHITECT approval"
        exit 2
    fi
    if echo "$command" | grep -qE '^git checkout --'; then
        echo "CAROL: forbidden — git checkout -- requires ARCHITECT approval"
        exit 2
    fi
    if echo "$command" | grep -qE '^git branch -D'; then
        echo "CAROL: forbidden — git branch -D requires ARCHITECT approval"
        exit 2
    fi
fi

exit 0
