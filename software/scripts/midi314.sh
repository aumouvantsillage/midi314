#!/bin/bash
trap "kill 0" SIGINT SIGTERM EXIT

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
    qjackctl --start --active-patchbay=$SHARE/midi314.qjackctl &
    jack_wait -w
else
    echo "---- midi@3.14 ---- Starting Jack server"
    killall --wait jackd || true

    # Repeatedly attempt to start jackd.
    while true; do
        jackd --realtime --realtime-priority 70 \
            -d alsa --playback --rate 48000 --device $DEVICE
        sleep 1
        echo "---- midi@3.14 ---- Failed to start Jack server, retrying"
    done &

    jack_wait --wait
    echo "---- midi@3.14 ---- Jack server started"
    jack-plumbing $SHARE/midi314.rules &
fi

echo "---- midi@3.14 ---- Starting ALSA to Jack MIDI daemon"
a2jmidid -e &

# Start the synthesizer.
echo "---- midi@3.14 ---- Starting synthesizer"
fluidsynth --server --no-shell \
    --audio-driver=jack \
    --midi-driver=jack \
    --sample-rate=48000 \
    --gain=2 \
    --chorus=no \
    --reverb=no \
    ${SOUNDFONT} &

# Start the looper.
echo "---- midi@3.14 ---- Starting display"
midi314-looper  &
echo "---- midi@3.14 ---- Starting looper"
midi314-display &

if [ $INTERFACE = "service" ]; then
    echo "---- midi@3.14 ---- Running forever"
    sleep infinity
else
    read -rsp $"---- midi@3.14 ---- Press a key to terminate...\n" -n1
fi
