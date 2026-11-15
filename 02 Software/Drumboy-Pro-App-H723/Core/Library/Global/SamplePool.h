// Sample memory pool: the SDRAM area for samples (14.4 MB) is handed out by the length of each sample
// instead of in fifteen fixed slots, so a sample may be up to 20 s long (kSampleMaxFrames) as long as the
// pool has room. Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// A block is [offset, offset + bytes) in the pool, 64-byte aligned (two cache lines). A stereo sample takes
// two planes of the same size, one after the other (left, then right). The pool itself is only the list of
// the blocks in use: the caller keeps them (the firmware's sample sectors) and asks where a new one fits.
#ifndef __SAMPLEPOOL_H
#define __SAMPLEPOOL_H

#include <stdint.h>

const uint32_t kSamplePoolBytes = 14400000u;  // 0xC0000000 .. 0xC0DBBA00, the SDRAM below the delay buffers
const uint32_t kSampleMaxFrames = 882000u;    // 20 s at 44.1 kHz; a longer sample is ignored by the loader
const uint32_t kSamplePoolAlign = 64u;
const uint8_t kSamplePoolMaxBlocks = 16;

struct PoolBlock {
    uint32_t offset;
    uint32_t bytes;
};

inline uint32_t samplePoolRound(uint32_t bytes_) { return (bytes_ + kSamplePoolAlign - 1u) & ~(kSamplePoolAlign - 1u); }

// One channel of a sample of frames_ frames, rounded to the alignment (32-bit floats).
inline uint32_t samplePlaneBytes(uint32_t frames_) { return samplePoolRound(frames_ * 4u); }

// The whole sample: one plane, or two for a stereo one.
inline uint32_t sampleBytes(uint32_t frames_, bool stereo_) { return samplePlaneBytes(frames_) * (stereo_ ? 2u : 1u); }

// First place (lowest offset) where bytes_ fit between the used blocks, or -1. used_ has count_ blocks
// in any order; empty blocks (bytes 0) are skipped.
inline int64_t samplePoolFind(const PoolBlock* used_, uint8_t count_, uint32_t total_, uint32_t bytes_) {
    const uint32_t need = samplePoolRound(bytes_);
    if ((need == 0) || (need > total_) || (count_ > kSamplePoolMaxBlocks)) return -1;
    PoolBlock sorted[kSamplePoolMaxBlocks];
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; i++) {
        if (used_[i].bytes == 0) continue;
        uint8_t k = n++;
        while ((k > 0) && (sorted[k - 1].offset > used_[i].offset)) {
            sorted[k] = sorted[k - 1];
            k--;
        }
        sorted[k] = used_[i];
    }
    uint32_t cursor = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (sorted[i].offset >= cursor + need) return cursor;
        const uint32_t end = sorted[i].offset + samplePoolRound(sorted[i].bytes);
        if (end > cursor) cursor = end;
    }
    return (cursor + need <= total_) ? static_cast<int64_t>(cursor) : -1;
}

// Packing: when the free space is scattered, the blocks can be moved down so it becomes one gap at the
// end. The plan lists the blocks that change place, lowest first, each moved to the end of the one before
// it, so a move never lands on a block that has not moved yet (the copy may overlap its own source, which
// memmove handles). block is the index in the used_ array.
struct PoolMove {
    uint8_t block;
    uint32_t from;
    uint32_t to;
};

inline uint8_t samplePoolPlanCompact(const PoolBlock* used_, uint8_t count_, PoolMove* moves_) {
    if (count_ > kSamplePoolMaxBlocks) return 0;
    uint8_t order[kSamplePoolMaxBlocks];
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; i++) {
        if (used_[i].bytes == 0) continue;
        uint8_t k = n++;
        while ((k > 0) && (used_[order[k - 1]].offset > used_[i].offset)) {
            order[k] = order[k - 1];
            k--;
        }
        order[k] = i;
    }
    uint32_t cursor = 0;
    uint8_t moves = 0;
    for (uint8_t i = 0; i < n; i++) {
        const PoolBlock& b = used_[order[i]];
        if (b.offset != cursor) moves_[moves++] = {order[i], b.offset, cursor};
        cursor += samplePoolRound(b.bytes);
    }
    return moves;
}

// The bytes in use, and the largest block that would fit.
inline uint32_t samplePoolUsed(const PoolBlock* used_, uint8_t count_) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < count_; i++) sum += samplePoolRound(used_[i].bytes);
    return sum;
}

inline uint32_t samplePoolLargestFree(const PoolBlock* used_, uint8_t count_, uint32_t total_) {
    uint32_t lo = 0, hi = total_;
    while (lo < hi) {  // the largest size samplePoolFind accepts
        const uint32_t mid = lo + (hi - lo + 1) / 2;
        if (samplePoolFind(used_, count_, total_, mid) >= 0)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo & ~(kSamplePoolAlign - 1u);
}

// Share of the pool in use, per mille (the MEM bar).
inline uint32_t samplePoolPermille(uint32_t usedBytes_, uint32_t total_) {
    if (total_ == 0) return 0;
    const uint64_t p = (static_cast<uint64_t>(usedBytes_) * 1000u) / total_;
    return static_cast<uint32_t>((p > 1000u) ? 1000u : p);
}

// Seconds of playback (44.1 kHz) of a number of frames, for messages.
inline uint32_t sampleFramesToTenthSeconds(uint32_t frames_) { return static_cast<uint32_t>((static_cast<uint64_t>(frames_) * 10u) / 44100u); }

#endif
