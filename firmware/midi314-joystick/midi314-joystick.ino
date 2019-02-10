
#include <TimerThree.h>
#include <midi314.h>

// Pins connected to each potentiometer (from left to right).
const byte potPins[] = {A3, A2};

// The number of potentiometers.
#define POTS sizeof(potPins)

// The step between consecutive potetiometer stops.
#define POT_STEP 8

// The hysteresis threshold to update the potentiometer value.
#define POT_MARGIN 2

// The MIDI channel of the instrument.
#define MIDI_CHANNEL DEFAULT_MIDI_CHANNEL

// The value of each potentiometer, range 0 to 127.
byte potValues[POTS];

// The default assignment of potentiometers.
const byte potFn[] = {POT_PITCH_BEND, POT_MODULATION};

void scan() {
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
                    case POT_PITCH_BEND: midi314.pitchBend(MIDI_CHANNEL, ((int)potValue - 64) * 128 + 0x2000);  break;
                    case POT_PAN:        midi314.controlChange(MIDI_CHANNEL, MIDI_CC_PAN, potValue);            break;
                    case POT_REVERB:     midi314.controlChange(MIDI_CHANNEL, MIDI_CC_REVERB, potValue);         break;
                    case POT_OTHER:      midi314.controlChange(MIDI_CHANNEL, MIDI_CC_OTHER_EFFECT, potValue);
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
        potValues[p] = analogRead(potPins[p]) / POT_STEP;
        midi314.pushEvent(POT_EVENT, p);
    }

    Serial.println("Ready");
}

void setup() {
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
