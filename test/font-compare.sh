#!/bin/bash
# Font rendering comparison script
# Opens a side-by-side view: left = reference terminal, right = this terminal (END)
# Screenshot both for comparison

FONT="Display Mono"
SIZE=14

# Sample text covering common rendering issues
cat << 'ENDOFTEXT'
ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
0123456789 !@#$%^&*()-=_+[]{}|;:'",.< >?/~
The quick brown fox jumps over the lazy dog.
fi fl ff ffi ffl st ct  (ligatures)
== != >= <= => -> :: ... /// /** */ <!--
||  &&  ??  ?.  ..  ...
iIlL1| oO0Q  ({[<>]})
    indented with 4 spaces
		indented with 2 tabs
ENDOFTEXT

echo "=== Font Rendering Comparison ==="
echo "Font: $FONT @ ${SIZE}pt"
echo "================================="
echo ""
echo "$TEXT"
echo ""
echo "================================="
echo "Run this script in both END and your reference terminal."
echo "Screenshot each for side-by-side comparison."
