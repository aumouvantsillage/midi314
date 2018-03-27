#!/bin/sh

qjackctl --start --active-patchbay=midi314.qjackctl &

fluidsynth --server --no-shell --dump --audio-driver=jack /usr/share/sounds/sf2/TimGM6mb.sf2 &

slgui --load-session=midi314.slsess --load-midi-binding=midi314.slb

killall fluidsynth
