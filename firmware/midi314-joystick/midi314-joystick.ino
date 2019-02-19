
#include <TimerThree.h>
#include <midi314.h>

// Pins connected to each potentiometer (from left to right).
static const byte potPins[] = {A3, A2};

// The number of potentiometers.
#define POTS sizeof(potPins)

// The MIDI channel of the instrument.
#define MIDI_CHANNEL DEFAULT_MIDI_CHANNEL

// The value of each potentiometer, range 0 to 127.
static byte potValues[POTS];

// The default assignment of potentiometers (Horizontal, Vertical).
static const byte potFn[] = {POT_PITCH_BEND, POT_PITCH_BEND};

// The minimum value of each potentiometer.
static int potLow[POTS];

// The maximum value of each potentiometer.
static int potHigh[POTS];

static inline byte potRead(int p) {
    return constrain(map(analogRead(potPins[p]), potLow[p], potHigh[p], 0, 127), 0, 127);
}

static void scan() {
    if (potFn[0] == POT_PITCH_BEND && potFn[1] == POT_PITCH_BEND) {
        // Select the pot value that is farther from the center.
        int p0 = potRead(0);
        int p1 = potRead(1);
        int d0 = max(p0 - 64, 64 - p0);
        int d1 = max(p1 - 64, 64 - p1);
        // The horizontal potentiometer maps to a tone.
        // The vertical potentiometer maps to a semitone.
        int value = d0 > d1 ? p0 : map(p1, 0, 127, 32, 95);
        // Process both potentiometer as one.
        if (value != potValues[0]) {
            potValues[0] = value;
            midi314.pushEvent(POT_EVENT, 0);
        }
    }
    else {
        for (int p = 0; p < POTS; p ++) {
            byte value = potRead(p);
            if (value != potValues[p]) {
                potValues[p] = value;
                midi314.pushEvent(POT_EVENT, p);
            }
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
        potValues[p] = potRead(p);
        midi314.pushEvent(POT_EVENT, p);
    }

    Serial.println("Ready");
}

void setup() {
    for (int p = 0; p < POTS; p ++) {
        int value = analogRead(potPins[p]);
        potLow[p] = 2 * value - 1023;
        if (potLow[p] < 0) {
            potLow[p] = 0;
        }
        potHigh[p] = 2 * value;
        if (potHigh[p] > 1023) {
            potHigh[p] = 1023;
        }
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
