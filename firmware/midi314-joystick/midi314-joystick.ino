
#include <TimerThree.h>
#include <midi314.h>

// Pins connected to each potentiometer (from left to right).
const byte potPins[] = {A3, A2};

// The number of potentiometers.
#define POTS sizeof(potPins)

// The MIDI channel of the instrument.
#define MIDI_CHANNEL DEFAULT_MIDI_CHANNEL

// The value of each potentiometer, range 0 to 127.
byte potValues[POTS];

// The default assignment of potentiometers.
const byte potFn[] = {POT_PITCH_BEND, POT_EXPRESSION};

const byte potScale[] = {POT_BIQUAD, POT_BIQUAD};

// The minimum value of each potentiometer.
int potLow[POTS];

// The maximum value of each potentiometer.
int potHigh[POTS];

byte potRead(int p) {
    int analog = analogRead(potPins[p]);
    long avg, dx, dy;
    long val = analog, low = potLow[p], high = potHigh[p];
    switch (potScale[p]) {
        case POT_QUAD:
            dx = analog - potLow[p];
            dy = potHigh[p] - potLow[p];
            val = sq(dx);
            low = 0;
            high = sq(dy);
            break;
        case POT_BIQUAD:
            avg = (potLow[p] + potHigh[p]) / 2;
            dx = analog - avg;
            dy = potHigh[p] - avg;
            val = dx >= 0 ? sq(dx) : -sq(dx);
            high = sq(dy);
            low = -high;
            break;
    }
    return constrain(map(val, low, high, 0, 127), 0, 127);
}

void scan() {
    // Read potentiometer values.
    for (int p = 0; p < POTS; p ++) {
        byte value = potRead(p);
        if (value != potValues[p]) {
            potValues[p] = value;
            midi314.pushEvent(POT_EVENT, p);
        }
    }
}

void forcePotEvents() {
    for (int p = 0; p < POTS; p ++) {
        midi314.pushEvent(POT_EVENT, p);
    }
}

void processEvents() {
    while (midi314.hasEvent()) {
        Event evt;
        midi314.pullEvent(&evt);

        byte potValue;

        switch (evt.kind) {
            case POT_EVENT:
                potValue = potValues[evt.row];
                switch (potFn[evt.row]) {
                    case POT_VOLUME:     midi314.controlChange(MIDI_CHANNEL, MIDI_CC_CHANNEL_VOLUME, potValue); break;
                    case POT_MODULATION: midi314.controlChange(MIDI_CHANNEL, MIDI_CC_MODULATIION, (potValue >= 64 ? potValue - 64 : 63 - potValue) * 2); break;
                    case POT_EXPRESSION: midi314.controlChange(MIDI_CHANNEL, MIDI_CC_EXPRESSION,  potValue);    break;
                    case POT_PITCH_BEND: midi314.pitchBend(MIDI_CHANNEL, ((int)potValue - 64) * 128 + 0x2000);  break;
                    case POT_PAN:        midi314.controlChange(MIDI_CHANNEL, MIDI_CC_PAN, potValue);            break;
                    case POT_REVERB:     midi314.controlChange(MIDI_CHANNEL, MIDI_CC_REVERB, potValue);         break;
                    case POT_CHORUS:     midi314.controlChange(MIDI_CHANNEL, MIDI_CC_CHORUS, potValue);         break;
                }
                break;

            case PRESS_EVENT:
                break;

            case RELEASE_EVENT:
                break;
        }
    }
}

void reset() {
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
