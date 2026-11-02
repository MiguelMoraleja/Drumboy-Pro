// Song mode: a chain of song banks played one after the other, each for a number of loops.
// Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// The sequencer calls chainLoopEnd() at every loop end while the chain plays; it says which bank
// to play next, or that the song is over. A manual bank change while the chain plays jumps the
// chain to the first step with that bank (chainJumpToBank); a bank the chain doesn't use turns
// the chain off. STOP keeps the position; RESET goes back to step 1 (chainRestart).
//
// File: song files (.rwf) get a chain block after their "EOF". Older firmware stops reading at
// "EOF", so the block needs no format version: it carries its own marker.
//   "CHN" | 1 (block version) | count | flags (bit 0 active, bit 1 loop) | 16 x (bank, repeats)
#ifndef __SONGCHAIN_H
#define __SONGCHAIN_H

#include <stdint.h>
#include <string.h>

const uint8_t kChainMaxSteps = 16;
const uint8_t kChainMaxRepeats = 16;
const uint8_t kChainBanks = 5;
const uint16_t kChainFileSize = 3 + 1 + 1 + 1 + 2 * kChainMaxSteps;  // 38

struct ChainStep {
    uint8_t bank;     // 0..4
    uint8_t repeats;  // 1..16 loops
};

struct SongChain {
    uint8_t count = 1;  // 1..16 steps in use
    ChainStep steps[kChainMaxSteps] = {{0, 1}};
    bool active = false;  // chain play on
    bool loop = true;     // after the last step: back to step 1 (true) or stop (false)
};

struct ChainPlay {
    uint8_t step = 0;       // step playing now
    uint8_t loopsDone = 0;  // loops of that step already finished
};

const int8_t kChainKeep = -1;  // stay on the current bank
const int8_t kChainStop = -2;  // the song is over: stop

inline void chainClear(SongChain& c_) { c_ = SongChain(); }

inline void chainRestart(const SongChain& c_, ChainPlay& p_) {
    (void)c_;
    p_.step = 0;
    p_.loopsDone = 0;
}

inline uint8_t chainCurrentBank(const SongChain& c_, const ChainPlay& p_) {
    return c_.steps[(p_.step < c_.count) ? p_.step : 0].bank;
}

// One loop finished: returns the bank to switch to, kChainKeep, or kChainStop.
inline int8_t chainLoopEnd(const SongChain& c_, ChainPlay& p_) {
    if (c_.count == 0) return kChainKeep;
    if (p_.step >= c_.count) p_.step = 0;
    const uint8_t before = c_.steps[p_.step].bank;
    p_.loopsDone++;
    if (p_.loopsDone < c_.steps[p_.step].repeats) return kChainKeep;
    p_.loopsDone = 0;
    if (p_.step + 1 < c_.count) {
        p_.step++;
    } else if (c_.loop) {
        p_.step = 0;
    } else {
        p_.step = 0;  // ready to play again from the top
        return kChainStop;
    }
    const uint8_t next = c_.steps[p_.step].bank;
    return (next == before) ? kChainKeep : static_cast<int8_t>(next);
}

// Manual bank change while the chain plays: the first step using that bank, from the current
// step on (wrapping), so a bank used twice continues forward. -1 if the chain doesn't use it.
inline int8_t chainJumpToBank(const SongChain& c_, ChainPlay& p_, uint8_t bank_) {
    for (uint8_t k = 0; k < c_.count; k++) {
        const uint8_t s = static_cast<uint8_t>((p_.step + k) % c_.count);
        if (c_.steps[s].bank == bank_) {
            p_.step = s;
            p_.loopsDone = 0;
            return static_cast<int8_t>(s);
        }
    }
    return -1;
}

// Editing. setCount grows by copying the last step; the play position stays inside the chain.
inline void chainSetCount(SongChain& c_, ChainPlay& p_, uint8_t count_) {
    if (count_ < 1) count_ = 1;
    if (count_ > kChainMaxSteps) count_ = kChainMaxSteps;
    for (uint8_t i = c_.count; i < count_; i++) c_.steps[i] = c_.steps[c_.count - 1];
    c_.count = count_;
    if (p_.step >= c_.count) chainRestart(c_, p_);
}

inline void chainSetStep(SongChain& c_, uint8_t step_, uint8_t bank_, uint8_t repeats_) {
    if (step_ >= c_.count) return;
    if (bank_ >= kChainBanks) bank_ = kChainBanks - 1;
    if (repeats_ < 1) repeats_ = 1;
    if (repeats_ > kChainMaxRepeats) repeats_ = kChainMaxRepeats;
    c_.steps[step_].bank = bank_;
    c_.steps[step_].repeats = repeats_;
}

// Total loops of the whole chain (for the screen).
inline uint16_t chainTotalLoops(const SongChain& c_) {
    uint16_t n = 0;
    for (uint8_t i = 0; i < c_.count; i++) n += c_.steps[i].repeats;
    return n;
}

inline void chainWrite(const SongChain& c_, uint8_t* out_) {
    memcpy(out_, "CHN", 3);
    out_[3] = 1;
    out_[4] = c_.count;
    out_[5] = static_cast<uint8_t>((c_.active ? 1 : 0) | (c_.loop ? 2 : 0));
    for (uint8_t i = 0; i < kChainMaxSteps; i++) {
        out_[6 + 2 * i] = c_.steps[i].bank;
        out_[7 + 2 * i] = c_.steps[i].repeats;
    }
}

// Returns false (and leaves the default chain) when the block is missing or not valid.
inline bool chainRead(SongChain& c_, const uint8_t* in_, uint16_t size_) {
    chainClear(c_);
    if ((size_ < kChainFileSize) || (memcmp(in_, "CHN", 3) != 0) || (in_[3] != 1)) return false;
    const uint8_t count = in_[4];
    if ((count < 1) || (count > kChainMaxSteps)) return false;
    SongChain c;
    c.count = count;
    c.active = (in_[5] & 1) != 0;
    c.loop = (in_[5] & 2) != 0;
    for (uint8_t i = 0; i < kChainMaxSteps; i++) {
        const uint8_t bank = in_[6 + 2 * i], repeats = in_[7 + 2 * i];
        if ((i < count) && ((bank >= kChainBanks) || (repeats < 1) || (repeats > kChainMaxRepeats))) return false;
        c.steps[i].bank = (bank < kChainBanks) ? bank : 0;
        c.steps[i].repeats = ((repeats >= 1) && (repeats <= kChainMaxRepeats)) ? repeats : 1;
    }
    c_ = c;
    return true;
}

#endif
