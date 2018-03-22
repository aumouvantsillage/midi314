
#include <MIDIUSB.h>
#include <pitchToNote.h>
#include <TimerThree.h>

// Pins connected to each row of the keyboard (from top to bottom).
const byte rowPins[] = {7, 5, 3, 2, 0, 1};

// Pins connected to each column of the keyboard (from left to right).
const byte colPins[] = {10, 16, 14, 15, 18, 19, 20};

// Pins connected to each potentiometer (from left to right).
const byte potPins[] = {A9, A8, A7, A6, A3};

// The number of keyboard rows.
#define ROWS sizeof(rowPins)

// The number of keyboard columns.
#define COLS sizeof(colPins)

// The number of potentiometers.
#define POTS sizeof(potPins)

// The time to validate a steady state of a key, in milliseconds.
#define BOUNCE_TIME_MS 5

// The number of consecutive milliseconds in the pressed state for each key.
byte keyPressedTimeMs[ROWS][COLS];

// The number of consecutive milliseconds in the released state for each key.
byte keyReleasedTimeMs[ROWS][COLS];

// The pressed state for each key after debouncing.
bool keyPressed[ROWS][COLS];

// Key press event indicators for each key.
bool keyPressedEvt[ROWS][COLS];

// Key release event indicators for each key.
bool keyReleasedEvt[ROWS][COLS];

// The default pitch associated with each key.
const byte keyNotes[ROWS][COLS] = {
    pitchC3,  pitchD3,  pitchE3, pitchG3b, pitchA3b, pitchB3b, pitchC4,
    pitchD3b, pitchE3b, pitchF3, pitchG3,  pitchA3,  pitchB3,  pitchD4b,
    pitchC3,  pitchD3,  pitchE3, pitchG3b, pitchA3b, pitchB3b, pitchC4,
    pitchD4,  pitchE4, pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5,
    pitchE4b, pitchF4, pitchG4,  pitchA4,  pitchB4,  pitchD5b, pitchE5b,
    pitchD4,  pitchE4, pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5
};

// The step between consecuting potetiometer stops.
#define POT_STEP 8

// The hysteresis threshold to update the potentiometer value.
#define POT_MARGIN 2

// The value of each potentiometer, range 0 to 127.
byte potValues[POTS];

// Potentiometer change events.
bool potEvt[POTS];

#define CHANNEL 0
byte velocity;

#define MIDI_CC_CHANNEL_VOLUME   7

void scan() {
    // All column output pins are supposed to be high before scanning.
    // Scan the keyboard column-wise.
    for (int c = 0; c < COLS; c ++) {
        // Enable the current column.
        digitalWrite(colPins[c], 0);
        
        // Read the current state of each key in the current column.
        for (int r = 0; r < ROWS; r ++) {
            bool keyStatus = !digitalRead(rowPins[r]);

            if (keyStatus) {
                // A key-pressed event is valid if the key was released for a sufficient time.
                if (!keyPressed[r][c] && keyReleasedTimeMs[r][c] == BOUNCE_TIME_MS) {
                    keyPressed[r][c] = true;
                    keyPressedEvt[r][c] = true;
                }

                // Count the time in the pressed state. Reset the released state counter.
                if (keyPressedTimeMs[r][c] < BOUNCE_TIME_MS) {
                    keyPressedTimeMs[r][c] ++;
                    keyReleasedTimeMs[r][c] = 0;
                }
            }
            else {
                // A key-released event is valid if the key was pressed for a sufficient time.
                if (keyPressed[r][c] && keyPressedTimeMs[r][c] == BOUNCE_TIME_MS) {
                    keyPressed[r][c] = false;
                    keyReleasedEvt[r][c] = true;
                }

                // Count the time in the released state. Reset the pressed state counter.
                if (keyReleasedTimeMs[r][c] < BOUNCE_TIME_MS) {
                    keyReleasedTimeMs[r][c] ++;
                    keyPressedTimeMs[r][c] = 0;
                }
            }
        }
        
        // Disable the current column.
        digitalWrite(colPins[c], 1);
    }

    // Read potentiometer values.
    for (int p = 0; p < POTS; p ++) {
        int analogPrev = (int)potValues[p] * POT_STEP;
        int analog = analogRead(potPins[p]);
        if (analog > analogPrev + POT_STEP + POT_MARGIN ||
            analog < analogPrev - POT_MARGIN) {
            potValues[p] = analog / POT_STEP;
            potEvt[p] = true;
        }
    }
}

static inline bool fnKeyPressed() {
    return keyPressed[0][0];
}

static inline void noteOn(byte channel, byte pitch, byte velocity) {
    midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOn);
}

static inline void noteOff(byte channel, byte pitch, byte velocity) {
    midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOff);
}

static inline void controlChange(byte channel, byte control, byte value) {
    midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
    MidiUSB.sendMIDI(event);
}

static inline void programChange(byte channel, byte value) {
    midiEventPacket_t event = {0x0C, 0xC0 | channel, value, value};
    MidiUSB.sendMIDI(event);
}

static inline void pitchBend(byte channel, int bend) {
    midiEventPacket_t event = {0x0E, 0xE0 | channel, bend & 0x7F, bend >> 7};
    MidiUSB.sendMIDI(event);
}

void processEvents() {
    bool sendMidi = false;

    // Keyboard events.
    if (!fnKeyPressed()) {
        for (int c = 0; c < COLS; c ++) {
            for (int r = 0; r < ROWS; r ++) {
                if (c == 0 && r == 0) {
                    continue;
                }
                
                if (keyPressedEvt[r][c]) {
                    keyPressedEvt[r][c] = false;
                    sendMidi = true;
                    noteOn(CHANNEL, keyNotes[r][c], velocity);
                }
                else if (keyReleasedEvt[r][c]) {
                    keyReleasedEvt[r][c] = false;
                    sendMidi = true;
                    noteOff(CHANNEL, keyNotes[r][c], velocity);
                }
            }
        }
    }
    else {
        // TODO Special functions.
    }

    // Potentiometer events.
    for (int p = 0; p < POTS; p ++) {
        if (potEvt[p]) {
            switch (p) {
                case 0: controlChange(CHANNEL, MIDI_CC_CHANNEL_VOLUME, potValues[0]); sendMidi = true; break;
                case 1: velocity = potValues[1]; break;
                case 2: pitchBend(CHANNEL, ((int)potValues[2] - 64) * 128 + 0x2000); sendMidi = true; break;
            }
            potEvt[p] = false;
        }
    }
    
    if (sendMidi) {
        MidiUSB.flush();
    }
}

void setup() {
    // Keyboard rows are attached to input pins with pull-ups.
    for (int r = 0; r < ROWS; r ++) {
        pinMode(rowPins[r], INPUT_PULLUP);
    }

    // Keyboard columns are attached to output pins.
    // We write 1 to each column to disable it.
    for (int c = 0; c < COLS; c ++) {
        pinMode(colPins[c], OUTPUT);
        digitalWrite(colPins[c], 1);
    }

    // Read the initial potentiometer values.
    for (int p = 0; p < POTS; p ++) {
        potValues[p] = analogRead(potPins[p]) / POT_STEP;
        potEvt[p] = true;
    }

    // Call the scan function every millisecond.
    Timer3.initialize(1000);
    Timer3.attachInterrupt(scan);
    
    Serial.begin(115200);
}

void loop() {
    processEvents();
    // TODO configuration via the serial line.
}

