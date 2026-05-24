#!/bin/bash
printf '\e[1mBold\e[0m \e[3mItalic\e[0m \e[4mUnder\e[0m \e[7mReverse\e[0m \e[9mStrike\e[0m\n'
printf '\e[31mRed \e[32mGreen \e[33mYellow \e[34mBlue \e[35mMagenta \e[36mCyan\e[0m\n'
printf '\e[91mBrRed \e[92mBrGreen \e[93mBrYellow \e[94mBrBlue \e[95mBrMagenta \e[96mBrCyan\e[0m\n'
printf '\e[48;5;236m Dark BG \e[48;5;240m Mid BG \e[48;5;245m Light BG \e[0m\n'
printf 'AV WAT fi fl ← → ≠ ≈ ∞ λ\n'
printf '\e[38;2;255;100;50mTruecolor gradient: '
for i in $(seq 0 15 255); do printf "\e[38;2;%d;%d;255m█" $i $((255-i)); done
printf '\e[0m\n'
