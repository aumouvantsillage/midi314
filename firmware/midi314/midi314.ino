
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
    pitchC3,  pitchD3,  pitchE3, pitchG3b, pitchA3b, pitchB3b, pitchC4,  // Top row, left
    pitchD3b, pitchE3b, pitchF3, pitchG3,  pitchA3,  pitchB3,  pitchD4b, // Middle row, left
    pitchC3,  pitchD3,  pitchE3, pitchG3b, pitchA3b, pitchB3b, pitchC4,  // Bottom row, left
    pitchD4,  pitchE4, pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5,  // Top row, right
    pitchE4b, pitchF4, pitchG4,  pitchA4,  pitchB4,  pitchD5b, pitchE5b, // Middle row, right
    pitchD4,  pitchE4, pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5   // Bottom row, right
};

// Indicates whether a note is playing for each key.
bool keyNoteOn[ROWS][COLS];

// Function ids for keys.
enum {
    KEY_NONE   = 0x00,

    KEY_OCTAVE = 0x10,
    KEY_SEMI   = 0x20,
    KEY_PROG   = 0x30,
    KEY_TEMPO  = 0x40,
    KEY_LOOP   = 0x50,

    KEY_DOWN   = 0x0F,
    KEY_UP     = 0x0E,
    KEY_DEL    = 0x0F,
    KEY_SOLO   = 0x0E,
};

// The special functions associated with each key.
const byte keyFn[ROWS][COLS] = {
    KEY_NONE,          KEY_OCTAVE|KEY_DOWN, KEY_OCTAVE|KEY_UP, KEY_TEMPO|KEY_DOWN, KEY_TEMPO|KEY_NONE, KEY_TEMPO|KEY_UP,   KEY_NONE,        // Top row, left
    KEY_SEMI|KEY_DOWN, KEY_SEMI  |KEY_UP,   KEY_PROG  |0,      KEY_PROG |1,        KEY_PROG |2,        KEY_PROG |3,        KEY_PROG|4,      // Middle row, left
    KEY_LOOP|KEY_DEL,  KEY_LOOP  |KEY_SOLO, KEY_NONE,          KEY_LOOP |0,        KEY_LOOP |1,        KEY_LOOP |2,        KEY_LOOP|3,      // Bottom row, left
    KEY_NONE,          KEY_NONE,            KEY_NONE,          KEY_NONE,           KEY_NONE,           KEY_NONE,           KEY_NONE,        // Top row, right
    KEY_PROG|5,        KEY_PROG  |6,        KEY_PROG  |7,      KEY_PROG |8,        KEY_PROG |9,        KEY_PROG |KEY_DOWN, KEY_PROG|KEY_UP, // Middle row, right
    KEY_LOOP|4,        KEY_LOOP  |5,        KEY_LOOP  |6,      KEY_LOOP |7,        KEY_LOOP |8,        KEY_NONE,           KEY_NONE         // Bottom row, right
};

// The step between consecuting potetiometer stops.
#define POT_STEP 8

// The hysteresis threshold to update the potentiometer value.
#define POT_MARGIN 2

// The value of each potentiometer, range 0 to 127.
byte potValues[POTS];

// Potentiometer change events.
bool potEvt[POTS];

// The default note velocity.
#define VELOCITY 127

// The pitch of the bottom-left key.
int pitchOffset;

// Indicates that MIDI events are pending.
bool midiSent;

// The MIDI program mapped to key P1.
int midiProgramOffset;

// The current MIDI program number.
byte midiProgram;

// The number of the current loop.
byte currentLoop;

// The number of supported loops.
#define LOOPS 9

enum {
    LOOP_EMPTY,
    LOOP_RECORDING,
    LOOP_PLAYING,
    LOOP_MUTED
};

// The current state of each loop.
byte loopState[LOOPS];

// MIDI control change events.
enum {
    MIDI_CC_CHANNEL_VOLUME       = 7,
    MIDI_CC_PAN                  = 10,
    MIDI_CC_REVERB               = 91,
    MIDI_CC_OTHER_EFFECT         = 92,

    // Looper events (non-standard).
    MIDI_CC_CUSTOM_RECORD          = 20,
    MIDI_CC_CUSTOM_PLAY            = 21,
    MIDI_CC_CUSTOM_MUTE            = 22,
    MIDI_CC_CUSTOM_DELETE          = 23,
    MIDI_CC_CUSTOM_SOLO            = 24,
    MIDI_CC_CUSTOM_SET_MIN_PITCH   = 25,
    MIDI_CC_CUSTOM_SET_MIN_PROGRAM = 26,
};

// The MIDI channel of the instrument.
#define MIDI_CHANNEL 0

// Function ids for potentiometers.
enum {
    POT_NONE,
    POT_VOLUME,
    POT_PITCH_BEND,
    POT_PAN,
    POT_REVERB,
    POT_OTHER,
};

// The default assignment of potentiometers.
const byte potFn[] = {POT_VOLUME, POT_PAN, POT_REVERB, POT_PITCH_BEND, POT_OTHER};

bool eventsPending;

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

    eventsPending = true;
}

#define FN_KEY_PRESSED keyPressed[0][0]

#define FN_KEY_PRESSED_EVT keyPressedEvt[0][0]

#define DEL_KEY_PRESSED keyPressed[2][0]

static inline void noteOn(byte channel, byte pitch, byte velocity) {
    midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOn);
    midiSent = true;
}

static inline void noteOff(byte channel, byte pitch, byte velocity) {
    midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOff);
    midiSent = true;
}

static inline void controlChange(byte channel, byte control, byte value) {
    midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
    MidiUSB.sendMIDI(event);
    midiSent = true;
}

static inline void programChange(byte channel, byte value) {
    midiEventPacket_t event = {0x0C, 0xC0 | channel, value, value};
    MidiUSB.sendMIDI(event);
    midiSent = true;
}

static inline void pitchBend(byte channel, int bend) {
    midiEventPacket_t event = {0x0E, 0xE0 | channel, bend & 0x7F, bend >> 7};
    MidiUSB.sendMIDI(event);
    midiSent = true;
}

static inline int getMinPitch() {
    return pitchOffset + keyNotes[2][0];
}

static inline int getMaxPitch() {
    return pitchOffset + keyNotes[ROWS-2][COLS-1];
}

void processNoteEvents() {
    for (int c = 0; c < COLS; c ++) {
        for (int r = 0; r < ROWS; r ++) {
            if (keyPressedEvt[r][c] && !FN_KEY_PRESSED) {
                keyPressedEvt[r][c] = false;
                keyNoteOn[r][c] = true;
                noteOn(MIDI_CHANNEL, pitchOffset + keyNotes[r][c], VELOCITY);
            }
            else if (keyReleasedEvt[r][c]) {
                keyReleasedEvt[r][c] = false;
                if (keyNoteOn[r][c]) {
                    keyNoteOn[r][c] = false;
                    noteOff(MIDI_CHANNEL, pitchOffset + keyNotes[r][c], VELOCITY);
                }
            }
        }
    }
}

inline void stopRecording() {
    loopState[currentLoop] = LOOP_PLAYING;
    controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_PLAY, currentLoop);
}

inline void updatePitchOffset(int n) {
    if (n < 0 && getMinPitch() + n >= pitchA0 || n > 0 && getMaxPitch() + n <= pitchC8) {
        pitchOffset += n;
    }
    controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_SET_MIN_PITCH, getMinPitch());
}

inline void updateMidiProgramOffset(int n) {
    int nextOffset = midiProgramOffset + n;
    if (nextOffset < 0) {
        midiProgramOffset = 120;
    }
    else if (nextOffset > 120) {
        midiProgramOffset = 0;
    }
    else {
        midiProgramOffset = nextOffset;
    }
    controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_SET_MIN_PROGRAM, midiProgramOffset);
}

inline void updateLoop(int n) {
    // TODO Solo
    switch (loopState[n]) {
        case LOOP_EMPTY:
            if (!DEL_KEY_PRESSED) {
                loopState[n] = LOOP_RECORDING;
                currentLoop = n;
                controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_RECORD, n);
            }
            break;
        case LOOP_PLAYING:
            if (DEL_KEY_PRESSED) {
                loopState[n] = LOOP_EMPTY;
                controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_DELETE, n);
            }
            else {
                loopState[n] = LOOP_MUTED;
                controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_MUTE, n);
            }
            break;
        case LOOP_MUTED:
            if (DEL_KEY_PRESSED) {
                loopState[n] = LOOP_EMPTY;
                controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_DELETE, n);
            }
            else {
                loopState[n] = LOOP_PLAYING;
                controlChange(MIDI_CHANNEL, MIDI_CC_CUSTOM_PLAY, n);
            }
            break;
    }
}

void processFunctionKeys() {
    if (FN_KEY_PRESSED_EVT && loopState[currentLoop] == LOOP_RECORDING) {
        stopRecording();
    }

    for (int c = 0; c < COLS; c ++) {
        for (int r = 0; r < ROWS; r ++) {
            if (keyPressedEvt[r][c]) {
                keyPressedEvt[r][c] = false;
                byte fn  = keyFn[r][c] & 0xF0;
                byte arg = keyFn[r][c] & 0x0F;

                switch (fn) {
                    case KEY_OCTAVE:
                        updatePitchOffset(arg == KEY_UP ? 12 : -12);
                        break;
                    case KEY_SEMI:
                        updatePitchOffset(arg == KEY_UP ? 1 : -1);
                        break;
                    case KEY_PROG:
                        switch (arg) {
                            case KEY_DOWN:
                                updateMidiProgramOffset(-10);
                                break;
                            case KEY_UP:
                                updateMidiProgramOffset(10);
                                break;
                            default:
                                midiProgram = (midiProgramOffset + arg) % 128;
                                programChange(MIDI_CHANNEL, midiProgram);
                        }
                        break;
                    case KEY_TEMPO:
                        // TODO Not implemented.
                        break;
                    case KEY_LOOP:
                        updateLoop(arg);
                        break;
                }
            }
        }
    }
}

void processEvents() {
    // Keyboard events.
    processNoteEvents();
    if (FN_KEY_PRESSED) {
        processFunctionKeys();
    }

    // Potentiometer events.
    for (int p = 0; p < POTS; p ++) {
        if (potEvt[p]) {
            potEvt[p] = false;
            switch (potFn[p]) {
                case POT_VOLUME:     controlChange(MIDI_CHANNEL, MIDI_CC_CHANNEL_VOLUME, potValues[p]); break;
                case POT_PITCH_BEND: pitchBend(MIDI_CHANNEL, ((int)potValues[p] - 64) * 128 + 0x2000);  break;
                case POT_PAN:        controlChange(MIDI_CHANNEL, MIDI_CC_PAN, potValues[p]);            break;
                case POT_REVERB:     controlChange(MIDI_CHANNEL, MIDI_CC_REVERB, potValues[p]);
                case POT_OTHER:      controlChange(MIDI_CHANNEL, MIDI_CC_OTHER_EFFECT, potValues[p]);
            }
        }
    }

    if (midiSent) {
        MidiUSB.flush();
        midiSent = false;
    }
}

void reset() {
    // Clear all keyboard events.
    for (int c = 0; c < COLS; c ++) {
        for (int r = 0; r < ROWS; r ++) {
            keyPressedEvt[r][c] = keyReleasedEvt[r][c] = false;
        }
    }

    // Read the initial potentiometer values.
    for (int p = 0; p < POTS; p ++) {
        potValues[p] = analogRead(potPins[p]) / POT_STEP;
        potEvt[p] = true;
    }

    pitchOffset = 0;
    midiProgramOffset = 0;
    midiProgram = 0;
    currentLoop = 0;

    for (int l = 0; l < LOOPS; l ++) {
        loopState[l] = LOOP_EMPTY;
    }

    midiSent = false;
    eventsPending = true;

    Serial.println("Ready");
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

    Serial.begin(115200);

    reset();

    // Call the scan function every millisecond.
    Timer3.initialize(1000);
    Timer3.attachInterrupt(scan);
}

void loop() {
    if (eventsPending) {
        eventsPending = false;
        processEvents();
    }

    if (Serial.available() > 0) {
        // TODO More configurations via the serial line.
        char c = Serial.read();
        if (c == 'R') {
            reset();
        }
    }
}
