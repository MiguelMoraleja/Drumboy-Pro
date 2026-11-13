// Host-side tests for the filter curve view (FilterCurve.h).

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "FilterCurve.h"

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

static const float kRate = 44100.0f;

// The firmware's coefficients (Filter::calculateCoef), with exact sine and cosine.
static FilterShape shape(int type, float hz, float q, bool slope12 = true, float wet = 1.0f, float dry = 0.0f) {
    const float w = 6.2831853f * hz / kRate, c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
    float b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (type) {
        case 1:  // low-pass
            b0 = (1 - c) * 0.5f, b1 = 1 - c, b2 = (1 - c) * 0.5f, a0 = 1 + alpha, a1 = -2 * c, a2 = 1 - alpha;
            break;
        case 2:  // high-pass
            b0 = (1 + c) * 0.5f, b1 = -(1 + c), b2 = (1 + c) * 0.5f, a0 = 1 + alpha, a1 = -2 * c, a2 = 1 - alpha;
            break;
        case 3:  // band-pass
            b0 = alpha, b1 = 0, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * c, a2 = 1 - alpha;
            break;
        case 4:  // band-stop
            b0 = 1, b1 = -2 * c, b2 = 1, a0 = 1 + alpha, a1 = -2 * c, a2 = 1 - alpha;
            break;
    }
    FilterShape s = {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0, slope12, wet, dry, true};
    return s;
}

int main() {
    // --- Axes ---
    CHECK(curvePointX(0) == kCurveX && curvePointX(kCurvePoints - 1) == kCurveX + kCurveWidth, "x ends");
    CHECK(std::fabs(curvePointHz(0) - 20.0f) < 0.01f && std::fabs(curvePointHz(kCurvePoints - 1) - 20000.0f) < 5.0f, "20 Hz to 20 kHz: %f %f", curvePointHz(0),
          curvePointHz(kCurvePoints - 1));
    CHECK(std::fabs(curvePointHz(140) - 200.0f) < 0.5f && std::fabs(curvePointHz(280) - 2000.0f) < 2.0f, "a decade every 140 points");
    CHECK(curveXOfHz(20) == 80 && curveXOfHz(200) == 360 && curveXOfHz(2000) == 640 && curveXOfHz(20000) == 920, "decades at %u %u %u %u", curveXOfHz(20), curveXOfHz(200),
          curveXOfHz(2000), curveXOfHz(20000));
    CHECK(curveXOfHz(1) == 80 && curveXOfHz(90000) == 920, "frequencies outside are clamped");
    for (uint16_t p = 0; p < kCurvePoints; p++) {
        const int dx = static_cast<int>(curveXOfHz(curvePointHz(p))) - curvePointX(p);
        CHECK(dx >= -1 && dx <= 1, "point %u: x %u vs %u", p, curveXOfHz(curvePointHz(p)), curvePointX(p));
    }
    CHECK(curveYOfDb(18) == kCurveY && curveYOfDb(-48) == kCurveY + kCurveHeight, "y ends");
    CHECK(curveYOfDb(0) == kCurveY + 63 && curveYOfDb(-12) == kCurveY + 105, "0 dB at %u, -12 dB at %u", curveYOfDb(0), curveYOfDb(-12));
    CHECK(curveYOfDb(60) == kCurveY && curveYOfDb(-120) == kCurveY + kCurveHeight, "levels outside are clamped");
    CHECK(kCurveX + kCurveWidth < 939 && kCurveY + kCurveHeight < kCurveLabelY && kCurveLabelY + 7 < 440 && kCurveLegendY + 9 < kCurveY, "the plot fits the lane area");

    // --- Response ---
    // Off: flat.
    FilterShape off = shape(1, 1000, 0.7f);
    off.on = false;
    CHECK(filterShapeDb(off, 5000, kRate) == 0.0f, "an off filter is flat");

    // Low-pass 1 kHz, Q 0.70: flat below, about -3 dB at the cutoff, 12 dB per octave above.
    const FilterShape lp = shape(1, 1000, 0.7071f);
    CHECK(std::fabs(filterShapeDb(lp, 50, kRate)) < 0.1f, "low-pass passband %f", filterShapeDb(lp, 50, kRate));
    CHECK(std::fabs(filterShapeDb(lp, 1000, kRate) + 3.01f) < 0.1f, "low-pass at cutoff %f", filterShapeDb(lp, 1000, kRate));
    const float lpOct = filterShapeDb(lp, 4000, kRate) - filterShapeDb(lp, 8000, kRate);
    CHECK(lpOct > 11.5f && lpOct < 16.0f, "low-pass slope %f dB per octave", lpOct);

    // High-pass: the mirror image.
    const FilterShape hp = shape(2, 1000, 0.7071f);
    CHECK(std::fabs(filterShapeDb(hp, 15000, kRate)) < 0.2f, "high-pass passband %f", filterShapeDb(hp, 15000, kRate));
    CHECK(std::fabs(filterShapeDb(hp, 1000, kRate) + 3.01f) < 0.1f, "high-pass at cutoff %f", filterShapeDb(hp, 1000, kRate));
    const float hpOct = filterShapeDb(hp, 250, kRate) - filterShapeDb(hp, 125, kRate);
    CHECK(std::fabs(hpOct - 12.0f) < 0.5f, "high-pass slope %f dB per octave", hpOct);

    // Resonance: a peak of 20 log10(Q) at the cutoff.
    const FilterShape res = shape(1, 1000, 5.5f);
    CHECK(std::fabs(filterShapeDb(res, 1000, kRate) - 20.0f * std::log10(5.5f)) < 0.1f, "peak %f dB", filterShapeDb(res, 1000, kRate));

    // Band-pass: 0 dB at the centre, lower on both sides. Band-stop: a notch there.
    const FilterShape bp = shape(3, 1000, 2.0f);
    CHECK(std::fabs(filterShapeDb(bp, 1000, kRate)) < 0.05f && filterShapeDb(bp, 100, kRate) < -20.0f && filterShapeDb(bp, 10000, kRate) < -20.0f, "band-pass");
    const FilterShape bs = shape(4, 1000, 2.0f);
    CHECK(filterShapeDb(bs, 1000, kRate) < -60.0f && std::fabs(filterShapeDb(bs, 50, kRate)) < 0.1f && std::fabs(filterShapeDb(bs, 15000, kRate)) < 0.1f, "band-stop");

    // The 6 dB setting is half filtered, half unfiltered: far above a low-pass it settles at -6 dB.
    const FilterShape six = shape(1, 500, 0.7071f, false);
    CHECK(std::fabs(filterShapeDb(six, 18000, kRate) + 6.02f) < 0.2f && std::fabs(filterShapeDb(six, 30, kRate)) < 0.1f, "6 dB setting: %f", filterShapeDb(six, 18000, kRate));

    // DRY and WET: wet 0 is silence unless dry is up; wet 0.5 is 6 dB lower; dry alone is flat.
    CHECK(filterShapeDb(shape(1, 1000, 0.7f, true, 0.0f, 0.0f), 100, kRate) == -120.0f, "no wet, no dry");
    CHECK(std::fabs(filterShapeDb(shape(1, 1000, 0.7071f, true, 0.5f, 0.0f), 50, kRate) + 6.02f) < 0.1f, "half wet");
    CHECK(std::fabs(filterShapeDb(shape(1, 1000, 0.7071f, true, 0.0f, 1.0f), 9000, kRate)) < 0.01f, "dry only is flat");
    // A high-pass with its dry added keeps the lows: a shelf, not a cut.
    const FilterShape shelf = shape(2, 1000, 0.7071f, true, 1.0f, 0.5f);
    CHECK(std::fabs(filterShapeDb(shelf, 30, kRate) + 6.02f) < 0.3f && std::fabs(filterShapeDb(shelf, 15000, kRate) - 3.52f) < 0.3f, "high-pass plus dry: %f %f",
          filterShapeDb(shelf, 30, kRate), filterShapeDb(shelf, 15000, kRate));

    // Every point of every curve lands inside the plot.
    for (int type = 1; type <= 4; type++) {
        const float kCutoffs[] = {20.0f, 150.0f, 1000.0f, 8000.0f, 20000.0f};
        for (float hz : kCutoffs) {
            const FilterShape s = shape(type, hz, 5.5f);
            for (uint16_t p = 0; p < kCurvePoints; p++) {
                const float db = filterShapeDb(s, curvePointHz(p), kRate);
                const uint16_t y = curveYOfDb(db);
                CHECK(std::isfinite(db) && y >= kCurveY && y <= kCurveY + kCurveHeight, "type %d %g Hz point %u: %f dB y %u", type, hz, p, db, y);
            }
        }
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
