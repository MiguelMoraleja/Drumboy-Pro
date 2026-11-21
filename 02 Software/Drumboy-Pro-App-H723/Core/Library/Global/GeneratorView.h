// Generator screens (backlog 66): what the lane area shows while a layer's GEN is on (instrument
// page 2) and on the HISS page (the third DUB page). Hardware-free so it can be tested on a PC
// (see 02 Software/tests, test_generatorview).
//
// GEN view (instrument page 2, in place of the sample waveform):
//   NOTE     the note as it plays: 520 columns, each the peak of a short burst of the real generator
//            (Generator.h) at that point of the note, with a tick for every 16th of its LENGTH
//   SWEEP    where the generator's frequency goes over the note (DIRECTION shape, SWEEP octaves),
//            with the frequencies at the start, the middle and the end
//   SPECTRUM the sound's spectrum from 20 Hz to 20 kHz at the start, the middle and the end of the
//            note (computed from the same filters the generator runs), or its lines for SIREN, LASER
//
// HISS view (DUB page 3):
//   TEXTURE  one second of the bed, left above and right below, in dBFS (crackle, wow, rumble)
//   SPECTRUM the bed's spectrum with the TONE low-pass
//   DUCK     live: how far the loudest layer pushes the bed down, and the floor DUCK allows
#ifndef __GENERATORVIEW_H
#define __GENERATORVIEW_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DspMath.h"  // finiteOrZero: a float compare does not catch NaN in the firmware build
#include "Generator.h"

/* Layout --------------------------------------------------------------------*/

const uint16_t kGvTitleX = 60, kGvTitleY = 134;

// NOTE waveform (the left column, top)
const uint16_t kGvNoteX = 60, kGvNoteCols = 520, kGvNoteCenterY = 226, kGvNoteHalf = 52;
const uint16_t kGvNoteLabelY = 158, kGvNoteTickY = 282;

// SWEEP path (the left column, bottom)
const uint16_t kGvSweepX = 60, kGvSweepW = 520, kGvSweepTop = 336, kGvSweepH = 64, kGvSweepLabelY = 318, kGvSweepTextY = 406;
const float kGvSweepRangeOct = 2.0f;  // the panel shows +-2 octaves around the PITCH frequency: the largest SWEEP (4 octaves) fits

// SPECTRUM (the right column)
const uint16_t kGvSpecX = 620, kGvSpecW = 280, kGvSpecTop = 172, kGvSpecH = 200, kGvSpecLabelY = 158, kGvSpecScaleY = 378, kGvSpecTextY = 396;
const float kGvSpecMinHz = 20.0f, kGvSpecMaxHz = 20000.0f;
const float kGvSpecRangeDb = 48.0f;  // 0 dB at the top (the loudest point of the three curves), -48 dB at the bottom

// HISS view: texture left, spectrum right, the duck meter across the bottom
const uint16_t kHvTextureX = 60, kHvTextureCols = 520, kHvTextureHalf = 40, kHvTextureLTop = 172, kHvTextureRTop = 262;  // L: y 172..252, R: 262..342 (centre lines at 212 and 302)
const uint16_t kHvTextureLabelY = 158;
const float kHvTextureFloorDb = 60.0f;  // the texture's height is dBFS: -60 dB (nothing) to 0 dB (full height)
const uint16_t kHvDuckX = 60, kHvDuckW = 520, kHvDuckY = 380, kHvDuckH = 14, kHvDuckLabelY = 366, kHvDuckTextY = 400;

// Peaks of the NOTE: a burst of kGvBurst samples at each column, after kGvWarm samples to settle the filters.
const uint16_t kGvWarm = 192, kGvBurst = 256;

/* Frequencies -----------------------------------------------------------------*/

// Column 0..cols-1 -> frequency, logarithmic from 20 Hz to 20 kHz.
inline float gvHzAtColumn(uint16_t c_, uint16_t cols_) {
    const float t = (cols_ > 1) ? static_cast<float>(c_) / static_cast<float>(cols_ - 1) : 0.0f;
    return kGvSpecMinHz * powf(kGvSpecMaxHz / kGvSpecMinHz, t);
}

// Frequency -> x inside a panel of width w_ starting at x_.
inline uint16_t gvXOfHz(float hz_, uint16_t x_, uint16_t w_) {
    if (hz_ < kGvSpecMinHz) hz_ = kGvSpecMinHz;
    if (hz_ > kGvSpecMaxHz) hz_ = kGvSpecMaxHz;
    const float t = log10f(hz_ / kGvSpecMinHz) / log10f(kGvSpecMaxHz / kGvSpecMinHz);
    return static_cast<uint16_t>(x_ + static_cast<uint16_t>(t * static_cast<float>(w_ - 1) + 0.5f));
}

// dB below the top -> y inside the spectrum panel (0 dB = top, -kGvSpecRangeDb = bottom). Clipped.
inline uint16_t gvYOfDb(float db_, uint16_t top_, uint16_t h_) {
    if (db_ > 0.0f) db_ = 0.0f;
    if (db_ < -kGvSpecRangeDb) db_ = -kGvSpecRangeDb;
    return static_cast<uint16_t>(top_ + static_cast<uint16_t>((-db_ / kGvSpecRangeDb) * static_cast<float>(h_ - 1) + 0.5f));
}

inline float gvDb(float mag_) { return 20.0f * log10f((mag_ > 1.0e-9f) ? mag_ : 1.0e-9f); }

/* Filter responses (the generator's own state-variable filter) -------------------*/

struct GvC {
    float re, im;
};
inline GvC gvDiv(float aRe_, float aIm_, float bRe_, float bIm_) {
    const float d = bRe_ * bRe_ + bIm_ * bIm_;
    return GvC{(aRe_ * bRe_ + aIm_ * bIm_) / d, (aIm_ * bRe_ - aRe_ * bIm_) / d};
}
inline float gvMag(const GvC& c_) { return sqrtf(c_.re * c_.re + c_.im * c_.im); }

// The response of genSvfSet(fc, q) at hz: x is the warped frequency ratio of the bilinear transform.
inline void gvSvf(float hz_, float fc_, float q_, GvC& lp_, GvC& bp_) {
    if (fc_ < 10.0f) fc_ = 10.0f;
    if (fc_ > 0.45f * kGenSampleRate) fc_ = 0.45f * kGenSampleRate;
    if (hz_ > 0.499f * kGenSampleRate) hz_ = 0.499f * kGenSampleRate;
    const float x = tanf(kGenPi * hz_ * kGenInvSampleRate) / tanf(kGenPi * fc_ * kGenInvSampleRate);
    const float k = 1.0f / q_;
    // H(s) = 1 / (1 - x^2 + j k x): low-pass; the band-pass is j k x times it (0 dB at the centre).
    lp_ = gvDiv(1.0f, 0.0f, 1.0f - x * x, k * x);
    bp_ = gvDiv(0.0f, k * x, 1.0f - x * x, k * x);
}

// The noise colours as amplitude against white noise: pink falls 3 dB per octave (0 dB at 1 kHz), brown
// is the leaky integrator of genBrownTick (about 6 dB per octave above 140 Hz).
inline float gvPinkMag(float hz_) { return sqrtf(1000.0f / hz_); }
inline float gvBrownMag(float hz_) {
    const float a = 1.0f / 1.02f;
    const float w = 2.0f * kGenPi * hz_ * kGenInvSampleRate;
    const float d = 1.0f - 2.0f * a * cosf(w) + a * a;
    const float ref = 1.0f - 2.0f * a * cosf(2.0f * kGenPi * 1000.0f * kGenInvSampleRate) + a * a;
    return sqrtf(ref / d);  // 1 at 1 kHz
}

/* The sweep -------------------------------------------------------------------*/

inline const char* genViewShapeName(uint8_t shape_) {
    static const char* const kName[4] = {"UP", "DOWN", "UP-DOWN", "DOWN-UP"};
    return kName[shape_ & 3];
}

// Octaves away from the PITCH frequency at progress p_ (0..1) of the note.
inline float genViewSweepOctave(uint8_t shape_, float octaves_, float p_) { return octaves_ * (genShape(shape_, p_) - 0.5f); }

// A frequency as text: 800 Hz, 12.0 kHz.
inline int gvFormatHz(char* out_, size_t size_, float hz_) {
    if (hz_ < 10.0f) return snprintf(out_, size_, "%u.%u Hz", static_cast<unsigned>(hz_), static_cast<unsigned>(fmodf(hz_, 1.0f) * 10.0f));
    if (hz_ < 1000.0f) return snprintf(out_, size_, "%u Hz", static_cast<unsigned>(hz_ + 0.5f));
    return snprintf(out_, size_, "%u.%u kHz", static_cast<unsigned>(hz_ / 1000.0f), static_cast<unsigned>(fmodf(hz_, 1000.0f) / 100.0f));
}

// The frequency a generator is "at": the cutoff of the noises, the band of TAPE and RISER, the pitch
// of LASER, the warble rate of SIREN (the sweep moves it; its pitch is in the spectrum), the clicks
// per second of DUST (then in per second, not Hz: see genViewIsRate).
// The filters and the oscillator stop at 0.45 of the sample rate (genSvfSet, GEN_LASER): the screen says so too.
inline float gvClampHz(float hz_) {
    const float top = 0.45f * kGenSampleRate;
    return (hz_ > top) ? top : ((hz_ < 10.0f) ? 10.0f : hz_);
}

inline float genViewFreq(uint8_t type_, float ratio_, float mul_) {
    switch (type_) {
        case GEN_WHITE:
        case GEN_PINK:
        case GEN_BROWN:
            return gvClampHz(kGenNoiseToneHz * ratio_ * mul_);
        case GEN_TAPE:
            return gvClampHz(kGenTapeHz * ratio_ * mul_);
        case GEN_DUST:
            return kGenDustPerSecond * ratio_ * mul_;
        case GEN_SIREN:
            return kGenSirenRateHz * mul_;
        case GEN_RISER:
            return gvClampHz(kGenRiserHz * ratio_ * mul_);
        case GEN_LASER:
            return gvClampHz(kGenLaserHz * ratio_ * mul_);
        default:
            return 0.0f;
    }
}
inline bool genViewIsRate(uint8_t type_) { return type_ == GEN_DUST; }

// What the frequency is called in the SWEEP panel's title for this type.
inline const char* genViewFreqName(uint8_t type_) {
    switch (type_) {
        case GEN_WHITE:
        case GEN_PINK:
        case GEN_BROWN:
            return "LOW-PASS";
        case GEN_TAPE:
        case GEN_RISER:
            return "BAND";
        case GEN_DUST:
            return "CLICKS";
        case GEN_SIREN:
            return "WARBLE";
        case GEN_LASER:
            return "PITCH";
        default:
            return "";
    }
}

inline int genViewFormatFreq(char* out_, size_t size_, uint8_t type_, float freq_) {
    if (genViewIsRate(type_)) return snprintf(out_, size_, "%u.%u /s", static_cast<unsigned>(freq_), static_cast<unsigned>(fmodf(freq_, 1.0f) * 10.0f));
    return gvFormatHz(out_, size_, freq_);
}

// SIREN's pitch warbles half an octave each way around its pitch: its range, as Hz.
inline void genViewWarbleRange(float ratio_, float& lo_, float& hi_) {
    lo_ = kGenSirenHz * ratio_ * exp2f(-0.5f);
    hi_ = kGenSirenHz * ratio_ * exp2f(0.5f);
}

/* The NOTE waveform -------------------------------------------------------------*/

// out_[c] = 0..half_: the peak of the generator at column c of the note, from a burst of kGvBurst
// samples (the filters settled by kGvWarm samples before it); rms_[c] (optional) the RMS of the same
// burst on the same scale, so the screen can show the body of the sound inside its peaks.
// Deterministic. length_ is in samples.
inline void genViewPeaks(uint8_t type_, float ratio_, float octaves_, uint8_t shape_, uint32_t length_, uint8_t* out_, uint16_t cols_, uint8_t half_, uint8_t* rms_ = nullptr) {
    for (uint16_t c = 0; c < cols_; c++) {
        out_[c] = 0;
        if (rms_) rms_[c] = 0;
    }
    if ((type_ == GEN_OFF) || (type_ >= kGenTypeCount) || (length_ == 0) || (cols_ == 0)) return;
    float block[64];
    for (uint16_t c = 0; c < cols_; c++) {
        const uint32_t at = static_cast<uint32_t>((static_cast<uint64_t>(c) * length_) / cols_);
        const uint32_t from = (at >= kGvWarm) ? at - kGvWarm : 0;
        GenVoice v;
        genVoiceStart(v, type_, 0x2545F491u + c * 0x9E3779B9u, ratio_, octaves_, shape_, length_);
        v.pos = from;
        uint32_t done = 0;
        float peak = 0.0f, sumSquares = 0.0f;
        const uint32_t total = (at - from) + kGvBurst;
        while (done < total) {
            const uint16_t n = static_cast<uint16_t>(((total - done) > 64) ? 64 : (total - done));
            genVoiceRender(v, block, n);
            for (uint16_t i = 0; i < n; i++) {
                if ((done + i) >= (at - from)) {
                    const float a = fabsf(block[i]);
                    if (a > peak) peak = a;
                    sumSquares += a * a;
                }
            }
            done += n;
        }
        if (peak > 1.0f) peak = 1.0f;
        uint8_t h = static_cast<uint8_t>(peak * static_cast<float>(half_) + 0.5f);
        if ((h == 0) && (peak > 0.0f)) h = 1;
        out_[c] = h;
        if (rms_) {
            float rms = sqrtf(sumSquares / static_cast<float>(kGvBurst));
            if (rms > peak) rms = peak;
            uint8_t r = static_cast<uint8_t>(rms * static_cast<float>(half_) + 0.5f);
            if (r > h) r = h;
            rms_[c] = r;
        }
    }
}

/* Spectrum of a generator ------------------------------------------------------------*/

// The spectrum of the noise types at hz_ with the sweep factor mul_, in dB against a flat 0 dB (the
// loudest point of the curves is found by the caller). Tonal types (SIREN, LASER) have lines instead.
inline float genViewSpectrumDb(uint8_t type_, float ratio_, float mul_, float hz_) {
    GvC lp, bp;
    switch (type_) {
        case GEN_WHITE:
            gvSvf(hz_, kGenNoiseToneHz * ratio_ * mul_, 0.707f, lp, bp);
            return gvDb(gvMag(lp));
        case GEN_PINK:
            gvSvf(hz_, kGenNoiseToneHz * ratio_ * mul_, 0.707f, lp, bp);
            return gvDb(gvMag(lp) * gvPinkMag(hz_));
        case GEN_BROWN:
            gvSvf(hz_, kGenNoiseToneHz * ratio_ * mul_, 0.707f, lp, bp);
            return gvDb(gvMag(lp) * gvBrownMag(hz_));
        case GEN_TAPE: {
            // x = 1.6 * band-pass + 0.25 * the pink noise itself
            gvSvf(hz_, kGenTapeHz * ratio_ * mul_, 0.6f, lp, bp);
            const GvC sum{1.6f * bp.re + 0.25f, 1.6f * bp.im};
            return gvDb(gvMag(sum) * gvPinkMag(hz_));
        }
        case GEN_DUST: {
            // the hiss floor under the clicks (0.12 of a pink noise through a low-pass) and the clicks' flat spectrum
            gvSvf(hz_, 7000.0f * ratio_, 0.707f, lp, bp);
            const float floorMag = 0.12f * gvMag(lp) * gvPinkMag(hz_);
            const float clickMag = 0.05f;
            return gvDb(sqrtf(floorMag * floorMag + clickMag * clickMag));
        }
        case GEN_RISER:
            gvSvf(hz_, kGenRiserHz * ratio_ * mul_, 5.0f, lp, bp);
            return gvDb(1.6f * gvMag(bp));
        default:
            return -200.0f;
    }
}

// The lines of the tonal types: SIREN's square (odd harmonics, 1/n, through the 6 kHz softening) and
// LASER's sine. Returns the count (0 for the noise types).
inline uint8_t genViewLines(uint8_t type_, float ratio_, float mul_, float* hz_, float* db_, uint8_t max_) {
    uint8_t n = 0;
    if (type_ == GEN_LASER) {
        float f = kGenLaserHz * ratio_ * mul_;
        if (f > 0.45f * kGenSampleRate) f = 0.45f * kGenSampleRate;
        if ((max_ > 0) && (f >= kGvSpecMinHz)) {
            hz_[0] = f;
            db_[0] = 0.0f;
            n = 1;
        }
    } else if (type_ == GEN_SIREN) {
        const float f0 = kGenSirenHz * ratio_;
        for (uint8_t h = 1; (h < 40) && (n < max_); h += 2) {
            const float f = f0 * static_cast<float>(h);
            if (f > kGvSpecMaxHz) break;
            GvC lp, bp;
            gvSvf(f, 6000.0f, 0.707f, lp, bp);
            hz_[n] = f;
            db_[n] = gvDb(gvMag(lp) / static_cast<float>(h));
            n++;
        }
    }
    return n;
}

/* The hiss bed ----------------------------------------------------------------------*/

// The bed's spectrum at hz_ (dB against flat): the source of each type through the TONE low-pass.
inline float hissViewSpectrumDb(uint8_t type_, uint8_t tone_, float hz_) {
    GvC lp, bp, band;
    gvSvf(hz_, hissToneHz(tone_), 0.707f, lp, bp);
    const float tone = gvMag(lp);
    switch (type_) {
        case HISS_TAPE: {
            gvSvf(hz_, 3500.0f, 0.5f, lp, band);
            const GvC sum{1.6f * band.re + 0.3f, 1.6f * band.im};
            return gvDb(gvMag(sum) * gvPinkMag(hz_) * tone);
        }
        case HISS_VINYL: {
            const float pink = 0.35f * gvPinkMag(hz_) * tone;
            const float rumble = 0.15f * 3.5f * gvBrownMag(hz_) * 0.03f;  // the brown noise is bright only far below the music
            const float clicks = 0.05f;
            return gvDb(sqrtf(pink * pink + rumble * rumble + clicks * clicks));
        }
        case HISS_PINK:
            return gvDb(gvPinkMag(hz_) * tone);
        case HISS_WHITE:
            return gvDb(0.5f * tone);
        default:
            return -200.0f;
    }
}

// The bed's texture: one second of it (kHvSeconds) as peaks in dBFS, left and right, cols_ columns.
// out*_[c] = 0..half_ (half_ = 0 dBFS, 0 = -kHvTextureFloorDb or less). levelDb_: the RMS level of the
// whole second, both channels, in dBFS (what the bed measures; LEVEL only sets the target and each type's
// source has its own loudness: -200 when the bed is off or silent).
inline void hissViewTexture(uint8_t type_, uint8_t level_, uint8_t tone_, uint8_t* outL_, uint8_t* outR_, uint16_t cols_, uint8_t half_, uint8_t* rmsL_ = nullptr, uint8_t* rmsR_ = nullptr,
                            float* levelDb_ = nullptr) {
    for (uint16_t c = 0; c < cols_; c++) {
        outL_[c] = outR_[c] = 0;
        if (rmsL_) rmsL_[c] = 0;
        if (rmsR_) rmsR_[c] = 0;
    }
    if (levelDb_) *levelDb_ = -200.0f;
    if ((type_ == HISS_OFF) || (type_ >= kHissTypeCount) || (cols_ == 0)) return;
    auto height = [half_](float linear_) {
        const float f = (gvDb(linear_) + kHvTextureFloorDb) / kHvTextureFloorDb;
        return static_cast<uint8_t>(((f > 0.0f) ? ((f > 1.0f) ? 1.0f : f) : 0.0f) * static_cast<float>(half_) + 0.5f);
    };
    // Streaming: the running peak and sum of squares of the column being filled, so the memory is constant
    // (this runs on the firmware's small stack).
    HissBed bed;
    memset(&bed, 0, sizeof bed);
    float l[32], r[32];
    const uint32_t total = 44100;
    uint16_t column = 0;
    float peakL = 0.0f, peakR = 0.0f, sumL = 0.0f, sumR = 0.0f, totalSquares = 0.0f;
    uint32_t count = 0, totalCount = 0;
    auto finish = [&](uint16_t c_) {
        totalSquares += sumL + sumR;  // a column's sum is small: the total keeps its precision in a float
        totalCount += 2 * count;
        outL_[c_] = height(peakL);
        outR_[c_] = height(peakR);
        const float n = (count > 0) ? static_cast<float>(count) : 1.0f;
        if (rmsL_) rmsL_[c_] = height(sqrtf(sumL / n));
        if (rmsR_) rmsR_[c_] = height(sqrtf(sumR / n));
        peakL = peakR = sumL = sumR = 0.0f;
        count = 0;
    };
    for (uint32_t at = 0; at < total; at += 32) {
        hissRender(bed, type_, level_, tone_, 0, 0.0f, l, r, 32);
        for (uint16_t i = 0; i < 32; i++) {
            if (at + i >= total) break;  // exactly one second: the last block is cut (a column past the end does not exist)
            const uint16_t c = static_cast<uint16_t>((static_cast<uint64_t>(at + i) * cols_) / total);
            if (c != column) {
                finish(column);
                column = c;
            }
            const float al = fabsf(l[i]), ar = fabsf(r[i]);
            if (al > peakL) peakL = al;
            if (ar > peakR) peakR = ar;
            sumL += al * al;
            sumR += ar * ar;
            count++;
        }
    }
    finish(column);
    if (levelDb_ && (totalCount > 0)) {
        const float rms = sqrtf(totalSquares / static_cast<float>(totalCount));
        *levelDb_ = (rms > 1e-9f) ? gvDb(rms) : -200.0f;
    }
}

// The duck meter: the bed's gain (1 = not ducked) as a bar of w_ pixels, and where the floor DUCK allows sits.
inline uint16_t hissViewGainPx(float gain_, uint16_t w_) {
    gain_ = finiteOrZero(gain_);  // NaN and infinity are tested in the bits (-ffast-math folds a compare away)
    if (gain_ < 0.0f) gain_ = 0.0f;
    if (gain_ > 1.0f) gain_ = 1.0f;
    return static_cast<uint16_t>(gain_ * static_cast<float>(w_) + 0.5f);
}
inline uint16_t hissViewFloorPx(uint8_t duck_, uint16_t w_) {
    const float floorGain = 1.0f - static_cast<float>(duck_ > kHissMax ? kHissMax : duck_) * (1.0f / kHissMax);
    return hissViewGainPx(floorGain, w_);
}

#endif
