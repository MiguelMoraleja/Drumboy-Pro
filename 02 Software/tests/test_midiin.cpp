// Host-side tests for MIDI input (Core/Library/Global/MidiIn.h).
//
// 1. Plain note-ons, and note-offs that make no event.
// 2. Note-on with velocity 0 is a note-off.
// 3. Running status, also across real-time bytes; system common and SysEx cancel it.
// 4. Channel filter, and other channel messages (CC, program change) kept in step.
// 5. Queue: a burst of notes arrives in order; full queue drops and counts; clear.
// 6. Velocity scaling.

#include <cstdio>
#include <vector>

#include "MidiIn.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        ++checks;                                                           \
        if (!(cond)) {                                                      \
            ++failures;                                                     \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond);     \
            std::printf(__VA_ARGS__);                                       \
            std::printf("\n");                                              \
        }                                                                   \
    } while (0)

// Feeds bytes to a fresh parser; returns the note-ons as key*1000+velocity.
static std::vector<int> run(const std::vector<int>& bytes_, uint8_t channel_ = 0) {
    MidiInParser p;
    std::vector<int> out;
    for (int b : bytes_) {
        MidiInNote n{};
        if (midiInParse(p, static_cast<uint8_t>(b), channel_, n)) out.push_back(n.key * 1000 + n.velocity);
    }
    return out;
}

static bool same(const std::vector<int>& a_, const std::vector<int>& b_) { return a_ == b_; }

static void testBasic() {
    CHECK(same(run({0x90, 0x24, 0x64}), {36100}), "one note-on");
    CHECK(same(run({0x80, 0x24, 0x40}), {}), "note-off makes no event");
    CHECK(same(run({0x90, 0x24, 0x64, 0x80, 0x24, 0x00, 0x90, 0x26, 0x7F}), {36100, 38127}), "on, off, on");
    CHECK(same(run({0x24, 0x64}), {}), "data with no status is ignored");
    CHECK(same(run({0x90, 0x24}), {}), "incomplete message makes no event");
}

static void testVelocityZero() {
    CHECK(same(run({0x90, 0x24, 0x00}), {}), "note-on velocity 0 is a note-off");
    // Typical running-status stream: on, "off" as on/0, on again.
    CHECK(same(run({0x90, 0x24, 0x64, 0x24, 0x00, 0x24, 0x50}), {36100, 36080}), "on/0 in running status");
}

static void testRunningStatus() {
    CHECK(same(run({0x90, 0x24, 0x64, 0x26, 0x50, 0x2A, 0x20}), {36100, 38080, 42032}), "three notes, one status");
    // Clock bytes in the middle of a message and between messages.
    CHECK(same(run({0x90, 0xF8, 0x24, 0xF8, 0x64, 0xF8, 0x26, 0xFE, 0x50}), {36100, 38080}), "real-time bytes change nothing");
    CHECK(same(run({0x90, 0x24, 0xFA, 0x64, 0xFC, 0x26, 0x50}), {36100, 38080}), "start/stop inside messages");
    // System common cancels running status, and its data bytes are skipped.
    CHECK(same(run({0x90, 0x24, 0x64, 0xF2, 0x10, 0x20, 0x26, 0x50}), {36100}), "song position cancels running status");
    CHECK(same(run({0x90, 0x24, 0x64, 0xF1, 0x26, 0x26, 0x50}), {36100}), "MTC quarter frame cancels it");
    CHECK(same(run({0x90, 0x24, 0x64, 0xF6, 0x26, 0x50}), {36100}), "tune request cancels it");
    // SysEx: payload is skipped, running status is gone afterwards; a new status works.
    CHECK(same(run({0x90, 0x24, 0x64, 0xF0, 0x7D, 0x52, 0x11, 0xF7, 0x26, 0x50, 0x90, 0x28, 0x40}), {36100, 40064}),
          "SysEx payload skipped");
    CHECK(same(run({0xF0, 0x90, 0x24, 0x64}), {36100}), "status byte ends an unterminated SysEx");
    CHECK(same(run({0xF0, 0x7D, 0x24, 0x64, 0xF7}), {}), "SysEx data never becomes notes");
}

static void testChannels() {
    CHECK(same(run({0x91, 0x24, 0x64}, 0), {}), "other channel ignored");
    CHECK(same(run({0x91, 0x24, 0x64}, 1), {36100}), "channel 2 selected");
    CHECK(same(run({0x9F, 0x24, 0x64}, 15), {36100}), "channel 16");
    // A CC (two data bytes) and a program change (one) between notes keep the parser in step.
    CHECK(same(run({0x90, 0x24, 0x64, 0xB0, 0x07, 0x64, 0x90, 0x26, 0x50}), {36100, 38080}), "CC between notes");
    CHECK(same(run({0xC0, 0x05, 0x06, 0x90, 0x24, 0x64}), {36100}), "program change running status, 1 byte each");
    CHECK(same(run({0xD0, 0x40, 0x90, 0x24, 0x64}), {36100}), "channel pressure");
    CHECK(same(run({0xE0, 0x00, 0x40, 0x90, 0x24, 0x64}), {36100}), "pitch bend");
    // CC running status must not turn into notes.
    CHECK(same(run({0xB0, 0x07, 0x64, 0x08, 0x10}), {}), "CC running status makes no notes");
    // Interleaved channels with running status per message.
    CHECK(same(run({0x90, 0x24, 0x64, 0x91, 0x26, 0x50, 0x28, 0x50, 0x90, 0x2A, 0x10}, 0), {36100, 42016}),
          "other-channel notes in between");
}

static void testQueue() {
    MidiInQueue<16> q;
    MidiInNote n{};
    CHECK(!q.pop(n), "empty");
    // A burst of 8 notes (a chord plus hits on the same step) all come out, in order.
    for (uint8_t i = 0; i < 8; i++) CHECK(q.push(MidiInNote{static_cast<uint8_t>(36 + i), static_cast<uint8_t>(10 + i)}), "push %d", i);
    for (uint8_t i = 0; i < 8; i++) {
        CHECK(q.pop(n) && (n.key == 36 + i) && (n.velocity == 10 + i), "pop %d", i);
    }
    CHECK(!q.pop(n), "empty again");
    // Fill: 15 fit (one slot stays empty), the 16th is dropped and counted.
    for (uint8_t i = 0; i < 15; i++) CHECK(q.push(MidiInNote{i, 1}), "fill %d", i);
    CHECK(!q.push(MidiInNote{99, 1}), "full");
    CHECK(q.dropped == 1, "dropped %d", q.dropped);
    CHECK(q.pop(n) && n.key == 0, "oldest first after full");
    CHECK(q.push(MidiInNote{100, 1}), "room again");
    // Wrap-around keeps the order.
    uint8_t expect = 1;
    while (q.pop(n)) {
        const uint8_t want = (expect < 15) ? expect : 100;
        CHECK(n.key == want, "wrap order: got %d want %d", n.key, want);
        expect++;
    }
    CHECK(expect == 16, "count after wrap %d", expect);
    q.push(MidiInNote{1, 1});
    q.clear();
    CHECK(!q.pop(n), "clear");
}

static void testVelocity() {
    CHECK(midiInVelocityTo100(127) == 100, "127");
    CHECK(midiInVelocityTo100(1) == 1, "1 -> %d", midiInVelocityTo100(1));
    CHECK(midiInVelocityTo100(64) == 50, "64 -> %d", midiInVelocityTo100(64));
    CHECK(midiInVelocityTo100(100) == 79, "100 -> %d", midiInVelocityTo100(100));
    for (int v = 1; v < 127; v++) CHECK(midiInVelocityTo100(static_cast<uint8_t>(v)) <= midiInVelocityTo100(static_cast<uint8_t>(v + 1)), "monotonic at %d", v);
    CHECK(midiInVelocityTo100(200) == 100, "clamped");
}

int main() {
    testBasic();
    testVelocityZero();
    testRunningStatus();
    testChannels();
    testQueue();
    testVelocity();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
