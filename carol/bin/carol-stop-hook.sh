#!/bin/bash
# CAROL Stop hook — play sound when END is not in focus
frontmost=$(osascript -e 'tell application "System Events" to get name of first application process whose frontmost is true' 2>/dev/null)
if [[ "$frontmost" != "END" ]]; then
    afplay /System/Library/Sounds/Submarine.aiff
fi
