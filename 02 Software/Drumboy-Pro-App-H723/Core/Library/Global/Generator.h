// Noise and tone generators (backlog 57), hardware-free so they can be tested on a PC (see
// 02 Software/tests, test_generator).
//
// Two uses:
//  1. GEN on a layer: instead of a sample the layer plays a generator. The sequencer, fills, notes,
//     MIDI, LFOs, routing, pan and the 50-sample fades stay exactly as for a sample. The layer's
//     own fields keep their meaning where they can:
//       LEVEL       level
//       PITCH       the generator's frequency (tone of a noise, pitch of the siren and the laser)
//       DIRECTION   the shape of the sweep: up, down, up-down, down-up
//       RESOLUTION  bitcrush, as for a sample
//       TIMING      PROB  as for a sample
//     and page 2 (START, END) shows the generator's own LENGTH (in 16ths, follows the tempo) and
//     SWEEP (0..4 octaves over the note) while GEN is on; the sample's START and END stay as they
//     were for when GEN goes back to OFF.
//  2. HISS on the mixer (DUB page 2): a stereo bed (tape, vinyl, pink or white) added to the master,
//     with a level, a tone and ducking keyed from the layers.
//
// Sound: everything runs at the engine's 44.1 kHz in 32-bit float. Filter coefficients follow a
// sweep every 4 samples (11 kHz control rate, inaudible on a sweep), which keeps the cost low
// without changing the sound.
//
// File: song files get a "GEN" block after the "DUB" block. Older firmware stops before it.
//   "GEN" | 1 (block version) | 10 x type | 10 x length | 10 x sweep | hiss type, level, tone, duck
#ifndef __GENERATOR_H
#define __GENERATOR_H

#include <math.h>
#include <stdint.h>
#include <string.h>

const float kGenSampleRate = 44100.0f;
const float kGenInvSampleRate = 1.0f / 44100.0f;
const float kGenPi = 3.14159265358979f;

/* Settings ------------------------------------------------------------------*/

enum GenType : uint8_t { GEN_OFF = 0, GEN_WHITE, GEN_PINK, GEN_BROWN, GEN_TAPE, GEN_DUST, GEN_SIREN, GEN_RISER, GEN_LASER };
const uint8_t kGenTypeCount = 9;
const char* const kGenTypeName[kGenTypeCount] = {"OFF", "WHITE", "PINK", "BROWN", "TAPE", "DUST", "SIREN", "RISER", "LASER"};

enum HissType : uint8_t { HISS_OFF = 0, HISS_TAPE, HISS_VINYL, HISS_PINK, HISS_WHITE };
const uint8_t kHissTypeCount = 5;
const char* const kHissTypeName[kHissTypeCount] = {"OFF", "TAPE", "VINYL", "PINK", "WHITE"};

const uint8_t kGenLayers = 10;
const uint8_t kGenLengthMin = 1;   // 16ths
const uint8_t kGenLengthMax = 64;  // 4 bars of 4/4
const uint8_t kGenLengthDefault = 4;
const uint8_t kGenSweepMax = 100;
const float kGenSweepOctaves = 4.0f;  // SWEEP 100
const uint8_t kHissMax = 100;
const uint8_t kHissLevelDefault = 30;
const uint8_t kHissToneDefault = 70;

const uint16_t kGenFileSize = 3 + 1 + 3 * kGenLayers + 4;  // 38

struct GenMix {
    uint8_t type[kGenLayers] = {0};
    uint8_t length[kGenLayers] = {kGenLengthDefault, kGenLengthDefault, kGenLengthDefault, kGenLengthDefault, kGenLengthDefault,
                                  kGenLengthDefault, kGenLengthDefault, kGenLengthDefault, kGenLengthDefault, kGenLengthDefault};
    uint8_t sweep[kGenLayers] = {0};
    uint8_t hissType = HISS_OFF;
    uint8_t hissLevel = kHissLevelDefault;
    uint8_t hissTone = kHissToneDefault;
    uint8_t hissDuck = 0;
};

inline void genClear(GenMix& g_) { g_ = GenMix(); }

inline void genWrite(const GenMix& g_, uint8_t* out_) {
    memcpy(out_, "GEN", 3);
    out_[3] = 1;
    for (uint8_t i = 0; i < kGenLayers; i++) {
        out_[4 + i] = g_.type[i];
        out_[4 + kGenLayers + i] = g_.length[i];
        out_[4 + 2 * kGenLayers + i] = g_.sweep[i];
    }
    out_[4 + 3 * kGenLayers] = g_.hissType;
    out_[5 + 3 * kGenLayers] = g_.hissLevel;
    out_[6 + 3 * kGenLayers] = g_.hissTone;
    out_[7 + 3 * kGenLayers] = g_.hissDuck;
}

// Returns false (and leaves the defaults: no generator, no hiss) when the block is missing or not valid.
inline bool genRead(GenMix& g_, const uint8_t* in_, uint16_t size_) {
    genClear(g_);
    if ((size_ < kGenFileSize) || (memcmp(in_, "GEN", 3) != 0) || (in_[3] != 1)) return false;
    GenMix g;
    for (uint8_t i = 0; i < kGenLayers; i++) {
        const uint8_t type = in_[4 + i], length = in_[4 + kGenLayers + i], sweep = in_[4 + 2 * kGenLayers + i];
        if ((type >= kGenTypeCount) || (length < kGenLengthMin) || (length > kGenLengthMax) || (sweep > kGenSweepMax)) return false;
        g.type[i] = type;
        g.length[i] = length;
        g.sweep[i] = sweep;
    }
    const uint8_t* h = in_ + 4 + 3 * kGenLayers;
    if ((h[0] >= kHissTypeCount) || (h[1] > kHissMax) || (h[2] > kHissMax) || (h[3] > kHissMax)) return false;
    g.hissType = h[0];
    g.hissLevel = h[1];
    g.hissTone = h[2];
    g.hissDuck = h[3];
    g_ = g;
    return true;
}

// The length of a note in samples: LENGTH 16ths at the tempo.
inline uint32_t genLengthSamples(uint8_t length16_, float tempo_) {
    if (length16_ < kGenLengthMin) length16_ = kGenLengthMin;
    if (length16_ > kGenLengthMax) length16_ = kGenLengthMax;
    if (tempo_ < 20.0f) tempo_ = 20.0f;
    return static_cast<uint32_t>(static_cast<float>(length16_) * (15.0f * kGenSampleRate / tempo_) + 0.5f);
}

inline float genSweepOctaves(uint8_t sweep_) { return static_cast<float>(sweep_ > kGenSweepMax ? kGenSweepMax : sweep_) * (kGenSweepOctaves / kGenSweepMax); }

/* Building blocks -----------------------------------------------------------*/

// xorshift32: a full-period generator, never seeded with 0.
inline uint32_t genRngNext(uint32_t& s_) {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
}
inline float genWhite(uint32_t& s_) { return static_cast<float>(static_cast<int32_t>(genRngNext(s_))) * 4.6566128730773926e-10f; }  // [-1, 1)
inline float genUnit(uint32_t& s_) { return static_cast<float>(genRngNext(s_) >> 8) * (1.0f / 16777216.0f); }                       // [0, 1)
inline uint32_t genSeed(uint32_t s_) { return (s_ == 0) ? 0x9E3779B9u : s_; }

// Pink noise, -3 dB per octave (Paul Kellet's refined filter, made for 44.1 kHz).
struct GenPink {
    float b0, b1, b2, b3, b4, b5, b6;
};
inline float genPinkTick(GenPink& p_, float w_) {
    p_.b0 = 0.99886f * p_.b0 + w_ * 0.0555179f;
    p_.b1 = 0.99332f * p_.b1 + w_ * 0.0750759f;
    p_.b2 = 0.96900f * p_.b2 + w_ * 0.1538520f;
    p_.b3 = 0.86650f * p_.b3 + w_ * 0.3104856f;
    p_.b4 = 0.55000f * p_.b4 + w_ * 0.5329522f;
    p_.b5 = -0.7616f * p_.b5 - w_ * 0.0168980f;
    const float out = p_.b0 + p_.b1 + p_.b2 + p_.b3 + p_.b4 + p_.b5 + p_.b6 + w_ * 0.5362f;
    p_.b6 = w_ * 0.115926f;
    return out * 0.11f;
}

// Brown (red) noise, -6 dB per octave: a leaky integrator of white noise.
inline float genBrownTick(float& y_, float w_) {
    y_ = (y_ + 0.02f * w_) * (1.0f / 1.02f);
    return y_ * 3.5f;
}

// State-variable filter (topology-preserving transform): stable under fast sweeps.
struct GenSvf {
    float ic1, ic2, a1, a2, a3, k;
};
inline void genSvfSet(GenSvf& f_, float fc_, float q_) {
    if (fc_ < 10.0f) fc_ = 10.0f;
    if (fc_ > 0.45f * kGenSampleRate) fc_ = 0.45f * kGenSampleRate;
    const float g = tanf(kGenPi * fc_ * kGenInvSampleRate);
    f_.k = 1.0f / q_;
    f_.a1 = 1.0f / (1.0f + g * (g + f_.k));
    f_.a2 = g * f_.a1;
    f_.a3 = g * f_.a2;
}
// One sample: low-pass and band-pass (the band-pass normalised to 0 dB at its centre).
inline void genSvfTick(GenSvf& f_, float x_, float& lp_, float& bp_) {
    const float v3 = x_ - f_.ic2;
    const float v1 = f_.a1 * f_.ic1 + f_.a2 * v3;
    const float v2 = f_.ic2 + f_.a2 * f_.ic1 + f_.a3 * v3;
    f_.ic1 = 2.0f * v1 - f_.ic1;
    f_.ic2 = 2.0f * v2 - f_.ic2;
    lp_ = v2;
    bp_ = f_.k * v1;
}

// The sweep over a note: progress 0..1 to a position 0..1, by the layer's DIRECTION.
inline float genShape(uint8_t shape_, float p_) {
    if (p_ < 0.0f) p_ = 0.0f;
    if (p_ > 1.0f) p_ = 1.0f;
    switch (shape_) {
        case 1:  // down
            return 1.0f - p_;
        case 2:  // up, then down
            return (p_ < 0.5f) ? 2.0f * p_ : 2.0f - 2.0f * p_;
        case 3:  // down, then up
            return (p_ < 0.5f) ? 1.0f - 2.0f * p_ : 2.0f * p_ - 1.0f;
        default:  // up
            return p_;
    }
}
// The factor a swept frequency gets: centred on the set value, SWEEP octaves from end to end.
inline float genSweepFactor(float octaves_, float pos_) { return exp2f(octaves_ * (pos_ - 0.5f)); }

// PolyBLEP correction for a step at phase 0 (t in [0, 1), dt = frequency / sample rate).
inline float genPolyBlep(float t_, float dt_) {
    if (t_ < dt_) {
        t_ /= dt_;
        return t_ + t_ - t_ * t_ - 1.0f;
    }
    if (t_ > 1.0f - dt_) {
        t_ = (t_ - 1.0f) / dt_;
        return t_ * t_ + t_ + t_ + 1.0f;
    }
    return 0.0f;
}

// Slow random drift (tape wow, used for amplitude): a new target every ~0.3 s, glided to.
struct GenDrift {
    float value, target;
    uint32_t count;
};
inline float genDriftTick(GenDrift& d_, uint32_t& rng_) {
    if (d_.count == 0) {
        d_.target = genWhite(rng_);
        d_.count = 8000 + (genRngNext(rng_) % 8000);
    }
    d_.count--;
    d_.value += (d_.target - d_.value) * 0.0002f;
    return d_.value;
}

// Vinyl dust: sparse clicks with mostly small and few large amplitudes, a few samples long.
struct GenDust {
    float click, hp;
};
inline float genDustTick(GenDust& d_, uint32_t& rng_, float perSample_) {
    if (genUnit(rng_) < perSample_) {
        const float a = genUnit(rng_);
        d_.click += ((genRngNext(rng_) & 1) ? 1.0f : -1.0f) * a * a * a * 0.9f;
    }
    const float out = d_.click - d_.hp;  // one-pole high-pass: crisp, no thump
    d_.hp += 0.15f * out;
    d_.click *= 0.55f;
    return out;
}

/* Layer voice ---------------------------------------------------------------*/

// Base frequencies at PITCH 0 (the PITCH factor multiplies them).
const float kGenNoiseToneHz = 12000.0f;  // WHITE, PINK, BROWN: low-pass
const float kGenTapeHz = 4000.0f;        // TAPE: band centre
const float kGenDustPerSecond = 6.0f;    // DUST: clicks per second
const float kGenSirenHz = 600.0f;        // SIREN: pitch
const float kGenSirenRateHz = 3.0f;      // SIREN: the warble rate SWEEP moves
const float kGenRiserHz = 1500.0f;       // RISER: band centre
const float kGenLaserHz = 800.0f;        // LASER: pitch
const uint32_t kGenAttack = 88;          // 2 ms fade-in: a noise starting at full level clicks
const uint8_t kGenControlStep = 4;       // samples per coefficient update

struct GenVoice {
    uint8_t type, shape;
    uint32_t rng;
    float ratio, octaves;
    uint32_t length, pos;
    GenPink pink;
    float brown;
    GenSvf svf, svf2;
    GenDrift drift;
    GenDust dust;
    float phase, lfoPhase;
    float mul;  // the sweep factor of the current control step
    uint8_t control;
};

// Starts a note: type, a seed (any value), the PITCH factor, SWEEP in octaves, DIRECTION, length in samples.
inline void genVoiceStart(GenVoice& v_, uint8_t type_, uint32_t seed_, float ratio_, float octaves_, uint8_t shape_, uint32_t length_) {
    memset(&v_, 0, sizeof(GenVoice));
    v_.type = (type_ < kGenTypeCount) ? type_ : static_cast<uint8_t>(GEN_OFF);
    v_.rng = genSeed(seed_);
    v_.ratio = (ratio_ > 0.0f) ? ratio_ : 1.0f;
    v_.octaves = octaves_;
    v_.shape = shape_;
    v_.length = (length_ > 0) ? length_ : 1;
    v_.mul = 1.0f;
    genSvfSet(v_.svf, 1000.0f, 0.707f);
    genSvfSet(v_.svf2, 6000.0f, 0.707f);
}

// The next control step: the sweep factor and the coefficients that follow it.
inline void genVoiceControl(GenVoice& v_) {
    const float p = static_cast<float>(v_.pos) / static_cast<float>(v_.length);
    v_.mul = genSweepFactor(v_.octaves, genShape(v_.shape, p));
    const float f = v_.ratio * v_.mul;
    switch (v_.type) {
        case GEN_WHITE:
        case GEN_PINK:
        case GEN_BROWN:
            genSvfSet(v_.svf, kGenNoiseToneHz * f, 0.707f);
            break;
        case GEN_TAPE:
            genSvfSet(v_.svf, kGenTapeHz * f, 0.6f);
            break;
        case GEN_DUST:
            genSvfSet(v_.svf, 7000.0f * v_.ratio, 0.707f);  // the hiss floor under the clicks
            break;
        case GEN_RISER:
            genSvfSet(v_.svf, kGenRiserHz * f, 5.0f);
            break;
        case GEN_SIREN:
            genSvfSet(v_.svf, 6000.0f, 0.707f);  // softens the square
            break;
        default:
            break;
    }
}

// Renders n samples into out_ (overwrites). Advances the note; past its length it keeps going
// (the caller ends the note with its fade).
inline void genVoiceRender(GenVoice& v_, float* out_, uint16_t n_) {
    for (uint16_t i = 0; i < n_; i++) {
        if (v_.control == 0) genVoiceControl(v_);
        v_.control = static_cast<uint8_t>((v_.control + 1) % kGenControlStep);
        float lp, bp, x = 0.0f;
        switch (v_.type) {
            case GEN_WHITE:
                genSvfTick(v_.svf, genWhite(v_.rng) * 0.5f, lp, bp);
                x = lp;
                break;
            case GEN_PINK:
                genSvfTick(v_.svf, genPinkTick(v_.pink, genWhite(v_.rng)), lp, bp);
                x = lp;
                break;
            case GEN_BROWN:
                genSvfTick(v_.svf, genBrownTick(v_.brown, genWhite(v_.rng)), lp, bp);
                x = lp;
                break;
            case GEN_TAPE: {
                const float pink = genPinkTick(v_.pink, genWhite(v_.rng));
                genSvfTick(v_.svf, pink, lp, bp);
                x = (bp * 1.6f + pink * 0.25f) * (1.0f + 0.12f * genDriftTick(v_.drift, v_.rng));
                break;
            }
            case GEN_DUST: {
                const float clicks = genDustTick(v_.dust, v_.rng, kGenDustPerSecond * v_.ratio * v_.mul * kGenInvSampleRate);
                genSvfTick(v_.svf, genPinkTick(v_.pink, genWhite(v_.rng)), lp, bp);
                x = clicks + lp * 0.12f;
                break;
            }
            case GEN_SIREN: {
                // A square whose pitch warbles half an octave each way; SWEEP moves the warble rate.
                v_.lfoPhase += kGenSirenRateHz * v_.mul * kGenInvSampleRate;
                if (v_.lfoPhase >= 1.0f) v_.lfoPhase -= 1.0f;
                const float tri = 4.0f * fabsf(v_.lfoPhase - 0.5f) - 1.0f;
                const float dt = kGenSirenHz * v_.ratio * exp2f(0.5f * tri) * kGenInvSampleRate;
                v_.phase += dt;
                if (v_.phase >= 1.0f) v_.phase -= 1.0f;
                float sq = (v_.phase < 0.5f) ? 1.0f : -1.0f;
                sq += genPolyBlep(v_.phase, dt);
                float t2 = v_.phase + 0.5f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                sq -= genPolyBlep(t2, dt);
                genSvfTick(v_.svf, sq * 0.25f, lp, bp);
                x = lp;
                break;
            }
            case GEN_RISER:
                genSvfTick(v_.svf, genWhite(v_.rng), lp, bp);
                x = bp * 1.6f;
                break;
            case GEN_LASER: {
                const float dt = kGenLaserHz * v_.ratio * v_.mul * kGenInvSampleRate;
                v_.phase += (dt < 0.45f) ? dt : 0.45f;
                if (v_.phase >= 1.0f) v_.phase -= 1.0f;
                x = sinf(2.0f * kGenPi * v_.phase) * 0.35f;
                break;
            }
            default:
                break;
        }
        if (v_.pos < kGenAttack) x *= static_cast<float>(v_.pos) * (1.0f / static_cast<float>(kGenAttack));
        out_[i] = x;
        v_.pos++;
    }
}

/* Hiss bed ------------------------------------------------------------------*/

struct HissBed {
    uint32_t rngL, rngR;
    GenPink pinkL, pinkR;
    GenSvf toneL, toneR, bandL, bandR;
    GenDrift drift;
    GenDust dustL, dustR;
    float brownL, brownR;
    float duckEnv, gain, amp;
    uint8_t toneSet;   // the TONE the coefficients are for (+1; 0 = not set yet)
    uint8_t lastType;  // what fades out over one block when the type is set to OFF
};

inline float hissAmplitude(uint8_t level_) {
    const float l = static_cast<float>(level_ > kHissMax ? kHissMax : level_) * (1.0f / kHissMax);
    return 0.125f * l * l;  // 100: -18 dBFS, 50: -30 dBFS, 10: -58 dBFS
}
inline float hissToneHz(uint8_t tone_) { return 1000.0f * exp2f(static_cast<float>(tone_ > kHissMax ? kHissMax : tone_) * 0.043f); }  // 1 kHz .. 19.7 kHz

// Renders n samples of the bed into outL_/outR_ (overwrites). key_ is the loudest layer sample of
// this block (0..1), for the ducking.
inline void hissRender(HissBed& h_, uint8_t type_, uint8_t level_, uint8_t tone_, uint8_t duck_, float key_, float* outL_, float* outR_, uint16_t n_) {
    if (h_.rngL == 0) h_.rngL = 0x2545F491u;
    if (h_.rngR == 0) h_.rngR = 0x68E31DA4u;
    if (h_.toneSet != static_cast<uint8_t>(tone_ + 1)) {
        h_.toneSet = static_cast<uint8_t>(tone_ + 1);
        genSvfSet(h_.toneL, hissToneHz(tone_), 0.707f);
        genSvfSet(h_.toneR, hissToneHz(tone_), 0.707f);
        genSvfSet(h_.bandL, 3500.0f, 0.5f);
        genSvfSet(h_.bandR, 3500.0f, 0.5f);
    }
    // Level and ducking glide across the block: no zipper noise.
    const float ampTo = (type_ == HISS_OFF) ? 0.0f : hissAmplitude(level_);
    const float release = 0.99710f;  // per block of 32: about 0.25 s
    const float key = (key_ > 0.0f) ? key_ : 0.0f;
    h_.duckEnv = (key > h_.duckEnv) ? key : h_.duckEnv * release;
    const float env = (h_.duckEnv * 2.0f > 1.0f) ? 1.0f : h_.duckEnv * 2.0f;
    const float gainTo = 1.0f - static_cast<float>(duck_ > kHissMax ? kHissMax : duck_) * (1.0f / kHissMax) * env;
    const float ampStep = (ampTo - h_.amp) / static_cast<float>(n_), gainStep = (gainTo - h_.gain) / static_cast<float>(n_);
    if (type_ != HISS_OFF) h_.lastType = type_;
    const uint8_t source = h_.lastType;
    for (uint16_t i = 0; i < n_; i++) {
        h_.amp += ampStep;
        h_.gain += gainStep;
        float l = 0.0f, r = 0.0f, lp, bp;
        switch (source) {
            case HISS_TAPE: {
                const float wow = 1.0f + 0.10f * genDriftTick(h_.drift, h_.rngL);
                const float pl = genPinkTick(h_.pinkL, genWhite(h_.rngL)), pr = genPinkTick(h_.pinkR, genWhite(h_.rngR));
                genSvfTick(h_.bandL, pl, lp, bp);
                float xl = bp * 1.6f + pl * 0.3f;
                genSvfTick(h_.bandR, pr, lp, bp);
                float xr = bp * 1.6f + pr * 0.3f;
                genSvfTick(h_.toneL, xl * wow, lp, bp);
                l = lp;
                genSvfTick(h_.toneR, xr * wow, lp, bp);
                r = lp;
                break;
            }
            case HISS_VINYL: {
                const float perSample = 5.0f * kGenInvSampleRate;
                const float rumbleL = genBrownTick(h_.brownL, genWhite(h_.rngL)) * 0.15f, rumbleR = genBrownTick(h_.brownR, genWhite(h_.rngR)) * 0.15f;
                genSvfTick(h_.toneL, genPinkTick(h_.pinkL, genWhite(h_.rngL)) * 0.35f, lp, bp);
                l = lp + rumbleL + genDustTick(h_.dustL, h_.rngL, perSample) * 1.5f;
                genSvfTick(h_.toneR, genPinkTick(h_.pinkR, genWhite(h_.rngR)) * 0.35f, lp, bp);
                r = lp + rumbleR + genDustTick(h_.dustR, h_.rngR, perSample) * 1.5f;
                break;
            }
            case HISS_PINK:
                genSvfTick(h_.toneL, genPinkTick(h_.pinkL, genWhite(h_.rngL)), lp, bp);
                l = lp;
                genSvfTick(h_.toneR, genPinkTick(h_.pinkR, genWhite(h_.rngR)), lp, bp);
                r = lp;
                break;
            case HISS_WHITE:
                genSvfTick(h_.toneL, genWhite(h_.rngL) * 0.5f, lp, bp);
                l = lp;
                genSvfTick(h_.toneR, genWhite(h_.rngR) * 0.5f, lp, bp);
                r = lp;
                break;
            default:
                break;
        }
        const float g = h_.amp * h_.gain * 4.0f;  // the sources sit around -12 dBFS RMS; amp is the target RMS level
        outL_[i] = l * g;
        outR_[i] = r * g;
    }
    h_.amp = ampTo;  // exactly, so a bed set to OFF reaches 0 and stops costing anything
    h_.gain = gainTo;
}

#endif
