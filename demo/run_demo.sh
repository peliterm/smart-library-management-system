#!/bin/sh
# Smart Library Management System - scripted demo
#
# Replays a fixed sequence of keystrokes through the console interface.
# The seed data and the input are both deterministic, so every run produces
# identical output. That is the point: the demo can be rehearsed, reviewed,
# and re-run without depending on live typing.
#
#   ./demo/run_demo.sh           play at a readable pace
#   ./demo/run_demo.sh --fast    no delay, for a quick verification run
#
# Scene list and narration: demo/DEMO_SCRIPT.md

BIN=./slms_app
INPUT=demo/demo_input.txt

if [ ! -x "$BIN" ]; then
    echo "Build first:"
    echo "  g++ -std=c++17 -Wall -Wextra -o slms_app src/app.cpp src/LibraryUI.cpp \\"
    echo "      src/BookInventory.cpp src/UserAccounts.cpp src/DueDateHeap.cpp \\"
    echo "      src/Recommender.cpp src/LibrarySystem.cpp"
    exit 1
fi

if [ "$1" = "--fast" ]; then
    "$BIN" < "$INPUT"
else
    # Feed one line at a time so the screens appear at a watchable pace.
    while IFS= read -r line; do
        printf '%s\n' "$line"
        sleep 1.2
    done < "$INPUT" | "$BIN"
fi
