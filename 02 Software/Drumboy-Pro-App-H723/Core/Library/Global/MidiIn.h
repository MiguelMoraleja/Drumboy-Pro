// MIDI input: a byte parser for channel messages and a queue of note events between the UART
// interrupt and the main loop. Kept free of hardware headers so it can be compiled and tested
// on a PC (see 02 Software/tests).
//
// What the parser follows from the MIDI 1.0 spec:
// - Running status: a channel status byte (0x80-0xEF) stays in effect for following data
//   bytes until another status byte arrives, so "90 24 64 26 64" is two note-ons.
// - Real-time bytes (0xF8-0xFF) may appear anywhere, even inside a message, and change
//   nothing. The caller handles them before calling midiInParse(); the parser ignores them too.
// - SysEx (0xF0 ... 0xF7) and system common bytes (0xF1-0xF7) cancel running status; their
//   data bytes are skipped.
// - Note-on with velocity 0 is a note-off.
//
// Only note-ons on the selected channel become events: samples are one-shots, so note-offs
// are parsed (to stay in step) but not queued.
#ifndef __MIDI_IN_H
#define __MIDI_IN_H

#include <stdint.h>

struct MidiInNote {
    uint8_t key;
    uint8_t velocity;  // 1-127
};

struct MidiInParser {
    uint8_t status   = 0;  // running status, 0 = none
    uint8_t expected = 0;  // data bytes in the current message
    uint8_t count    = 0;  // data bytes received so far
    uint8_t data0    = 0;
    bool sysEx       = false;
};

// Number of data bytes after a status byte; 0 for messages without data.
inline uint8_t midiInDataLength(uint8_t status_) {
    if (status_ < 0xF0) {
        const uint8_t kind = status_ & 0xF0;
        return ((kind == 0xC0) || (kind == 0xD0)) ? 1 : 2;
    }
    switch (status_) {
        case 0xF1: return 1;  // MTC quarter frame
        case 0xF2: return 2;  // song position
        case 0xF3: return 1;  // song select
        default:   return 0;
    }
}

// Feeds one byte. Returns true and fills note_ when a note-on with velocity > 0 on channel_
// (0-15) is complete.
inline bool midiInParse(MidiInParser& p_, uint8_t byte_, uint8_t channel_, MidiInNote& note_) {
    if (byte_ >= 0xF8) return false;  // real-time: no effect on the message in progress

    if (byte_ & 0x80) {  // status byte
        p_.sysEx = (byte_ == 0xF0);
        p_.count = 0;
        if (byte_ < 0xF0) {
            p_.status = byte_;
            p_.expected = midiInDataLength(byte_);
        } else {
            // System common and SysEx cancel running status. Their data is skipped: keep a
            // status so data bytes are counted, then drop it when the message is complete.
            p_.status = (byte_ == 0xF0 || byte_ == 0xF7) ? 0 : byte_;
            p_.expected = midiInDataLength(byte_);
            if (p_.expected == 0) p_.status = 0;
        }
        return false;
    }

    // data byte
    if (p_.sysEx || (p_.status == 0)) return false;  // SysEx payload, or no status yet

    if (p_.count == 0) p_.data0 = byte_;
    if (++p_.count < p_.expected) return false;

    // message complete
    const uint8_t status = p_.status;
    p_.count = 0;
    if (status >= 0xF0) {  // system common: no running status
        p_.status = 0;
        return false;
    }
    if ((status & 0x0F) != (channel_ & 0x0F)) return false;
    if (((status & 0xF0) == 0x90) && (byte_ > 0)) {
        note_.key = p_.data0;
        note_.velocity = byte_;
        return true;
    }
    return false;  // note-off, note-on with velocity 0, or another channel message
}

// Single-producer, single-consumer queue: push() from the UART interrupt, pop() from the main
// loop. Size must be a power of two; one slot stays empty to tell full from empty. A note
// arriving while the queue is full is dropped and counted.
template <uint8_t N>
struct MidiInQueue {
    static_assert((N >= 2) && ((N & (N - 1)) == 0), "MidiInQueue size must be a power of two");

    MidiInNote notes[N];
    volatile uint8_t head    = 0;  // written by push()
    volatile uint8_t tail    = 0;  // written by pop()
    volatile uint16_t dropped = 0;

    bool push(const MidiInNote& note_) {
        const uint8_t h = head;
        const uint8_t next = static_cast<uint8_t>((h + 1) & (N - 1));
        if (next == tail) {
            dropped = static_cast<uint16_t>(dropped + 1);
            return false;
        }
        notes[h] = note_;
        __atomic_signal_fence(__ATOMIC_RELEASE);  // note written before head moves
        head = next;
        return true;
    }

    bool pop(MidiInNote& note_) {
        const uint8_t t = tail;
        if (t == head) return false;
        __atomic_signal_fence(__ATOMIC_ACQUIRE);  // head read before the note
        note_ = notes[t];
        __atomic_signal_fence(__ATOMIC_RELEASE);  // note read before the slot is freed
        tail = static_cast<uint8_t>((t + 1) & (N - 1));
        return true;
    }

    // Main loop only, with nothing pushing (e.g. MIDI in switched off): forget queued notes.
    void clear() { tail = head; }
};

// Velocity 1-127 to the 0-100 scale of kNumberDataLibrary, rounded. 127 gives 100.
inline uint8_t midiInVelocityTo100(uint8_t velocity_) {
    if (velocity_ > 127) velocity_ = 127;
    return static_cast<uint8_t>((velocity_ * 100 + 63) / 127);
}

// Recording: velocities below this become GHOST beats, the rest FLAT. Plain beats have only
// these two playing levels until they get a real velocity (backlog item 14 B).
const uint8_t kMidiInGhostBelow = 64;

#endif
