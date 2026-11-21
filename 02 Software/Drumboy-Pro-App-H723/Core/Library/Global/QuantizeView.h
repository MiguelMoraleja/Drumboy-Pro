// The numbers of the quantize screen (backlog 59): the full-area view of the RHYTHM page while the QUANTIZE or
// the STRENGTH field is selected. Hardware-free so it can be tested on a PC (02 Software/tests, test_quantizeview);
// QuantizeScreens.h draws what this computes.
//
// The screen shows every grid as a row of points over two beats (the strip), the chosen grid highlighted, what
// the chosen grid is, and what STRENGTH does to a hit played a little late.
#ifndef __QUANTIZEVIEW_H
#define __QUANTIZEVIEW_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Quantize.h"

const uint16_t kQvSpan = 1600;                      // the strip shows two beats (intervals)
const uint16_t kQvStripX = 108, kQvStripW = 540;    // its columns on the screen
const uint8_t kQvMaxTicks = 40;                     // 1:64 has 32 points in two beats

enum QvKind : uint8_t { QV_STRAIGHT, QV_TRIPLET, QV_MIXED, QV_OFF };

inline QvKind qvKind(uint8_t grid_) {
    if (grid_ == kQuantizeGridOff) return QV_OFF;
    const QuantizeGridInfo& g = quantizeGrid(grid_);
    if (g.periodThirdsB != 0) return QV_MIXED;
    return (g.periodThirds % 3u == 0) ? QV_STRAIGHT : QV_TRIPLET;
}

// The column of an interval on the strip.
inline uint16_t qvStripXOf(uint16_t interval_) { return static_cast<uint16_t>(kQvStripX + (static_cast<uint32_t>(interval_) * kQvStripW) / kQvSpan); }

// A grid's name without its padding ("1:16+T").
inline void qvName(uint8_t grid_, char* out_, size_t size_) {
    const char* name = quantizeGrid(grid_).nameLongR;
    while (*name == ' ') name++;
    size_t n = 0;
    while ((n + 1 < size_) && (name[n] != 0)) {
        out_[n] = name[n];
        n++;
    }
    out_[n] = 0;
}

// Who owns a point: 1 = the straight grid, 2 = a triplet grid, 3 = both (a mixed grid's shared points).
struct QvTicks {
    uint8_t n;
    uint16_t at[kQvMaxTicks];
    uint8_t owner[kQvMaxTicks];
};

// The grid's points before the end of the strip, in order. OFF has none (every interval is a point).
inline void qvTicks(uint8_t grid_, QvTicks& out_) {
    out_.n = 0;
    if (grid_ == kQuantizeGridOff) return;
    const QuantizeGridInfo& g = quantizeGrid(grid_);
    const QvKind kind = qvKind(grid_);
    // Both grids' points in order, merged.
    uint32_t ka = 0, kb = 0;
    while (out_.n < kQvMaxTicks) {
        const uint32_t pa = quantizePointOf(g.periodThirds, ka);
        const uint32_t pb = (g.periodThirdsB != 0) ? quantizePointOf(g.periodThirdsB, kb) : 0xFFFFFFFFu;
        const uint32_t p = (pa < pb) ? pa : pb;
        if (p >= kQvSpan) break;
        uint8_t owner = 0;
        if (pa == p) {
            owner |= (kind == QV_TRIPLET) ? 2 : 1;
            ka++;
        }
        if (pb == p) {
            owner |= 2;
            kb++;
        }
        out_.at[out_.n] = static_cast<uint16_t>(p);
        out_.owner[out_.n] = owner;
        out_.n++;
    }
}

// How many points the grid has in one beat (800 intervals); 0 for OFF.
inline uint8_t qvPointsPerBeat(uint8_t grid_) {
    QvTicks t;
    qvTicks(grid_, t);
    uint8_t n = 0;
    for (uint8_t i = 0; i < t.n; i++) {
        if (t.at[i] < kQuantizeIntervalsPerBeat) n++;
    }
    return n;
}

// What STRENGTH does, with a hit played a little late of a point: where it was played, where it lands.
struct QvDemo {
    bool valid;         // false for OFF: nothing is moved
    uint16_t point;     // the grid point
    uint16_t played;    // the hit, later than the point
    uint16_t landed;    // where it lands at the strength
    uint16_t lateBy;    // played - point
    int16_t landedBy;   // landed - point (0 = on the point)
};

inline QvDemo qvDemo(uint8_t grid_, uint8_t strength_) {
    QvDemo d;
    memset(&d, 0, sizeof d);
    if (grid_ == kQuantizeGridOff) return d;
    // A point near the middle of the first beat, and a lateness of 30 intervals, or less on a fine grid.
    const uint16_t point = static_cast<uint16_t>(quantizeNearest(400, grid_));
    const QuantizeSpec spec = quantizeSpec(grid_);
    uint32_t gap = quantizeSpecNext(point, spec) - point;
    if (point > 0) {
        const uint32_t before = point - quantizeSpecPrev(point, spec);
        if (before < gap) gap = before;
    }
    uint16_t late = static_cast<uint16_t>((gap / 3 < 30) ? gap / 3 : 30);
    if (late < 1) late = 1;
    d.valid = true;
    d.point = point;
    d.lateBy = late;
    d.played = static_cast<uint16_t>(point + late);
    d.landed = quantizeSnap(d.played, grid_, strength_, kQvSpan);
    d.landedBy = static_cast<int16_t>(static_cast<int32_t>(d.landed) - point);
    return d;
}

// The three short texts of the grid panel (every text only uses characters of the firmware's font).
inline const char* qvKindName(QvKind k_) {
    static const char* const kName[4] = {"STRAIGHT GRID", "TRIPLET GRID", "MIXED GRID", "NO GRID"};
    return kName[k_];
}
inline void qvKindLine1(uint8_t grid_, char* out_, size_t size_) {
    switch (qvKind(grid_)) {
        case QV_STRAIGHT: snprintf(out_, size_, "A POINT EVERY %u INTERVALS", static_cast<unsigned>(quantizeGrid(grid_).periodThirds / 3u)); break;
        case QV_TRIPLET: snprintf(out_, size_, "3 NOTES IN THE TIME OF 2"); break;
        case QV_MIXED: snprintf(out_, size_, "STRAIGHT AND TRIPLET POINTS"); break;
        default: snprintf(out_, size_, "A HIT STAYS WHERE IT WAS PLAYED"); break;
    }
}
inline void qvKindLine2(uint8_t grid_, char* out_, size_t size_) {
    QvTicks t;
    qvTicks(grid_, t);
    switch (qvKind(grid_)) {
        case QV_MIXED: snprintf(out_, size_, "THE NEARER ONE WINS: %u PER BEAT", static_cast<unsigned>(qvPointsPerBeat(grid_))); break;
        case QV_OFF: snprintf(out_, size_, "EVERY INTERVAL IS A POINT"); break;
        default:
            if (t.n < 2) {  // 1:2 and 1:1: a point only every two or four beats
                snprintf(out_, size_, "A POINT EVERY %u BEATS", static_cast<unsigned>(quantizeGrid(grid_).periodThirds / 3u / kQuantizeIntervalsPerBeat));
            } else if (t.n % 2) {  // 1:4T: three points in two beats
                snprintf(out_, size_, "%u POINTS IN 2 BEATS", static_cast<unsigned>(t.n));
            } else if (t.n == 2) {
                snprintf(out_, size_, "1 POINT IN A BEAT");
            } else {
                snprintf(out_, size_, "%u POINTS IN A BEAT", static_cast<unsigned>(t.n / 2));
            }
            break;
    }
}

// What a change of the grid does (the QUANTIZE field moves every beat of every layer and bank onto the new grid).
inline void qvChangeNote(uint8_t grid_, char* out_, size_t size_) {
    if (grid_ == kQuantizeGridOff) snprintf(out_, size_, "OFF KEEPS THE BEATS WHERE THEY ARE");
    else snprintf(out_, size_, "A NEW GRID MOVES ALL BEATS ONTO IT");
}

// What the strength says in a few words.
inline void qvStrengthPhrase(uint8_t strength_, char* out_, size_t size_) {
    if (strength_ >= 100) snprintf(out_, size_, "A HIT SNAPS TO ITS POINT");
    else if (strength_ == 0) snprintf(out_, size_, "A HIT STAYS WHERE IT WAS PLAYED");
    else snprintf(out_, size_, "A HIT MOVES %u %% OF THE WAY", static_cast<unsigned>(strength_));
}

// The two lines under the demo picture.
inline void qvDemoLines(const QvDemo& d_, char* line1_, size_t size1_, char* line2_, size_t size2_) {
    snprintf(line1_, size1_, "PLAYED %u LATE", static_cast<unsigned>(d_.lateBy));
    if (d_.landedBy == 0) snprintf(line2_, size2_, "LANDS ON THE POINT");
    else snprintf(line2_, size2_, "LANDS %d LATE", static_cast<int>(d_.landedBy));
}

#endif
