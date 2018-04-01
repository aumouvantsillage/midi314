#!/bin/bash

# TODO reset keyboard

INTERFACE=${1:-"cli"}

PIDS=()

if [ $INTERFACE = "gui" ]; then
    qjackctl --start --active-patchbay=midi314.qjackctl & PIDS+=($!)
else
    jackd -P 70 --realtime -d alsa -d hw:1 & PIDS+=($!)
    jack-plumbing midi314.rules & PIDS+=($!)
fi

sleep 1
a2jmidid -e & PIDS+=($!)

fluidsynth --server --no-shell --dump \
    --audio-driver=jack \
    --midi-driver=jack \
    --gain=2 \
    --chorus=no \
    --reverb=no \
    /usr/share/sounds/sf2/TimGM6mb.sf2 & PIDS+=($!)

if [ $INTERFACE = "gui" ]; then
    LOOPER="slgui"
else
    LOOPER="sooperlooper"
fi

$LOOPER --load-session=midi314.slsess --load-midi-binding=midi314.slb & PIDS+=($!)

read -rsp $'Press any key to terminate...\n' -n1

for p in ${PIDS[@]}; do
    echo "Killing $p"
    kill $p || true
done
