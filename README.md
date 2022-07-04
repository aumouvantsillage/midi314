
midi@3:14 is a home-made electronic keyboard for playing music.

This repository is no longer maintained.
Its content has been split into separate repositories available under the [Tiliosys](https://github.com/tiliosys) organization.

----

Keyboard
========

The midi@3:14 keyboard has the following characteristics:

* [Jankó](https://en.wikipedia.org/wiki/Jank%C3%B3_keyboard) layout with 3 rows of 14 keys.
* A *function key*, to assign special functions to the other keys.
* 5 potentiometers,
* USB-MIDI interface.

See the following files for more details:

* Keyboard [description](doc/Layout.md) and [layout](doc/Layout.svg).
* Printed circuit board [source files](hardware) and [bill of material](doc/BOM.md).

Firmware
========

The keyboard is equiped with an Arduino Pro Micro module.
The firmware converts user actions into MIDI events using the [MIDIUSB](https://www.arduino.cc/en/Reference/MIDIUSB)
library.

The source files for the Arduino IDE are located in the [firmware](firmware) folder.

Software
========

The [software](software) folder contains the programs that can be run on a
computer to play music and record loops.

* [midi314-display](software/midi314-display) shows the current state of the keyboard (selected program, loop states).
* [midi314-looper](software/midi314-looper) records and plays loops, and is controlled by MIDI events from the keyboard.

These programs are written in Rust and use [Jack](http://jackaudio.org/) as
the audio and MIDI interface.

The [scripts](software/scripts) folder contains a sample configuration for using the
midi@3:14 keyboard and software with [fluidsynth](http://www.fluidsynth.org/).
Fluidsynth 2 is recommended for monophonic instruments and breath controller support.
The following commands in the fluidsynth shell :

```
setbreathmode 0 0 1 1
setlegatomode 0 1
```

TODO
====

* Add metronome.
* Use metronome as a reference for the looper.
* Overdub in looper.
* Add effects.
* Record loop to a file.
* Try alternative synthesizers.
* Find a solution for safe power-off.
* Raspberry Pi: reduce boot time.
* Raspberry Pi: fix unresponsive keyboard for several seconds on boot.
* Check issue with volume after recording several loops.
