// Host-side tests for the dub desk geometry (DubView.h).

#include <cstdio>

#include "DubView.h"

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
    // Ten strips, then the two returns, all inside the lane area (x 20..940, y 131..440).
    CHECK(deskStripX(0) == 24 && deskStripX(9) + kDeskStripWidth == 24 + 9 * 60 + 57, "strip positions");
    CHECK(deskStripX(9) + kDeskStripStep <= kDeskReturnX[0], "the returns start after the strips: %u vs %u", deskStripX(9) + kDeskStripStep, kDeskReturnX[0]);
    CHECK(kDeskReturnX[0] + kDeskStripWidth < kDeskReturnX[1] && kDeskReturnX[1] + kDeskStripWidth < 940, "returns fit");
    CHECK(kDeskFaderY + kDeskFaderHeight < kDeskBusY && kDeskBusY + 7 <= 440 && kDeskNumY >= 131, "rows fit the lane area");
    CHECK(kDeskNameY > kDeskNumY + 9 && kDeskEchoY > kDeskNameY + 7 && kDeskVerbY > kDeskEchoY + kDeskBarHeight && kDeskFaderY > kDeskVerbY + kDeskBarHeight, "rows do not overlap");
    CHECK(kDeskBarOffset + kDeskBarWidth <= kDeskStripWidth, "send bars fit the strip");
    CHECK(kDeskMeterX + kDeskMeterWidth <= kDeskStripWidth && kDeskThumbX + kDeskThumbWidth < kDeskMeterX, "fader and meter fit");
    // Fader.
    CHECK(deskThumbY(100) == kDeskFaderY && deskThumbY(0) == kDeskFaderY + kDeskFaderHeight - kDeskThumbHeight && deskThumbY(200) == kDeskFaderY, "fader ends");
    uint16_t prev = 0xFFFF;
    for (uint8_t v = 0; v <= 100; v++) {
        CHECK(deskThumbY(v) <= prev, "fader monotonic at %u", v);
        prev = deskThumbY(v);
    }
    // Send bars.
    CHECK(deskBarFill(0) == 0 && deskBarFill(1) == 1 && deskBarFill(100) == kDeskBarWidth - 2 && deskBarFill(255) == kDeskBarWidth - 2 && deskBarFill(50) == 18, "bar fill: %u", deskBarFill(50));
    // Meter.
    CHECK(deskMeterHeight(0, 18) == 0 && deskMeterHeight(18, 18) == kDeskFaderHeight - 2 && deskMeterHeight(99, 18) == kDeskFaderHeight - 2 && deskMeterHeight(9, 18) == 103 && deskMeterHeight(5, 0) == 0, "meter");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
