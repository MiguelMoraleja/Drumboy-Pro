// Host-side tests for the lane marks and the ring geometry (LaneView.h).

#include <cstdio>
#include <cstdlib>

#include "LaneView.h"

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

int main() {
    // A beat without note settings looks as before: FLAT full height; GHOST is now shorter.
    CHECK(laneMarkHeight(0, 100) == 9, "FLAT %u", laneMarkHeight(0, 100));
    CHECK(laneMarkHeight(0, 50) == 6, "GHOST %u", laneMarkHeight(0, 50));
    CHECK(laneMarkHeight(127, 50) == 9 && laneMarkHeight(1, 100) == 2 && laneMarkHeight(64, 100) == 6, "velocity ends");
    uint8_t prev = 0;
    for (int v = 1; v <= 127; v++) {
        const uint8_t h = laneMarkHeight(static_cast<uint8_t>(v), 100);
        CHECK(h >= kLaneMarkMinHeight && h <= kLaneMarkMaxHeight && h >= prev, "velocity %d height %u", v, h);
        prev = h;
    }
    CHECK(!laneMarkHollow(0) && !laneMarkDot(0), "plain beat: solid, no dot");
    CHECK(laneMarkHollow(noteWithChancePercent(0, 95)) && !laneMarkHollow(noteWithChancePercent(0, 100)), "hollow below 100%%");
    CHECK(laneMarkDot(noteWithCond(0, kNoteCondFill)) && !laneMarkDot(noteWithNudge(0, 5)), "dot for a condition only");

    // Rings: layer 1 outside, 12 px apart, all inside the lane area (y 132-440) with the playhead orbit.
    CHECK(ringRadius(0) == 140 && ringRadius(9) == 32, "radii");
    CHECK(kRingCenterY - kRingPlayRadius - 2 >= 132 && kRingCenterY + kRingPlayRadius + 2 <= 440, "orbit inside the lane area");
    CHECK(kRingCenterX - kRingPlayRadius - 2 >= 21, "orbit inside the left edge");
    const uint32_t song = 6400;
    RingPoint p = ringPoint(0, song, 100);
    CHECK(p.x == 200 && p.y == 186, "loop start at the top: %d,%d", p.x, p.y);
    p = ringPoint(song / 4, song, 100);
    CHECK(p.x == 300 && p.y == 286, "a quarter: right: %d,%d", p.x, p.y);
    p = ringPoint(song / 2, song, 100);
    CHECK(p.x == 200 && p.y == 386, "half: bottom: %d,%d", p.x, p.y);
    p = ringPoint(3 * song / 4, song, 100);
    CHECK(p.x == 100 && p.y == 286, "three quarters: left: %d,%d", p.x, p.y);
    p = ringPoint(song, song, 100);
    CHECK(p.x == 200 && p.y == 186, "the end wraps to the start");
    p = ringPoint(5, 0, 100);
    CHECK(p.x == 200 && p.y == 186, "no song length: the top");
    // Every point lies on its ring (within rounding), for every layer.
    for (uint8_t l = 0; l < 10; l++) {
        for (uint32_t t = 0; t < song; t += 50) {
            const RingPoint q = ringPoint(t, song, ringRadius(l));
            const long dx = q.x - kRingCenterX, dy = q.y - kRingCenterY, r = ringRadius(l);
            if (labs(dx * dx + dy * dy - r * r) > 2 * r + 2) {
                CHECK(false, "layer %u t %u off the ring", l, t);
                break;
            }
        }
    }
    CHECK(ringDotRadius(false, 0, 100) == 2 && ringDotRadius(true, 0, 100) == 4 && ringDotRadius(true, 0, 50) == 3 && ringDotRadius(true, 100, 50) == 4, "dot sizes");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
