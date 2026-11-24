// Host-side tests for the LFOs on the filters (Global/LfoMod.h): the frequency sweep, the resonance offset,
// the hysteresis and the song block.

#include <string.h>
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "LfoMod.h"

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

static void testFreq() {
    // The middle of the LFO and a depth of 0 leave the cutoff alone.
    CHECK(lfoModFreq(1000.0f, 0.5f, 1.0f, 10.0f, 20000.0f) == 1000.0f, "d = 0.5");
    CHECK(lfoModFreq(1000.0f, 0.0f, 0.0f, 10.0f, 20000.0f) == 1000.0f && lfoModFreq(1000.0f, 1.0f, 0.0f, 10.0f, 20000.0f) == 1000.0f, "depth 0");
    // Full depth: +-3 octaves, then the range.
    CHECK(std::fabs(lfoModFreq(1000.0f, 1.0f, 1.0f, 10.0f, 20000.0f) - 8000.0f) < 1.0f, "full swing up is +3 octaves = 8000 Hz");
    CHECK(std::fabs(lfoModFreq(1000.0f, 0.0f, 1.0f, 10.0f, 20000.0f) - 125.0f) < 0.01f, "full swing down is -3 octaves = 125 Hz");
    CHECK(std::fabs(lfoModFreq(1000.0f, 1.0f, 0.5f, 10.0f, 20000.0f) - 2828.4271f) < 0.1f, "half depth: +1.5 octaves");
    // The range clamps.
    CHECK(lfoModFreq(5000.0f, 1.0f, 1.0f, 10.0f, 20000.0f) == 20000.0f && lfoModFreq(50.0f, 0.0f, 1.0f, 40.0f, 20000.0f) == 40.0f, "clamped to the filter's range");
    CHECK(lfoModFreq(0.0f, 0.7f, 1.0f, 10.0f, 20000.0f) == 10.0f, "a bad base falls to the minimum");
    // Monotonic in d and symmetric in octaves.
    float previous = 0.0f;
    for (int i = 0; i <= 100; i++) {
        const float hz = lfoModFreq(1000.0f, i / 100.0f, 0.8f, 10.0f, 20000.0f);
        CHECK(hz > previous, "monotonic at %d", i);
        previous = hz;
        const float up = lfoModFreq(1000.0f, 0.5f + i / 200.0f, 0.8f, 1.0f, 1e9f), down = lfoModFreq(1000.0f, 0.5f - i / 200.0f, 0.8f, 1.0f, 1e9f);
        CHECK(std::fabs(up * down - 1000.0f * 1000.0f) < 1.0f, "symmetric in octaves at %d: %f %f", i, up, down);
    }
}

static void testIndex() {
    CHECK(lfoModIndex(50, 0, 100, 0.5f, 1.0f) == 50 && lfoModIndex(50, 0, 100, 1.0f, 0.0f) == 50, "centre and depth 0");
    CHECK(lfoModIndex(50, 0, 100, 1.0f, 1.0f) == 100 && lfoModIndex(50, 0, 100, 0.0f, 1.0f) == 0, "full depth reaches the ends from the middle");
    CHECK(lfoModIndex(80, 0, 100, 1.0f, 1.0f) == 100 && lfoModIndex(20, 0, 100, 0.0f, 1.0f) == 0, "clamped");
    CHECK(lfoModIndex(10, 0, 100, 1.0f, 0.5f) == 35, "half depth: +25 (%d)", lfoModIndex(10, 0, 100, 1.0f, 0.5f));
}

static void testHysteresis() {
    CHECK(!lfoFreqMoved(1000.0f, 1000.0f) && !lfoFreqMoved(1000.0f, 1001.0f) && lfoFreqMoved(1000.0f, 1002.0f) && lfoFreqMoved(1000.0f, 998.0f), "more than 1/512 octave (about 0.14 %%)");
    CHECK(lfoFreqMoved(0.0f, 500.0f) && lfoFreqMoved(500.0f, 0.0f), "no last value: always");
    // A still LFO costs nothing; a sine of 3200 steps follows the sweep nearly step by step (never more than one design per step).
    float last = 0.0f;
    int still = 0, moving = 0;
    for (int i = 0; i <= 3200; i++) {
        const float hz = lfoModFreq(1000.0f, 0.5f, 1.0f, 10.0f, 20000.0f);
        if (lfoFreqMoved(last, hz)) {
            last = hz;
            still++;
        }
    }
    last = 0.0f;
    for (int i = 0; i <= 3200; i++) {
        const float hz = lfoModFreq(1000.0f, 0.5f + 0.5f * std::sin(i * 6.2831853f / 3200.0f), 1.0f, 10.0f, 20000.0f);
        if (lfoFreqMoved(last, hz)) {
            last = hz;
            moving++;
        }
    }
    CHECK(still == 1, "a still LFO designs once (%d)", still);
    CHECK(moving > 2000 && moving <= 3201, "a sine LFO is followed almost step by step (%d designs of 3201 steps)", moving);
    // The designs of a slow sweep never jump by more than 1/48 octave from one to the next (nothing audible steps).
    float lastHz = lfoModFreq(1000.0f, 0.5f, 1.0f, 10.0f, 20000.0f), worst = 0.0f;
    for (int i = 0; i <= 3200; i++) {
        const float hz = lfoModFreq(1000.0f, 0.5f + 0.5f * std::sin(i * 6.2831853f / 3200.0f), 1.0f, 10.0f, 20000.0f);
        worst = std::fmax(worst, std::fabs(std::log2(hz / lastHz)));
        lastHz = hz;
    }
    CHECK(worst < 1.0f / 48.0f, "the biggest step between two steps of the LFO is %f octave", worst);
    // A bad LFO value is no modulation (also under -ffast-math: tested in the bits).
    float nan = std::nanf(""), inf = INFINITY;
    CHECK(lfoModFreq(1000.0f, nan, 1.0f, 10.0f, 20000.0f) == 1000.0f && lfoModFreq(1000.0f, 0.9f, inf, 10.0f, 20000.0f) == 1000.0f && lfoModIndex(40, 0, 100, nan, 1.0f) == 40, "NaN and infinity: no modulation");
    CHECK(lfoFreqMoved(nan, 1000.0f) && lfoFreqMoved(1000.0f, inf), "a bad frequency always designs");
}

static void testBlock() {
    uint8_t block[kLfoModFileSize];
    const uint8_t assign[kLfoModDests] = {1, 0, 10, 4};
    lfoModWriteBlock(assign, block);
    uint8_t back[kLfoModDests] = {9, 9, 9, 9};
    CHECK(lfoModReadBlock(back, block, kLfoModFileSize) && back[0] == 1 && back[1] == 0 && back[2] == 10 && back[3] == 4, "round trip");
    const uint8_t wild[kLfoModDests] = {11, 200, 3, 0};
    lfoModWriteBlock(wild, block);
    CHECK(block[4] == 0 && block[5] == 0 && block[6] == 3, "written in range");
    block[4] = 99;
    CHECK(lfoModReadBlock(back, block, kLfoModFileSize) && back[0] == 0, "an LFO number out of range reads as none");
    uint8_t keep[kLfoModDests] = {7, 7, 7, 7};
    lfoModWriteBlock(assign, block);
    block[0] = 'X';
    CHECK(!lfoModReadBlock(keep, block, kLfoModFileSize) && keep[0] == 7, "wrong marker");
    lfoModWriteBlock(assign, block);
    block[3] = 2;
    CHECK(!lfoModReadBlock(keep, block, kLfoModFileSize) && keep[0] == 7, "unknown version");
    lfoModWriteBlock(assign, block);
    CHECK(!lfoModReadBlock(keep, block, kLfoModFileSize - 1) && !lfoModReadBlock(keep, block, 0) && keep[0] == 7, "short or no block");
}

int main() {
    testFreq();
    testIndex();
    testHysteresis();
    testBlock();
    {  // the label of a frequency the LFO moved: the format of the set values
        char t[16];
        CHECK(!lfoFreqLabel(440.4f, t, sizeof t) && strcmp(t, "       440") == 0, "440 Hz: '%s'", t);
        CHECK(!lfoFreqLabel(10.0f, t, sizeof t) && strcmp(t, "        10") == 0, "10 Hz");
        CHECK(!lfoFreqLabel(999.4f, t, sizeof t) && strcmp(t, "       999") == 0, "999 Hz");
        CHECK(lfoFreqLabel(999.5f, t, sizeof t) && strcmp(t, "       1.0") == 0, "1.0 kHz: '%s'", t);
        CHECK(lfoFreqLabel(12345.0f, t, sizeof t) && strcmp(t, "      12.3") == 0, "12.3 kHz: '%s'", t);
        CHECK(!lfoFreqLabel(-5.0f, t, sizeof t) && strcmp(t, "         0") == 0, "negative is 0");
        CHECK(lfoFreqLabelKey(440.4f) == lfoFreqLabelKey(440.2f) && lfoFreqLabelKey(440.4f) != lfoFreqLabelKey(441.0f), "key follows the shown Hz");
        CHECK(lfoFreqLabelKey(5000.0f) == lfoFreqLabelKey(5030.0f) && lfoFreqLabelKey(5000.0f) != lfoFreqLabelKey(5100.0f) && lfoFreqLabelKey(999.0f) != lfoFreqLabelKey(1000.0f), "key follows the shown kHz");
    }
    {   // The counter of an LFO: always an entry of the table, whatever moment it is read at, and a PHASE change wraps it into [0, 3200).
        CHECK(lfoTableIndex(0.0f) == 0 && lfoTableIndex(1599.7f) == 1599 && lfoTableIndex(3199.9f) == 3199, "index of the counter");
        CHECK(lfoTableIndex(3200.0f) == 3199 && lfoTableIndex(99999.0f) == 3199, "at the size: the last entry");
        CHECK(lfoTableIndex(-0.4f) == 0 && lfoTableIndex(-5000.0f) == 0, "below zero: the first entry, not 65535");
        uint32_t bits = 0x7FC00000u;
        float nan;
        memcpy(&nan, &bits, 4);
        bits = 0x7F800000u;
        float inf;
        memcpy(&inf, &bits, 4);
        CHECK(lfoTableIndex(nan) == 0 && lfoTableIndex(inf) == 0, "not a number: the first entry");
        // The old wrap ("greater than size - 1") turned 3199.6 into -0.4: now it is a counter inside the table.
        const float c = lfoShiftCounter(2399.6f, 800);
        CHECK(c >= 0.0f && c < 3200.0f, "2399.6 + 800 = %f is inside", c);
        for (float start = 0.0f; start < 3200.0f; start += 0.7f) {
            for (int32_t shift : {-3200 + 1, -1600, -800, -1, 0, 1, 800, 1600, 3199}) {
                const float r = lfoShiftCounter(start, shift);
                CHECK(r >= 0.0f && r < 3200.0f && lfoTableIndex(r) < 3200, "counter %f shifted by %d gives %f", static_cast<double>(start), static_cast<int>(shift), static_cast<double>(r));
            }
        }
        CHECK(std::fabs(lfoShiftCounter(100.0f, 800) - 900.0f) < 1e-3f && std::fabs(lfoShiftCounter(100.0f, -800) - 2500.0f) < 1e-3f && lfoShiftCounter(5.5f, 0) == 5.5f, "ordinary shifts");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
