
#include "MIDIUSB.h"
#include "pitchToNote.h"

// Pins connected to each row of the keyboard (from top to bottom).
const byte row_pins[] = {7, 5, 3, 2, 0, 1};

// Pins connected to each column of the keyboard (from left to right).
const byte col_pins[] = {10, 16, 14, 15, 18, 19, 20};

// Pins connected to each potentiometer (from left to right).
const byte pot_pins[] = {4, 6, 8, 9};

// The number of keyboard rows.
#define ROWS sizeof(row_pins)

// The number of keyboard columns.
#define COLS sizeof(col_pins)

// The number of potentiometers.
#define POTS sizeof(pot_pins)

// The time to validate a steady state of a key, in milliseconds.
#define BOUNCE_TIME_MS 5

// The number of consecutive milliseconds in the pressed state for each key.
byte key_pressed_count[ROWS][COLS];

// The number of consecutive milliseconds in the released state for each key.
byte key_released_count[ROWS][COLS];

// The pressed state for each key after debouncing.
bool key_pressed_state[ROWS][COLS];

// Key press event indicators for each key.
bool key_pressed_evt[ROWS][COLS];

// Key release event indicators for each key.
bool key_released_evt[ROWS][COLS];

// The pitch associated with each key.
const byte key_notes[ROWS][COLS] = {
    pitchC3,  pitchD3,  pitchE3, pitchG3b, pitchA3b, pitchB3b, pitchC4,
    pitchD3b, pitchE3b, pitchF3, pitchG3,  pitchA3,  pitchB3,  pitchD4b,
    pitchC3,  pitchD3,  pitchE3, pitchG3b, pitchA3b, pitchB3b, pitchC4,
    pitchD4,  pitchE4, pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5,
    pitchE4b, pitchF4, pitchG4,  pitchA4,  pitchB4,  pitchD5b, pitchE5b,
    pitchD4,  pitchE4, pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5
};

#define CHANNEL 0
#define VELOCITY 100

void setup() {
    // Keyboard rows are attached to input pins with pull-ups.
    for (int r = 0; r < ROWS; r ++) {
        pinMode(row_pins[r], INPUT_PULLUP);
    }

    // Keyboard columns are attached to output pins.
    // We write 1 to each column to disable it.
    for (int c = 0; c < COLS; c ++) {
        pinMode(col_pins[c], OUTPUT);
        digitalWrite(col_pins[c], 1);
    }
}

void scan() {
    // All column output pins are supposed to be high before scanning.
    // Scan the keyboard column-wise.
    for (int c = 0; c < COLS; c ++) {
        // Enable the current column.
        digitalWrite(col_pins[c], 0);
        
        // Read the current state of each key in the current column.
        for (int r = 0; r < ROWS; r ++) {
            bool key_on = !digitalRead(row_pins[r]);

            // Clear the event indicators for the current key.
            key_pressed_evt[r][c]  = false;
            key_released_evt[r][c] = false;
            
            if (key_on) {
                // Count the time in the pressed state. Reset the released state counter.
                if (key_pressed_count[r][c] < BOUNCE_TIME_MS) {
                    key_pressed_count[r][c] ++;
                    key_released_count[r][c] = 0;
                }
                
                // A key-pressed event is valid if the key was released for a sufficient time.
                if (!key_pressed_state[r][c] && key_released_count[r][c] == BOUNCE_TIME_MS) {
                    key_pressed_state[r][c] = true;
                    key_pressed_evt[r][c] = true;
                }
            }
            else {
                // Count the time in the released state. Reset the pressed state counter.
                if (key_released_count[r][c] < BOUNCE_TIME_MS) {
                    key_released_count[r][c] ++;
                    key_pressed_count[r][c] = 0;
                }
                
                // A key-released event is valid if the key was pressed for a sufficient time.
                if (key_pressed_state[r][c] && key_pressed_count[r][c] == BOUNCE_TIME_MS) {
                    key_pressed_state[r][c] = false;
                    key_released_evt[r][c] = true;
                }
            }
        }
        
        // Disable the current column.
        digitalWrite(col_pins[c], 1);
    }
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

void midiEvents() {
    if (!key_pressed_state[0][0]) {
        for (int c = 0; c < COLS; c ++) {
            for (int r = 0; r < ROWS; r ++) {
                if (key_pressed_evt[r][c]) {
                    noteOn(CHANNEL, key_notes[r][c], VELOCITY);
                }
                else if (key_released_evt[r][c]) {
                    noteOff(CHANNEL, key_notes[r][c], VELOCITY);
                }
            }
        }
    }
}

// TODO use library TimerThree to use timer interrupts for scanning.
// TODO check that scan() runs in less than 1 ms

void loop() {
    scan();
    midiEvents();
    delay(1);
}
