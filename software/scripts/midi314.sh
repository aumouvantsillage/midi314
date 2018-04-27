#!/bin/bash

DIR=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)
SHARE=$DIR/../share/midi314

# Reset the keyboard.
echo "R" > /dev/ttyACM0

INTERFACE=${INTERFACE:-"cli"}
SOUNDFONT=${SOUNDFONT:-/usr/share/sounds/sf2/FluidR3_GM.sf2}
DEVICE=${DEVICE:-hw:1}

PIDS=()

# Start Jack and the connection daemons.
if [ $INTERFACE = "gui" ]; then
    qjackctl --start --active-patchbay=$SHARE/midi314.qjackctl & PIDS+=($!)
else
    jackd --realtime --realtime-priority 70 \
        -d alsa --rate 48000 --device $DEVICE & PIDS+=($!)
    jack-plumbing $SHARE/midi314.rules & PIDS+=($!)
fi

sleep 1
a2jmidid -e & PIDS+=($!)

# Start the synthesizer.
fluidsynth --server --no-shell \
    --audio-driver=jack \
    --midi-driver=jack \
    --sample-rate=48000 \
    --gain=2 \
    --chorus=no \
    --reverb=no \
    ${SOUNDFONT} & PIDS+=($!)

# Start the looper.
midi314-looper  & PIDS+=($!)
midi314-display & PIDS+=($!)

if [ $INTERFACE = "service" ]; then
    echo "Running forever"
    sleep infinity
else
    read -rsp $"Press a key to terminate...\n" -n1
fi

# Kill all processes started by this script.
# This will not kill sooperlooper if it was started by the GUI.
for p in ${PIDS[@]}; do
    echo "Killing $p"
    kill $p || true
done
