// Signal routing: where each source and each processing module sends its audio.
//
// Nodes run in a fixed order (EQ, Filter 1, Filter 2, FX 1, FX 2, Reverb) and a module may
// only send to a later node or to the master, so every patch is loop-free and adds no
// latency. A destination is a bitmask (bit n = node n), so one signal can feed several
// nodes; an empty mask mutes it. Signals stay mono until the Reverb, which produces the
// stereo pair; mono signals that reach the master go to both channels.
//
// Kept free of hardware headers so the routing can be tested on a PC (02 Software/tests).
#ifndef __ROUTING_H
#define __ROUTING_H

#include <stdint.h>
#include <string.h>

enum RouteNode : uint8_t {
    NODE_EQ = 0,
    NODE_F1,
    NODE_F2,
    NODE_FX1,
    NODE_FX2,
    NODE_REV,
    NODE_MASTER,
    kRouteNodeCount  // 7
};

const uint8_t kRouteModuleCount = NODE_MASTER;  // 6 processing modules
const uint8_t kRouteAllNodes = (1u << kRouteNodeCount) - 1;

constexpr uint8_t routeBit(uint8_t node_) { return static_cast<uint8_t>(1u << node_); }

// Where each module's output goes.
struct RoutePatch {
    uint8_t moduleDest[kRouteModuleCount];
};

// Today's serial chain: EQ -> Filter 1 -> Filter 2 -> FX 1 -> FX 2 -> Reverb -> Master.
const RoutePatch kDefaultRoutePatch = {{routeBit(NODE_F1), routeBit(NODE_F2), routeBit(NODE_FX1), routeBit(NODE_FX2), routeBit(NODE_REV), routeBit(NODE_MASTER)}};

// A module may send only to later nodes (no feedback); the Reverb always feeds the master.
inline bool routeModuleDestValid(uint8_t module_, uint8_t mask_) {
    if (module_ >= kRouteModuleCount) return false;
    if (module_ == NODE_REV) return mask_ == routeBit(NODE_MASTER);
    const uint8_t allowed = static_cast<uint8_t>(kRouteAllNodes & ~((routeBit(module_) << 1) - 1));  // bits above module_
    return (mask_ & ~allowed) == 0;
}

inline bool routePatchValid(const RoutePatch& patch_) {
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        if (!routeModuleDestValid(m, patch_.moduleDest[m])) return false;
    }
    return true;
}

// The firmware's per-source "mixer in" setting (MixerInMode: EQ, FILTER_0, FILTER_1, EFFECT_0,
// EFFECT_1, REVERB, SEND) lists the nodes in the same order, SEND being the master. So an
// entry point converts directly to a single-destination mask; anything out of range is muted.
inline uint8_t routeMaskFromEntry(uint8_t mixerInMode_) {
    return (mixerInMode_ < kRouteNodeCount) ? routeBit(mixerInMode_) : 0;
}

// Buffers for one block. node[m] is module m's input, processed in place to become its output.
// The Reverb writes its left output in place and its right output to reverbRight.
struct RouteBuffers {
    float* node[kRouteModuleCount];
    float* reverbRight;
    float* masterLeft;
    float* masterRight;
};

// Adds a mono buffer into every node in mask_ (the master gets it in both channels).
template <uint16_t kBlock>
inline void routeScatter(const RouteBuffers& buf_, const float* src_, uint8_t mask_) {
    if (mask_ == 0) return;
    for (uint8_t n = 0; n < kRouteModuleCount; n++) {
        if (mask_ & routeBit(n)) {
            float* dst = buf_.node[n];
            for (uint16_t i = 0; i < kBlock; i++) dst[i] += src_[i];
        }
    }
    if (mask_ & routeBit(NODE_MASTER)) {
        for (uint16_t i = 0; i < kBlock; i++) {
            buf_.masterLeft[i] += src_[i];
            buf_.masterRight[i] += src_[i];
        }
    }
}

// Runs one block: clears the node inputs and the master, adds every source into its
// destinations, then processes the modules in order, each one passing its output on.
//   sources_[s], sourceMasks_[s]: mono source buffers and their destinations
//   process_(m): processes node m in place; a module that is switched off must leave its buffer
//                unchanged (pass-through). The Reverb also writes reverbRight (zeroed beforehand),
//                which it sets equal to its input when switched off.
template <uint16_t kBlock, typename Process>
inline void routeBlock(const RoutePatch& patch_, const RouteBuffers& buf_, const float* const* sources_, const uint8_t* sourceMasks_, uint8_t sourceCount_, Process process_) {
    // 1. CLEAR
    for (uint8_t n = 0; n < kRouteModuleCount; n++) memset(buf_.node[n], 0, kBlock * sizeof(float));
    memset(buf_.reverbRight, 0, kBlock * sizeof(float));
    memset(buf_.masterLeft, 0, kBlock * sizeof(float));
    memset(buf_.masterRight, 0, kBlock * sizeof(float));

    // 2. SCATTER SOURCES
    for (uint8_t s = 0; s < sourceCount_; s++) routeScatter<kBlock>(buf_, sources_[s], sourceMasks_[s]);

    // 3. PROCESS MODULES IN ORDER, PASSING EACH OUTPUT ON
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        process_(m);
        if (m == NODE_REV) {
            if (patch_.moduleDest[m] & routeBit(NODE_MASTER)) {
                for (uint16_t i = 0; i < kBlock; i++) {
                    buf_.masterLeft[i] += buf_.node[NODE_REV][i];
                    buf_.masterRight[i] += buf_.reverbRight[i];
                }
            }
        } else {
            // Only later nodes are allowed, so an output never lands in an already processed node.
            const uint8_t dest = patch_.moduleDest[m] & static_cast<uint8_t>(~((routeBit(m) << 1) - 1));
            routeScatter<kBlock>(buf_, buf_.node[m], dest);
        }
    }
}

#endif
