// Host-side tests for the MIDI CC map (MidiCcMap.h): lookup, bank resolution, scaling, the
// relative jog, and pickup.

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include "MidiCcMap.h"

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

static void testLookup() {
    CcInput in = ccLookup(20);
    CHECK(in.control == CCC_FADER && in.strip == 0, "fader 1");
    in = ccLookup(27);
    CHECK(in.control == CCC_FADER && in.strip == 7, "fader 8");
    in = ccLookup(28);
    CHECK(in.control == CCC_NONE, "28 unused");
    in = ccLookup(35);
    CHECK(in.control == CCC_KNOB && in.strip == 5, "knob 6");
    CHECK(ccLookup(43).control == CCC_MUTE && ccLookup(43).strip == 3, "mute 4");
    CHECK(ccLookup(57).control == CCC_SOLO && ccLookup(57).strip == 7, "solo 8");
    CHECK(ccLookup(60).control == CCC_REC, "rec 1");
    CHECK(ccLookup(77).control == CCC_SELECT, "select 8");
    CHECK(ccLookup(80).control == CCC_PLAY, "play");
    CHECK(ccLookup(81).control == CCC_STOP, "stop");
    CHECK(ccLookup(82).control == CCC_RECORD, "record");
    CHECK(ccLookup(85).control == CCC_CYCLE, "cycle");
    CHECK(ccLookup(86).control == CCC_TRACK_PREV && ccLookup(87).control == CCC_TRACK_NEXT, "track");
    CHECK(ccLookup(91).control == CCC_JOG, "jog");
    CHECK(ccLookup(92).control == CCC_NONE && ccLookup(0).control == CCC_NONE && ccLookup(127).control == CCC_NONE, "outside the map");

    // Every absolute control has a unique index 0-15 and maps back.
    bool seen[16] = {false};
    for (int cc = 0; cc < 128; cc++) {
        const CcInput c = ccLookup(static_cast<uint8_t>(cc));
        if (!ccIsAbsolute(c.control)) continue;
        const uint8_t i = ccAbsoluteIndex(c);
        CHECK(i < 16 && !seen[i], "index %d of cc %d", i, cc);
        if (i < 16) seen[i] = true;
        const CcInput back = ccAbsoluteInput(i);
        CHECK(back.control == c.control && back.strip == c.strip, "round trip cc %d", cc);
    }
    for (bool s : seen) CHECK(s, "all 16 absolute controls");
    CHECK(!ccIsAbsolute(CCC_MUTE) && !ccIsAbsolute(CCC_JOG), "buttons and jog are queued");
}

static void testResolve() {
    // Bank A: layers 1-8.
    for (uint8_t s = 0; s < 8; s++) {
        CHECK(ccResolve(0, CCC_FADER, s) == (CcAction{CCT_LAYER_LEVEL, s, 0}), "A fader %d", s);
        CHECK(ccResolve(0, CCC_KNOB, s) == (CcAction{CCT_LAYER_END, s, 0}), "A knob %d", s);
        CHECK(ccResolve(0, CCC_MUTE, s) == (CcAction{CCT_LAYER_MUTE, s, 0}), "A mute %d", s);
        CHECK(ccResolve(0, CCC_SOLO, s) == (CcAction{CCT_LAYER_SOLO, s, 0}), "A solo %d", s);
        CHECK(ccResolve(0, CCC_REC, s).target == CCT_NONE && ccResolve(0, CCC_SELECT, s).target == CCT_NONE, "throws reserved");
    }
    // Bank B: layers 9-10 on strips 1-2.
    CHECK(ccResolve(1, CCC_FADER, 0) == (CcAction{CCT_LAYER_LEVEL, 8, 0}), "B fader 1 = layer 9");
    CHECK(ccResolve(1, CCC_MUTE, 1) == (CcAction{CCT_LAYER_MUTE, 9, 0}), "B mute 2 = layer 10");
    CHECK(ccResolve(1, CCC_FADER, 2).target == CCT_NONE && ccResolve(1, CCC_MUTE, 4).target == CCT_NONE, "B strip 3 and desk buttons unused");
    // The desk.
    CHECK(ccResolve(1, CCC_FADER, 4) == (CcAction{CCT_EFFECT_PARAM, 0, CCP_C}), "feedback");
    CHECK(ccResolve(1, CCC_KNOB, 4) == (CcAction{CCT_EFFECT_PARAM, 0, CCP_A}), "time");
    CHECK(ccResolve(1, CCC_FADER, 5) == (CcAction{CCT_EFFECT_PARAM, 0, CCP_F}), "echo wet");
    CHECK(ccResolve(1, CCC_FADER, 6) == (CcAction{CCT_REVERB_WET, 0, 0}), "reverb wet");
    CHECK(ccResolve(1, CCC_FADER, 7) == (CcAction{CCT_FILTER_FREQ, 0, 0}), "dub filter");
    CHECK(ccResolve(1, CCC_KNOB, 7) == (CcAction{CCT_FILTER_RES, 0, 0}), "resonance");
    CHECK(ccResolve(1, CCC_FADER, 3) == (CcAction{CCT_EFFECT_PARAM, 1, CCP_F}), "effect 2 wet");
    // Out of range.
    CHECK(ccResolve(2, CCC_FADER, 0).target == CCT_NONE && ccResolve(0, CCC_FADER, 8).target == CCT_NONE, "out of range");
}

static void testScaling() {
    CHECK(ccToRange(0, 0, 100) == 0 && ccToRange(127, 0, 100) == 100, "ends 0-100");
    CHECK(ccToRange(64, 0, 100) == 50, "middle: %d", ccToRange(64, 0, 100));
    CHECK(ccToRange(127, 0, 5) == 5 && ccToRange(0, 0, 5) == 0, "ends 0-5");
    CHECK(ccToRange(127, 0, 199) == 199, "filter top");
    CHECK(ccToRange(200, 0, 80) == 80, "clamped input");
    CHECK(ccToRange(50, 10, 10) == 10, "empty range");
    // Monotonic and every step of a small range reachable.
    for (int max : {5, 80, 100, 199}) {
        int prev = -1;
        bool reach[256] = {false};
        for (int v = 0; v < 128; v++) {
            const int r = ccToRange(static_cast<uint8_t>(v), 0, static_cast<uint8_t>(max));
            CHECK(r >= prev, "monotonic max %d at %d", max, v);
            prev = r;
            reach[r] = true;
        }
        if (max <= 127)
            for (int r = 0; r <= max; r++) CHECK(reach[r], "max %d: %d reachable", max, r);
    }
    // rangeToCc is the inverse within one step.
    for (int max : {5, 80, 100, 199}) {
        for (int v = 0; v <= max; v++) {
            const uint8_t c = rangeToCc(static_cast<uint8_t>(v), 0, static_cast<uint8_t>(max));
            const int back = ccToRange(c, 0, static_cast<uint8_t>(max));
            CHECK(std::abs(back - v) <= ((max > 127) ? 1 : 0), "inverse max %d v %d -> cc %d -> %d", max, v, c, back);
        }
    }
    CHECK(rangeToCc(0, 0, 100) == 0 && rangeToCc(100, 0, 100) == 127, "inverse ends");
}

static void testRelative() {
    CHECK(ccRelative(1) == 1 && ccRelative(3) == 3 && ccRelative(63) == 63, "up");
    CHECK(ccRelative(127) == -1 && ccRelative(125) == -3 && ccRelative(65) == -63, "down");
    CHECK(ccRelative(0) == 0 && ccRelative(64) == 0, "none");
    CHECK(ccStepRange(198, 5, 0, 199) == 199 && ccStepRange(2, -5, 0, 199) == 0 && ccStepRange(50, -2, 0, 199) == 48, "clamped steps");
    CHECK(ccButtonOn(127) && ccButtonOn(64) && !ccButtonOn(0) && !ccButtonOn(63), "buttons");
}

static void testPickup() {
    // Far from the current value: nothing until the control reaches it.
    CcPickup p;
    CHECK(!ccPickupStep(p, 10, 80), "far below");
    CHECK(!ccPickupStep(p, 40, 80), "still below");
    CHECK(ccPickupStep(p, 90, 80), "crossed upward");
    ccPickupObserved(p, 90);
    CHECK(ccPickupStep(p, 70, 90), "engaged follows anywhere");
    ccPickupObserved(p, 70);

    // Landing near the value engages at once.
    p.reset();
    CHECK(ccPickupStep(p, 62, 64), "within the window");

    // A first message far away never engages, even if it is the only one.
    p.reset();
    CHECK(!ccPickupStep(p, 127, 0), "first message far");
    CHECK(!ccPickupStep(p, 120, 0), "moving but not across");
    CHECK(ccPickupStep(p, 2, 0), "reached the bottom");
    ccPickupObserved(p, 2);

    // Crossing downward.
    p.reset();
    CHECK(!ccPickupStep(p, 100, 50), "above");
    CHECK(ccPickupStep(p, 30, 50), "crossed downward");
    ccPickupObserved(p, 30);

    // The target changes elsewhere (encoder, song load): let go and catch again.
    CHECK(!ccPickupStep(p, 31, 100), "target moved away: released");
    CHECK(!ccPickupStep(p, 60, 100), "still released");
    CHECK(ccPickupStep(p, 105, 100), "caught again");
    ccPickupObserved(p, 105);

    // A dropped update (the target was busy) keeps the target where it was: observed records it,
    // so the next message does not count as an outside change.
    p.reset();
    CHECK(ccPickupStep(p, 50, 50), "engaged");
    ccPickupObserved(p, 40);  // the set was refused, value stayed 40
    CHECK(ccPickupStep(p, 52, 40), "still engaged after a refused set");
    ccPickupObserved(p, 52);

    // Quantized targets: the observed value after a set is what counts, not the raw CC.
    p.reset();
    CHECK(ccPickupStep(p, 64, 63), "delay time near");
    ccPickupObserved(p, rangeToCc(ccToRange(64, 0, 5), 0, 5));
    const uint8_t cur = rangeToCc(ccToRange(64, 0, 5), 0, 5);
    CHECK(ccPickupStep(p, 70, cur), "stays engaged on a coarse target");
}

int main() {
    testLookup();
    testResolve();
    testScaling();
    testRelative();
    testPickup();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
