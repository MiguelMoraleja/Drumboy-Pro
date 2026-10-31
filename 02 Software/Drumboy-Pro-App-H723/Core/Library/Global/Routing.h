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

// Old per-instrument settings (EQ on/off, Filter OFF/02/01+02, Effect OFF/02/01+02, Reverb on/off)
// <-> entry point. Kit and song files keep storing the four settings, so files stay compatible
// with older firmware. Entry point from the settings: the earliest stage that is on
// (same priority as the old layerInst_calculateMixMode).
inline uint8_t routeEntryFromLegacy(uint8_t eq_, uint8_t filter_, uint8_t effect_, uint8_t reverb_) {
    if (eq_) return NODE_EQ;
    if (filter_ == 2) return NODE_F1;  // 01+02
    if (filter_ == 1) return NODE_F2;  // 02
    if (effect_ == 2) return NODE_FX1;
    if (effect_ == 1) return NODE_FX2;
    if (reverb_) return NODE_REV;
    return NODE_MASTER;
}

// The four settings the old page produced for each entry point (its exclusive rules switch on
// every later stage), in the order eq, filter, effect, reverb.
const uint8_t kRouteLegacyFromEntry[kRouteNodeCount][4] = {
    {1, 2, 2, 1},  // EQ
    {0, 2, 2, 1},  // Filter 1 (01+02)
    {0, 1, 2, 1},  // Filter 2 (02)
    {0, 0, 2, 1},  // FX 1 (01+02)
    {0, 0, 1, 1},  // FX 2 (02)
    {0, 0, 0, 1},  // Reverb
    {0, 0, 0, 0},  // Master
};

// File storage. Kit and song files share a 1500-byte kit section whose spare bytes hold the patch:
//   490-495  module destinations (EQ ... Reverb), one mask each
//   498      marker 0xA5: the patch bytes are valid
//   240 + 25*layer + 23  the layer's destination mask (its entry point is also kept in the four
//            old settings at +16..+19, which stay authoritative so older firmware reads the file)
// Files without the marker (older firmware, or the blank-kit template whose spare bytes are
// spaces) load the default chain. An invalid module mask falls back to the default for that module.
const uint16_t kRouteFileModuleOffset = 490;
const uint16_t kRouteFileMarkerOffset = 498;
const uint8_t kRouteFileMarker = 0xA5;
const uint8_t kRouteFileLayerByte = 23;

inline void routeWritePatch(uint8_t* kit_, const RoutePatch& patch_) {
    for (uint8_t m = 0; m < kRouteModuleCount; m++) kit_[kRouteFileModuleOffset + m] = patch_.moduleDest[m];
    kit_[kRouteFileMarkerOffset] = kRouteFileMarker;
}

// Returns true when the file carried a patch.
inline bool routeReadPatch(const uint8_t* kit_, RoutePatch& patch_) {
    patch_ = kDefaultRoutePatch;
    if (kit_[kRouteFileMarkerOffset] != kRouteFileMarker) return false;
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        const uint8_t mask = kit_[kRouteFileModuleOffset + m];
        if (routeModuleDestValid(m, mask)) patch_.moduleDest[m] = mask;
    }
    return true;
}

// The metronome destination is a global setting, like the other metronome settings, kept in
// the system preset's byte 11 upper nibble as entry + 1 (byte 11 bit 0 is the line-in mute,
// saved but never loaded; older firmware ignores the byte). 0 = not set: the reverb, as before.
inline uint8_t routePackMetro(uint8_t presetByte_, uint8_t metroEntry_) {
    return static_cast<uint8_t>((presetByte_ & 0x0F) | ((metroEntry_ < kRouteNodeCount ? metroEntry_ + 1 : 0) << 4));
}

inline uint8_t routeUnpackMetro(uint8_t presetByte_) {
    const uint8_t v = presetByte_ >> 4;
    return (v >= 1 && v <= kRouteNodeCount) ? static_cast<uint8_t>(v - 1) : static_cast<uint8_t>(NODE_REV);
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
// gain_ (optional) scales each sample, for crossfades.
template <uint16_t kBlock>
inline void routeScatter(const RouteBuffers& buf_, const float* src_, uint8_t mask_, const float* gain_ = nullptr) {
    if (mask_ == 0) return;
    for (uint8_t n = 0; n < kRouteNodeCount; n++) {
        if (!(mask_ & routeBit(n))) continue;
        if (n == NODE_MASTER) {
            if (gain_) {
                for (uint16_t i = 0; i < kBlock; i++) {
                    const float v = src_[i] * gain_[i];
                    buf_.masterLeft[i] += v;
                    buf_.masterRight[i] += v;
                }
            } else {
                for (uint16_t i = 0; i < kBlock; i++) {
                    buf_.masterLeft[i] += src_[i];
                    buf_.masterRight[i] += src_[i];
                }
            }
        } else {
            float* dst = buf_.node[n];
            if (gain_) {
                for (uint16_t i = 0; i < kBlock; i++) dst[i] += src_[i] * gain_[i];
            } else {
                for (uint16_t i = 0; i < kBlock; i++) dst[i] += src_[i];
            }
        }
    }
}

// Click-free route changes. A continuously running signal (line-in, metronome, a module's
// output) whose destination changes fades out of the old nodes while fading into the new ones
// over kRouteFadeLength samples. A change that arrives during a fade waits until it has
// finished, so fast encoder turns never cut a fade short. A route that has never been set
// (kRouteUnset) takes its first destination at once.
const uint8_t kRouteFadeLength = 64;  // samples, ~1.5 ms at 44.1 kHz
const uint8_t kRouteUnset = 0xFF;

struct RouteFade {
    uint8_t mask = kRouteUnset;            // current destination
    uint8_t previous = 0;                  // destination fading out
    uint8_t position = kRouteFadeLength;   // samples into the running fade
};

inline bool routeFading(const RouteFade& fade_) { return fade_.position < kRouteFadeLength; }

// Call once per block with the wanted destination.
inline void routeFadeTarget(RouteFade& fade_, uint8_t mask_) {
    if (fade_.mask == kRouteUnset) {
        fade_.mask = mask_;
        fade_.position = kRouteFadeLength;
    } else if ((mask_ != fade_.mask) && !routeFading(fade_)) {
        fade_.previous = fade_.mask;
        fade_.mask = mask_;
        fade_.position = 0;
    }
}

// Adds src_ to its destination, crossfading if a fade is running, and advances the fade.
template <uint16_t kBlock>
inline void routeScatterFaded(const RouteBuffers& buf_, const float* src_, RouteFade& fade_) {
    if (!routeFading(fade_)) {
        routeScatter<kBlock>(buf_, src_, fade_.mask);
        return;
    }
    float up[kBlock], down[kBlock];
    for (uint16_t i = 0; i < kBlock; i++) {
        float t = (float)(fade_.position + i + 1) / (float)kRouteFadeLength;
        if (t > 1.0f) t = 1.0f;
        up[i] = t;
        down[i] = 1.0f - t;
    }
    routeScatter<kBlock>(buf_, src_, fade_.mask & fade_.previous);                                      // unchanged nodes
    routeScatter<kBlock>(buf_, src_, static_cast<uint8_t>(fade_.mask & ~fade_.previous), up);          // new nodes
    routeScatter<kBlock>(buf_, src_, static_cast<uint8_t>(fade_.previous & ~fade_.mask), down);        // old nodes
    const uint16_t next = fade_.position + kBlock;
    fade_.position = (next < kRouteFadeLength) ? static_cast<uint8_t>(next) : kRouteFadeLength;
}

// A source for one block: its buffer, where it goes, and its fade state. Sources that latch
// their route per note (instrument layers) pass fade = nullptr and switch at the next hit.
struct RouteSource {
    const float* buffer;
    uint8_t mask;
    RouteFade* fade;
};

// Fade state of the module outputs, kept between blocks.
struct RouteState {
    RouteFade module[kRouteModuleCount];
};

// Runs one block: clears the node inputs and the master, adds every source into its
// destinations, then processes the modules in order, each one passing its output on.
//   process_(m): processes node m in place; a module that is switched off must leave its buffer
//                unchanged (pass-through). The Reverb also writes reverbRight (zeroed beforehand),
//                which it sets equal to its input when switched off.
template <uint16_t kBlock, typename Process>
inline void routeBlock(const RoutePatch& patch_, RouteState& state_, const RouteBuffers& buf_, RouteSource* sources_, uint8_t sourceCount_, Process process_) {
    // 1. CLEAR
    for (uint8_t n = 0; n < kRouteModuleCount; n++) memset(buf_.node[n], 0, kBlock * sizeof(float));
    memset(buf_.reverbRight, 0, kBlock * sizeof(float));
    memset(buf_.masterLeft, 0, kBlock * sizeof(float));
    memset(buf_.masterRight, 0, kBlock * sizeof(float));

    // 2. SCATTER SOURCES
    for (uint8_t s = 0; s < sourceCount_; s++) {
        RouteSource& src = sources_[s];
        if (src.fade) {
            routeFadeTarget(*src.fade, src.mask);
            routeScatterFaded<kBlock>(buf_, src.buffer, *src.fade);
        } else {
            routeScatter<kBlock>(buf_, src.buffer, src.mask);
        }
    }

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
            routeFadeTarget(state_.module[m], dest);
            routeScatterFaded<kBlock>(buf_, buf_.node[m], state_.module[m]);
        }
    }
}

#endif
