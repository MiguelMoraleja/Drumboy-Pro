// The numbers of the Performance page (backlog 45): the second step of the SONG key, a full-area view for playing
// live. Hardware-free so it can be tested on a PC (02 Software/tests, test_performview); PerformScreens.h draws
// what this computes.
//
// The top band shows the bar and the beat, the tempo and the chain. Below it, one tile for each of the ten
// layers: its number and name, its state in big letters, its pattern over 16 cells with the playhead, and its level.
#ifndef __PERFORMVIEW_H
#define __PERFORMVIEW_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

const uint8_t kPfLayers = 10;
const uint8_t kPfCells = 16;           // the pattern row: the loop in 16 equal cells
const uint8_t kPfMeterSegments = 13;   // as the desk's LED ladder (DubView.h): 0-8 green, 9-11 yellow, 12 red
const uint8_t kPfMeterGreen = 9, kPfMeterYellow = 12;

// Geometry (the lane area is x 20..939, y 131..439).
const uint16_t kPfTileX = 24, kPfTileStep = 92, kPfTileW = 88, kPfTileY = 218, kPfTileH = 216;
const uint16_t kPfNumberY = 224, kPfNameY = 244, kPfStateY = 262, kPfStateH = 30;
const uint16_t kPfPatternY = 302, kPfCellStep = 5, kPfCellW = 4, kPfCellH = 6, kPfHeadY = 310;
const uint16_t kPfMeterY = 322, kPfMeterPitch = 8, kPfMeterH = 6, kPfMeterW = 20;

enum PfState : uint8_t { PF_ON = 0, PF_MUTE, PF_SOLO, PF_SILENT };

// What a layer is doing: MUTE wins; a layer in the solo mask is SOLO; with any solo set the others are SILENT.
inline PfState pfState(bool mute_, uint16_t soloMask_, uint8_t layer_) {
    if (mute_) return PF_MUTE;
    if (soloMask_ & (1u << layer_)) return PF_SOLO;
    if (soloMask_ != 0) return PF_SILENT;
    return PF_ON;
}

inline const char* pfStateName(PfState s_) {
    static const char* const kName[4] = {"ON", "MUTE", "SOLO", "SOLO OFF"};
    return kName[s_ > PF_SILENT ? PF_ON : s_];
}

inline uint16_t pfTileX(uint8_t layer_) { return static_cast<uint16_t>(kPfTileX + layer_ * kPfTileStep); }

// The loop's 16 cells: a bit for every cell that holds the start of a beat.
inline uint16_t pfPattern(const uint16_t* starts_, uint16_t count_, uint16_t loopInterval_) {
    if (loopInterval_ == 0) return 0;
    uint16_t bits = 0;
    for (uint16_t i = 0; i < count_; i++) {
        if (starts_[i] >= loopInterval_) continue;
        const uint32_t cell = (static_cast<uint32_t>(starts_[i]) * kPfCells) / loopInterval_;
        bits = static_cast<uint16_t>(bits | (1u << (cell < kPfCells ? cell : kPfCells - 1)));
    }
    return bits;
}

// The cell the playhead is in (0 while stopped is up to the caller).
inline uint8_t pfPlayCell(uint16_t playInterval_, uint16_t loopInterval_) {
    if (loopInterval_ == 0) return 0;
    const uint32_t cell = (static_cast<uint32_t>(playInterval_ % loopInterval_) * kPfCells) / loopInterval_;
    return static_cast<uint8_t>(cell < kPfCells ? cell : kPfCells - 1);
}

// Bar and beat (both from 1) at a position: a bar is measure_ beats of beatInterval_ each; barCount_ bars make the loop.
inline void pfBarBeat(uint16_t playInterval_, uint8_t measure_, uint16_t beatInterval_, uint8_t barCount_, uint8_t& bar_, uint8_t& beat_) {
    bar_ = 1;
    beat_ = 1;
    if ((measure_ == 0) || (beatInterval_ == 0)) return;
    const uint32_t beats = playInterval_ / beatInterval_;
    uint32_t bar = beats / measure_;
    if ((barCount_ > 0) && (bar >= barCount_)) bar = barCount_ - 1;
    bar_ = static_cast<uint8_t>(bar + 1);
    beat_ = static_cast<uint8_t>(beats % measure_ + 1);
}

// Lit segments of a layer's level bar from the lane meter's pixels (0..full_), as the desk's ladder.
inline uint8_t pfMeterLit(uint16_t pixels_, uint16_t full_) {
    if ((full_ == 0) || (pixels_ == 0)) return 0;
    if (pixels_ > full_) pixels_ = full_;
    const uint32_t n = (static_cast<uint32_t>(pixels_) * kPfMeterSegments + full_ - 1) / full_;
    return static_cast<uint8_t>(n > kPfMeterSegments ? kPfMeterSegments : n);
}
inline uint16_t pfSegmentY(uint8_t k_) { return static_cast<uint16_t>(kPfMeterY + (kPfMeterSegments - 1 - k_) * kPfMeterPitch); }

// The chain as the top band shows it.
const uint8_t kPfMaxSteps = 16;
struct PfChain {
    uint8_t count;                 // steps in use
    uint8_t bank[kPfMaxSteps];     // 0..4
    uint8_t repeats[kPfMaxSteps];  // 1..16
    bool active;                   // chain play on
    bool loop;
    uint8_t step;                  // the step playing (or selected, when off)
    uint8_t loopsDone;             // its finished loops
};

inline uint16_t pfChainTotal(const PfChain& c_) {
    uint16_t n = 0;
    for (uint8_t i = 0; i < c_.count && i < kPfMaxSteps; i++) n = static_cast<uint16_t>(n + (c_.repeats[i] ? c_.repeats[i] : 1));
    return n;
}
// The width of one loop on the chain strip: at most 40 px, less for a long chain (the strip is 540 px; 256 loops at 2 px).
inline uint16_t pfChainUnit(const PfChain& c_) {
    const uint16_t total = pfChainTotal(c_);
    if (total == 0) return 40;
    const uint16_t u = static_cast<uint16_t>(540u / total);
    return u > 40 ? 40 : (u < 2 ? 2 : u);
}
// What follows the playing step: its number and bank, or -1 at the end of a chain that stops.
inline int8_t pfChainNext(const PfChain& c_) {
    if (c_.count == 0) return -1;
    if (c_.step + 1 < c_.count) return static_cast<int8_t>(c_.step + 1);
    return c_.loop ? 0 : -1;
}

#endif
