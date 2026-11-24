// The numbers of the stereo stage page (backlog 71): the fifth instrument page, a picture of the whole stereo mix. Hardware-free
// so it can be tested on a PC (02 Software/tests, test_stageview); StageScreens.h draws what this computes.
//
// Every layer is a bar on one stage: centred at its pan, as wide as its stereo width (the IMAGE block of the channel strip, from
// a mono point to twice a stereo sample), with a yellow mark where the bass is mono, and at the right the layer's correlation
// (+1 where the two sides are the same signal, 0 unrelated, -1 opposite: a negative one cancels when the mix is summed to mono),
// measured in the audio while the page is open.
#ifndef __STAGEVIEW_H
#define __STAGEVIEW_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ChannelStrip.h"
#include "Pan.h"

const uint8_t kStLayers = 10;
const uint16_t kStStageX = 150, kStStageW = 640;  // the stage: left edge to right edge
const uint16_t kStRowY = 158, kStRowStep = 28, kStRowH = 26;
const uint16_t kStCorrRight = 900;
const int8_t kStNoCorr = 127;  // no signal to measure

// What one layer shows.
struct StRow {
    uint8_t pan;        // 0..40, 20 = centre
    uint16_t widthPct;  // 0..400
    uint16_t bassHz;    // 0 = bass is not mono
    int8_t corr;        // -100..100, kStNoCorr = silent
    bool stereoSample;  // the layer plays a stereo sample
    bool on;            // the IMAGE block is on
};

inline uint16_t stRowY(uint8_t layer_) { return static_cast<uint16_t>(kStRowY + layer_ * kStRowStep); }

// The width of a layer's image in percent: a stereo sample is 100 %, a mono layer is as wide as SPREAD makes it (0 to 100 %);
// the WIDTH setting scales it (50 = 100 %); an IMAGE block that is off leaves it as it is.
inline uint16_t stWidthPct(const ChSettings& s_, bool stereoSample_) {
    const bool on = chOn(s_, CH_IMAGE);
    const uint32_t base = stereoSample_ ? 100u : (on ? static_cast<uint32_t>(s_.imgSpread > kChMaxIndex ? kChMaxIndex : s_.imgSpread) : 0u);
    const uint32_t factor = on ? static_cast<uint32_t>(s_.imgWidth > kChMaxIndex ? kChMaxIndex : s_.imgWidth) * 2u : 100u;  // percent
    return static_cast<uint16_t>((base * factor + 50u) / 100u);
}
inline uint16_t stBassHz(const ChSettings& s_) { return (chOn(s_, CH_IMAGE) && (s_.imgBass > 0)) ? static_cast<uint16_t>(chBassHz(s_.imgBass) + 0.5f) : static_cast<uint16_t>(0); }

inline StRow stRowOf(uint8_t pan_, const ChSettings& s_, bool stereoSample_, int8_t corr_) {
    StRow r;
    r.pan = (pan_ > kPanMax) ? kPanMax : pan_;
    r.widthPct = stWidthPct(s_, stereoSample_);
    r.bassHz = stBassHz(s_);
    r.corr = corr_;
    r.stereoSample = stereoSample_;
    r.on = chOn(s_, CH_IMAGE);
    return r;
}

// The bar on the stage: its centre column, and its left and right edges (held inside the stage). 100 % is a quarter of the stage.
inline uint16_t stCentreX(uint8_t pan_) { return static_cast<uint16_t>(kStStageX + (static_cast<uint32_t>(pan_ > kPanMax ? kPanMax : pan_) * kStStageW) / kPanMax); }
inline void stBar(const StRow& r_, uint16_t& left_, uint16_t& right_) {
    const uint16_t cx = stCentreX(r_.pan);
    uint32_t half = (static_cast<uint32_t>(r_.widthPct) * (kStStageW / 4u)) / 100u / 2u;
    if (half < 2u) half = 2u;
    left_ = static_cast<uint16_t>((cx > kStStageX + half) ? cx - half : kStStageX);
    right_ = static_cast<uint16_t>((cx + half < kStStageX + kStStageW) ? cx + half : kStStageX + kStStageW);
}

// The correlation of a block of a stereo signal from the three sums (left x left, right x right, left x right): -1..1, or
// "no signal" when there is nothing to measure.
inline int8_t stCorrOf(float ll_, float rr_, float lr_) {
    const float e = ll_ * rr_;
    if (!(e > 1.0e-12f)) return kStNoCorr;
    float c = lr_ / sqrtf(e);
    if (c > 1.0f) c = 1.0f;
    if (c < -1.0f) c = -1.0f;
    return static_cast<int8_t>(lroundf(c * 100.0f));
}
// A new reading folded into the one shown: slow enough to read, quick enough to follow.
inline int8_t stCorrSmooth(int8_t shown_, int8_t reading_) {
    if (reading_ == kStNoCorr) return shown_;
    if (shown_ == kStNoCorr) return reading_;
    return static_cast<int8_t>(lroundf(0.7f * shown_ + 0.3f * reading_));
}
enum StCorrState : uint8_t { ST_CORR_NONE = 0, ST_CORR_GOOD, ST_CORR_WIDE, ST_CORR_BAD };  // silent, +0.6 or more, 0.1 to 0.6, below 0.1
inline StCorrState stCorrState(int8_t c_) {
    if (c_ == kStNoCorr) return ST_CORR_NONE;
    if (c_ >= 60) return ST_CORR_GOOD;
    return (c_ >= 10) ? ST_CORR_WIDE : ST_CORR_BAD;
}
inline void stCorrText(int8_t c_, bool mono_, char* out_, size_t size_) {
    if (mono_) snprintf(out_, size_, "MONO");
    else if (c_ == kStNoCorr) snprintf(out_, size_, "--");
    else snprintf(out_, size_, "%c%d.%02d", (c_ < 0) ? '-' : '+', static_cast<int>(abs(c_)) / 100, static_cast<int>(abs(c_)) % 100);
}

#endif
