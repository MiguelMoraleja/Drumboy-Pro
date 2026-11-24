// The numbers of the chain page's pictures (backlog 71): the full-area view of CHAIN_MENU, the layer's channel strip.
// Hardware-free so it can be tested on a PC (02 Software/tests, test_chainview); ChainScreens.h draws what this computes.
//
// The view shows the layer's name and a list of the six blocks (on or off, with a short summary), the six blocks as boxes
// in the order the sound runs, each with a small picture of what it does, and the selected block big below them. Every
// picture is one function of x (chvY01) that the boxes and the big plot both use: the filter and the EQ draw the response
// of the very coefficients the audio runs (FilterCurve.h's design and the EQ band's), the compressor its transfer curve,
// the envelope its level over time, the drive its transfer curve, and IMAGE the mid and the side signal's response.
#ifndef __CHAINVIEW_H
#define __CHAINVIEW_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ChannelStrip.h"

// Geometry (the lane area is x 20..939, y 131..439).
const uint16_t kChvListX = 24, kChvListW = 232, kChvListY = 184, kChvListStep = 30, kChvListH = 26;
const uint16_t kChvBoxX = 280, kChvBoxW = 100, kChvBoxGap = 10, kChvBoxY = 138, kChvBoxH = 78;
const uint16_t kChvMiniX = 10, kChvMiniY = 30, kChvMiniW = 80, kChvMiniH = 40;  // inside a box
const uint16_t kChvTitleY = 232, kChvPlotX = 280, kChvPlotY = 250, kChvPlotW = 650, kChvPlotH = 150, kChvCaptionY = 408;
const float kChvSampleRate = 44100.0f;

inline uint16_t chvBoxX(uint8_t block_) { return static_cast<uint16_t>(kChvBoxX + block_ * (kChvBoxW + kChvBoxGap)); }

// How many curves a block draws (IMAGE: its side and its mid), and what its x axis is.
inline uint8_t chvCurves(ChBlock b_) { return (b_ == CH_IMAGE) ? 2 : 1; }
enum ChvAxis : uint8_t { CHV_FREQ = 0, CHV_TIME, CHV_LEVEL_IN, CHV_SIGNAL };  // 20 Hz to 20 kHz, ms, dB in, -1 to +1
inline ChvAxis chvAxis(ChBlock b_) {
    switch (b_) {
        case CH_ENV: return CHV_TIME;
        case CH_DRIVE: return CHV_SIGNAL;
        case CH_COMP: return CHV_LEVEL_IN;
        default: return CHV_FREQ;
    }
}
// The y range in dB (for the dB pictures): top and bottom.
inline void chvDbRange(ChBlock b_, float& top_, float& bottom_) {
    switch (b_) {
        case CH_FILTER: top_ = 18.0f, bottom_ = -48.0f; break;
        case CH_EQ: top_ = 24.0f, bottom_ = -24.0f; break;
        case CH_COMP: top_ = 30.0f, bottom_ = -60.0f; break;
        default: top_ = 12.0f, bottom_ = -48.0f; break;  // IMAGE
    }
}
// The length of the time axis of the envelope picture (ms).
inline float chvEnvSpanMs(const ChSettings& s_) {
    const float dec = chDecayMs(s_.envDecay), span = (dec > 0.0f) ? dec * 1.25f : 1000.0f;
    return (span < 20.0f) ? 20.0f : span;
}

inline float chvClamp01(float v_) { return (v_ < 0.0f) ? 0.0f : ((v_ > 1.0f) ? 1.0f : (v_ == v_ ? v_ : 0.0f)); }

// The response in dB of a block at a frequency (FILTER, EQ and the side of IMAGE).
inline float chvDbAt(ChBlock b_, const ChSettings& s_, float hz_, uint8_t curve_) {
    float re = 1.0f, im = 0.0f;
    if (b_ == CH_FILTER) {
        BiquadCoef sec[3];
        uint8_t n = 0;
        if (s_.filterHp > 0) filterDesignSection(2, chHz(s_.filterHp), 0.70710678f, kChvSampleRate, sec[n++]);
        BiquadCoef a, b;
        const uint8_t m = filterDesign(s_.filterType, s_.filterSlope, chHz(s_.filterCutoff), chQ(s_.filterReson), kChvSampleRate, a, b);
        sec[n++] = a;
        if (m == 2) sec[n++] = b;
        biquadResponse(sec, n, hz_, kChvSampleRate, re, im);
    } else if (b_ == CH_EQ) {
        BiquadCoef c;
        chEqDesign(s_.eqType, chHz(s_.eqFreq), chEqDb(s_.eqGain), chQ(s_.eqQ), kChvSampleRate, c);
        biquadResponse(&c, 1, hz_, kChvSampleRate, re, im);
    } else if (b_ == CH_IMAGE) {
        if (curve_ == 1) return 0.0f;  // the mid is never touched
        float gain = static_cast<float>(s_.imgWidth > kChMaxIndex ? kChMaxIndex : s_.imgWidth) * 0.02f;
        if (s_.imgBass > 0) {
            BiquadCoef a, b;
            filterDesign(2, 2, chBassHz(s_.imgBass), 0.70710678f, kChvSampleRate, a, b);
            BiquadCoef sec[2] = {a, b};
            float hr, hi;
            biquadResponse(sec, 2, hz_, kChvSampleRate, hr, hi);
            gain *= sqrtf(hr * hr + hi * hi);
        }
        return (gain > 1.0e-6f) ? 20.0f * log10f(gain) : -120.0f;
    }
    return curveDb(re, im);
}

// A picture as a function of x (0..1): the height of the curve, 0 at the bottom and 1 at the top of its plot.
inline float chvY01(ChBlock b_, const ChSettings& s_, float x01_, uint8_t curve_ = 0) {
    x01_ = chvClamp01(x01_);
    float top, bottom;
    chvDbRange(b_, top, bottom);
    switch (b_) {
        case CH_ENV: {
            const float t = x01_ * chvEnvSpanMs(s_), atk = chAttackMs(s_.envAttack), dec = chDecayMs(s_.envDecay);
            const float ramp = (atk > 0.0f) ? ((t < atk) ? t / atk : 1.0f) : 1.0f;
            const float tail = (dec > 0.0f) ? expf(-6.9077553f * t / dec) : 1.0f;
            return chvClamp01(ramp * tail);
        }
        case CH_DRIVE: {
            const float g = chDriveGain(s_.driveAmount), x = x01_ * 2.0f - 1.0f;
            return chvClamp01((chShape(g * x) / chShape(g) + 1.0f) * 0.5f);
        }
        case CH_COMP: {
            const float in = -60.0f + x01_ * 60.0f, thr = chThreshDb(s_.compThresh), ratio = chRatio(s_.compRatio);
            const float out = ((in <= thr) ? in : thr + (in - thr) / ratio) + chMakeupDb(s_.compMakeup);
            return chvClamp01((out - bottom) / (top - bottom));
        }
        default: {
            const float hz = 20.0f * powf(10.0f, 3.0f * x01_);
            return chvClamp01((chvDbAt(b_, s_, hz, curve_) - bottom) / (top - bottom));
        }
    }
}

// The position of the block's main setting on its x axis (a marker line), or -1: the cutoff, the band, the threshold, the
// bass corner.
inline float chvMarker(ChBlock b_, const ChSettings& s_) {
    switch (b_) {
        case CH_FILTER: return log10f(chHz(s_.filterCutoff) / 20.0f) / 3.0f;
        case CH_EQ: return log10f(chHz(s_.eqFreq) / 20.0f) / 3.0f;
        case CH_COMP: return chvClamp01((chThreshDb(s_.compThresh) + 60.0f) / 60.0f);
        case CH_IMAGE: return (s_.imgBass > 0) ? log10f(chBassHz(s_.imgBass) / 20.0f) / 3.0f : -1.0f;
        default: return -1.0f;
    }
}

// A short text of what the block does now, for the list (up to 14 characters).
inline void chvSummary(ChBlock b_, const ChSettings& s_, char* out_, size_t size_) {
    char t[24];
    switch (b_) {
        case CH_ENV: snprintf(t, sizeof t, "%.0fMS A%.0f", static_cast<double>(chDecayMs(s_.envDecay)), static_cast<double>(chAttackMs(s_.envAttack))); if (s_.envDecay >= kChMaxIndex) snprintf(t, sizeof t, "HOLD A%.0f", static_cast<double>(chAttackMs(s_.envAttack))); break;
        case CH_DRIVE: snprintf(t, sizeof t, "AMOUNT %u", static_cast<unsigned>(s_.driveAmount)); break;
        case CH_FILTER: {
            static const char* const kT[5] = {"-", "LP", "HP", "BP", "NOTCH"};
            static const char* const kS[3] = {"6", "12", "24"};
            const float hz = chHz(s_.filterCutoff);
            if (hz < 1000.0f) snprintf(t, sizeof t, "%s%s %.0f", kT[s_.filterType <= 4 ? s_.filterType : 0], kS[s_.filterSlope <= 2 ? s_.filterSlope : 1], static_cast<double>(hz));
            else snprintf(t, sizeof t, "%s%s %.1fK", kT[s_.filterType <= 4 ? s_.filterType : 0], kS[s_.filterSlope <= 2 ? s_.filterSlope : 1], static_cast<double>(hz) / 1000.0);
            break;
        }
        case CH_EQ: {
            static const char* const kT[3] = {"BELL", "LOW", "HIGH"};
            snprintf(t, sizeof t, "%s %+.0f", kT[s_.eqType <= 2 ? s_.eqType : 0], static_cast<double>(chEqDb(s_.eqGain)));
            break;
        }
        case CH_COMP: snprintf(t, sizeof t, "%.0f:1 %.0f", static_cast<double>(chRatio(s_.compRatio)), static_cast<double>(chThreshDb(s_.compThresh))); break;
        default: snprintf(t, sizeof t, "W%u B%.0f", static_cast<unsigned>(s_.imgWidth * 2u), static_cast<double>(s_.imgBass > 0 ? chBassHz(s_.imgBass) : 0.0f)); break;
    }
    snprintf(out_, size_, "%.14s", t);
}

// What the big picture is, for its title.
inline const char* chvTitle(ChBlock b_) {
    static const char* const kT[CH_BLOCKS] = {"ENV: THE LEVEL OF THE NOTE OVER TIME", "DRIVE: LEVEL IN AGAINST LEVEL OUT", "FILTER: THE RESPONSE FROM 20 HZ TO 20 KHZ", "EQ: THE BAND FROM 20 HZ TO 20 KHZ",
                                              "COMP: LEVEL IN AGAINST LEVEL OUT", "IMAGE: MID AND SIDE FROM 20 HZ TO 20 KHZ"};
    return kT[b_ < CH_BLOCKS ? b_ : 0];
}

#endif
