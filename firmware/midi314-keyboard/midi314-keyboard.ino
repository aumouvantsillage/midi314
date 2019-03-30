
#include <pitchToNote.h>
#include <TimerThree.h>
#include <midi314.h>

// Pins connected to each row of the keyboard (from top to bottom).
static const byte rowPins[] = {7, 5, 3, 2, 0, 1};

// Pins connected to each column of the keyboard (from left to right).
static const byte colPins[] = {10, 16, 14, 15, 18, 19, 20};

// Pins connected to each potentiometer (from left to right).
static const byte potPins[] = {A9, A8, A7, A6, A3};

// The number of keyboard rows.
#define ROWS sizeof(rowPins)

// The number of keyboard columns.
#define COLS sizeof(colPins)

// The step between consecutive potetiometer stops.
#define POT_STEP 8

// The hysteresis threshold to update the potentiometer value.
#define POT_MARGIN 2

// The number of potentiometers.
#define POTS sizeof(potPins)

// The time to validate a steady state of a key, in milliseconds.
#define BOUNCE_TIME_MS 5

// The number of consecutive milliseconds in the pressed state for each key.
static byte keyPressedTimeMs[ROWS][COLS];

// The number of consecutive milliseconds in the released state for each key.
static byte keyReleasedTimeMs[ROWS][COLS];

// The pressed state for each key after debouncing.
static bool keyPressed[ROWS][COLS];

// The default pitch associated with each key.
static const byte keyNotes[ROWS][COLS] = {
    pitchC3,  pitchD3,  pitchE3,  pitchG3b, pitchA3b, pitchB3b, pitchC4,  // Top row, left
    pitchD3b, pitchE3b, pitchF3,  pitchG3,  pitchA3,  pitchB3,  pitchD4b, // Middle row, left
    pitchC3,  pitchD3,  pitchE3,  pitchG3b, pitchA3b, pitchB3b, pitchC4,  // Bottom row, left
    pitchD4,  pitchE4,  pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5,  // Top row, right
    pitchE4b, pitchF4,  pitchG4,  pitchA4,  pitchB4,  pitchD5b, pitchE5b, // Middle row, right
    pitchD4,  pitchE4,  pitchG4b, pitchA4b, pitchB4b, pitchC5,  pitchD5   // Bottom row, right
};

// The percussion number associated with each key.
// Not used: 58, 69, 71, 72, 73, 74, 75
static const byte keyPerc[ROWS][COLS] = {
    0,  61, 60, 64, 63, 62, 66, // Top row, left
    42, 44, 46, 49, 51, 52, 55, // Middle row, left
    35, 36, 38, 40, 37, 41, 43, // Bottom row, left
    65, 68, 67, 77, 76, 79, 78, // Top row, right
    57, 59, 54, 53, 56, 80, 81, // Middle row, right
    45, 47, 48, 50, 69, 70, 39  // Bottom row, right
};

// Indicates whether a note is playing for each key.
static bool keyNoteOn[ROWS][COLS];

// Function ids for keys.
enum {
    KEY_NONE   = 0x00,

    KEY_FN     = 0xFF,

    KEY_OCTAVE = 0x10,
    KEY_SEMI   = 0x20,
    KEY_PROG   = 0x30,
    KEY_TEMPO  = 0x40,
    KEY_LOOP   = 0x50,
    KEY_PERC   = 0x60,
    KEY_MONO   = 0x70,
    KEY_PANIC  = 0x80,

    // Multi-purpose Up/Down key codes, to be combined with
    // KEY_OCTAVE, KEY_SEMI, KEY_PROG, KEY_TEMPO.
    KEY_DOWN   = 0x0F,
    KEY_UP     = 0x0E,

    // Loop control key codes, to be combined with KEY_LOOP.
    KEY_DEL    = 0x0F,
    KEY_SOLO   = 0x0E,
    KEY_ALL    = 0x0D
};

// The special functions associated with each key.
static const byte keyFn[ROWS][COLS] = {
    KEY_FN,            KEY_OCTAVE|KEY_DOWN, KEY_OCTAVE|KEY_UP,  KEY_TEMPO|KEY_DOWN, KEY_TEMPO|KEY_NONE, KEY_TEMPO|KEY_UP,   KEY_NONE,        // Top row, left
    KEY_SEMI|KEY_DOWN, KEY_SEMI  |KEY_UP,   KEY_PROG  |0,       KEY_PROG |1,        KEY_PROG |2,        KEY_PROG |3,        KEY_PROG|4,      // Middle row, left
    KEY_LOOP|KEY_DEL,  KEY_LOOP  |KEY_SOLO, KEY_LOOP  |KEY_ALL, KEY_LOOP |0,        KEY_LOOP |1,        KEY_LOOP |2,        KEY_LOOP|3,      // Bottom row, left
    KEY_NONE,          KEY_NONE,            KEY_NONE,           KEY_NONE,           KEY_NONE,           KEY_MONO,           KEY_PERC,        // Top row, right
    KEY_PROG|5,        KEY_PROG  |6,        KEY_PROG  |7,       KEY_PROG |8,        KEY_PROG |9,        KEY_PROG |KEY_DOWN, KEY_PROG|KEY_UP, // Middle row, right
    KEY_LOOP|4,        KEY_LOOP  |5,        KEY_LOOP  |6,       KEY_LOOP |7,        KEY_LOOP |8,        KEY_NONE,           KEY_PANIC        // Bottom row, right
};

#define FN_KEY_PRESSED keyPressed[0][0]
#define DEL_KEY_PRESSED keyPressed[2][0]
#define SOLO_KEY_PRESSED keyPressed[2][1]

// The value of each potentiometer, range 0 to 127.
static byte potValues[POTS];

// The default note velocity.
#define VELOCITY 127

// The pitch of the bottom-left key.
static int pitchOffset;

// The MIDI program mapped to key P1.
static int midiProgramOffset;

// The current MIDI program number.
static byte midiProgram;

// The number of the current loop.
static byte currentLoop;

// The number of supported loops.
#define LOOPS 9

enum {
    LOOP_EMPTY,
    LOOP_RECORDING,
    LOOP_PLAYING,
    LOOP_MUTED
};

// The current state of each loop.
static byte loopState[LOOPS];

// The current MIDI channel.
static byte midiChannel;

// The current mono/polyphonic mode.
static bool monoMode;

// The default assignment of potentiometers.
static const byte potFn[] = {POT_VOLUME, POT_PAN, POT_REVERB, POT_CHORUS, POT_MODULATION};

static void scan() {
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
                    midi314.pushEvent(PRESS_EVENT, r, c);
                }

                // Count the time in the pressed state. Reset the released state counter.
                if (keyPressedTimeMs[r][c] < BOUNCE_TIME_MS) {
                    keyPressedTimeMs[r][c] ++;
                }

                keyReleasedTimeMs[r][c] = 0;
            }
            else {
                // A key-released event is valid if the key was pressed for a sufficient time.
                if (keyPressed[r][c] && keyPressedTimeMs[r][c] == BOUNCE_TIME_MS) {
                    keyPressed[r][c] = false;
                    midi314.pushEvent(RELEASE_EVENT, r, c);
                }

                // Count the time in the released state. Reset the pressed state counter.
                if (keyReleasedTimeMs[r][c] < BOUNCE_TIME_MS) {
                    keyReleasedTimeMs[r][c] ++;
                }

                keyPressedTimeMs[r][c] = 0;
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
            midi314.pushEvent(POT_EVENT, p);
        }
    }
}

static inline int getMinPitch() {
    return pitchOffset + keyNotes[2][0];
}

static inline int getMaxPitch() {
    return pitchOffset + keyNotes[ROWS-2][COLS-1];
}

static bool isRecording;

static inline void stopRecording() {
    loopState[currentLoop] = LOOP_PLAYING;
    isRecording = false;
    midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_PLAY, currentLoop);
}

static inline void updatePitchOffset(int n) {
    if (n < 0 && getMinPitch() + n >= pitchA0 || n > 0 && getMaxPitch() + n <= pitchC8) {
        pitchOffset += n;
    }
    midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_SET_MIN_PITCH, getMinPitch());
}

static inline void updateMidiProgramOffset(int n) {
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
    midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_SET_MIN_PROGRAM, midiProgramOffset);
}

static void playSolo(int n) {
    if (loopState[n] == LOOP_MUTED) {
        loopState[n] = LOOP_PLAYING;
    }
    for (int i = 0; i < LOOPS; i ++) {
        if (i != n && loopState[i] == LOOP_PLAYING) {
            loopState[i] = LOOP_MUTED;
        }
    }
    midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_SOLO, n);
}

static void playAllLoops() {
    // Unmute all muted loops.
    for (int i = 0; i < LOOPS; i ++) {
        if (loopState[i] == LOOP_MUTED) {
            loopState[i] = LOOP_PLAYING;
        }
    }
    midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_ALL, 0);
}

static inline void updateLoop(int n) {
    switch (loopState[n]) {
        case LOOP_EMPTY:
            if (!DEL_KEY_PRESSED && !isRecording) {
                loopState[n] = LOOP_RECORDING;
                currentLoop = n;
                isRecording = true;
                midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_RECORD, n);
            }
            break;
        case LOOP_PLAYING:
            if (DEL_KEY_PRESSED) {
                loopState[n] = LOOP_EMPTY;
                midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_DELETE, n);
            }
            else if (SOLO_KEY_PRESSED) {
                playSolo(n);
            }
            else {
                loopState[n] = LOOP_MUTED;
                midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_MUTE, n);
            }
            break;
        case LOOP_MUTED:
            if (DEL_KEY_PRESSED) {
                loopState[n] = LOOP_EMPTY;
                midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_DELETE, n);
            }
            else if (SOLO_KEY_PRESSED) {
                playSolo(n);
            }
            else {
                loopState[n] = LOOP_PLAYING;
                midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_PLAY, n);
            }
            break;
    }
}

static void forcePotEvents() {
    for (int p = 0; p < POTS; p ++) {
        midi314.pushEvent(POT_EVENT, p);
    }
}

static void processFunctionKey(int r, int c) {
    byte fn  = keyFn[r][c] & 0xF0;
    byte arg = keyFn[r][c] & 0x0F;

    switch (fn) {
        case KEY_OCTAVE:
            updatePitchOffset(arg == KEY_UP ? 12 : -12);
            break;
        case KEY_SEMI:
            updatePitchOffset(arg == KEY_UP ? 1 : -1);
            break;
        case KEY_PERC:
            if (midiChannel == DEFAULT_MIDI_CHANNEL) {
                midiChannel = PERC_MIDI_CHANNEL;
                midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_PERCUSSION, 127);
            }
            else {
                midiChannel = DEFAULT_MIDI_CHANNEL;
                midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_CUSTOM_PERCUSSION, 0);
            }
            forcePotEvents();
            break;
        case KEY_MONO:
            monoMode = !monoMode;
            if (monoMode) {
                midi314.controlChange(midiChannel, MIDI_CC_MONO_MODE_ON, 0);
            }
            else {
                midi314.controlChange(midiChannel, MIDI_CC_POLY_MODE_ON, 0);
            }
            break;
        case KEY_PROG:
            if (midiChannel != PERC_MIDI_CHANNEL) {
                switch (arg) {
                    case KEY_DOWN:
                        updateMidiProgramOffset(-10);
                        break;
                    case KEY_UP:
                        updateMidiProgramOffset(10);
                        break;
                    default:
                        midiProgram = (midiProgramOffset + arg) % 128;
                        midi314.programChange(midiChannel, midiProgram);
                }
            }
            break;
        case KEY_TEMPO:
            // TODO Not implemented.
            break;
        case KEY_LOOP:
            switch (arg) {
                case KEY_ALL:
                    playAllLoops();
                    break;
                case KEY_SOLO:
                case KEY_DEL:
                    break;
                default:
                    updateLoop(arg);
            }
            break;
        case KEY_PANIC:
            midi314.controlChange(DEFAULT_MIDI_CHANNEL, MIDI_CC_ALL_NOTES_OFF, 0);
            break;
    }
}

static inline byte getNote(int r, int c) {
    return midiChannel == PERC_MIDI_CHANNEL ?
        keyPerc[r][c] :
        pitchOffset + keyNotes[r][c];
}

static void processEvents() {
    while (midi314.hasEvent()) {
        Event evt;
        midi314.pullEvent(&evt);

        byte potValue;

        switch (evt.kind) {
            case POT_EVENT:
                midi314.processPotEvent(midiChannel, potFn[evt.row], potValues[evt.row]);
                break;

            case PRESS_EVENT:
                if (keyFn[evt.row][evt.col] == KEY_FN) {
                    if (isRecording) {
                        stopRecording();
                    }
                }
                else if (FN_KEY_PRESSED) {
                    processFunctionKey(evt.row, evt.col);
                }
                else {
                    keyNoteOn[evt.row][evt.col] = true;
                    midi314.noteOn(midiChannel, getNote(evt.row, evt.col), VELOCITY);
                }
                break;

            case RELEASE_EVENT:
                if (keyNoteOn[evt.row][evt.col]) {
                    keyNoteOn[evt.row][evt.col] = false;
                    midi314.noteOff(midiChannel, getNote(evt.row, evt.col), VELOCITY);
                }
                break;
        }
    }
}

static void reset() {
    midi314.reset();

    // Read the initial potentiometer values.
    for (int p = 0; p < POTS; p ++) {
        potValues[p] = analogRead(potPins[p]) / POT_STEP;
        midi314.pushEvent(POT_EVENT, p);
    }

    pitchOffset = 0;
    midiChannel = DEFAULT_MIDI_CHANNEL;
    midiProgramOffset = 0;
    midiProgram = 0;
    currentLoop = 0;
    isRecording = false;
    monoMode = false;

    // FIXME send MIDI events to ensure this state is consistent with synth

    for (int l = 0; l < LOOPS; l ++) {
        loopState[l] = LOOP_EMPTY;
    }

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
    processEvents();
    midi314.flushMidi();

    if (Serial.available() > 0) {
        // TODO More configurations via the serial line.
        char c = Serial.read();
        if (c == 'R') {
            reset();
        }
    }
}
