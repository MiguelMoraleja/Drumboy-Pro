// Quantization grids and snapping (backlog 59), hardware-free so it can be tested on a PC
// (02 Software/tests, test_quantize).
//
// A song is kMeasureInterval (800) intervals per beat; a recorded hit snaps to a grid. The first seven
// grids are the straight ones the firmware has always had (1:64 to 1:1), in the same order, so a song
// saved with one of them keeps its meaning. New ones are appended:
//   triplets  1:32T 1:16T 1:8T 1:4T  three notes in the time of two: 1:8T is 800/3 intervals
//   mixed     1:16+T 1:8+T           the straight grid and its triplet grid together: a hit snaps to
//                                    whichever point is nearer (a tie goes to the straight one)
//   OFF                              no snap: every interval is a grid point
// Positions are whole intervals; a triplet point is rounded to the nearest one (the error is under
// 1/1600 of a beat). The arithmetic is done in thirds of an interval so no grid needs a float.
//
// Strength (0..100 %) moves a hit that far towards its grid point: 100 snaps, 50 goes halfway, 0 leaves it.
#ifndef __QUANTIZE_H
#define __QUANTIZE_H

#include <stdint.h>

const uint8_t kQuantizeGridCount = 14;
const uint8_t kQuantizeStraightCount = 7;   // the grids of V1.17 and before: indices 0..6
const uint8_t kQuantizeGridOff = 13;
const uint8_t kQuantizeGridDefault = 2;     // 1:16
const uint8_t kQuantizeStrengthDefault = 100;
const uint16_t kQuantizeIntervalsPerBeat = 800;

struct QuantizeGridInfo {
    char nameLongR[11];     // right-aligned in the 10-character field (the font has no lowercase)
    uint16_t periodThirds;  // the grid's step in thirds of an interval
    uint16_t periodThirdsB; // a second grid snapped to as well (0 = none)
};

static const QuantizeGridInfo kQuantizeGridInfo[kQuantizeGridCount] = {
    {"      1:64", 150, 0},   {"      1:32", 300, 0},   {"      1:16", 600, 0},   {"       1:8", 1200, 0},  {"       1:4", 2400, 0},
    {"       1:2", 4800, 0},  {"       1:1", 9600, 0},  {"     1:32T", 200, 0},   {"     1:16T", 400, 0},   {"      1:8T", 800, 0},
    {"      1:4T", 1600, 0},  {"    1:16+T", 600, 400}, {"     1:8+T", 1200, 800}, {"       OFF", 3, 0}};

inline const QuantizeGridInfo& quantizeGrid(uint8_t grid_) { return kQuantizeGridInfo[(grid_ < kQuantizeGridCount) ? grid_ : kQuantizeGridDefault]; }

// One grid with one step size: what EUCLID can use (a mixed grid or OFF has no even steps).
inline bool quantizeIsSingleGrid(uint8_t grid_) { return (grid_ < kQuantizeGridCount) && (grid_ != kQuantizeGridOff) && (kQuantizeGridInfo[grid_].periodThirdsB == 0); }

// The k-th point of a single grid, in intervals (rounded to the nearest one).
inline uint32_t quantizePoint(uint8_t grid_, uint32_t k_) { return (k_ * quantizeGrid(grid_).periodThirds + 1u) / 3u; }

// How many points of a single grid lie inside a song (before its end): the first one is at 0. A grid longer
// than the song (1:1 in a one-beat song) has one point, the start. The EUCLID page walks these points.
inline uint32_t quantizePointsIn(uint8_t grid_, uint32_t songInterval_) {
    if (songInterval_ == 0) return 0;
    const uint32_t p = quantizeGrid(grid_).periodThirds;
    return (songInterval_ * 3u + p - 2u) / p;  // the smallest k whose point k*p/3 reaches the end
}

// The step the editing keys (MOVE, SHIFT ALL, insert) take: the grid's own, the straight one of a mixed grid,
// 1:64 when OFF.
inline uint16_t quantizeEditStep(uint8_t grid_) {
    if (grid_ == kQuantizeGridOff) return 50;
    return static_cast<uint16_t>((quantizeGrid(grid_).periodThirds + 1u) / 3u);
}

inline uint32_t quantizeNearestThirds(uint32_t x3_, uint16_t period3_) { return ((x3_ + period3_ / 2u) / period3_) * period3_; }

// The grid point nearest to interval_ (before the strength and the wrap): in whole intervals.
inline uint32_t quantizeNearest(uint16_t interval_, uint8_t grid_) {
    const QuantizeGridInfo& g = quantizeGrid(grid_);
    const uint32_t x3 = static_cast<uint32_t>(interval_) * 3u;
    uint32_t best3 = quantizeNearestThirds(x3, g.periodThirds);
    if (g.periodThirdsB != 0) {
        const uint32_t other3 = quantizeNearestThirds(x3, g.periodThirdsB);
        const uint32_t distA = (best3 > x3) ? best3 - x3 : x3 - best3, distB = (other3 > x3) ? other3 - x3 : x3 - other3;
        if (distB < distA) best3 = other3;  // a tie keeps the straight point
    }
    return (best3 + 1u) / 3u;
}

// Where a hit at interval_ lands: the nearest grid point, moved to only strength_ percent of the way, and
// the end of the song (or past it) wraps to the start, as it always did.
inline uint16_t quantizeSnap(uint16_t interval_, uint8_t grid_, uint8_t strength_, uint32_t songInterval_) {
    int32_t snapped = static_cast<int32_t>(quantizeNearest(interval_, grid_));
    if (strength_ < 100) {
        const int32_t delta = snapped - static_cast<int32_t>(interval_);
        snapped = static_cast<int32_t>(interval_) + (delta * static_cast<int32_t>(strength_) + ((delta >= 0) ? 50 : -50)) / 100;
    }
    if ((songInterval_ > 0) && (static_cast<uint32_t>(snapped) >= songInterval_)) snapped = 0;
    return static_cast<uint16_t>(snapped);
}

// --- Grids as a set of points, for stepping, inserting and EUCLID --------------------------------------
// A QuantizeSpec is one or two grids (a mixed grid has two) as periods in thirds of an interval; the points
// of a grid are P(k) = (k x period + 1) / 3 (k = 0, 1, 2 ...), and a spec's points are the points of both.
// A plain step of s intervals is {3 x s, 0}: BeatList.h and Euclid.h keep taking a step in intervals and
// make a spec of it, so the straight grids behave exactly as before.
struct QuantizeSpec {
    uint32_t p3;   // the grid's step in thirds of an interval (0 = no grid)
    uint32_t p3b;  // a second grid (0 = none)
};

inline QuantizeSpec quantizeSpecOfStep(uint16_t step_) { return QuantizeSpec{step_ * 3u, 0}; }

// The grid the editing keys and EUCLID step through. OFF has no points: it steps by 1:64.
inline QuantizeSpec quantizeSpec(uint8_t grid_) {
    if (grid_ == kQuantizeGridOff) return QuantizeSpec{kQuantizeGridInfo[0].periodThirds, 0};
    const QuantizeGridInfo& g = quantizeGrid(grid_);
    return QuantizeSpec{g.periodThirds, g.periodThirdsB};
}

inline uint32_t quantizePointOf(uint32_t p3_, uint32_t k_) { return (k_ * p3_ + 1u) / 3u; }

// The smallest point of the spec after x_ (not wrapped), the largest before x_ (x_ >= 1) and the largest at or
// before x_. Each is the best of the two grids.
inline uint32_t quantizeSpecNext(uint32_t x_, const QuantizeSpec& s_) {
    uint32_t best = quantizePointOf(s_.p3, (3u * x_ + 2u + s_.p3 - 1u) / s_.p3);
    if (s_.p3b != 0) {
        const uint32_t other = quantizePointOf(s_.p3b, (3u * x_ + 2u + s_.p3b - 1u) / s_.p3b);
        if (other < best) best = other;
    }
    return best;
}
inline uint32_t quantizeSpecPrev(uint32_t x_, const QuantizeSpec& s_) {
    uint32_t best = quantizePointOf(s_.p3, (3u * x_ - 2u) / s_.p3);
    if (s_.p3b != 0) {
        const uint32_t other = quantizePointOf(s_.p3b, (3u * x_ - 2u) / s_.p3b);
        if (other > best) best = other;
    }
    return best;
}
inline uint32_t quantizeSpecFloor(uint32_t x_, const QuantizeSpec& s_) {
    uint32_t best = quantizePointOf(s_.p3, (3u * x_ + 1u) / s_.p3);
    if (s_.p3b != 0) {
        const uint32_t other = quantizePointOf(s_.p3b, (3u * x_ + 1u) / s_.p3b);
        if (other > best) best = other;
    }
    return best;
}
inline bool quantizeSpecIsPoint(uint32_t x_, const QuantizeSpec& s_) { return quantizeSpecFloor(x_, s_) == x_; }

// How many points lie before the end of a song (both grids; the shared ones count twice, which only makes a
// bound a little larger).
inline uint32_t quantizeSpecCount(const QuantizeSpec& s_, uint32_t songInterval_) {
    if (songInterval_ == 0) return 0;
    uint32_t n = (songInterval_ * 3u + s_.p3 - 2u) / s_.p3;
    if (s_.p3b != 0) n += (songInterval_ * 3u + s_.p3b - 2u) / s_.p3b;
    return n;
}

// The step in whole intervals when the spec is one grid of whole intervals (every straight grid, and a step
// made by quantizeSpecOfStep), else 0: a triplet or a mixed grid has no single whole step.
inline uint16_t quantizeSpecWholeStep(const QuantizeSpec& s_) { return ((s_.p3b == 0) && (s_.p3 % 3u == 0)) ? static_cast<uint16_t>(s_.p3 / 3u) : 0; }

// --- Strength, saved with the song ---------------------------------------------------------------------
// A "QNT" block after the GEN block (older firmware never reads that far): the marker, a version and the
// strength. Without the block a song loads with the full snap (100).
const uint16_t kQuantizeFileSize = 5;

inline void quantizeWriteBlock(uint8_t strength_, uint8_t* out_) {
    out_[0] = 'Q';
    out_[1] = 'N';
    out_[2] = 'T';
    out_[3] = 1;
    out_[4] = (strength_ > 100) ? 100 : strength_;
}

// True when the block is whole and recognised (strength_ then holds its value, at most 100).
inline bool quantizeReadBlock(uint8_t& strength_, const uint8_t* in_, uint16_t size_) {
    if ((size_ < kQuantizeFileSize) || (in_[0] != 'Q') || (in_[1] != 'N') || (in_[2] != 'T') || (in_[3] != 1)) return false;
    strength_ = (in_[4] > 100) ? 100 : in_[4];
    return true;
}

#endif
