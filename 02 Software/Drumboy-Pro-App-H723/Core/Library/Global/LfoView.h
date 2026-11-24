// The numbers of the LFO overview (backlog 3, 35, 45): the full-area view of the LFO page. Hardware-free so it can
// be tested on a PC (02 Software/tests, test_lfoview); LfoScreens.h draws what this computes.
//
// Left: the ten LFOs as rows (number, a small picture of the wave, how many things it moves). Right: the selected
// LFO as a big wave with a dot at its phase now, and the USED BY list: every layer field and filter field that
// follows it. The tables of the waves hold values from 0 to 1 (0.5 is the middle), as the sound reads them.
#ifndef __LFOVIEW_H
#define __LFOVIEW_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

const uint8_t kLvLfos = 10;
const uint8_t kLvLayerFields = 8;   // LEVEL, PITCH, DIRECTION, RESOLUTION, START, END, TIMING, PROBABILITY
const uint8_t kLvFilterFields = 2;  // FREQ, RESON
const uint8_t kLvMaxUses = kLvLfos * 0 + 10 * kLvLayerFields + 2 * kLvFilterFields;  // 84 in all
const uint8_t kLvShownUses = 10;    // the list shows ten, then "+ N MORE"

// Geometry (the lane area is x 20..939, y 131..439).
const uint16_t kLvRowX = 24, kLvRowW = 236, kLvRowY = 138, kLvRowStep = 30, kLvRowH = 28;
const uint16_t kLvWaveX = 76, kLvWaveW = 120;                       // the small wave inside a row
const uint16_t kLvTitleX = 278, kLvTitleY = 138;
const uint16_t kLvPlotX = 278, kLvPlotY = 160, kLvPlotW = 650, kLvPlotH = 130;
const uint16_t kLvPlotCycles = 2;                                   // cycles across the plot
const uint16_t kLvCaptionY = 295;
const uint16_t kLvUsedLabelY = 308, kLvUsedY = 322, kLvUsedStep = 22, kLvUsedH = 20, kLvUsedX = 278, kLvUsedW = 316, kLvUsedColStep = 334;

inline uint16_t lvRowY(uint8_t lfo_) { return static_cast<uint16_t>(kLvRowY + lfo_ * kLvRowStep); }

// A thing an LFO moves: a layer field (kind 0) or a filter field (kind 1).
struct LvUse {
    uint8_t kind;   // 0 layer, 1 filter
    uint8_t who;    // layer 0..9, filter 0..1
    uint8_t field;  // layer field 0..7, filter field 0..1
};

inline const char* lvLayerFieldName(uint8_t f_) {
    static const char* const kName[kLvLayerFields] = {"LEVEL", "PITCH", "DIRECTION", "RESOLUTION", "START", "END", "TIMING", "PROBABILITY"};
    return kName[f_ < kLvLayerFields ? f_ : 0];
}
inline const char* lvFilterFieldName(uint8_t f_) { return (f_ == 0) ? "FREQ" : "RESON"; }
inline const char* lvUseWhat(const LvUse& u_) { return (u_.kind == 0) ? lvLayerFieldName(u_.field) : lvFilterFieldName(u_.field); }

// The things LFO number slot_ (1..10; 0 is "none" in the assignment fields) moves, in the order layer 1 to 10
// (fields in their order), then filter 1, filter 2. Returns how many there are; fills at most max_ of them.
inline uint16_t lvCollect(uint8_t slot_, const uint8_t layerLfo[][kLvLayerFields], uint8_t layerCount_, const uint8_t filterLfo[][kLvFilterFields], LvUse* out_, uint16_t max_) {
    if (slot_ == 0) return 0;
    uint16_t n = 0;
    for (uint8_t l = 0; l < layerCount_; l++) {
        for (uint8_t f = 0; f < kLvLayerFields; f++) {
            if (layerLfo[l][f] != slot_) continue;
            if (n < max_) out_[n] = LvUse{0, l, f};
            n++;
        }
    }
    for (uint8_t fl = 0; fl < 2; fl++) {
        for (uint8_t f = 0; f < kLvFilterFields; f++) {
            if (filterLfo[fl][f] != slot_) continue;
            if (n < max_) out_[n] = LvUse{1, fl, f};
            n++;
        }
    }
    return n;
}

// The text of a use: "L3 HAT CLOSED" or "F1" for the left of the box, the field for the right.
inline void lvUseWho(const LvUse& u_, const char* layerName_, char* out_, size_t size_) {
    if (u_.kind == 0) snprintf(out_, size_, "L%u %.10s", static_cast<unsigned>(u_.who + 1), layerName_);
    else snprintf(out_, size_, "F%u", static_cast<unsigned>(u_.who + 1));
}

// A table value (0..1, 0.5 in the middle) as a height from the middle of a plot, up positive: the depth scales it,
// the invert flips it (the sound reads the wave the same way). amp_ is the largest height.
inline int16_t lvWaveHeight(float value_, bool invert_, float depth_, int16_t amp_) {
    float v = invert_ ? 1.0f - value_ : value_;
    if (!(v >= 0.0f)) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (!(depth_ >= 0.0f)) depth_ = 0.0f;
    if (depth_ > 1.0f) depth_ = 1.0f;
    const float h = (v - 0.5f) * 2.0f * depth_ * static_cast<float>(amp_);
    return static_cast<int16_t>(h >= 0.0f ? h + 0.5f : h - 0.5f);
}

// The table value at a position of the cycle (0..1, wraps), table_ with size_ entries.
inline float lvTableAt(const float* table_, uint16_t size_, float frac_) {
    if ((table_ == nullptr) || (size_ == 0)) return 0.5f;
    if (!(frac_ >= 0.0f)) frac_ = 0.0f;
    uint32_t i = static_cast<uint32_t>(frac_ * static_cast<float>(size_));
    return table_[i % size_];
}

// The plot column of the phase dot: the LFO's position in its cycle (0..1) over the first cycle of the plot.
inline uint16_t lvDotColumn(float cycleFrac_) {
    if (!(cycleFrac_ >= 0.0f)) cycleFrac_ = 0.0f;
    if (cycleFrac_ >= 1.0f) cycleFrac_ -= static_cast<float>(static_cast<uint32_t>(cycleFrac_));
    return static_cast<uint16_t>(cycleFrac_ * static_cast<float>(kLvPlotW / kLvPlotCycles));
}

#endif
