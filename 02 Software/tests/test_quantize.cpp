// Host-side tests for the quantization grids (Global/Quantize.h, backlog 59).
//
// 1. The seven straight grids of V1.17 and before give exactly the old result (the formula of
//    Controller::layerSong_quantizeInterval), for every interval, so an old song and an old habit are untouched.
// 2. Every grid: a snapped hit is a grid point and is no further than half a step away; snapping twice changes
//    nothing; the snap never moves backwards (monotonic); the end of the song wraps to the start.
// 3. Triplets: 1:8T has exactly three points per beat (0, 267, 533 intervals); the points are the rounded k*p/3.
// 4. Mixed grids pick the nearer of the two grids, a tie the straight one; OFF leaves every interval alone.
// 5. Strength: 100 snaps, 0 leaves the hit, 50 goes halfway, never past the grid point, never outside the song.
// 6. The names fit the 10-character field and only use characters the firmware's font has.
// 7. quantizePointsIn counts the points the EUCLID page walks; the edit step is the grid's own.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

#include "Quantize.h"
#include "fakelcd.h"  // glyphIndex: the font's character map

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

// The formula of Controller::layerSong_quantizeInterval before this module.
static uint16_t oldSnap(uint16_t interval_, uint8_t grid_, uint32_t songInterval_) {
    static const uint16_t q[] = {50, 100, 200, 400, 800, 1600, 3200};
    const uint16_t quantizeInterval = q[grid_];
    uint16_t startInterval = ((interval_ / quantizeInterval) + ((interval_ % quantizeInterval) / (quantizeInterval / 2))) * quantizeInterval;
    if (startInterval >= songInterval_) startInterval = 0;
    return startInterval;
}

// Songs of 1 to 12 beats of 800 intervals, and a length that is no multiple of any grid.
static const uint32_t kSongs[] = {800, 1600, 2400, 3200, 4800, 9600, 7700};

static void testStraightUnchanged() {
    CHECK(kQuantizeStraightCount == 7, "the old grids are indices 0 to 6");
    for (uint32_t song : kSongs) {
        for (uint8_t g = 0; g < kQuantizeStraightCount; g++) {
            for (uint32_t i = 0; i < song; i++) {
                const uint16_t want = oldSnap(static_cast<uint16_t>(i), g, song);
                const uint16_t got = quantizeSnap(static_cast<uint16_t>(i), g, 100, song);
                CHECK(got == want, "song %u grid %u interval %u: old %u new %u", static_cast<unsigned>(song), g, static_cast<unsigned>(i), want, got);
                if (got != want) return;  // one report is enough
            }
        }
    }
    // The default is the 1:16 of the machine: 200 intervals.
    CHECK(kQuantizeGridDefault == 2 && quantizeGrid(kQuantizeGridDefault).periodThirds == 600, "default grid is 1:16");
    CHECK(quantizeEditStep(0) == 50 && quantizeEditStep(2) == 200 && quantizeEditStep(6) == 3200, "edit step of the straight grids");
}

static bool isGridPoint(uint16_t at_, uint8_t grid_, uint32_t song_) {
    const QuantizeGridInfo& g = quantizeGrid(grid_);
    for (uint8_t which = 0; which < 2; which++) {
        const uint16_t p3 = which ? g.periodThirdsB : g.periodThirds;
        if (p3 == 0) continue;
        for (uint32_t k = 0; quantizePoint(grid_, 0) + (k * p3 + 1) / 3 <= song_ + 3200; k++) {
            if ((k * p3 + 1) / 3 == at_) return true;
        }
    }
    return false;
}

static void testEveryGrid() {
    const uint32_t song = 4800;
    for (uint8_t g = 0; g < kQuantizeGridCount; g++) {
        const QuantizeGridInfo& info = quantizeGrid(g);
        // The widest gap between two grid points (for a mixed grid the larger of the two combined gaps is smaller
        // than either grid's own step; its own step is the bound).
        const uint32_t halfStep = (info.periodThirds + 5) / 6 + 1;  // half the step in intervals, plus rounding
        uint16_t previous = 0;
        for (uint32_t i = 0; i < song; i++) {
            const uint16_t at = quantizeSnap(static_cast<uint16_t>(i), g, 100, song);
            const int32_t distance = (at > i) ? at - static_cast<int32_t>(i) : static_cast<int32_t>(i) - at;
            const bool wrapped = (at == 0) && (i > song / 2);
            CHECK(wrapped || (static_cast<uint32_t>(distance) <= halfStep), "grid %u interval %u -> %u is %d away (max %u)", g, static_cast<unsigned>(i), at, distance, static_cast<unsigned>(halfStep));
            CHECK(at < song, "grid %u interval %u lands past the song: %u", g, static_cast<unsigned>(i), at);
            CHECK(isGridPoint(at, g, song) || (at == 0), "grid %u interval %u -> %u is no grid point", g, static_cast<unsigned>(i), at);
            // Idempotent: a point snaps to itself.
            CHECK(quantizeSnap(at, g, 100, song) == at, "grid %u: %u is not stable", g, at);
            // Monotonic, until the wrap to the start at the end of the song.
            if (!wrapped && (i > 0)) CHECK(at >= previous, "grid %u: interval %u went back from %u to %u", g, static_cast<unsigned>(i), previous, at);
            if (!wrapped) previous = at;
            if (failures > 20) return;
        }
    }
}

// A single grid snaps to its nearest point, measured exactly (in thirds of an interval): no other point is nearer.
static void testNearestExact() {
    for (uint8_t g = 0; g <= 10; g++) {
        const uint32_t p3 = quantizeGrid(g).periodThirds;
        for (uint32_t i = 0; i < 4800; i++) {
            const uint32_t at = quantizeSnap(static_cast<uint16_t>(i), g, 100, 1000000);
            // The index k of the point that was chosen.
            uint32_t k = 0;
            while (quantizePoint(g, k) < at) k++;
            CHECK(quantizePoint(g, k) == at, "grid %u: %u is no point", g, static_cast<unsigned>(at));
            const int64_t x3 = static_cast<int64_t>(i) * 3, chosen = std::llabs(x3 - static_cast<int64_t>(k) * p3);
            const int64_t before = (k > 0) ? std::llabs(x3 - static_cast<int64_t>(k - 1) * p3) : chosen, after = std::llabs(x3 - static_cast<int64_t>(k + 1) * p3);
            CHECK(chosen <= before && chosen <= after, "grid %u interval %u: point %u (k %u) is not the nearest (%lld vs %lld / %lld thirds)", g, static_cast<unsigned>(i), static_cast<unsigned>(at), static_cast<unsigned>(k),
                  static_cast<long long>(chosen), static_cast<long long>(before), static_cast<long long>(after));
            if (failures > 20) return;
        }
    }
}

static void testTriplets() {
    // 1:8T: three points per 800-interval beat.
    CHECK(quantizePoint(9, 0) == 0 && quantizePoint(9, 1) == 267 && quantizePoint(9, 2) == 533 && quantizePoint(9, 3) == 800, "1:8T points %u %u %u %u", (unsigned)quantizePoint(9, 0), (unsigned)quantizePoint(9, 1), (unsigned)quantizePoint(9, 2), (unsigned)quantizePoint(9, 3));
    // 1:16T: six per beat; 1:32T: twelve; 1:4T: 1.5 per beat (three every two beats).
    CHECK(quantizePoint(8, 6) == 800 && quantizePoint(7, 12) == 800 && quantizePoint(10, 3) == 1600, "triplet points meet the beats");
    // A hit near the second 1:8T point lands on it, not on a straight grid point.
    CHECK(quantizeSnap(270, 9, 100, 3200) == 267 && quantizeSnap(260, 9, 100, 3200) == 267 && quantizeSnap(530, 9, 100, 3200) == 533, "1:8T snaps");
    CHECK(quantizeSnap(400, 9, 100, 3200) == 267 || quantizeSnap(400, 9, 100, 3200) == 533, "the middle of a triplet step goes to a neighbour");
    // Triplet points are the rounded multiples of the exact period, with an error under 1/2 interval.
    for (uint8_t g = 7; g <= 10; g++) {
        const uint32_t p3 = quantizeGrid(g).periodThirds;
        for (uint32_t k = 0; k < 400; k++) {
            const double exact = k * p3 / 3.0;
            const double err = quantizePoint(g, k) - exact;
            CHECK(err > -0.5 - 1e-9 && err < 0.5 + 1e-9, "grid %u point %u error %f", g, static_cast<unsigned>(k), err);
        }
    }
    // The error does not pile up: after 1000 beats of 1:8T the point is where the exact one is, to the interval.
    CHECK(quantizePoint(9, 3000) == 800000, "1:8T has no drift (3000 points = 1000 beats)");
}

static void testMixed() {
    const uint32_t song = 3200;
    for (uint8_t g : {static_cast<uint8_t>(11), static_cast<uint8_t>(12)}) {
        const QuantizeGridInfo& info = quantizeGrid(g);
        const uint8_t straight = static_cast<uint8_t>(g == 11 ? 2 : 3), triplet = static_cast<uint8_t>(g == 11 ? 8 : 9);
        CHECK(info.periodThirds == quantizeGrid(straight).periodThirds && info.periodThirdsB == quantizeGrid(triplet).periodThirds, "mixed grid %u is its straight and its triplet grid", g);
        for (uint32_t i = 0; i < song; i++) {
            const uint16_t a = quantizeSnap(static_cast<uint16_t>(i), straight, 100, 1000000), b = quantizeSnap(static_cast<uint16_t>(i), triplet, 100, 1000000);
            const uint16_t m = quantizeSnap(static_cast<uint16_t>(i), g, 100, 1000000);
            const uint32_t da = (a > i) ? a - i : i - a, db = (b > i) ? b - i : i - b;
            // The nearer of the two (a margin of one interval for the rounded triplet point). The grids never tie exactly
            // (a tie would need a position that is a multiple of 3 thirds in the middle of two points, and there is none),
            // so the rule "a tie goes to the straight grid" is only a safeguard.
            if (da + 1 < db) CHECK(m == a, "mixed %u interval %u: straight %u should win over triplet %u, got %u", g, static_cast<unsigned>(i), a, b, m);
            if (db + 1 < da) CHECK(m == b, "mixed %u interval %u: triplet %u should win over straight %u, got %u", g, static_cast<unsigned>(i), b, a, m);
            const uint32_t dm = (m > i) ? m - i : i - m;
            CHECK(dm <= da && dm <= db + 1, "mixed %u interval %u: %u is no nearer than %u / %u", g, static_cast<unsigned>(i), m, a, b);
            if (failures > 20) return;
        }
        // The two grids share the beat points: a hit on a beat is not moved.
        for (uint32_t beat = 0; beat < song; beat += 400) CHECK(quantizeSnap(static_cast<uint16_t>(beat), g, 100, song) == beat, "mixed %u keeps %u", g, static_cast<unsigned>(beat));
    }
    // 1:16+T: a hit 3 intervals from the 1:16 point (200) is a straight hit; one 3 intervals from 1:16T (133) a triplet hit.
    CHECK(quantizeSnap(203, 11, 100, 3200) == 200 && quantizeSnap(130, 11, 100, 3200) == 133, "1:16+T picks per hit");
    // OFF leaves everything where it is, even a hit at the very end of the song (which still wraps).
    for (uint32_t song2 : kSongs) {
        for (uint32_t i = 0; i < song2; i += 1) {
            CHECK(quantizeSnap(static_cast<uint16_t>(i), kQuantizeGridOff, 100, song2) == i, "OFF moved %u", static_cast<unsigned>(i));
            if (failures > 20) return;
        }
    }
    CHECK(!quantizeIsSingleGrid(kQuantizeGridOff) && !quantizeIsSingleGrid(11) && !quantizeIsSingleGrid(12) && !quantizeIsSingleGrid(14) && !quantizeIsSingleGrid(255), "EUCLID cannot use OFF or a mixed grid");
    for (uint8_t g = 0; g <= 10; g++) CHECK(quantizeIsSingleGrid(g), "grid %u is a single grid", g);
}

static void testStrength() {
    const uint32_t song = 3200;
    for (uint8_t g = 0; g < kQuantizeGridCount; g++) {
        for (uint32_t i = 0; i < song; i += 7) {
            const uint16_t full = quantizeSnap(static_cast<uint16_t>(i), g, 100, 1000000);
            CHECK(quantizeSnap(static_cast<uint16_t>(i), g, 0, song) == i, "grid %u strength 0 moved %u", g, static_cast<unsigned>(i));
            uint16_t last = static_cast<uint16_t>(i);
            for (uint8_t s = 0; s <= 100; s += 10) {
                const uint16_t at = quantizeSnap(static_cast<uint16_t>(i), g, s, 1000000);
                // Between the hit and its grid point, nearer the grid point the higher the strength.
                const uint32_t lo = (full < i) ? full : i, hi = (full < i) ? i : full;
                CHECK((at >= lo) && (at <= hi), "grid %u interval %u strength %u -> %u outside %u..%u", g, static_cast<unsigned>(i), s, at, static_cast<unsigned>(lo), static_cast<unsigned>(hi));
                const int32_t moved = (full >= i) ? static_cast<int32_t>(at) - static_cast<int32_t>(last) : static_cast<int32_t>(last) - static_cast<int32_t>(at);
                CHECK(moved >= 0, "grid %u interval %u: strength %u moved the wrong way", g, static_cast<unsigned>(i), s);
                last = at;
            }
            CHECK(quantizeSnap(static_cast<uint16_t>(i), g, 100, 1000000) == full, "strength 100 is the plain snap");
            // Half way: within one interval of the middle.
            const int32_t half = quantizeSnap(static_cast<uint16_t>(i), g, 50, 1000000);
            const int32_t middle2 = static_cast<int32_t>(i) + full;  // twice the middle
            CHECK(std::abs(2 * half - middle2) <= 1, "grid %u interval %u strength 50 -> %d, middle %d/2", g, static_cast<unsigned>(i), static_cast<int>(half), static_cast<int>(middle2));
            // Values above 100 are the full snap.
            CHECK(quantizeSnap(static_cast<uint16_t>(i), g, 250, 1000000) == full, "strength above 100 is clamped");
            if (failures > 20) return;
        }
    }
    // Moving part of the way rounds half away from the hit's own position: 3 intervals at 50 % is 2, at 25 % is 1.
    CHECK(quantizeSnap(203, 2, 50, 3200) == 201 && quantizeSnap(197, 2, 50, 3200) == 199 && quantizeSnap(203, 2, 25, 3200) == 202 && quantizeSnap(197, 2, 25, 3200) == 198, "partial strength rounds");
    CHECK(quantizeSnap(203, 2, 10, 3200) == 203 && quantizeSnap(203, 2, 16, 3200) == 203 && quantizeSnap(203, 2, 17, 3200) == 202 && quantizeSnap(203, 2, 84, 3200) == 200 && quantizeSnap(203, 2, 83, 3200) == 201, "rounding at the edges");
    // A full snap onto the end of the song wraps to the start; half way towards it the hit stays inside the song.
    CHECK(quantizeSnap(790, 4, 50, 800) == 795, "half-way towards the end of the song stays inside (795, not the wrapped 0)");
    CHECK(quantizeSnap(790, 4, 100, 800) == 0, "full snap to the end of the song wraps to 0");
}

static void testNames() {
    std::set<std::string> seen;
    for (uint8_t g = 0; g < kQuantizeGridCount; g++) {
        const char* name = quantizeGrid(g).nameLongR;
        CHECK(strlen(name) == 10, "grid %u name '%s' is not 10 characters", g, name);
        CHECK(seen.insert(name).second, "grid %u: duplicate name '%s'", g, name);
        for (const char* c = name; *c; c++) CHECK((glyphIndex(*c) != 52) || (*c == ' '), "grid %u name '%s' has '%c' the font lacks", g, name, *c);
        CHECK(name[9] != ' ', "grid %u name is right-aligned", g);
    }
    // The first seven names are the ones the firmware shows today.
    static const char* const old[] = {"      1:64", "      1:32", "      1:16", "       1:8", "       1:4", "       1:2", "       1:1"};
    for (uint8_t g = 0; g < 7; g++) CHECK(strcmp(quantizeGrid(g).nameLongR, old[g]) == 0, "grid %u name changed", g);
    // An out-of-range index (a file from the future) shows the default.
    CHECK(quantizeGrid(200).periodThirds == quantizeGrid(kQuantizeGridDefault).periodThirds, "out-of-range grid falls back to the default");
}

static void testPointsAndSteps() {
    for (uint8_t g = 0; g <= 10; g++) {
        for (uint32_t song : kSongs) {
            // Count by walking the points, as EUCLID does.
            uint32_t walked = 0;
            while (quantizePoint(g, walked) < song) walked++;
            CHECK(quantizePointsIn(g, song) == walked, "grid %u song %u: %u points, walked %u", g, static_cast<unsigned>(song), static_cast<unsigned>(quantizePointsIn(g, song)), static_cast<unsigned>(walked));
        }
    }
    CHECK(quantizePointsIn(6, 800) == 1 && quantizePointsIn(2, 800) == 4 && quantizePointsIn(9, 800) == 3 && quantizePointsIn(9, 0) == 0, "points in a one-beat song");
    CHECK(quantizePointsIn(8, 3200) == 24, "24 sixteenth-triplets in four beats");
    // The edit step: the grid's own step in whole intervals, the straight one of a mixed grid, 1:64 for OFF.
    CHECK(quantizeEditStep(9) == 267 && quantizeEditStep(8) == 133 && quantizeEditStep(7) == 67 && quantizeEditStep(10) == 533, "triplet edit steps");
    CHECK(quantizeEditStep(11) == 200 && quantizeEditStep(12) == 400 && quantizeEditStep(kQuantizeGridOff) == 50, "mixed and OFF edit steps");
}

int main() {
    testStraightUnchanged();
    testEveryGrid();
    testNearestExact();
    testTriplets();
    testMixed();
    testStrength();
    testNames();
    testPointsAndSteps();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
