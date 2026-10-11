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
# The same session, narrated: Final_Project_Presentation_SLMS.mp4 from 1:42.

BIN=./slms_app
INPUT=demo/demo_input.txt

if [ ! -x "$BIN" ]; then
    echo "Build first:  make"
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
