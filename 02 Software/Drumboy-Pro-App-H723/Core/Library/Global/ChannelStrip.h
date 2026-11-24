// The channel strip (backlog 71): a sound chain for one layer, run on the layer's own buffer before the layer goes to the
// mixer. Hardware-free so it can be tested on a PC (02 Software/tests, test_channelstrip). Filter 1 and 2 are not part of it.
//
// The order is fixed: ENV, DRIVE, FILTER, EQ, COMP, IMAGE. Each block has its own switch (bit in ChSettings::on); a block that is
// off costs nothing and a strip with every block off leaves the buffer untouched, bit for bit.
//   ENV     attack (a ramp up) and decay (an exponential tail: the time to -60 dB); decay 100 = no decay
//   DRIVE   a soft saturation, run at twice the sample rate (halfband filters) so it does not alias; a full-scale input stays
//           within about 1 dB of full scale (the filter overshoots a clipped wave by up to 9 %); the oversampling filters
//           delay the layer by 7 samples (0.16 ms)
//   FILTER  LP, HP, BP or notch with 6, 12 or 24 dB per octave (FilterCurve.h's design, the same as Filter 1 and 2), and an
//           extra 12 dB high-pass
//   EQ      one band: bell, low shelf or high shelf
//   COMP    peak detector, threshold, ratio, attack, release and makeup, from the layer itself or from another layer's buffer
//           (the sidechain); the gain is worked out once per block and ramped across it
//   IMAGE   the stereo image (backlog 71): SPREAD turns a mono layer into a decorrelated pair (all-pass chains, 0 to 100 %),
//           WIDTH scales the side signal (0 = mono, 50 = as it is, 100 = twice the side), BASS MONO high-passes the side signal
//           (24 dB per octave, corner 40 to 400 Hz) so everything below it is mono: the middle is never touched, so a mono
//           signal comes out exactly as it went in and the lows keep their phase and level. The layer's pan stays where it
//           is (Pan.h) and works on the pair this block makes.
// Settings are 0..100 style indices, so the page, the LFO and the file all speak the same small numbers.
#ifndef __CHANNELSTRIP_H
#define __CHANNELSTRIP_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "DspMath.h"
#include "FilterCurve.h"

enum ChBlock : uint8_t { CH_ENV = 0, CH_DRIVE, CH_FILTER, CH_EQ, CH_COMP, CH_IMAGE, CH_BLOCKS };
const uint8_t kChSide = 10;          // compSide: 0 = the layer itself, 1..10 = that layer's buffer
const uint8_t kChMaxIndex = 100;
const uint8_t kChEqGainSteps = 72;   // eqGain 0..72 = -18 .. +18 dB in half dB
const uint8_t kChRatios = 14;

struct ChSettings {
    uint8_t on;  // bit k = block k (ChBlock) is on
    uint8_t envAttack, envDecay;
    uint8_t driveAmount;
    uint8_t filterType, filterSlope, filterCutoff, filterReson, filterHp;  // type 1 LP, 2 HP, 3 BP, 4 notch; slope 0..2; hp 0 = off
    uint8_t eqType, eqFreq, eqGain, eqQ;                                   // type 0 bell, 1 low shelf, 2 high shelf
    uint8_t compThresh, compRatio, compAttack, compRelease, compMakeup, compSide;
    uint8_t imgWidth, imgBass, imgSpread;                                  // width 0..100 (50 = 100 %), bass mono 0 = off, 1..100 = 40..400 Hz, spread 0..100
};

inline ChSettings chDefaults() {
    ChSettings s;
    memset(&s, 0, sizeof s);
    s.envAttack = 0;
    s.envDecay = 60;
    s.driveAmount = 30;
    s.filterType = 1;
    s.filterSlope = 1;
    s.filterCutoff = 75;  // about 3 kHz
    s.filterReson = 20;
    s.filterHp = 0;
    s.eqType = 0;
    s.eqFreq = 50;  // about 630 Hz
    s.eqGain = 36;  // 0 dB
    s.eqQ = 30;
    s.compThresh = 70;
    s.compRatio = 6;
    s.compAttack = 30;
    s.compRelease = 50;
    s.compMakeup = 0;
    s.compSide = 0;
    s.imgWidth = 50;
    s.imgBass = 0;
    s.imgSpread = 0;
    return s;
}

inline bool chOn(const ChSettings& s_, ChBlock b_) { return (s_.on & (1u << b_)) != 0; }

// --- The parameter scales -------------------------------------------------------------------------------------
inline float chHz(uint8_t i_) { return 20.0f * powf(10.0f, 3.0f * static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_) / 100.0f); }  // 20 Hz .. 20 kHz
inline float chQ(uint8_t i_) { return 0.5f * powf(20.0f, static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_) / 100.0f); }           // 0.5 .. 10
inline float chAttackMs(uint8_t i_) { return 0.5f * static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_); }                          // 0 .. 50 ms
inline float chDecayMs(uint8_t i_) { return (i_ >= kChMaxIndex) ? 0.0f : 10.0f * powf(400.0f, static_cast<float>(i_) / 99.0f); }       // 10 ms .. 4 s; 0 = none
inline float chEqDb(uint8_t i_) { return (static_cast<float>(i_ > kChEqGainSteps ? kChEqGainSteps : i_) - 36.0f) * 0.5f; }              // -18 .. +18 dB
inline float chThreshDb(uint8_t i_) { return -60.0f + 0.6f * static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_); }                 // -60 .. 0 dB
inline float chRatio(uint8_t i_) {
    static const float kRatio[kChRatios] = {1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f, 20.0f};
    return kRatio[i_ < kChRatios ? i_ : kChRatios - 1];
}
inline float chCompAttackMs(uint8_t i_) { return 0.1f * powf(1000.0f, static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_) / 100.0f); }    // 0.1 .. 100 ms
inline float chCompReleaseMs(uint8_t i_) { return 10.0f * powf(200.0f, static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_) / 100.0f); }  // 10 ms .. 2 s
inline float chBassHz(uint8_t i_) { return 40.0f * powf(10.0f, static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_) / 100.0f); }  // 1..100 = 40 .. 400 Hz
inline float chMakeupDb(uint8_t i_) { return 0.3f * static_cast<float>(i_ > kChMaxIndex ? kChMaxIndex : i_); }                          // 0 .. 30 dB

// --- The blocks ----------------------------------------------------------------------------------------------
struct ChBiquadState {
    float z1 = 0.0f, z2 = 0.0f;
};

DSPINLINE float chBiquad(const BiquadCoef& c_, ChBiquadState& s_, float x_) {
    const float y = c_.b0 * x_ + s_.z1;
    s_.z1 = c_.b1 * x_ - c_.a1 * y + s_.z2;
    s_.z2 = c_.b2 * x_ - c_.a2 * y;
    return y;
}

// One EQ band, from the RBJ cookbook.
inline void chEqDesign(uint8_t type_, float hz_, float gainDb_, float q_, float rate_, BiquadCoef& c_) {
    if (hz_ > 0.45f * rate_) hz_ = 0.45f * rate_;
    const float a = powf(10.0f, gainDb_ / 40.0f), w = 6.2831853f * hz_ / rate_, cs = cosf(w), sn = sinf(w), alpha = sn / (2.0f * q_);
    float b0, b1, b2, a0, a1, a2;
    if (type_ == 1) {
        const float r = 2.0f * sqrtf(a) * alpha;
        b0 = a * ((a + 1.0f) - (a - 1.0f) * cs + r), b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cs), b2 = a * ((a + 1.0f) - (a - 1.0f) * cs - r);
        a0 = (a + 1.0f) + (a - 1.0f) * cs + r, a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cs), a2 = (a + 1.0f) + (a - 1.0f) * cs - r;
    } else if (type_ == 2) {
        const float r = 2.0f * sqrtf(a) * alpha;
        b0 = a * ((a + 1.0f) + (a - 1.0f) * cs + r), b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cs), b2 = a * ((a + 1.0f) + (a - 1.0f) * cs - r);
        a0 = (a + 1.0f) - (a - 1.0f) * cs + r, a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cs), a2 = (a + 1.0f) - (a - 1.0f) * cs - r;
    } else {
        b0 = 1.0f + alpha * a, b1 = -2.0f * cs, b2 = 1.0f - alpha * a;
        a0 = 1.0f + alpha / a, a1 = -2.0f * cs, a2 = 1.0f - alpha / a;
    }
    const float inv = 1.0f / a0;
    c_.b0 = b0 * inv, c_.b1 = b1 * inv, c_.b2 = b2 * inv, c_.a1 = a1 * inv, c_.a2 = a2 * inv;
}

// The drive curve: the rational tanh, exact in slope at 0 and limited to +-1 (above |3| it is the limit).
DSPINLINE float chShape(float v_) {
    if (v_ > 3.0f) return 1.0f;
    if (v_ < -3.0f) return -1.0f;
    return v_ * (27.0f + v_ * v_) / (27.0f + 9.0f * v_ * v_);
}
inline float chDriveGain(uint8_t amount_) {
    const float g = 0.25f * static_cast<float>(amount_ > kChMaxIndex ? kChMaxIndex : amount_);
    return (g < 0.05f) ? 0.05f : g;  // 0.05 is linear to the eye; 25 is hard
}

struct ChDriveState {
    float w[8];    // the input, newest first
    float yo[8];   // the shaped in-between samples, newest first
    float ye[4];   // the shaped samples, newest first
    ChDriveState() { memset(this, 0, sizeof *this); }
};

// The drive, one sample, at twice the sample rate: the in-between sample is interpolated with a 15-tap halfband, both are
// shaped, and the pair is filtered and decimated with the same halfband. The output is 7 input samples late.
inline float chDriveSample(ChDriveState& s_, float x_, float g_, float norm_) {
    static const float kH1 = 0.298736f, kH3 = -0.058863f, kH5 = 0.010956f, kH7 = -0.000665f;
    for (int k = 7; k > 0; k--) s_.w[k] = s_.w[k - 1];
    s_.w[0] = x_;
    const float mid = 2.0f * (kH1 * (s_.w[4] + s_.w[3]) + kH3 * (s_.w[5] + s_.w[2]) + kH5 * (s_.w[6] + s_.w[1]) + kH7 * (s_.w[7] + s_.w[0]));
    for (int k = 7; k > 0; k--) s_.yo[k] = s_.yo[k - 1];
    for (int k = 3; k > 0; k--) s_.ye[k] = s_.ye[k - 1];
    s_.yo[0] = chShape(mid * g_) * norm_;
    s_.ye[0] = chShape(s_.w[4] * g_) * norm_;
    return 0.5f * s_.ye[3] + kH1 * (s_.yo[3] + s_.yo[4]) + kH3 * (s_.yo[2] + s_.yo[5]) + kH5 * (s_.yo[1] + s_.yo[6]) + kH7 * (s_.yo[0] + s_.yo[7]);
}

struct ChState {
    // ENV
    float envAttackRamp = 1.0f, envGain = 1.0f;
    // DRIVE
    ChDriveState drive[2];
    // FILTER: a high-pass section and up to two more, per channel; EQ: one band per channel
    ChBiquadState filter[2][3];
    ChBiquadState eq[2];
    // COMP
    float compEnv = 0.0f, compGain = 1.0f;
    // IMAGE: the all-pass chains of the spread, and the bass mono low-passes
    float apA[4], apB[4];
    ChBiquadState bass[2];  // the side signal's high-pass: two sections
    BiquadCoef bassCoef[2];
    // Coefficients, designed again when the settings or the rate change
    BiquadCoef filterCoef[3];
    uint8_t filterSections = 0;
    bool filterHp = false;
    BiquadCoef eqCoef;
    uint32_t designKey = 0xFFFFFFFFu;
    float designRate = 0.0f;
    ChState() {
        memset(apA, 0, sizeof apA);
        memset(apB, 0, sizeof apB);
    }
};

inline void chReset(ChState& st_) { st_ = ChState(); memset(st_.apA, 0, sizeof st_.apA); memset(st_.apB, 0, sizeof st_.apB); }

// A new note: the envelope starts again.
inline void chTrigger(ChState& st_) {
    st_.envAttackRamp = 0.0f;
    st_.envGain = 1.0f;
}

inline uint32_t chDesignKey(const ChSettings& s_) {
    uint32_t h = 2166136261u;
    const uint8_t v[10] = {s_.filterType, s_.filterSlope, s_.filterCutoff, s_.filterReson, s_.filterHp, s_.eqType, s_.eqFreq, s_.eqGain, s_.eqQ, s_.imgBass};
    for (uint8_t k = 0; k < 10; k++) h = (h ^ v[k]) * 16777619u;
    return h;
}

inline void chDesign(const ChSettings& s_, ChState& st_, float rate_) {
    st_.filterSections = 0;
    st_.filterHp = (s_.filterHp > 0);
    if (st_.filterHp) filterDesignSection(2, chHz(s_.filterHp), 0.70710678f, rate_, st_.filterCoef[st_.filterSections++]);
    BiquadCoef a, b;
    const uint8_t n = filterDesign(s_.filterType, s_.filterSlope, chHz(s_.filterCutoff), chQ(s_.filterReson), rate_, a, b);
    st_.filterCoef[st_.filterSections++] = a;
    if (n == 2) st_.filterCoef[st_.filterSections++] = b;
    chEqDesign(s_.eqType, chHz(s_.eqFreq), chEqDb(s_.eqGain), chQ(s_.eqQ), rate_, st_.eqCoef);
    if (s_.imgBass > 0) filterDesign(2, 2, chBassHz(s_.imgBass), 0.70710678f, rate_, st_.bassCoef[0], st_.bassCoef[1]);  // a 24 dB Butterworth high-pass
    st_.designKey = chDesignKey(s_);
    st_.designRate = rate_;
}

// Runs the strip on n_ samples in place. l_ is the layer's buffer; r_ its right channel when stereo_ is true (a stereo
// sample), else null or a scratch buffer: the IMAGE block fills it from the mono signal. side_ is the sidechain buffer of
// the COMP (null: the layer itself). Returns true when r_ holds the right channel afterwards (the layer was stereo, or the
// IMAGE block made it so). A strip with every block off returns at once and leaves the buffers as they are.
inline bool chProcess(const ChSettings& s_, ChState& st_, float* l_, float* r_, bool stereo_, const float* side_, uint16_t n_, float rate_) {
    if ((s_.on & ((1u << CH_BLOCKS) - 1u)) == 0) return stereo_;
    const uint8_t chans = (stereo_ && (r_ != nullptr)) ? 2 : 1;
    float* buf[2] = {l_, r_};
    if ((st_.designKey != chDesignKey(s_)) || (st_.designRate != rate_)) chDesign(s_, st_, rate_);

    // ENV: the ramp and the tail multiply both channels alike
    if (chOn(s_, CH_ENV)) {
        const float atkMs = chAttackMs(s_.envAttack), decMs = chDecayMs(s_.envDecay);
        const float atkStep = (atkMs > 0.0f) ? 1000.0f / (atkMs * rate_) : 1.0f;
        const float decMul = (decMs > 0.0f) ? expf(-6.9077553f * 1000.0f / (decMs * rate_)) : 1.0f;
        float ramp = st_.envAttackRamp, gain = st_.envGain;
        for (uint16_t i = 0; i < n_; i++) {
            const float g = ramp * gain;
            for (uint8_t c = 0; c < chans; c++) buf[c][i] *= g;
            ramp += atkStep;
            if (ramp > 1.0f) ramp = 1.0f;
            gain *= decMul;
        }
        if (gain < 1.0e-9f) gain = 0.0f;
        st_.envAttackRamp = ramp;
        st_.envGain = gain;
    }
    // DRIVE
    if (chOn(s_, CH_DRIVE)) {
        const float g = chDriveGain(s_.driveAmount), norm = 1.0f / chShape(g);
        for (uint8_t c = 0; c < chans; c++) {
            for (uint16_t i = 0; i < n_; i++) buf[c][i] = chDriveSample(st_.drive[c], finiteOrZero(buf[c][i]), g, norm);
        }
    }
    // FILTER (the extra high-pass first)
    if (chOn(s_, CH_FILTER)) {
        for (uint8_t c = 0; c < chans; c++) {
            for (uint16_t i = 0; i < n_; i++) {
                float y = finiteOrZero(buf[c][i]);
                for (uint8_t k = 0; k < st_.filterSections; k++) y = chBiquad(st_.filterCoef[k], st_.filter[c][k], y);
                buf[c][i] = y;
            }
            for (uint8_t k = 0; k < 3; k++) {  // one bad value must not poison the history for good
                if (!isFiniteBits(st_.filter[c][k].z1) || !isFiniteBits(st_.filter[c][k].z2)) st_.filter[c][k] = ChBiquadState();
            }
        }
    }
    // EQ
    if (chOn(s_, CH_EQ)) {
        for (uint8_t c = 0; c < chans; c++) {
            for (uint16_t i = 0; i < n_; i++) buf[c][i] = chBiquad(st_.eqCoef, st_.eq[c], finiteOrZero(buf[c][i]));
            if (!isFiniteBits(st_.eq[c].z1) || !isFiniteBits(st_.eq[c].z2)) st_.eq[c] = ChBiquadState();
        }
    }
    // COMP
    if (chOn(s_, CH_COMP)) {
        const float attack = 1.0f - expf(-1000.0f / (chCompAttackMs(s_.compAttack) * rate_));
        const float release = 1.0f - expf(-1000.0f / (chCompReleaseMs(s_.compRelease) * rate_));
        float env = st_.compEnv;
        for (uint16_t i = 0; i < n_; i++) {
            float x = (side_ != nullptr) ? fabsf(finiteOrZero(side_[i])) : fabsf(finiteOrZero(buf[0][i]));
            if ((side_ == nullptr) && (chans == 2)) {
                const float xr = fabsf(finiteOrZero(buf[1][i]));
                if (xr > x) x = xr;
            }
            env += ((x > env) ? attack : release) * (x - env);
        }
        if (!isFiniteBits(env)) env = 0.0f;
        st_.compEnv = env;
        const float envDb = 20.0f * log10f(env + 1.0e-9f), over = envDb - chThreshDb(s_.compThresh);
        const float reductionDb = (over > 0.0f) ? over * (1.0f - 1.0f / chRatio(s_.compRatio)) : 0.0f;
        const float target = exp2f((chMakeupDb(s_.compMakeup) - reductionDb) * 0.16609640f);  // 10^(dB/20)
        const float from = st_.compGain, step = (target - from) / static_cast<float>(n_ > 0 ? n_ : 1);
        float g = from;
        for (uint16_t i = 0; i < n_; i++) {
            g += step;
            for (uint8_t c = 0; c < chans; c++) buf[c][i] *= g;
        }
        st_.compGain = target;
    }
    // IMAGE: spread (a mono layer becomes a pair), then width, then bass mono
    bool stereoOut = (chans == 2);
    if (chOn(s_, CH_IMAGE) && (r_ != nullptr)) {
        if (chans == 1) {
            const float sp = static_cast<float>(s_.imgSpread > kChMaxIndex ? kChMaxIndex : s_.imgSpread) * 0.01f;
            static const float kA[4] = {0.13f, 0.47f, 0.72f, 0.89f}, kB[4] = {-0.21f, -0.55f, -0.78f, -0.91f};
            for (uint16_t i = 0; i < n_; i++) {
                const float x = finiteOrZero(l_[i]);
                float a = x, b = x;
                if (sp > 0.0f) {
                    for (uint8_t k = 0; k < 4; k++) {  // first-order all-passes: y = c x + z, z = x - c y
                        const float ya = kA[k] * a + st_.apA[k];
                        st_.apA[k] = a - kA[k] * ya;
                        a = ya;
                        const float yb = kB[k] * b + st_.apB[k];
                        st_.apB[k] = b - kB[k] * yb;
                        b = yb;
                    }
                }
                l_[i] = x + sp * (a - x);
                r_[i] = x + sp * (b - x);
            }
            if (!isFiniteBits(st_.apA[0] + st_.apA[1] + st_.apA[2] + st_.apA[3] + st_.apB[0] + st_.apB[1] + st_.apB[2] + st_.apB[3])) {
                memset(st_.apA, 0, sizeof st_.apA);
                memset(st_.apB, 0, sizeof st_.apB);
            }
        }
        const float width = static_cast<float>(s_.imgWidth > kChMaxIndex ? kChMaxIndex : s_.imgWidth) * 0.02f;  // 0 .. 2
        if (width != 1.0f) {
            for (uint16_t i = 0; i < n_; i++) {
                const float m = 0.5f * (l_[i] + r_[i]), sd = 0.5f * (l_[i] - r_[i]) * width;
                l_[i] = m + sd;
                r_[i] = m - sd;
            }
        }
        if (s_.imgBass > 0) {
            for (uint16_t i = 0; i < n_; i++) {
                const float m = 0.5f * (l_[i] + r_[i]);
                const float sd = chBiquad(st_.bassCoef[1], st_.bass[1], chBiquad(st_.bassCoef[0], st_.bass[0], 0.5f * (l_[i] - r_[i])));
                l_[i] = m + sd;
                r_[i] = m - sd;
            }
            for (uint8_t k = 0; k < 2; k++) {
                if (!isFiniteBits(st_.bass[k].z1) || !isFiniteBits(st_.bass[k].z2)) st_.bass[k] = ChBiquadState();
            }
        }
        stereoOut = true;
    }
    // A last guard: nothing that is not a number leaves the strip.
    const uint8_t outChans = stereoOut ? 2 : 1;
    for (uint8_t c = 0; c < outChans; c++) {
        for (uint16_t i = 0; i < n_; i++) buf[c][i] = finiteOrZero(buf[c][i]);
    }
    return stereoOut;
}

// --- The file block: "CHN", version 1, then for each of ten layers its settings --------------------------------
const uint8_t kChSettingBytes = 22;
const uint16_t kChFileSize = 3 + 1 + 10 * kChSettingBytes;  // 224

inline void chWriteBlock(const ChSettings* s_, uint8_t* out_) {
    memcpy(out_, "CHN", 3);
    out_[3] = 1;
    for (uint8_t l = 0; l < 10; l++) {
        const ChSettings& s = s_[l];
        const uint8_t v[kChSettingBytes] = {s.on, s.envAttack, s.envDecay, s.driveAmount, s.filterType, s.filterSlope, s.filterCutoff, s.filterReson, s.filterHp, s.eqType, s.eqFreq,
                                            s.eqGain, s.eqQ, s.compThresh, s.compRatio, s.compAttack, s.compRelease, s.compMakeup, s.compSide, s.imgWidth, s.imgBass, s.imgSpread};
        memcpy(out_ + 4 + l * kChSettingBytes, v, kChSettingBytes);
    }
}

// Reads the block; false (and every layer at its defaults, every block off) when it is missing or has a value out of range.
inline bool chReadBlock(ChSettings* s_, const uint8_t* in_, uint16_t size_) {
    for (uint8_t l = 0; l < 10; l++) s_[l] = chDefaults();
    if ((size_ < kChFileSize) || (memcmp(in_, "CHN", 3) != 0) || (in_[3] != 1)) return false;
    ChSettings tmp[10];
    for (uint8_t l = 0; l < 10; l++) {
        const uint8_t* v = in_ + 4 + l * kChSettingBytes;
        ChSettings s;
        s.on = v[0], s.envAttack = v[1], s.envDecay = v[2], s.driveAmount = v[3], s.filterType = v[4], s.filterSlope = v[5], s.filterCutoff = v[6], s.filterReson = v[7], s.filterHp = v[8];
        s.eqType = v[9], s.eqFreq = v[10], s.eqGain = v[11], s.eqQ = v[12], s.compThresh = v[13], s.compRatio = v[14], s.compAttack = v[15], s.compRelease = v[16], s.compMakeup = v[17], s.compSide = v[18], s.imgWidth = v[19], s.imgBass = v[20], s.imgSpread = v[21];
        if ((s.on >= (1u << CH_BLOCKS)) || (s.envAttack > kChMaxIndex) || (s.envDecay > kChMaxIndex) || (s.driveAmount > kChMaxIndex) || (s.filterType < 1) || (s.filterType > 4) || (s.filterSlope > 2) ||
            (s.filterCutoff > kChMaxIndex) || (s.filterReson > kChMaxIndex) || (s.filterHp > kChMaxIndex) || (s.eqType > 2) || (s.eqFreq > kChMaxIndex) || (s.eqGain > kChEqGainSteps) || (s.eqQ > kChMaxIndex) ||
            (s.compThresh > kChMaxIndex) || (s.compRatio >= kChRatios) || (s.compAttack > kChMaxIndex) || (s.compRelease > kChMaxIndex) || (s.compMakeup > kChMaxIndex) || (s.compSide > kChSide) || (s.imgWidth > kChMaxIndex) || (s.imgBass > kChMaxIndex) || (s.imgSpread > kChMaxIndex))
            return false;
        tmp[l] = s;
    }
    for (uint8_t l = 0; l < 10; l++) s_[l] = tmp[l];
    return true;
}

#endif
