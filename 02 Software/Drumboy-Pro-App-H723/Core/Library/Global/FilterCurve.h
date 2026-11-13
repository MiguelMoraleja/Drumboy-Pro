// Filter curve view (FILTER 1 and FILTER 2 pages): the filter's response drawn over the lane area,
// 20 Hz to 20 kHz on a logarithmic axis, +18 dB to -48 dB. Hardware-free so it can be tested on a
// PC (see 02 Software/tests).
//
// The curve is computed from the same five coefficients the audio code uses, so it shows what is
// heard: the biquad, the 6 dB setting (half filtered, half unfiltered), and the page's DRY and WET
// added with their phase. DRIVE is left out: it is a gain with a soft clip before the filter, not
// part of its shape.
#ifndef __FILTERCURVE_H
#define __FILTERCURVE_H

#include <math.h>
#include <stdint.h>

// The plot: one point every 2 px.
const uint16_t kCurveX = 80, kCurveWidth = 840, kCurveStep = 2, kCurvePoints = 421;
const uint16_t kCurveY = 160, kCurveHeight = 231;
const float kCurveTopDb = 18.0f, kCurveBottomDb = -48.0f;  // 3.5 px per dB
const float kCurveMinHz = 20.0f, kCurveDecades = 3.0f;     // 20 Hz .. 20 kHz
const uint16_t kCurveLegendY = 140, kCurveLabelY = 398;

inline uint16_t curvePointX(uint16_t point_) { return static_cast<uint16_t>(kCurveX + point_ * kCurveStep); }

// Frequency of a point, and the x of a frequency (clamped to the plot).
inline float curvePointHz(uint16_t point_) {
    return kCurveMinHz * powf(10.0f, kCurveDecades * static_cast<float>(point_) / static_cast<float>(kCurvePoints - 1));
}

inline uint16_t curveXOfHz(float hz_) {
    if (hz_ < kCurveMinHz) hz_ = kCurveMinHz;
    float turn = log10f(hz_ / kCurveMinHz) / kCurveDecades;
    if (turn > 1.0f) turn = 1.0f;
    return static_cast<uint16_t>(kCurveX + lroundf(turn * static_cast<float>(kCurveWidth)));
}

// The y of a level, clamped to the plot.
inline uint16_t curveYOfDb(float db_) {
    if (db_ > kCurveTopDb) db_ = kCurveTopDb;
    if (db_ < kCurveBottomDb) db_ = kCurveBottomDb;
    const float perDb = static_cast<float>(kCurveHeight) / (kCurveTopDb - kCurveBottomDb);
    return static_cast<uint16_t>(kCurveY + lroundf((kCurveTopDb - db_) * perDb));
}

// What one filter does to the signal.
struct FilterShape {
    float b0, b1, b2, a1, a2;  // the biquad as the audio code runs it (a0 = 1)
    bool slope12;              // false: the 6 dB setting, half filtered and half unfiltered
    float wet, dry;            // 0..1 and above
    bool on;                   // false: the filter is off and passes the signal unchanged
};

// Its response at hz_ in dB (not below -120).
inline float filterShapeDb(const FilterShape& s_, float hz_, float sampleRate_) {
    if (!s_.on) return 0.0f;
    const float w = 6.2831853f * hz_ / sampleRate_;
    const float c1 = cosf(w), s1 = sinf(w), c2 = cosf(2.0f * w), s2 = sinf(2.0f * w);
    // H = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2), z^-1 = cos w - j sin w
    const float nr = s_.b0 + s_.b1 * c1 + s_.b2 * c2, ni = -(s_.b1 * s1 + s_.b2 * s2);
    const float dr = 1.0f + s_.a1 * c1 + s_.a2 * c2, di = -(s_.a1 * s1 + s_.a2 * s2);
    const float dd = dr * dr + di * di;
    float hr = 1.0f, hi = 0.0f;
    if (dd > 1.0e-20f) {
        hr = (nr * dr + ni * di) / dd;
        hi = (ni * dr - nr * di) / dd;
    }
    if (!s_.slope12) {
        hr = 0.5f * hr + 0.5f;
        hi = 0.5f * hi;
    }
    const float tr = s_.wet * hr + s_.dry, ti = s_.wet * hi;
    const float mag2 = tr * tr + ti * ti;
    if (mag2 < 1.0e-12f) return -120.0f;
    return 10.0f * log10f(mag2);
}

#endif
