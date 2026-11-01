// Host-side tests for Euclidean rhythms (Core/Library/Global/Euclid.h).
//
// 1. Known rhythms from Toussaint (2005), e.g. E(3,8) = x..x..x.
// 2. Every steps/hits/rotate: hit count, maximal evenness (gaps differ by at most one step),
//    first hit on step 0 before rotation, rotation is a cyclic shift.
// 3. Laying out on a bank: REPLACE gives exactly the pattern's beats; MERGE keeps the base
//    beats and their fills; full banks report the dropped hits.

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <random>

#include "Euclid.h"

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

static void testKnown() {
    struct {
        uint8_t hits, steps;
        const char* text;
    } known[] = {
        {1, 4, "x..."},     {2, 5, "x.x.."},       {3, 4, "x.xx"},          {3, 5, "x.x.x"},
        {3, 7, "x.x.x.."},  {3, 8, "x..x..x."},    {4, 7, "x.x.x.x"},       {4, 9, "x.x.x.x.."},
        {4, 11, "x..x..x..x."}, {5, 6, "x.xxxx"},  {5, 7, "x.xx.xx"},       {5, 8, "x.xx.xx."},
        {5, 9, "x.x.x.x.x"},    {5, 11, "x.x.x.x.x.."}, {5, 12, "x..x.x..x.x."},
        {5, 16, "x..x..x..x..x..."}, {7, 8, "x.xxxxxx"}, {7, 12, "x.xx.x.xx.x."},
        {4, 16, "x...x...x...x..."}, {16, 16, "xxxxxxxxxxxxxxxx"}, {0, 8, "........"},
    };
    char text[kEuclidMaxSteps + 1];
    for (const auto& k : known) {
        euclidText(euclidPattern(k.steps, k.hits, 0), k.steps, text);
        CHECK(!strcmp(text, k.text), "E(%u,%u) = %s, expected %s", k.hits, k.steps, text, k.text);
    }
    euclidText(euclidPattern(8, 3, 1), 8, text);
    CHECK(!strcmp(text, ".x..x..x"), "E(3,8) rotated 1 = %s", text);
    euclidText(euclidPattern(8, 3, 9), 8, text);
    CHECK(!strcmp(text, ".x..x..x"), "rotation wraps: %s", text);
    CHECK(euclidPattern(4, 9, 0) == 0xF, "more hits than steps fills every step");
    CHECK(euclidPattern(0, 3, 0) == 0 && euclidPattern(33, 3, 0) == 0, "invalid step counts give no hits");
    CHECK(euclidPattern(32, 32, 0) == 0xFFFFFFFFUL, "32 steps all hit");
}

static void testProperties() {
    for (uint8_t steps = 1; steps <= kEuclidMaxSteps; steps++) {
        for (uint8_t hits = 0; hits <= steps; hits++) {
            const uint32_t p = euclidPattern(steps, hits, 0);
            CHECK((steps == 32) || !(p >> steps), "E(%u,%u) has bits past the last step", hits, steps);
            CHECK(static_cast<uint8_t>(__builtin_popcount(p)) == hits, "E(%u,%u) has %d hits", hits, steps, __builtin_popcount(p));
            if (hits == 0) continue;
            CHECK(p & 1, "E(%u,%u) starts with a hit", hits, steps);
            // Gaps between consecutive hits (cyclic) take at most two neighbouring values.
            int minGap = 99, maxGap = 0, prev = -1, first = -1;
            for (int i = 0; i < steps; i++) {
                if (!(p & (1UL << i))) continue;
                if (prev >= 0) {
                    minGap = std::min(minGap, i - prev);
                    maxGap = std::max(maxGap, i - prev);
                } else {
                    first = i;
                }
                prev = i;
            }
            const int wrap = first + steps - prev;
            minGap = std::min(minGap, wrap);
            maxGap = std::max(maxGap, wrap);
            CHECK(maxGap - minGap <= 1, "E(%u,%u) gaps %d..%d not even", hits, steps, minGap, maxGap);
            for (uint8_t r = 0; r < steps; r++) {
                const uint32_t q = euclidPattern(steps, hits, r);
                for (uint8_t i = 0; i < steps; i++) {
                    const bool a = p & (1UL << i), b = q & (1UL << ((i + r) % steps));
                    if (a != b) {
                        CHECK(false, "E(%u,%u) rotate %u is not a cyclic shift", hits, steps, r);
                        break;
                    }
                }
            }
        }
    }
}

static int beatsAt(const Bank& b, uint16_t start) { return beatListFind(b, start) != -1; }

static void testApply() {
    // REPLACE: E(3,8) on 1/16 (200) in a 2-bar 4/4 song (6400): two cycles of 1600.
    Bank base, out;
    beatListAdd(base, 100, 6400);
    beatListAdd(base, 600, 6400);
    beatListRebuild(base, 6400);
    base.setBeatFill(1, 1, 2, 0, 0);  // 600: WAVE_C
    const uint32_t e38 = euclidPattern(8, 3, 0);
    CHECK(euclidApply(out, base, false, e38, 8, 200, 6400) == 0, "fits");
    CHECK(out.lastActiveBeatNum == 11, "3 hits x 4 cycles = 12 beats, got %d", out.lastActiveBeatNum + 1);
    for (uint16_t c = 0; c < 6400; c += 1600) {
        CHECK(beatsAt(out, c) && beatsAt(out, c + 600) && beatsAt(out, c + 1200), "hits at cycle %u", c);
    }
    CHECK(!beatsAt(out, 100), "REPLACE drops the old beats");

    // MERGE keeps the old beats and their fills; a hit on an old beat keeps that beat.
    CHECK(euclidApply(out, base, true, e38, 8, 200, 6400) == 0, "fits");
    CHECK(out.lastActiveBeatNum == 12, "12 hits + 1 extra old beat, got %d", out.lastActiveBeatNum + 1);
    CHECK(beatsAt(out, 100), "MERGE keeps beat at 100");
    const int16_t i600 = beatListFind(out, 600);
    CHECK(i600 != -1 && out.beatLibrary[i600].getFillType() == 1 && out.beatLibrary[i600].getFillPattern() == 2, "MERGE keeps the fill of the beat a hit landed on");

    // Pattern longer than the song: cut at the song end.
    CHECK(euclidApply(out, base, false, euclidPattern(32, 32, 0), 32, 3200, 6400) == 0, "fits");
    CHECK(out.lastActiveBeatNum == 1, "only steps 0 and 3200 are inside a 6400 song, got %d", out.lastActiveBeatNum + 1);

    // Too many hits: the bank fills up and the rest are counted.
    const uint16_t dropped = euclidApply(out, base, false, euclidPattern(16, 16, 0), 16, 50, 6400);
    CHECK(out.lastActiveBeatNum == kBeatLibrarySize - 1 && dropped == 128 - kBeatLibrarySize, "full bank: %d beats, %u dropped", out.lastActiveBeatNum + 1, dropped);

    // Zero hits in REPLACE empties the layer; in MERGE leaves the base.
    euclidApply(out, base, false, 0, 8, 200, 6400);
    CHECK(out.lastActiveBeatNum == -1, "no hits, no beats");
    euclidApply(out, base, true, 0, 8, 200, 6400);
    CHECK(out.lastActiveBeatNum == 1 && beatsAt(out, 100) && beatsAt(out, 600), "no hits keeps the base");

    // Random settings: the result always satisfies the sequencer's bank rules.
    std::mt19937 rng(7);
    const uint16_t grids[] = {50, 100, 200, 400, 800, 1600, 3200};
    for (int n = 0; n < 20000; n++) {
        const uint16_t song = static_cast<uint16_t>(800 * (1 + rng() % 8) * (1 + rng() % 8));
        const uint8_t steps = static_cast<uint8_t>(1 + rng() % 32);
        const uint32_t p = euclidPattern(steps, static_cast<uint8_t>(rng() % (steps + 1)), static_cast<uint8_t>(rng() % steps));
        const uint16_t grid = grids[rng() % 7];
        const bool merge = rng() % 2;
        euclidApply(out, base, merge, p, steps, grid, song);
        bool sorted = true;
        for (int16_t i = 0; i <= out.lastActiveBeatNum; i++) {
            const Beat& b = out.beatLibrary[i];
            const uint16_t end = (i < out.lastActiveBeatNum) ? out.beatLibrary[i + 1].getStartInterval() : song;
            if (!b.getActive() || b.getStartInterval() >= song || b.getEndInterval() != end || (i && out.beatLibrary[i - 1].getStartInterval() >= b.getStartInterval())) sorted = false;
        }
        CHECK(sorted, "bank rules broken: steps %u grid %u song %u merge %d", steps, grid, song, merge);
        for (int16_t i = out.lastActiveBeatNum + 1; i < static_cast<int16_t>(kBeatLibrarySize); i++) {
            if (out.beatLibrary[i].getActive()) {
                CHECK(false, "active beat past the end");
                break;
            }
        }
    }
}

int main() {
    testKnown();
    testProperties();
    testApply();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
