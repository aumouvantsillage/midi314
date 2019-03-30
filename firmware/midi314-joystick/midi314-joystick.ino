
#include <TimerThree.h>
#include <midi314.h>

// Pins connected to each potentiometer (from left to right).
static const byte potPins[] = {A0, A1, A6};

// The step between consecutive potetiometer stops.
#define POT_STEP 2

// The hysteresis threshold to update the potentiometer value.
#define POT_MARGIN 1

// The number of potentiometers.
#define POTS sizeof(potPins)

// The MIDI channel of the instrument.
#define MIDI_CHANNEL DEFAULT_MIDI_CHANNEL

// The value of each potentiometer, range 0 to 127.
static byte potValues[POTS];

// The default assignment of potentiometers (Horizontal, Vertical).
static const byte potFn[] = {POT_PITCH_BEND, POT_PITCH_BEND, POT_BREATH};

// The idle value of each potentiometer.
static int potIdle[POTS];

static inline byte joyRead(int p) {
    return constrain(map(analogRead(potPins[p]), potIdle[p] - 512, potIdle[p] + 511, 127, 0), 0, 127);
}

static void scan() {
    // Joystick (potentiometers 0 and 1)

    // Select the pot value that is farther from the center.
    int p0 = joyRead(0);
    int p1 = joyRead(1);
    int d0 = max(p0 - 64, 64 - p0);
    int d1 = max(p1 - 64, 64 - p1);
    // The horizontal potentiometer maps to a tone.
    // The vertical potentiometer maps to a semitone.
    int value = d0 > d1 ? p0 : map(p1, 0, 127, 32, 95);
    // Process both potentiometers as one.
    if (value != potValues[0]) {
        potValues[0] = value;
        midi314.pushEvent(POT_EVENT, 0);
    }

    // Others
    for (int p = 2; p < POTS; p ++) {
        int analogPrev = (int)potValues[p] * POT_STEP;
        int analog = analogRead(potPins[p]);
        if (analog > analogPrev + POT_STEP + POT_MARGIN ||
            analog < analogPrev - POT_MARGIN) {
            potValues[p] = constrain(analog / POT_STEP, 0, 127);
            midi314.pushEvent(POT_EVENT, p);
        }
    }
}

static void forcePotEvents() {
    for (int p = 0; p < POTS; p ++) {
        midi314.pushEvent(POT_EVENT, p);
    }
}

static void processEvents() {
    while (midi314.hasEvent()) {
        Event evt;
        midi314.pullEvent(&evt);

        byte potValue;

        switch (evt.kind) {
            case POT_EVENT:
                potValue = potValues[evt.row];
                if (potFn[evt.row] == POT_MODULATION) {
                    potValue = (potValue >= 64 ? potValue - 64 : 63 - potValue) * 2;
                }
                midi314.processPotEvent(MIDI_CHANNEL, potFn[evt.row], potValue);
                break;

            case PRESS_EVENT:
                break;

            case RELEASE_EVENT:
                break;
        }
    }
}

static void reset() {
    midi314.reset();

    // Read the initial potentiometer values.
    for (int p = 0; p < POTS; p ++) {
        potValues[p] = p < 2 ?
            joyRead(p) :
            potValues[p] = analogRead(potPins[p]) / POT_STEP;
        midi314.pushEvent(POT_EVENT, p);
    }

    Serial.println("Ready");
}

void setup() {
    for (int p = 0; p < POTS; p ++) {
        potIdle[p] = analogRead(potPins[p]);
    }

    Serial.begin(115200);

    reset();

    // Call the scan function every 2 milliseconds.
    Timer3.initialize(2000);
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
