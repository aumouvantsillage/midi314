#!/bin/bash

DIR=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)

# Reset the keyboard.
echo "R" > /dev/ttyACM0

INTERFACE=${1:-"cli"}

PIDS=()

# Start Jack and the connection daemons.
if [ $INTERFACE = "gui" ]; then
    qjackctl --start --active-patchbay=$DIR/midi314.qjackctl & PIDS+=($!)
else
    jackd -P 70 --realtime -d alsa -d hw:1 & PIDS+=($!)
    jack-plumbing $DIR/midi314.rules & PIDS+=($!)
fi

sleep 1
a2jmidid -e & PIDS+=($!)

# Start the synthesizer.
fluidsynth --server --no-shell --dump \
    --audio-driver=jack \
    --midi-driver=jack \
    --gain=2 \
    --chorus=no \
    --reverb=no \
    /usr/share/sounds/sf2/TimGM6mb.sf2 & PIDS+=($!)

# Start the looper.
if [ $INTERFACE = "gui" ]; then
    LOOPER="slgui"
else
    LOOPER="sooperlooper"
fi

$LOOPER --load-session=$DIR/midi314.slsess --load-midi-binding=$DIR/midi314.slb & PIDS+=($!)

# Wait until the user presses a key.
read -rsp $'Press any key to terminate...\n' -n1

# Kill all processes started by this script.
# This will not kill sooperlooper if it was started by the GUI.
for p in ${PIDS[@]}; do
    echo "Killing $p"
    kill $p || true
done
