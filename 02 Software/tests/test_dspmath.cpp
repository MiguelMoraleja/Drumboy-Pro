// Host-side tests for the small DSP functions (DspMath.h): the soft clip, the NaN test, the bit depth
// reduction, the wavefolder and the overdrive curve.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "DspMath.h"

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

int main() {
    // Soft knee: identity up to 2/3, never above 1, continuous, monotonic, odd.
    float prev = -2.0f;
    float worstStep = 0.0f, peak = 0.0f;
    for (float x = -3.0f; x <= 3.0f; x += 0.0005f) {
        const float y = softKneeClip(x);
        peak = std::fmax(peak, std::fabs(y));
        CHECK(y >= prev - 1e-6f, "monotonic at %f", x);
        if (x > -3.0f) worstStep = std::fmax(worstStep, std::fabs(y - prev));
        prev = y;
        CHECK(std::fabs(softKneeClip(-x) + y) < 1e-6f, "odd at %f", x);
        if (std::fabs(x) <= 0.6666f) CHECK(y == x, "identity at %f", x);
    }
    CHECK(peak <= 1.0f + 1e-6f, "never over full scale: %f", peak);
    CHECK(worstStep < 0.001f, "no step: %f", worstStep);
    CHECK(std::fabs(softKneeClip(1.1666f) - 1.0f) < 1e-3f && softKneeClip(1.1668f) == 1.0f && softKneeClip(5.0f) == 1.0f && softKneeClip(-5.0f) == -1.0f, "reaches 1.0 at the end of the knee");
    CHECK(std::fabs(softKneeClip(1.0f) - 0.9506f) < 1e-3f, "value at 1.0: %f", softKneeClip(1.0f));

    // NaN and infinity are caught in the bits.
    CHECK(finiteOrZero(0.5f) == 0.5f && finiteOrZero(-1e30f) == -1e30f && finiteOrZero(0.0f) == 0.0f, "finite values pass");
    uint32_t bits = 0x7FC00000u;
    float nanValue;
    std::memcpy(&nanValue, &bits, 4);
    bits = 0x7F800000u;
    float infValue;
    std::memcpy(&infValue, &bits, 4);
    bits = 0xFF800000u;
    float negInf;
    std::memcpy(&negInf, &bits, 4);
    CHECK(finiteOrZero(nanValue) == 0.0f && finiteOrZero(infValue) == 0.0f && finiteOrZero(negInf) == 0.0f, "NaN and infinities become zero");

    // Bit depth reduction.
    CHECK(std::fabs(quantize24(0.0f, 0x00FFFFFFu)) < 1e-6f, "full resolution: silence");
    for (uint32_t bitsKept = 1; bitsKept <= 24; bitsKept++) {
        const uint32_t mask = (0x00FFFFFFu << (24 - bitsKept)) & 0x00FFFFFFu;
        CHECK(std::fabs(quantize24(0.0f, mask)) < 1e-6f, "silence stays silence at %u bits: %f", bitsKept, quantize24(0.0f, mask));
        const float tiny = quantize24(1e-4f, mask);
        CHECK(std::fabs(tiny) < 0.5f || bitsKept < 3, "tiny stays small at %u bits", bitsKept);
    }
    {
        const uint32_t mask = 0x00F00000u;  // 4 bits
        CHECK(std::fabs(quantize24(0.0f, mask)) < 1e-6f && std::fabs(quantize24(0.05f, mask)) < 1e-6f && std::fabs(quantize24(-0.05f, mask)) < 1e-6f, "a small signal of either sign rounds to 0, not to a step below");
        CHECK(std::fabs(quantize24(0.5f, mask) - 0.5f) < 0.07f, "0.5 within half a step: %f", quantize24(0.5f, mask));
        CHECK(quantize24(2.0f, mask) <= 1.0f && quantize24(-2.0f, mask) >= -1.0f, "clamped");
        float worst = 0;
        for (float x = -0.9f; x <= 0.9f; x += 0.001f) worst = std::fmax(worst, std::fabs(quantize24(x, mask) - x));
        CHECK(worst <= 0.0626f, "error within half a 4-bit step (0.0625): %f", worst);
    }
    {
        float worst = 0;
        for (float x = -1.0f; x <= 1.0f; x += 0.0007f) worst = std::fmax(worst, std::fabs(quantize24(x, 0x00FFFFFFu) - x));
        CHECK(worst < 2e-7f * 4, "full resolution is transparent: %g", worst);
    }

    // Wavefolder: equals the reflecting loop inside a few widths, stays inside the limits, takes any input.
    std::srand(3);
    for (int i = 0; i < 20000; i++) {
        const float lo = -0.01f - 0.99f * (std::rand() % 1000) / 1000.0f;
        const float hi = 0.01f + 0.99f * (std::rand() % 1000) / 1000.0f;
        const float x = (std::rand() % 4000 - 2000) / 500.0f;
        float ref = x;
        for (int guard = 0; (ref > hi || ref < lo) && guard < 1000; guard++) {
            if (ref > hi) ref = hi - (ref - hi);
            if (ref < lo) ref = lo - (ref - lo);
        }
        const float y = foldSignal(x, lo, hi);
        CHECK(std::fabs(y - ref) < 1e-4f, "fold %f in [%f, %f]: %f vs %f", x, lo, hi, y, ref);
        CHECK(y >= lo - 1e-5f && y <= hi + 1e-5f, "inside the limits");
    }
    CHECK(foldSignal(0.3f, -0.5f, 0.5f) == 0.3f, "in range is untouched");
    CHECK(std::isfinite(foldSignal(1e30f, -0.5f, 0.5f)) && std::fabs(foldSignal(1e30f, -0.5f, 0.5f)) <= 0.5f, "a huge value is folded in constant time");
    CHECK(foldSignal(infValue, -0.5f, 0.5f) <= 0.5f, "an infinity cannot hang it");
    CHECK(foldSignal(3.0f, 0.5f, 0.5f) == 0.5f, "no width: the limit");

    // Overdrive curve: continuous, odd, and equal to the old curve at threshold 1.
    for (float thr : {0.0631f, 0.1f, 0.25f, 0.5f, 0.7943f, 1.0f}) {
        const float below = overdriveShape(thr * 0.99999f, thr), above = overdriveShape(thr * 1.00001f, thr);
        CHECK(std::fabs(below - above) < 1e-4f * thr + 1e-6f, "continuous at the threshold %f: %f %f", thr, below, above);
        CHECK(std::fabs(overdriveShape(-0.3f * thr, thr) + overdriveShape(0.3f * thr, thr)) < 1e-6f, "odd");
        CHECK(std::fabs(overdriveShape(10.0f, thr) - 0.66666667f * thr) < 1e-6f, "plateau");
    }
    for (float x = -1.0f; x <= 1.0f; x += 0.01f) CHECK(std::fabs(overdriveShape(x, 1.0f) - (x - x * x * x / 3.0f)) < 1e-6f, "the old curve at threshold 1");
    CHECK(std::fabs(overdriveShape(0.01f, 0.5f) - 0.01f) < 1e-4f, "unity slope for small signals");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
