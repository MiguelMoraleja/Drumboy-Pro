// Host-side tests for the generator screens (GeneratorView.h): the layout fits the lane area, the
// NOTE peaks come from the real generator and are deterministic, the SWEEP shapes, the analytic
// spectra against what the generator really produces, the lines of the tonal types, the hiss
// texture and the duck meter.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "GeneratorView.h"

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

// The lane area is x 40..912 and y 125..434 (SampleView.h draws its plot at x 60..899, y 134..399).
static_assert(kGvNoteX + kGvNoteCols < kGvSpecX, "NOTE and SPECTRUM do not overlap");
static_assert(kGvSweepX + kGvSweepW < kGvSpecX, "SWEEP and SPECTRUM do not overlap");
static_assert(kGvSpecX + kGvSpecW <= 900, "SPECTRUM inside the lane area");
static_assert(kGvNoteCenterY + kGvNoteHalf < kGvNoteTickY, "ticks below the waveform");
static_assert(kGvNoteCenterY - kGvNoteHalf > kGvNoteLabelY + 8, "label above the waveform");
static_assert(kGvNoteTickY + 8 < kGvSweepLabelY, "NOTE ends above the SWEEP label");
static_assert(kGvSweepTop + kGvSweepH < kGvSweepTextY, "SWEEP text below the panel");
static_assert(kGvSweepTextY + 9 <= 434, "SWEEP text inside the area");
static_assert(kGvSpecTop + kGvSpecH < kGvSpecScaleY, "SPECTRUM scale below the panel");
static_assert(kGvSpecTextY + 9 <= 434, "SPECTRUM text inside the area");
static_assert(kHvTextureX + kHvTextureCols < 620, "hiss texture leaves room for its spectrum");
static_assert(kHvTextureLTop + 2 * kHvTextureHalf < kHvTextureRTop, "texture L above R");
static_assert(kHvTextureRTop + 2 * kHvTextureHalf < kHvDuckLabelY, "texture above the duck meter");
static_assert(kHvDuckY + kHvDuckH < kHvDuckTextY, "duck text below the bar");
static_assert(kHvDuckTextY + 9 <= 434, "duck text inside the area");

static void testHelpers() {
    CHECK(fabsf(gvHzAtColumn(0, 280) - 20.0f) < 0.01f, "first column 20 Hz");
    CHECK(fabsf(gvHzAtColumn(279, 280) - 20000.0f) < 1.0f, "last column 20 kHz");
    CHECK(gvXOfHz(20.0f, 620, 280) == 620, "20 Hz at the left edge");
    CHECK(gvXOfHz(20000.0f, 620, 280) == 620 + 279, "20 kHz at the right edge");
    CHECK(gvXOfHz(1000.0f, 620, 280) > gvXOfHz(100.0f, 620, 280), "log axis rises");
    const uint16_t x1k = gvXOfHz(632.5f, 0, 1001);  // the geometric middle of 20 Hz and 20 kHz
    CHECK((x1k >= 499) && (x1k <= 501), "the middle: %u", x1k);
    CHECK(gvYOfDb(0.0f, 172, 200) == 172, "0 dB at the top");
    CHECK(gvYOfDb(-48.0f, 172, 200) == 172 + 199, "the floor at the bottom");
    CHECK(gvYOfDb(5.0f, 172, 200) == 172, "above 0 dB clips");
    CHECK(gvYOfDb(-90.0f, 172, 200) == 172 + 199, "below the floor clips");
    char t[24];
    gvFormatHz(t, sizeof t, 800.0f);
    CHECK(strcmp(t, "800 Hz") == 0, "%s", t);
    gvFormatHz(t, sizeof t, 12000.0f);
    CHECK(strcmp(t, "12.0 kHz") == 0, "%s", t);
    gvFormatHz(t, sizeof t, 4350.0f);
    CHECK(strcmp(t, "4.3 kHz") == 0, "%s", t);
    genViewFormatFreq(t, sizeof t, GEN_DUST, 6.0f);
    CHECK(strcmp(t, "6.0 /s") == 0, "%s", t);
    genViewFormatFreq(t, sizeof t, GEN_SIREN, 600.0f);
    CHECK(strcmp(t, "600 Hz") == 0, "%s", t);
    gvFormatHz(t, sizeof t, 3.0f);
    CHECK(strcmp(t, "3.0 Hz") == 0, "%s", t);
    gvFormatHz(t, sizeof t, 4.5f);
    CHECK(strcmp(t, "4.5 Hz") == 0, "%s", t);
    CHECK(strcmp(genViewShapeName(2), "UP-DOWN") == 0, "shape name");
    CHECK(strcmp(genViewShapeName(7), "DOWN-UP") == 0, "shape wraps");
}

static void testSweep() {
    CHECK(fabsf(genViewSweepOctave(0, 2.0f, 0.0f) + 1.0f) < 1e-5f, "UP starts 1 octave below");
    CHECK(fabsf(genViewSweepOctave(0, 2.0f, 1.0f) - 1.0f) < 1e-5f, "UP ends 1 octave above");
    CHECK(fabsf(genViewSweepOctave(1, 2.0f, 0.0f) - 1.0f) < 1e-5f, "DOWN starts above");
    CHECK(fabsf(genViewSweepOctave(1, 2.0f, 1.0f) + 1.0f) < 1e-5f, "DOWN ends below");
    CHECK(fabsf(genViewSweepOctave(2, 4.0f, 0.5f) - 2.0f) < 1e-5f, "UP-DOWN peaks in the middle: +SWEEP/2");
    CHECK(fabsf(genViewSweepOctave(3, 4.0f, 0.5f) + 2.0f) < 1e-5f, "DOWN-UP dips in the middle");
    CHECK(fabsf(genViewSweepOctave(2, 4.0f, 0.0f) + 2.0f) < 1e-5f, "UP-DOWN starts low");
    CHECK(fabsf(genViewSweepOctave(2, 4.0f, 1.0f) + 2.0f) < 1e-5f, "and ends low");
    // The largest SWEEP (4 octaves) fits the panel's +-2 octaves.
    for (uint8_t shape = 0; shape < 4; shape++) {
        for (int i = 0; i <= 100; i++) {
            const float o = genViewSweepOctave(shape, genSweepOctaves(kGenSweepMax), i / 100.0f);
            CHECK(fabsf(o) <= kGvSweepRangeOct + 1e-4f, "shape %u p %d: %f", shape, i, o);
        }
    }
    CHECK(genViewSweepOctave(0, 0.0f, 0.3f) == 0.0f, "no sweep, no movement");
    // The frequencies the title shows.
    CHECK(fabsf(genViewFreq(GEN_WHITE, 1.0f, 1.0f) - 12000.0f) < 1.0f, "noise cutoff");
    CHECK(fabsf(genViewFreq(GEN_TAPE, 2.0f, 1.0f) - 8000.0f) < 1.0f, "TAPE band follows PITCH");
    CHECK(fabsf(genViewFreq(GEN_SIREN, 1.0f, 4.0f) - 12.0f) < 0.01f, "SIREN: the sweep moves the warble rate (3 Hz x 4), not the pitch");
    CHECK(strcmp(genViewFreqName(GEN_SIREN), "WARBLE") == 0 && strcmp(genViewFreqName(GEN_LASER), "PITCH") == 0, "names");
    CHECK(fabsf(genViewFreq(GEN_LASER, 1.0f, 0.5f) - 400.0f) < 1.0f, "LASER pitch follows the sweep");
    // The filters stop at 0.45 of the sample rate, and so does what the screen says.
    CHECK(fabsf(genViewFreq(GEN_WHITE, 1.0f, 4.0f) - 19845.0f) < 1.0f, "12 kHz x 4 is shown as 19.8 kHz: %f", genViewFreq(GEN_WHITE, 1.0f, 4.0f));
    CHECK(fabsf(genViewFreq(GEN_LASER, 8.0f, 4.0f) - 19845.0f) < 1.0f, "LASER too");
    CHECK(genViewFreq(GEN_RISER, 0.001f, 0.25f) >= 10.0f, "and not below 10 Hz");
    CHECK(genViewIsRate(GEN_DUST) && !genViewIsRate(GEN_PINK), "DUST is a rate");
    CHECK(genViewFreq(GEN_OFF, 1.0f, 1.0f) == 0.0f, "OFF has none");
    float lo, hi;
    genViewWarbleRange(1.0f, lo, hi);
    CHECK((lo > 420.0f) && (lo < 430.0f) && (hi > 840.0f) && (hi < 860.0f), "warble %f..%f", lo, hi);
}

static void testPeaks() {
    const uint32_t len = genLengthSamples(8, 120.0f);  // 8 sixteenths at 120 BPM = 1 s
    for (uint8_t type = GEN_WHITE; type < kGenTypeCount; type++) {
        std::vector<uint8_t> a(kGvNoteCols), b(kGvNoteCols);
        genViewPeaks(type, 1.0f, 1.0f, 0, len, a.data(), kGvNoteCols, static_cast<uint8_t>(kGvNoteHalf));
        genViewPeaks(type, 1.0f, 1.0f, 0, len, b.data(), kGvNoteCols, static_cast<uint8_t>(kGvNoteHalf));
        CHECK(a == b, "type %u: deterministic", type);
        uint32_t sum = 0;
        uint8_t mx = 0;
        for (uint16_t c = 0; c < kGvNoteCols; c++) {
            CHECK(a[c] <= kGvNoteHalf, "type %u col %u: %u over the half height", type, c, a[c]);
            sum += a[c];
            if (a[c] > mx) mx = a[c];
        }
        CHECK(mx >= 3, "type %u: audible (peak %u)", type, mx);
        CHECK(sum > 100u, "type %u: sum %u", type, sum);
    }
    // OFF and empty: nothing.
    std::vector<uint8_t> z(kGvNoteCols, 9);
    genViewPeaks(GEN_OFF, 1.0f, 0.0f, 0, len, z.data(), kGvNoteCols, 52);
    for (uint16_t c = 0; c < kGvNoteCols; c++) CHECK(z[c] == 0, "OFF col %u", c);
    std::fill(z.begin(), z.end(), 9);
    genViewPeaks(GEN_PINK, 1.0f, 0.0f, 0, 0, z.data(), kGvNoteCols, 52);
    for (uint16_t c = 0; c < kGvNoteCols; c++) CHECK(z[c] == 0, "length 0 col %u", c);
    // The LASER is a sine of 0.35: every column peaks at about 0.35 of the height.
    std::vector<uint8_t> l(kGvNoteCols);
    genViewPeaks(GEN_LASER, 1.0f, 0.0f, 0, len, l.data(), kGvNoteCols, 100);
    for (uint16_t c = 8; c < kGvNoteCols; c++) CHECK((l[c] >= 32) && (l[c] <= 36), "LASER col %u: %u", c, l[c]);
    // The RMS body: never above the peak, a sine is 0.707 of its peak, a square is its peak, noise well below.
    {
        std::vector<uint8_t> pk(kGvNoteCols), rm(kGvNoteCols);
        for (uint8_t type = GEN_WHITE; type < kGenTypeCount; type++) {
            genViewPeaks(type, 1.0f, 1.0f, 0, len, pk.data(), kGvNoteCols, 100, rm.data());
            for (uint16_t c = 0; c < kGvNoteCols; c++) CHECK(rm[c] <= pk[c], "type %u col %u: rms %u above peak %u", type, c, rm[c], pk[c]);
        }
        genViewPeaks(GEN_LASER, 1.0f, 0.0f, 0, len, pk.data(), kGvNoteCols, 100, rm.data());
        for (uint16_t c = 8; c < kGvNoteCols; c++) CHECK((rm[c] >= 23) && (rm[c] <= 27), "LASER rms %u (0.35 x 0.707 = 25)", rm[c]);
        genViewPeaks(GEN_WHITE, 1.0f, 0.0f, 0, len, pk.data(), kGvNoteCols, 100, rm.data());
        uint32_t sp = 0, sr = 0;
        for (uint16_t c = 8; c < kGvNoteCols; c++) {
            sp += pk[c];
            sr += rm[c];
        }
        CHECK(sr * 10 < sp * 7, "noise: the body is well inside its peaks (%u of %u)", sr, sp);
        // Without the optional array the peaks are the same.
        std::vector<uint8_t> a(kGvNoteCols), b(kGvNoteCols);
        genViewPeaks(GEN_PINK, 1.0f, 1.0f, 0, len, a.data(), kGvNoteCols, 52);
        genViewPeaks(GEN_PINK, 1.0f, 1.0f, 0, len, b.data(), kGvNoteCols, 52, rm.data());
        CHECK(a == b, "the RMS array does not change the peaks");
    }
    // The first column holds the 2 ms fade-in of a note that starts at full level: no jump at the left edge.
    std::vector<uint8_t> w(kGvNoteCols);
    genViewPeaks(GEN_WHITE, 1.0f, 0.0f, 0, len, w.data(), kGvNoteCols, 100);
    CHECK(w[0] <= w[100] + 15, "first column %u, a later one %u", w[0], w[100]);
    // PITCH moves the noise's tone: a lower low-pass makes a quieter white noise.
    std::vector<uint8_t> bright(kGvNoteCols), dull(kGvNoteCols);
    genViewPeaks(GEN_WHITE, 1.0f, 0.0f, 0, len, bright.data(), kGvNoteCols, 100);
    genViewPeaks(GEN_WHITE, 0.05f, 0.0f, 0, len, dull.data(), kGvNoteCols, 100);
    uint32_t sb = 0, sd = 0;
    for (uint16_t c = 0; c < kGvNoteCols; c++) {
        sb += bright[c];
        sd += dull[c];
    }
    CHECK(sd < sb, "a 600 Hz low-pass is quieter than a 12 kHz one: %u vs %u", sd, sb);
    // A sweep changes the picture of a band-limited sound: RISER swept up is louder at the end than at the start? (its band moves
    // through the noise: not a strict law, so only check that the two pictures differ)
    std::vector<uint8_t> flat(kGvNoteCols), swept(kGvNoteCols);
    genViewPeaks(GEN_RISER, 1.0f, 0.0f, 0, len, flat.data(), kGvNoteCols, 100);
    genViewPeaks(GEN_RISER, 1.0f, 4.0f, 0, len, swept.data(), kGvNoteCols, 100);
    CHECK(flat != swept, "a sweep changes the RISER's picture");
}

// A band-limited measurement of the real generator: the energy of the output in a band, with a second-order band-pass (a biquad).
static float bandEnergy(const std::vector<float>& x, float centreHz) {
    const float w0 = 2.0f * 3.14159265f * centreHz / 44100.0f, alpha = sinf(w0) / (2.0f * 2.0f);
    const float b0 = alpha, b2 = -alpha, a0 = 1.0f + alpha, a1 = -2.0f * cosf(w0), a2 = 1.0f - alpha;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0, sum = 0;
    for (size_t i = 0; i < x.size(); i++) {
        const float y = (b0 * x[i] + b2 * x2 - a1 * y1 - a2 * y2) / a0;
        x2 = x1;
        x1 = x[i];
        y2 = y1;
        y1 = y;
        if (i > 2000) sum += y * y;
    }
    return sum / static_cast<float>(x.size());
}

static std::vector<float> renderNote(uint8_t type, float ratio, uint32_t samples, uint32_t seed) {
    GenVoice v;
    genVoiceStart(v, type, seed, ratio, 0.0f, 0, samples * 4);  // no sweep: a steady sound
    const uint32_t padded = (samples + 31) / 32 * 32;           // whole blocks of 32
    std::vector<float> out(padded);
    for (uint32_t at = 0; at < padded; at += 32) genVoiceRender(v, out.data() + at, 32);
    out.resize(samples);
    return out;
}

// The drawn spectrum is the spectrum the generator makes: compare the shape (dB differences between
// bands) of the analytic curve with a measurement of the real thing.
static void testSpectrumMatchesGenerator() {
    const float bands[5] = {200.0f, 800.0f, 2500.0f, 6000.0f, 14000.0f};
    for (uint8_t type : {GEN_WHITE, GEN_PINK, GEN_BROWN, GEN_TAPE, GEN_RISER}) {
        const std::vector<float> x = renderNote(type, 1.0f, 44100 * 6, 12345u + type);
        float measured[5], analytic[5];
        for (int i = 0; i < 5; i++) {
            measured[i] = 10.0f * log10f(bandEnergy(x, bands[i]) + 1e-20f) - 10.0f * log10f(bands[i]);  // power density: the band is as wide as it is high (Q 2)
            analytic[i] = genViewSpectrumDb(type, 1.0f, 1.0f, bands[i]);
        }
        // Compare relative to the 800 Hz band; the measurement has a +-3 dB statistical and band-width error
        // (a band-pass of Q 2 sees a sloped spectrum over its width: allow for the slope).
        for (int i = 0; i < 5; i++) {
            const float m = measured[i] - measured[1], a = analytic[i] - analytic[1];
            const float tolerance = (type == GEN_RISER) ? 8.0f : 5.0f;
            CHECK(fabsf(m - a) < tolerance, "type %u band %.0f Hz: measured %.1f dB, drawn %.1f dB (against 800 Hz)", type, bands[i], m, a);
        }
    }
}

static void testSpectrumShapes() {
    // WHITE: flat below its cutoff, falling above it.
    CHECK(fabsf(genViewSpectrumDb(GEN_WHITE, 1.0f, 1.0f, 100.0f)) < 0.1f, "white is flat at 100 Hz");
    CHECK(genViewSpectrumDb(GEN_WHITE, 1.0f, 1.0f, 19000.0f) < -4.0f, "and falls towards 20 kHz");
    // The cutoff moves with PITCH and with the sweep factor.
    CHECK(genViewSpectrumDb(GEN_WHITE, 0.25f, 1.0f, 6000.0f) < genViewSpectrumDb(GEN_WHITE, 1.0f, 1.0f, 6000.0f) - 6.0f, "PITCH lowers the low-pass");
    CHECK(genViewSpectrumDb(GEN_WHITE, 1.0f, 0.25f, 6000.0f) < genViewSpectrumDb(GEN_WHITE, 1.0f, 1.0f, 6000.0f) - 6.0f, "the sweep factor moves it too");
    // PINK falls 3 dB per octave.
    const float pinkSlope = genViewSpectrumDb(GEN_PINK, 1.0f, 1.0f, 100.0f) - genViewSpectrumDb(GEN_PINK, 1.0f, 1.0f, 1000.0f);
    CHECK(fabsf(pinkSlope - 10.0f) < 0.3f, "pink: 10 dB over a decade, got %f", pinkSlope);
    // BROWN falls about 6 dB per octave above 140 Hz, so more than pink.
    const float brownSlope = genViewSpectrumDb(GEN_BROWN, 1.0f, 1.0f, 500.0f) - genViewSpectrumDb(GEN_BROWN, 1.0f, 1.0f, 5000.0f);
    CHECK((brownSlope > 17.0f) && (brownSlope < 21.0f), "brown: about 20 dB over a decade, got %f", brownSlope);
    // TAPE is pink noise times a band gain (1.6 x band-pass + 0.25): the gain peaks at its band, 4 kHz, and moves with PITCH.
    for (float ratio : {1.0f, 2.0f}) {
        float best = -1000.0f, bestHz = 0.0f;
        for (int c = 0; c < 560; c++) {
            const float hz = gvHzAtColumn(static_cast<uint16_t>(c), 560);
            if (hz > 15000.0f) break;
            const float gain = genViewSpectrumDb(GEN_TAPE, ratio, 1.0f, hz) - gvDb(gvPinkMag(hz));
            if (gain > best) {
                best = gain;
                bestHz = hz;
            }
        }
        CHECK(fabsf(bestHz - 4000.0f * ratio) < 600.0f * ratio, "TAPE band gain peaks at %f Hz for PITCH %f", bestHz, ratio);
        CHECK(fabsf(best - gvDb(1.6f + 0.25f)) < 0.6f, "TAPE band gain %f dB", best);
    }
    float best = -1000.0f, bestHz = 0.0f;
    // RISER: a narrow peak at 1.5 kHz (Q 5), 2x PITCH moves it to 3 kHz.
    for (float ratio : {1.0f, 2.0f}) {
        best = -1000.0f;
        bestHz = 0.0f;
        for (int c = 0; c < 560; c++) {
            const float hz = gvHzAtColumn(static_cast<uint16_t>(c), 560);
            const float db = genViewSpectrumDb(GEN_RISER, ratio, 1.0f, hz);
            if (db > best) {
                best = db;
                bestHz = hz;
            }
        }
        CHECK(fabsf(bestHz - 1500.0f * ratio) < 120.0f * ratio, "RISER peak at %f Hz for PITCH %f", bestHz, ratio);
        CHECK(fabsf(best - gvDb(1.6f)) < 0.6f, "RISER peak level %f dB (1.6 = 4.1 dB)", best);
    }
    // DUST: a hiss floor and a flat click floor, never below the clicks.
    CHECK(genViewSpectrumDb(GEN_DUST, 1.0f, 1.0f, 15000.0f) > gvDb(0.049f), "DUST: the click floor stays");
    // The tonal types and OFF are not drawn as curves.
    CHECK(genViewSpectrumDb(GEN_SIREN, 1.0f, 1.0f, 1000.0f) < -100.0f, "SIREN has lines");
    CHECK(genViewSpectrumDb(GEN_OFF, 1.0f, 1.0f, 1000.0f) < -100.0f, "OFF has nothing");
    // Finite everywhere, for every type and every column, also at the extremes of PITCH and the sweep.
    for (uint8_t type = GEN_WHITE; type < kGenTypeCount; type++) {
        for (float ratio : {0.05f, 1.0f, 8.0f}) {
            for (float mul : {0.25f, 1.0f, 4.0f}) {
                for (int c = 0; c < 280; c++) {
                    const float db = genViewSpectrumDb(type, ratio, mul, gvHzAtColumn(static_cast<uint16_t>(c), 280));
                    CHECK(std::isfinite(db), "type %u ratio %f mul %f col %d: %f", type, ratio, mul, c, db);
                }
            }
        }
    }
}

static void testLines() {
    float hz[40], db[40];
    CHECK(genViewLines(GEN_PINK, 1.0f, 1.0f, hz, db, 40) == 0, "noise has no lines");
    CHECK(genViewLines(GEN_OFF, 1.0f, 1.0f, hz, db, 40) == 0, "OFF has none");
    uint8_t n = genViewLines(GEN_LASER, 1.0f, 1.0f, hz, db, 40);
    CHECK((n == 1) && (fabsf(hz[0] - 800.0f) < 0.1f) && (db[0] == 0.0f), "LASER: one line at 800 Hz");
    n = genViewLines(GEN_LASER, 2.0f, 2.0f, hz, db, 40);
    CHECK((n == 1) && (fabsf(hz[0] - 3200.0f) < 0.1f), "LASER follows PITCH and the sweep: %f", hz[0]);
    n = genViewLines(GEN_LASER, 8.0f, 4.0f, hz, db, 40);
    CHECK((n == 1) && (hz[0] <= 0.45f * 44100.0f + 0.1f), "LASER stays below Nyquist: %f", hz[0]);
    n = genViewLines(GEN_SIREN, 1.0f, 1.0f, hz, db, 40);
    CHECK(n >= 10, "SIREN: odd harmonics up to 20 kHz: %u", n);
    CHECK(fabsf(hz[0] - 600.0f) < 0.1f && fabsf(hz[1] - 1800.0f) < 0.1f && fabsf(hz[2] - 3000.0f) < 0.1f, "odd harmonics 1, 3, 5");
    CHECK(fabsf((db[1] - db[0]) + 9.54f) < 1.5f, "the third harmonic is 9.5 dB down (1/3, plus the softening): %f", db[1] - db[0]);
    for (uint8_t i = 0; i < n; i++) CHECK((hz[i] <= kGvSpecMaxHz) && std::isfinite(db[i]), "line %u: %f Hz %f dB", i, hz[i], db[i]);
    n = genViewLines(GEN_SIREN, 1.0f, 1.0f, hz, db, 3);
    CHECK(n == 3, "capped at the room given: %u", n);
    n = genViewLines(GEN_SIREN, 4.0f, 1.0f, hz, db, 40);
    CHECK((n >= 1) && (n < 10), "a high pitch has fewer lines below 20 kHz: %u", n);
    // The measured siren has its energy where the lines are.
    const std::vector<float> x = renderNote(GEN_SIREN, 1.0f, 44100 * 4, 777u);
    CHECK(bandEnergy(x, 600.0f) > bandEnergy(x, 20000.0f) * 10.0f, "the siren's energy is at its pitch");
}

static void testHiss() {
    for (uint8_t type = HISS_TAPE; type < kHissTypeCount; type++) {
        std::vector<uint8_t> l(kHvTextureCols), r(kHvTextureCols), l2(kHvTextureCols), r2(kHvTextureCols);
        hissViewTexture(type, 60, 70, l.data(), r.data(), kHvTextureCols, static_cast<uint8_t>(kHvTextureHalf));
        hissViewTexture(type, 60, 70, l2.data(), r2.data(), kHvTextureCols, static_cast<uint8_t>(kHvTextureHalf));
        CHECK((l == l2) && (r == r2), "hiss %u: deterministic", type);
        uint32_t sl = 0, sr = 0;
        for (uint16_t c = 0; c < kHvTextureCols; c++) {
            CHECK((l[c] <= kHvTextureHalf) && (r[c] <= kHvTextureHalf), "hiss %u col %u", type, c);
            sl += l[c];
            sr += r[c];
        }
        CHECK((sl > 500u) && (sr > 500u), "hiss %u: audible on both sides (%u, %u)", type, sl, sr);
        CHECK(l != r, "hiss %u: the two sides are different noises", type);
    }
    // Louder LEVEL: taller texture.
    std::vector<uint8_t> quiet(kHvTextureCols), loud(kHvTextureCols), dummy(kHvTextureCols);
    hissViewTexture(HISS_PINK, 20, 70, quiet.data(), dummy.data(), kHvTextureCols, static_cast<uint8_t>(kHvTextureHalf));
    hissViewTexture(HISS_PINK, 90, 70, loud.data(), dummy.data(), kHvTextureCols, static_cast<uint8_t>(kHvTextureHalf));
    uint32_t sq = 0, sl = 0;
    for (uint16_t c = 0; c < kHvTextureCols; c++) {
        sq += quiet[c];
        sl += loud[c];
    }
    CHECK(sl > sq, "LEVEL 90 is taller than 20: %u vs %u", sl, sq);
    // The texture's RMS body is inside its peaks, and the pops of VINYL stand out of it.
    {
        std::vector<uint8_t> pl(kHvTextureCols), pr(kHvTextureCols), rl(kHvTextureCols), rr(kHvTextureCols);
        for (uint8_t type = HISS_TAPE; type < kHissTypeCount; type++) {
            hissViewTexture(type, 60, 70, pl.data(), pr.data(), kHvTextureCols, 40, rl.data(), rr.data());
            for (uint16_t c = 0; c < kHvTextureCols; c++) CHECK((rl[c] <= pl[c]) && (rr[c] <= pr[c]), "hiss %u col %u: rms above peak", type, c);
        }
        hissViewTexture(HISS_VINYL, 60, 70, pl.data(), pr.data(), kHvTextureCols, 40, rl.data(), rr.data());
        uint8_t widest = 0;
        for (uint16_t c = 0; c < kHvTextureCols; c++) widest = std::max<uint8_t>(widest, static_cast<uint8_t>(pl[c] - rl[c]));
        CHECK(widest >= 6, "a pop stands out of the body: %u", widest);
    }
    // VINYL has pops: some columns stand clearly above the median.
    std::vector<uint8_t> v(kHvTextureCols), vr(kHvTextureCols);
    hissViewTexture(HISS_VINYL, 60, 70, v.data(), vr.data(), kHvTextureCols, static_cast<uint8_t>(kHvTextureHalf));
    std::vector<uint8_t> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    CHECK(sorted.back() >= sorted[sorted.size() / 2] + 6, "VINYL: pops above the floor: median %u, max %u", sorted[sorted.size() / 2], sorted.back());
    // Any number of columns (it streams, with constant memory): the same picture at 520 and 2000, each column filled.
    for (uint16_t cols : {static_cast<uint16_t>(1), static_cast<uint16_t>(7), static_cast<uint16_t>(520), static_cast<uint16_t>(2000)}) {
        std::vector<uint8_t> a(cols), b(cols), ra(cols), rb(cols);
        hissViewTexture(HISS_PINK, 60, 70, a.data(), b.data(), cols, 40, ra.data(), rb.data());
        for (uint16_t c = 0; c < cols; c++) CHECK((a[c] > 0) && (b[c] > 0) && (ra[c] > 0) && (rb[c] > 0), "%u columns: column %u is empty", cols, c);
    }
    // OFF and out of range: nothing.
    std::vector<uint8_t> z(kHvTextureCols, 5), z2(kHvTextureCols, 5);
    hissViewTexture(HISS_OFF, 60, 70, z.data(), z2.data(), kHvTextureCols, 40);
    for (uint16_t c = 0; c < kHvTextureCols; c++) CHECK((z[c] == 0) && (z2[c] == 0), "OFF col %u", c);
    hissViewTexture(77, 60, 70, z.data(), z2.data(), kHvTextureCols, 40);
    for (uint16_t c = 0; c < kHvTextureCols; c++) CHECK((z[c] == 0) && (z2[c] == 0), "bad type col %u", c);

    // Spectrum: the TONE low-pass moves with TONE.
    CHECK(hissViewSpectrumDb(HISS_WHITE, 20, 12000.0f) < hissViewSpectrumDb(HISS_WHITE, 90, 12000.0f) - 6.0f, "a low TONE cuts the highs");
    CHECK(fabsf(hissViewSpectrumDb(HISS_WHITE, 70, 100.0f) - gvDb(0.5f)) < 0.2f, "WHITE: flat at 0.5 below the cutoff");
    const float pink = hissViewSpectrumDb(HISS_PINK, 100, 100.0f) - hissViewSpectrumDb(HISS_PINK, 100, 1000.0f);
    CHECK(fabsf(pink - 10.0f) < 0.5f, "PINK bed: 10 dB per decade, got %f", pink);
    float best = -1000.0f, bestHz = 0.0f;
    for (int c = 0; c < 280; c++) {
        const float hz = gvHzAtColumn(static_cast<uint16_t>(c), 280);
        if (hz > 12000.0f) break;
        const float gain = hissViewSpectrumDb(HISS_TAPE, 100, hz) - hissViewSpectrumDb(HISS_PINK, 100, hz);  // TAPE = the pink bed times a band gain
        if (gain > best) {
            best = gain;
            bestHz = hz;
        }
    }
    CHECK((bestHz > 2500.0f) && (bestHz < 5000.0f), "TAPE bed band gain peaks at %f Hz", bestHz);
    CHECK(hissViewSpectrumDb(HISS_OFF, 70, 1000.0f) < -100.0f, "OFF: nothing");
    for (uint8_t type = HISS_TAPE; type < kHissTypeCount; type++) {
        for (uint8_t tone : {0, 50, 100}) {
            for (int c = 0; c < 280; c++) CHECK(std::isfinite(hissViewSpectrumDb(type, tone, gvHzAtColumn(static_cast<uint16_t>(c), 280))), "hiss %u tone %u col %d", type, tone, c);
        }
    }
    // The level text: LEVEL 100 = -18 dBFS, 50 = -30 dBFS.
    CHECK(fabsf(hissViewLevelDb(100) + 18.06f) < 0.1f, "100: %f", hissViewLevelDb(100));
    CHECK(fabsf(hissViewLevelDb(50) + 30.1f) < 0.1f, "50: %f", hissViewLevelDb(50));
    CHECK(hissViewLevelDb(0) < -150.0f, "0 is silence");
}

static void testDuckMeter() {
    CHECK(hissViewGainPx(1.0f, 520) == 520, "no duck: full");
    CHECK(hissViewGainPx(0.5f, 520) == 260, "half");
    CHECK(hissViewGainPx(0.0f, 520) == 0, "none");
    CHECK(hissViewGainPx(-1.0f, 520) == 0 && hissViewGainPx(3.0f, 520) == 520, "clipped");
    CHECK(hissViewGainPx(NAN, 520) == 0, "NaN is empty");
    CHECK(hissViewFloorPx(0, 520) == 520, "DUCK 0: the floor is the full bar");
    CHECK(hissViewFloorPx(100, 520) == 0, "DUCK 100: down to nothing");
    CHECK(hissViewFloorPx(40, 520) == 312, "DUCK 40: the bed falls to 60 %%");
    CHECK(hissViewFloorPx(250, 520) == 0, "out of range clamps");
}

int main() {
    testHelpers();
    testSweep();
    testPeaks();
    testSpectrumMatchesGenerator();
    testSpectrumShapes();
    testLines();
    testHiss();
    testDuckMeter();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
