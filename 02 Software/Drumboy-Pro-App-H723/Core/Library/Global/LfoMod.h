// LFOs on the modules (V1.21, backlog 3): what an LFO does to a filter's FREQ and RESONANCE. Hardware-free so it
// can be tested on a PC (02 Software/tests, test_lfomod); the Controller applies it about 200 times a second.
//
// The layer destinations sample an LFO once per hit. A filter has no hits: it follows the LFO continuously, from
// the same LFO tables (the value d is 0..1 with 0.5 in the middle, the depth dep 0..1, as in layer_playBeat).
//   FREQ       d - 0.5 times the depth sweeps +-3 octaves at full depth (an octave is a doubling); the result is
//              kept inside the filter's range
//   RESONANCE  +-50 % of the resonance range at full depth, as LEVEL does for a layer
// Smoothness (sound quality first): the filter is designed again at every pass (200 times a second) in which the
// frequency moved by more than 1/512 octave (about 0.14 %, far below what is heard as a step) or the resonance step
// changed, so a moving LFO is followed almost continuously, and a still one costs nothing. A new set of
// coefficients is published at once, without interpolation: at this rate and step size the change is small.
#ifndef __LFOMOD_H
#define __LFOMOD_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "DspMath.h"  // isFiniteBits: a float compare does not catch NaN in the firmware build

const float kLfoFreqOctaves = 6.0f;           // total sweep at full depth: +-3 octaves
const float kLfoFreqStepOctaves = 1.0f / 512.0f;
const uint16_t kLfoTableSize = 3200;          // the entries of an LFO table (kLfoDataLibrary)
const uint8_t kLfoModUpdateMs = 5;            // the main loop looks at the LFOs this often

// The entry of an LFO table that the counter points at, always inside the table. The counter is a float that the
// sequencer interrupt advances and wraps and the main loop shifts (PHASE), so it can be read between those two: it may
// be a hair below zero or at the size itself, and a negative float turned into an unsigned index is 65535.
inline uint16_t lfoTableIndex(float counter_) {
    if (!isFiniteBits(counter_) || !(counter_ >= 0.0f)) return 0;
    const float at = floorf(counter_);
    return (at >= static_cast<float>(kLfoTableSize)) ? static_cast<uint16_t>(kLfoTableSize - 1) : static_cast<uint16_t>(at);
}

// The counter after a PHASE change moved it by shift_ entries, wrapped into [0, size). (It used to be wrapped with
// "greater than size - 1", so a counter between 3199 and 3200 became a small negative number.)
inline float lfoShiftCounter(float counter_, int32_t shift_) {
    return wrapRange(counter_ + static_cast<float>(shift_), static_cast<float>(kLfoTableSize));
}

// The filter's cutoff with an LFO on it.
inline float lfoModFreq(float baseHz_, float d_, float dep_, float minHz_, float maxHz_) {
    if (!isFiniteBits(baseHz_) || !(baseHz_ > 0.0f)) return minHz_;
    if (!isFiniteBits(d_) || !isFiniteBits(dep_)) return (baseHz_ < minHz_) ? minHz_ : ((baseHz_ > maxHz_) ? maxHz_ : baseHz_);  // a bad LFO value: no modulation
    const float octaves = kLfoFreqOctaves * (d_ - 0.5f) * dep_;
    float hz = baseHz_ * exp2f(octaves);
    if (!isFiniteBits(hz)) hz = baseHz_;
    if (hz < minHz_) hz = minHz_;
    if (hz > maxHz_) hz = maxHz_;
    return hz;
}

// A value with an LFO on it, kept in [min_, max_]: the layer's pattern, round(range * (d - 0.5) * dep).
inline int16_t lfoModIndex(int16_t base_, int16_t min_, int16_t max_, float d_, float dep_) {
    if (!isFiniteBits(d_) || !isFiniteBits(dep_)) return base_;  // a bad LFO value: no modulation
    const float range = static_cast<float>(max_ - min_);
    int32_t v = static_cast<int32_t>(base_) + static_cast<int32_t>(lroundf(range * (d_ - 0.5f) * dep_));
    if (v < min_) v = min_;
    if (v > max_) v = max_;
    return static_cast<int16_t>(v);
}

// Has the frequency moved enough to design the filter again?
inline bool lfoFreqMoved(float lastHz_, float newHz_) {
    if (!isFiniteBits(lastHz_) || !isFiniteBits(newHz_) || !(lastHz_ > 0.0f) || !(newHz_ > 0.0f)) return true;
    return fabsf(log2f(newHz_ / lastHz_)) > kLfoFreqStepOctaves;
}

// The assignments of a song: the LFO number (0 = none, 1 to 10) on each filter's FREQ and RESONANCE. A block
// after the QNT block ("LFD", a version, the four numbers); older firmware never reads that far.
const uint8_t kLfoModDests = 4;               // filter 1 freq, filter 1 resonance, filter 2 freq, filter 2 resonance
const uint8_t kLfoMaxNumber = 10;             // the number of LFOs
const uint16_t kLfoModFileSize = 4 + kLfoModDests;

inline void lfoModWriteBlock(const uint8_t* assign_, uint8_t* out_) {
    out_[0] = 'L';
    out_[1] = 'F';
    out_[2] = 'D';
    out_[3] = 1;
    for (uint8_t i = 0; i < kLfoModDests; i++) out_[4 + i] = (assign_[i] > kLfoMaxNumber) ? 0 : assign_[i];
}

// True when the block is whole and recognised; a number out of range reads as none.
inline bool lfoModReadBlock(uint8_t* assign_, const uint8_t* in_, uint16_t size_) {
    if ((size_ < kLfoModFileSize) || (in_[0] != 'L') || (in_[1] != 'F') || (in_[2] != 'D') || (in_[3] != 1)) return false;
    for (uint8_t i = 0; i < kLfoModDests; i++) assign_[i] = (in_[4 + i] > kLfoMaxNumber) ? 0 : in_[4 + i];
    return true;
}

// What the FREQ field shows for a frequency the LFO moved (the same format as the table of set values:
// 10 characters, whole Hz below 1000, kHz with one decimal above). Returns true for the KHZ unit.
inline bool lfoFreqLabel(float hz_, char* out_, size_t size_) {
    if (!(hz_ > 0.0f)) hz_ = 0.0f;
    if (hz_ < 999.5f) {
        snprintf(out_, size_, "%10ld", lroundf(hz_));
        return false;
    }
    snprintf(out_, size_, "%10.1f", static_cast<double>(hz_) / 1000.0);
    return true;
}
// A number that changes exactly when the label above does (to redraw the field only then).
inline uint32_t lfoFreqLabelKey(float hz_) {
    if (!(hz_ > 0.0f)) return 1;
    if (hz_ < 999.5f) return 1u + static_cast<uint32_t>(lroundf(hz_));
    return 100000u + static_cast<uint32_t>(lroundf(hz_ / 100.0f));
}

#endif
