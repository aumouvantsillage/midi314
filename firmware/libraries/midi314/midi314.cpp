
#include "midi314.h"

Midi314 midi314;

Midi314::Midi314() {
    reset();
}

void Midi314::processPotEvent(byte channel, byte fn, byte value) {
    switch (fn) {
        case POT_VOLUME:     midi314.controlChange(channel, MIDI_CC_CHANNEL_VOLUME, value); break;
        case POT_MODULATION: midi314.controlChange(channel, MIDI_CC_MODULATION, value);     break;
        case POT_EXPRESSION: midi314.controlChange(channel, MIDI_CC_EXPRESSION,  value);    break;
        case POT_BREATH:     midi314.controlChange(channel, MIDI_CC_BREATH,  value);        break;
        case POT_PITCH_BEND: midi314.pitchBend(channel, (int)value * 128);  break;
        case POT_PAN:        midi314.controlChange(channel, MIDI_CC_PAN, value);            break;
        case POT_REVERB:     midi314.controlChange(channel, MIDI_CC_REVERB, value);         break;
        case POT_CHORUS:     midi314.controlChange(channel, MIDI_CC_CHORUS, value);         break;
    }
}

void Midi314::reset() {
    eventWriteIndex = 0;
    eventReadIndex = 0;
    eventCount = 0;
    flushMidi();
}

void Midi314::pushEvent(int k, int r, int c = 0) {
    // Write an event to the current write location.
    // If the queue was full, an event will be lost.
    eventQueue[eventWriteIndex ++] = {r, c, k};

    if (eventWriteIndex == KEY_EVENT_QUEUE_LENGTH) {
        eventWriteIndex = 0;
    }

    if (eventCount < KEY_EVENT_QUEUE_LENGTH) {
        eventCount ++;
    }
}

void Midi314::pullEvent(Event *evt) {
    // Precondition: eventCount > 0.
    // Read an event from the current read location.
    *evt = eventQueue[eventReadIndex ++];

    if (eventReadIndex == KEY_EVENT_QUEUE_LENGTH) {
        eventReadIndex = 0;
    }

    eventCount --;
}
