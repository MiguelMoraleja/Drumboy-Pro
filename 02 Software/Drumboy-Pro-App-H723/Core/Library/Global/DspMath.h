// Small pure DSP functions of the audio path, hardware-free so they can be tested on a PC (see
// 02 Software/tests/test_dspmath.cpp): the master soft clip, the NaN/infinity test, the bit depth
// reduction, the wavefolder and the overdrive curve.
#ifndef __DSPMATH_H
#define __DSPMATH_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__)
#define DSPINLINE __attribute__((always_inline)) inline
#else
#define DSPINLINE inline
#endif

// Soft knee: the identity up to 2/3, then a cubic that leaves 2/3 with slope 1 and reaches exactly 1.0 with
// slope 0 where the knee ends, at 2/3 + 1.5 * (1 - 2/3) = 1.1666667; above that it is the hard limit. (The
// knee used to end at 1.25 with the same polynomial, which only reaches 1.0556 there: the output went over
// full scale and dropped by 0.056 above 1.25.)
DSPINLINE float softKneeClip(float x) {
    const float kThreshold = 0.6666666667f;
    const float kKneeEnd = 1.1666666667f;
    const float absX = fabsf(x);
    if (absX <= kThreshold) return x;
    if (absX > kKneeEnd) return (x > 0) ? 1.0f : -1.0f;
    const float s = (absX - kThreshold) / (kKneeEnd - kThreshold);
    const float val = kThreshold + (kKneeEnd - kThreshold) * (s - 0.3333333333f * s * s * s);
    return (x > 0) ? val : -val;
}

// A NaN or an infinity is not caught by a float compare in the firmware build (-Ofast implies
// -ffinite-math-only), so it is tested in the bits: the exponent all ones. Used where one bad value would
// poison a filter's history for good (the master low-pass, the samples loaded from a file).
DSPINLINE bool isFiniteBits(float x) {
    uint32_t bits;
    memcpy(&bits, &x, sizeof bits);
    return (bits & 0x7F800000u) != 0x7F800000u;
}
DSPINLINE float finiteOrZero(float x) { return isFiniteBits(x) ? x : 0.0f; }

// A value wrapped into [0, size) in constant time. The loops this replaces (while (x >= size) x -= size, and the
// same upwards) ran as long as the value was far out, and for an infinity they never ended: in the audio interrupt
// that stalls the whole unit. A value that is not a number gives 0; one already inside comes back untouched.
DSPINLINE float wrapRange(float x, float size) {
    if (!isFiniteBits(x)) return 0.0f;
    if ((x >= 0.0f) && (x < size)) return x;
    x -= size * floorf(x / size);
    if (!(x >= 0.0f) || !(x < size)) x = 0.0f;  // a tiny negative value plus the size rounds up to the size itself
    return x;
}

// The history of a filter (the last inputs and outputs, n values in a row): a NaN or an infinity in it is fed back for good, so
// every output after it is NaN and the route stays silent. Looked at once per block; when one value is bad all are cleared (a
// click at worst). Returns true when it cleared them.
DSPINLINE bool historySanitize(float* h, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        if (!isFiniteBits(h[i])) {
            for (uint8_t k = 0; k < n; k++) h[k] = 0.0f;
            return true;
        }
    }
    return false;
}

// Bit depth reduction of a sample in [-1, 1] to the kept bits of a 24-bit offset-binary code (mask = the kept
// bits, 0x00FFFFFF for all). It rounds to the nearest step, so the centre code stays the centre: digital
// silence stays silence. Truncating (the old way) turned 0.0 into the step below the middle, a DC offset of
// -0.125 at 4 bits.
DSPINLINE float quantize24(float x, uint32_t mask) {
    float n = (x + 1.0f) * 0.5f;
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    uint32_t code = static_cast<uint32_t>(n * 16777215.0f + 0.5f);
    const uint32_t step = ((~mask) & 0x00FFFFFFu) + 1u;
    code += step >> 1;
    if (code > 0x00FFFFFFu) code = 0x00FFFFFFu;
    code &= mask;
    return static_cast<float>(code) * (2.0f / 16777215.0f) - 1.0f;
}

// Wavefolder: the signal reflected back between the limits, as a triangle wave, in constant time. (The loop
// that reflected one limit at a time ran about |x| / width times: for a very loud input it took
// milliseconds in the audio interrupt, and for an infinity it never ended.)
DSPINLINE float foldSignal(float x, float limNeg, float limPos) {
    if (x > 64.0f) x = 64.0f;
    if (x < -64.0f) x = -64.0f;
    const float width = limPos - limNeg;
    if (width <= 1.0e-6f) return limPos;
    float m = fmodf(x - limNeg, 2.0f * width);
    if (m < 0.0f) m += 2.0f * width;
    return limNeg + ((m <= width) ? m : (2.0f * width - m));
}

// Overdrive curve: the cubic x - x^3/3 on the signal relative to the threshold, which reaches 2/3 * threshold
// exactly where the plateau starts (it used the raw signal, so the two only met at a threshold of 1: at the
// default -6 dB the output jumped by 0.125 at every crossing). Equal to the old curve at threshold 1.
DSPINLINE float overdriveShape(float drive, float threshold) {
    const float u = drive / threshold;
    if (u < -1.0f) return -0.66666667f * threshold;
    if (u > 1.0f) return 0.66666667f * threshold;
    return threshold * (u - (u * u * u) * 0.33333333f);
}

#endif
