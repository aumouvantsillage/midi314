
Potentiometers
==============

| Pot | Function                    |
|:----|:----------------------------|
| 1   | Channel volume              |
| 2   | Balance                     |
| 3   | Reverb                      |
| 4   | Pitch bend                  |
| 5   | Other (instrument-specific) |

There is no potentiometer for the main volume: the MIDI instrument must provide
a way to set the output volume.

Keyboard
========

When the `Fn` (top-left) key is released, the other keys behave like a 3-row Janko keyboard.
Pressing a key sends a note-on MIDI event to channel 0, releasing a key sends a note-off MIDI event.

When the `Fn` key is pressed, the other keys have special functions:

| Abbreviation  | Function                                             |
|:--------------|:-----------------------------------------------------|
| `O-`          | Tune the keyboard one octave down.                   |
| `O+`          | Tune the keyboard one octave up.                     |
| `S-`          | Tune the keyboard one semitone down.                 |
| `S+`          | Tune the keyboard one semitone up.                   |
| `T-`          | Tempo down.                                          |
| `T+`          | Tempo up.                                            |
| `TT`          | Tap tempo.                                           |
| `P#`          | Instrument selection (MIDI Program Change).          |
| `P-`          | Previous 10 instruments.                             |
| `P+`          | Next 10 instruments.                                 |
| `Per`         | Toggle the percussion (channel 10) instrument mode   |
| `L#`          | Record, start, mute a loop.                          |
| `Del` + `L#`  | Delete a loop.                                       |
| `Solo` + `L#` | Mute all other loops and play the selected one only. |
| `All`         | Unmute all muted loops.                              |

While recording a loop, pressing `Fn` alone stops the recording and starts the loop.

Not implemented yet
===================

* Reverb
* Other effect
* Tempo
