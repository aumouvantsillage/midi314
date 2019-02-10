
#include "midi314.h"

Midi314 midi314;

Midi314::Midi314() {
    reset();
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
