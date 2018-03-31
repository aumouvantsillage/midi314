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

fluidsynth --server --no-shell --dump \
    --audio-driver=jack \
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

if [ $INTERFACE != "gui" ]; then
    function get_alsa_port {
        while [ true ]; do
            client=$(aconnect $1 | grep -E "$2" | grep -oE "client\s*[0-9]+" | grep -oE "[0-9]+")
            if [ "$client" = "" ]; then
                sleep 1
                continue
            fi
            port=$(aconnect $1 | grep -E "$3" | grep -oE "^\s*[0-9]+" | grep -oE "[0-9]+")
            echo "$client:$port"
            break
        done
    }

    echo "Looking up ALSA port for MIDI input"
    MIDI_PORT=$(get_alsa_port -i "Arduino Leonardo" "Arduino Leonardo MIDI 1")
    echo "Found $MIDI_PORT"

    echo "Looking up ALSA port for synth"
    SYNTH_PORT=$(get_alsa_port -o "FLUID Synth \([0-9]+\)" "Synth input port \([0-9]+:[0-9]+\)")
    echo "Found $SYNTH_PORT"

    echo "Looking up ALSA port for looper"
    LOOPER_PORT=$(get_alsa_port -o "sooperlooper" "sooperlooper")
    echo "Found $LOOPER_PORT"

    aconnect $MIDI_PORT $SYNTH_PORT
    aconnect $MIDI_PORT $LOOPER_PORT
fi

read -rsp $'Press any key to continue...\n' -n1

for p in ${PIDS[@]}; do
    echo "Killing $p"
    kill $p || true
done
