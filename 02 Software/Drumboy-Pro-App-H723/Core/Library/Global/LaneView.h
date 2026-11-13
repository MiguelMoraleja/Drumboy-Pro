// What a beat looks like in a lane, and where a beat sits on a ring of the EUCLID view
// (backlog 38 and 45). Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// Lanes: a plain beat is a mark 2 px wide in the 9 px song row. Its height shows its velocity
// (FLAT 9 px, GHOST 6 px, velocity 1 = 2 px), bottom-aligned; a beat with a chance below 100% is
// drawn hollow, 3 px wide; a beat with a condition gets a 2x2 dot above the row. A beat without
// note settings looks exactly as before V1.14.
//
// Rings: the EUCLID page shows the ten layers as concentric rings, layer 1 outside. One turn is
// one loop, starting at the top and going clockwise.
#ifndef __LANEVIEW_H
#define __LANEVIEW_H

#include <math.h>
#include <stdint.h>

#include "BeatNote.h"

const uint8_t kLaneMarkMaxHeight = 9;   // kSongHeight
const uint8_t kLaneMarkMinHeight = 2;
const uint8_t kLaneDotRise = 4;         // the condition dot sits 4 px above the song row, 2 px high

// Mark height for a plain beat: velocity_ 0 = from its pattern (patternLevel_ 100 FLAT, 50 GHOST).
inline uint8_t laneMarkHeight(uint8_t velocity_, uint8_t patternLevel_) {
    const uint8_t level = noteLevel(velocity_, patternLevel_);  // 0-100
    const uint8_t span = kLaneMarkMaxHeight - kLaneMarkMinHeight;
    return static_cast<uint8_t>(kLaneMarkMinHeight + (level * span + 50) / 100);
}

inline bool laneMarkHollow(uint16_t note_) { return noteChancePercent(note_) < 100; }
inline bool laneMarkDot(uint16_t note_) { return noteCondCode(note_) != 0; }

// Rings.
const int16_t kRingCenterX = 200, kRingCenterY = 286;
const uint16_t kRingOuter = 140, kRingStep = 12;   // layer 1: r 140 ... layer 10: r 32
const uint16_t kRingPlayRadius = 150;              // the playhead marker orbits outside the rings

inline uint16_t ringRadius(uint8_t layer_) { return static_cast<uint16_t>(kRingOuter - layer_ * kRingStep); }

struct RingPoint {
    int16_t x, y;
};

// Where interval_ (0 .. songInterval_) lies on a ring of radius r_: the loop start is at the top,
// time runs clockwise.
inline RingPoint ringPoint(uint32_t interval_, uint32_t songInterval_, uint16_t r_) {
    const float turn = (songInterval_ > 0) ? static_cast<float>(interval_ % songInterval_) / static_cast<float>(songInterval_) : 0.0f;
    const float a = turn * 6.2831853f;
    return RingPoint{static_cast<int16_t>(kRingCenterX + lroundf(static_cast<float>(r_) * sinf(a))),
                     static_cast<int16_t>(kRingCenterY - lroundf(static_cast<float>(r_) * cosf(a)))};
}

// Dot radius of a beat on a ring: larger on the selected layer, and for louder beats there.
inline uint8_t ringDotRadius(bool selectedLayer_, uint8_t velocity_, uint8_t patternLevel_) {
    if (!selectedLayer_) return 2;
    return (noteLevel(velocity_, patternLevel_) >= 75) ? 4 : 3;
}

#endif
