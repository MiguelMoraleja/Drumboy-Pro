// The numbers of "Quantize now" (backlog 59): quantizing notes that are already recorded, with a look before it is
// done. Hardware-free so it can be tested on a PC (02 Software/tests, test_quantizenow); QuantizeNowScreens.h draws
// what this computes. The snapping is Quantize.h's own (quantizeSnap: the nearest grid point, moved only STRENGTH
// percent of the way, the end of the song wrapping to the start), so a note lands where a recorded hit would.
#ifndef __QUANTIZENOW_H
#define __QUANTIZENOW_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Quantize.h"

const uint8_t kQnScopeLayer = 0;  // the selected layer, the active bank
const uint8_t kQnScopeBank = 1;   // every layer, the active bank
const uint8_t kQnScopeAll = 2;    // every layer, every bank
const uint8_t kQnScopes = 3;
const uint16_t kQnMaxBeats = 256;  // beats of one bank the counting can hold
const uint16_t kQnSpan = 1600;     // the picture shows the first two beats of the loop (intervals)
const uint8_t kQnMaxNotes = 16;    // notes in the picture (1:64 has 16 points in two beats)
const uint8_t kQnMaxGrid = 12;     // the last grid with points (13 is OFF: nothing to snap to)

inline const char* qnScopeName(uint8_t scope_) {
    static const char* const kName[kQnScopes] = {"THIS LAYER", "THIS BANK", "ALL LAYERS"};
    return kName[scope_ < kQnScopes ? scope_ : 0];
}

// How far a note moves (the song is a loop: a note at the end that lands on the start moves only a little).
inline uint32_t qnDistance(uint16_t from_, uint16_t to_, uint32_t song_) {
    const uint32_t d = (from_ > to_) ? (from_ - to_) : (to_ - from_);
    if ((song_ > 0) && (d > song_ / 2)) return song_ - d;
    return d;
}

struct QnTotals {
    uint32_t notes, moves, onGrid, merged, largest;
};

// Adds one bank's notes (their start intervals) to the totals. A note "moves" when it lands somewhere else, is "on
// grid" when it is already on a point, and counts as "merged" when another note of the bank lands on the same point
// (the bank keeps one of them).
inline void qnAddBank(QnTotals& t_, const uint16_t* starts_, uint16_t count_, uint8_t grid_, uint8_t strength_, uint32_t song_) {
    if (count_ > kQnMaxBeats) count_ = kQnMaxBeats;
    uint16_t lands[kQnMaxBeats];
    for (uint16_t i = 0; i < count_; i++) {
        const uint16_t s = starts_[i];
        const uint16_t l = (grid_ == kQuantizeGridOff) ? s : quantizeSnap(s, grid_, strength_, song_);
        lands[i] = l;
        t_.notes++;
        if (l != s) t_.moves++;
        if ((grid_ == kQuantizeGridOff) || (quantizeNearest(s, grid_) == s)) t_.onGrid++;
        const uint32_t d = qnDistance(s, l, song_);
        if (d > t_.largest) t_.largest = d;
        for (uint16_t j = 0; j < i; j++) {
            if (lands[j] == l) {
                t_.merged++;
                break;
            }
        }
    }
}

struct QnNote {
    uint16_t start, land;
};

// The notes of the first two beats for the picture, in the order given: where each is and where it would land.
inline uint8_t qnPreview(const uint16_t* starts_, uint16_t count_, uint8_t grid_, uint8_t strength_, uint32_t song_, QnNote* out_) {
    uint8_t n = 0;
    for (uint16_t i = 0; (i < count_) && (n < kQnMaxNotes); i++) {
        if (starts_[i] >= kQnSpan) continue;
        out_[n].start = starts_[i];
        out_[n].land = (grid_ == kQuantizeGridOff) ? starts_[i] : quantizeSnap(starts_[i], grid_, strength_, song_);
        n++;
    }
    return n;
}

// The APPLY field: two steps, so that a stray turn never changes the notes.
enum QnApplyState : uint8_t { QN_READY = 0, QN_SURE, QN_DONE, QN_NOGRID };

inline const char* qnApplyName(QnApplyState s_) {
    static const char* const kName[4] = {"READY", "SURE?", "DONE", "NO GRID"};
    return kName[s_ > QN_NOGRID ? QN_READY : s_];
}

// Turning APPLY right: READY or DONE arms it (SURE?), SURE? applies; turning left disarms. Returns the next
// state, and sets apply_ when this turn is the one that applies.
inline QnApplyState qnApplyTurn(QnApplyState s_, bool right_, bool& apply_) {
    apply_ = false;
    if (s_ == QN_NOGRID) return QN_NOGRID;
    if (!right_) return QN_READY;
    if (s_ == QN_SURE) {
        apply_ = true;
        return QN_DONE;
    }
    return QN_SURE;
}

#endif
