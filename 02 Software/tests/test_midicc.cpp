// Host-side tests for the MIDI control change input (Global/MidiCc.h): the CC map, the scaling, the latest-
// value table, presses, the take-over of faders and the window of strips; and the parser's CC events that feed it.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "MidiCc.h"
#include "MidiParser.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                \
    do {                                                                \
        ++checks;                                                       \
        if (!(cond)) {                                                  \
            ++failures;                                                 \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                   \
            std::printf("\n");                                          \
        }                                                               \
    } while (0)

static void testMap() {
    int faders = 0, knobs = 0, mutes = 0, prev = 0, next = 0, none = 0, solos = 0, recs = 0, jogs = 0;
    for (int cc = 0; cc < 128; cc++) {
        const CcControl c = ccDecode(static_cast<uint8_t>(cc));
        switch (c.kind) {
            case CC_FADER: faders++; CHECK(cc == kCcFader[c.strip], "fader cc %d", cc); break;
            case CC_KNOB: knobs++; CHECK(c.strip == cc - 13 && cc >= 13 && cc <= 20, "knob cc %d", cc); break;
            case CC_MUTE: mutes++; CHECK(c.strip == cc - 21 && cc >= 21 && cc <= 28, "mute cc %d", cc); break;
            case CC_TRACK_PREV: prev++; CHECK(cc == 60, "prev cc %d", cc); break;
            case CC_TRACK_NEXT: next++; CHECK(cc == 61, "next cc %d", cc); break;
            case CC_SOLO: solos++; CHECK(cc == kCcSolo[c.strip], "S cc %d", cc); break;
            case CC_REC: recs++; CHECK(c.strip == cc - 38 && cc >= 38 && cc <= 45, "R cc %d", cc); break;
            case CC_JOG: jogs++; CHECK(cc == 86, "jog cc %d", cc); break;
            default: none++; break;
        }
    }
    CHECK(faders == 8 && knobs == 8 && mutes == 8 && prev == 1 && next == 1 && solos == 8 && recs == 8 && jogs == 1 && none == 128 - 43, "counts %d %d %d %d %d %d %d %d %d", faders, knobs, mutes, prev, next, solos, recs, jogs, none);
    // Read from the unit: S 29 30 31 33 to 37, R 38 to 45, the Select buttons 46 to 53 are not used, the wheel is 86.
    CHECK(ccDecode(32).kind == CC_NONE && ccDecode(33).kind == CC_SOLO && ccDecode(33).strip == 3 && ccDecode(37).strip == 7 && ccDecode(38).kind == CC_REC && ccDecode(45).strip == 7 && ccDecode(46).kind == CC_NONE && ccDecode(53).kind == CC_NONE && ccDecode(86).kind == CC_JOG, "numbers read from the unit");
    // The Studio's own numbers: faders 2 3 4 5 6 8 9 12, knobs 13 to 20, M buttons 21 to 28, TRACK 60 and 61.
    const uint8_t faderCc[8] = {2, 3, 4, 5, 6, 8, 9, 12};
    for (uint8_t s = 0; s < 8; s++) CHECK(ccDecode(faderCc[s]).kind == CC_FADER && ccDecode(faderCc[s]).strip == s, "fader %u", s + 1);
    CHECK(ccDecode(13).kind == CC_KNOB && ccDecode(13).strip == 0 && ccDecode(20).strip == 7 && ccDecode(21).kind == CC_MUTE && ccDecode(21).strip == 0 && ccDecode(28).strip == 7, "knobs and M buttons");
    CHECK(ccDecode(60).kind == CC_TRACK_PREV && ccDecode(61).kind == CC_TRACK_NEXT, "TRACK");
    // Common controllers on other numbers stay unmapped: the mod wheel, volume, pan, sustain, all notes off.
    for (uint8_t cc : {1, 7, 10, 64, 120, 123}) CHECK(ccDecode(cc).kind == CC_NONE, "cc %u is not mapped", cc);
    CHECK(kCcMaxWindow == 5 && ccColumn(5, 7) == 12 && ccColumn(0, 0) == 0, "thirteen columns");
}

static void testScaling() {
    CHECK(ccTo100(0) == 0 && ccTo100(127) == 100 && ccTo100(255) == 100, "ends");
    uint8_t previous = 0;
    for (int v = 0; v < 128; v++) {
        const uint8_t s = ccTo100(static_cast<uint8_t>(v));
        CHECK(s >= previous && s <= 100, "monotonic at %d", v);
        previous = s;
    }
    CHECK(ccTo100(64) == 50 || ccTo100(64) == 51, "the middle is about 50 (%u)", ccTo100(64));
    // Ranges: 0..100 onto [min, max] and back, hitting both ends.
    CHECK(ccToRange(0, 0, 80) == 0 && ccToRange(100, 0, 80) == 80 && ccToRange(50, 0, 80) == 40 && ccToRange(100, 10, 30) == 30 && ccToRange(0, 10, 30) == 10, "to range");
    for (uint8_t p = 0; p <= 100; p++) {
        const uint8_t v = ccToRange(p, 0, 80);
        const uint8_t back = ccFromRange(v, 0, 80);
        CHECK(back >= p - 1 && back <= p + 1, "round trip %u -> %u -> %u", p, v, back);
    }
    CHECK(ccFromRange(0, 0, 80) == 0 && ccFromRange(80, 0, 80) == 100 && ccFromRange(200, 0, 80) == 100 && ccFromRange(5, 7, 7) == 0, "from range edges");
}

static void testTable() {
    CcTable t;
    uint8_t v = 99;
    CHECK(!t.poll(20, v) && v == 99, "nothing yet");
    t.set(20, 10);
    t.set(20, 50);
    t.set(20, 70);  // three messages between two polls: only the latest is read
    CHECK(t.poll(20, v) && v == 70, "the latest value (%u)", v);
    CHECK(!t.poll(20, v), "read once");
    t.set(21, 5);
    CHECK(!t.poll(20, v) && t.poll(21, v) && v == 5, "per controller");
    // The sequence counter wraps (256 messages) without losing a message.
    for (int i = 0; i < 300; i++) t.set(22, static_cast<uint8_t>(i & 127));
    CHECK(t.poll(22, v) && v == ((299) & 127), "after a wrap (%u)", v);
    t.set(22, 3);
    CHECK(t.poll(22, v) && v == 3, "and again");
    // Exactly 256 messages between two polls (the old 8-bit sequence would have looked like silence), and 512.
    CcTable w;
    for (int i = 0; i < 256; i++) w.set(23, static_cast<uint8_t>(i & 127));
    CHECK(w.poll(23, v) && v == (255 & 127), "256 messages are seen (%u)", v);
    for (int i = 0; i < 512; i++) w.set(23, static_cast<uint8_t>((i + 1) & 127));
    CHECK(w.poll(23, v) && v == (512 & 127), "512 messages are seen (%u)", v);
    t.set(200, 1);  // out of range: ignored
    // Presses: values of 64 or more; a press and release between two passes still counts.
    CcTable b;
    CHECK(b.takePresses(40) == 0, "no press");
    b.set(40, 127);
    b.set(40, 0);
    CHECK(b.takePresses(40) == 1 && b.takePresses(40) == 0, "one press, even when released at once");
    b.set(40, 127);
    b.set(40, 0);
    b.set(40, 127);
    b.set(40, 0);
    CHECK(b.takePresses(40) == 2, "two presses");
    for (int i = 0; i < 260; i++) {
        b.set(41, 127);
        b.set(41, 0);
    }
    CHECK(b.takePresses(41) == (260 & 255), "the count wraps like a byte (%u)", b.takePresses(41));
}

static void testTakeover() {
    CcTakeover t;
    // The fader is far from the shown value (50): nothing applies until it gets there.
    CHECK(!ccTakeoverStep(t, 10, 50) && !ccTakeoverStep(t, 20, 50) && !ccTakeoverStep(t, 30, 50), "below the value: not applied");
    CHECK(ccTakeoverStep(t, 49, 50) && t.engaged, "within 2: engaged");
    CHECK(ccTakeoverStep(t, 20, 50), "once engaged it follows everywhere, also away from the value");
    ccDisengage(t);
    CHECK(!t.engaged && !t.hasLast, "disengaged");
    // Crossing between two messages (a fast move) engages too.
    CHECK(!ccTakeoverStep(t, 10, 50), "start low");
    CHECK(ccTakeoverStep(t, 90, 50), "jumped across 50: engaged and applied");
    ccDisengage(t);
    // From above, downwards.
    CHECK(!ccTakeoverStep(t, 100, 50) && !ccTakeoverStep(t, 70, 50) && ccTakeoverStep(t, 40, 50), "crossing downwards");
    ccDisengage(t);
    // The first message ever, close to the value: engaged at once. Far: it waits for a second message.
    CHECK(ccTakeoverStep(t, 51, 50), "close at the start");
    ccDisengage(t);
    CHECK(!ccTakeoverStep(t, 100, 50), "far at the start waits (a crossing needs a previous value)");
    // The value changed by another route (the encoder, a loaded song): the fader lets go and has to reach it again.
    ccDisengage(t);
    CHECK(ccTakeoverStep(t, 50, 50), "engaged at 50");
    CHECK(ccTakeoverStep(t, 52, 51), "it follows and sets 52 (the shown value follows it)");
    CHECK(!ccTakeoverStep(t, 53, 10), "the shown value was changed to 10 elsewhere: the fader lets go (53 is far from 10)");
    CHECK(!t.engaged, "disengaged by the outside change");
    CHECK(!ccTakeoverStep(t, 40, 10) && !ccTakeoverStep(t, 25, 10), "it is not applied while it comes down");
    CHECK(ccTakeoverStep(t, 9, 10), "reaching the new value engages again");
    // Rounding between the controller's scale and a parameter's range is not an outside change.
    ccDisengage(t);
    ccTakeoverStep(t, 50, 50);
    CHECK(ccTakeoverStep(t, 60, 62) && ccTakeoverStep(t, 70, 71) && ccTakeoverStep(t, 20, 18), "a shown value within the tolerance of what was set stays engaged");
    // Every combination of start, current and a monotone sweep engages by the time the sweep reaches the current value.
    for (int current = 0; current <= 100; current += 5) {
        for (int start = 0; start <= 100; start += 7) {
            for (int dir : {-1, 1}) {
                CcTakeover s;
                ccDisengage(s);
                bool applied = false;
                int at = start;
                bool reached = false;
                for (int step = 0; step < 300 && !reached; step++) {
                    applied = ccTakeoverStep(s, static_cast<uint8_t>(at), static_cast<uint8_t>(current)) || applied;
                    if (at == current) reached = true;
                    at += dir;
                    if (at < 0 || at > 100) break;
                }
                if (reached) CHECK(applied, "sweep from %d (%d) reached %d without engaging", start, dir, current);
            }
        }
    }
}

static void testRelativeAndHeld() {
    CcTable t;
    CHECK(t.takeRelative(47) == 0, "nothing");
    for (int i = 0; i < 3; i++) t.set(47, 1);   // three clicks one way
    t.set(47, 127);                              // one the other
    CHECK(t.takeRelative(47) == 2 && t.takeRelative(47) == 0, "3 up, 1 down = +2, then nothing");
    t.set(47, 127);
    t.set(47, 126);
    t.set(47, 65);
    CHECK(t.takeRelative(47) == -3, "any value from 65 to 127 is one click down");
    t.set(47, 64);  // the middle: neither
    t.set(47, 0);
    CHECK(t.takeRelative(47) == 0, "0 and 64 are no click");
    for (int i = 0; i < 300; i++) t.set(47, 1);
    CHECK(t.takeRelative(47) == 300, "300 clicks between two passes are all counted");
    for (int i = 0; i < 256; i++) t.set(47, 1);
    CHECK(t.takeRelative(47) == 256, "exactly 256 clicks are not lost");
    // A held button: its last value is 64 or more.
    CcTable b;
    CHECK(!b.held(29), "not held at the start");
    b.set(29, 127);
    CHECK(b.held(29), "held");
    b.set(29, 0);
    CHECK(!b.held(29), "released");
}

static void testFeedbackPad() {
    uint8_t index = 0;
    float boost = 0.0f;
    ccFeedbackFromPad(0, 80, index, boost);
    CHECK(index == 0 && boost == 0.0f, "bottom: no feedback");
    ccFeedbackFromPad(40, 80, index, boost);
    CHECK(index == 40 && boost == 0.0f, "half way: index 40 (%u)", index);
    ccFeedbackFromPad(80, 80, index, boost);
    CHECK(index == 80 && boost == 0.0f, "80 %% of the travel is the normal maximum");
    ccFeedbackFromPad(90, 80, index, boost);
    CHECK(index == 80 && std::fabs(boost - 0.175f) < 1e-5f, "90 %%: half the boost (%f)", boost);
    ccFeedbackFromPad(100, 80, index, boost);
    CHECK(index == 80 && std::fabs(boost - kCcFeedbackBoost) < 1e-5f, "the top: the whole boost");
    ccFeedbackFromPad(255, 80, index, boost);
    CHECK(index == 80 && boost <= kCcFeedbackBoost, "above 100 is 100");
    // Monotonic: the total loop gain (index/100 + boost) never goes down as the pad goes up, and passes 1.0 only above the top fifth.
    float previous = -1.0f;
    for (int p = 0; p <= 100; p++) {
        ccFeedbackFromPad(static_cast<uint8_t>(p), 80, index, boost);
        const float gain = index / 100.0f + boost;
        CHECK(gain >= previous, "monotonic at %d", p);
        CHECK(gain <= 0.8f + kCcFeedbackBoost + 1e-5f, "the loop gain never passes %f (%f)", 0.8f + kCcFeedbackBoost, gain);
        if (p <= 80) CHECK(boost == 0.0f, "no boost up to 80");
        previous = gain;
    }
    CHECK(0.8f + kCcFeedbackBoost > 1.0f, "the top of the pad is past 100 %% loop gain");
}

static void testWindow() {
    CHECK(ccMoveWindow(0, -1) == 0 && ccMoveWindow(0, 1) == 1 && ccMoveWindow(4, 1) == 5 && ccMoveWindow(5, 1) == 5 && ccMoveWindow(2, -1) == 1 && ccMoveWindow(0, 100) == 5, "the window slides and stops at its ends");
    // The strips cover layers 1-8 at 0, and columns 5-12 (layers 5-10, ECHO, VERB) at 4.
    CHECK(ccColumn(0, 7) == 7 && ccColumn(4, 0) == 4 && ccColumn(5, 7) == 12, "columns");
}

// The parser delivers the controller's messages: CC on channel 3, running status, interleaved clock bytes.
static void testParserFeed() {
    MidiParser p;
    MidiEvent e;
    CcTable table;
    auto feed = [&](std::initializer_list<uint8_t> bytes) {
        for (uint8_t b : bytes) {
            if (midiParse(p, b, e) && (e.type == MIDI_EVENT_CC)) table.set(e.data1, e.data2);  // any channel
        }
    };
    feed({0xB0, 2, 64, 2, 66, 0xF8, 2, 70});  // fader 1 on channel 1 in running status, a clock byte in between
    uint8_t v = 0;
    CHECK(table.poll(2, v) && v == 70, "fader 1: latest value %u", v);
    feed({0xB5, 3, 99});  // any channel counts
    CHECK(table.poll(3, v) && v == 99, "other channels work too");
    feed({0xB0, 21, 127, 21, 0});
    CHECK(table.takePresses(21) == 1, "a button press through the parser");
    feed({0x90, 36, 100});  // a note does not disturb it
    CHECK(!table.poll(36, v), "notes are not controllers");
}

int main() {
    testMap();
    testScaling();
    testTable();
    testTakeover();
    testRelativeAndHeld();
    testFeedbackPad();
    testWindow();
    testParserFeed();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
