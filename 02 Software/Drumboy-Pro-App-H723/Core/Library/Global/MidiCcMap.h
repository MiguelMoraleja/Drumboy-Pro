// MIDI CC input (backlog 30, dub mixer phase D0): a fixed CC map laid out like a Korg
// nanoKONTROL Studio (8 strips of fader, knob and Mute/Solo/Rec/Select, transport, Track, Cycle,
// Marker, jog wheel). Any controller that sends these CC numbers on the MIDI IN channel works the
// same way (BeatStep Pro, Launch Control XL 3, ...). Hardware-free so it can be tested on a PC
// (see 02 Software/tests/test_midicc.cpp).
//
// CC numbers, one decade per kind of control, strip n = 1..8:
//   fader n  20+n-1   knob n  30+n-1   Mute n  40+n-1   Solo n  50+n-1   Rec n  60+n-1   Select n  70+n-1
//   PLAY 80  STOP 81  REC 82  REW 83  FF 84  CYCLE 85  TRACK< 86  TRACK> 87
//   MARKER SET 88  MARKER< 89  MARKER> 90  JOG 91 (relative, two's complement: 1..63 up, 65..127 down)
//
// Two banks, chosen with TRACK< (A) and TRACK> (B):
//   A  strips 1-8 = layers 1-8: fader LEVEL, knob END, Mute = MUTE, Solo = SOLO
//   B  strips 1-2 = layers 9-10 (as in A)
//      strip 4: fader EFFECT 2 WET, knob EFFECT 2 A
//      strip 5: fader EFFECT 1 C (delay FEEDBACK), knob EFFECT 1 A (delay TIME)
//      strip 6: fader EFFECT 1 F (delay WET),      knob EFFECT 1 D (delay DAMPING)
//      strip 7: fader REVERB WET,                  knob REVERB DECAY
//      strip 8: fader FILTER 1 FREQUENCY,          knob FILTER 1 RESONANCE
//   Both banks: jog = FILTER 1 FREQUENCY, CYCLE held + jog = EFFECT 1 C (FEEDBACK),
//               PLAY / STOP / REC = the Drumboy's transport keys.
//   Rec and Select on the strips are reserved for the echo and reverb throws of phase D2.
//
// Faders and knobs are absolute and use pickup: after a song load, a bank change or an edit on
// the Drumboy, a control only takes over once it passes the current value (no jumps). Mute and
// Solo follow the button: 127 = on, 0 = off, so a Toggle button latches and a Momentary button
// mutes while held (a "drop").
#ifndef __MIDICCMAP_H
#define __MIDICCMAP_H

#include <stdint.h>

// Physical controls ----------------------------------------------------------------------------

enum CcControl : uint8_t {
    CCC_NONE = 0,
    CCC_FADER,
    CCC_KNOB,
    CCC_MUTE,
    CCC_SOLO,
    CCC_REC,
    CCC_SELECT,
    CCC_PLAY,
    CCC_STOP,
    CCC_RECORD,
    CCC_REW,
    CCC_FF,
    CCC_CYCLE,
    CCC_TRACK_PREV,
    CCC_TRACK_NEXT,
    CCC_MARKER_SET,
    CCC_MARKER_PREV,
    CCC_MARKER_NEXT,
    CCC_JOG,
};

struct CcInput {
    CcControl control;
    uint8_t strip;  // 0-7 for strip controls, 0 otherwise
};

const uint8_t kCcStripCount = 8;
const uint8_t kCcFaderBase = 20;
const uint8_t kCcKnobBase = 30;
const uint8_t kCcMuteBase = 40;
const uint8_t kCcSoloBase = 50;
const uint8_t kCcRecBase = 60;
const uint8_t kCcSelectBase = 70;
const uint8_t kCcPlay = 80;
const uint8_t kCcJog = 91;

inline CcInput ccLookup(uint8_t cc_) {
    const uint8_t bases[6] = {kCcFaderBase, kCcKnobBase, kCcMuteBase, kCcSoloBase, kCcRecBase, kCcSelectBase};
    const CcControl kinds[6] = {CCC_FADER, CCC_KNOB, CCC_MUTE, CCC_SOLO, CCC_REC, CCC_SELECT};
    for (uint8_t i = 0; i < 6; i++) {
        if ((cc_ >= bases[i]) && (cc_ < bases[i] + kCcStripCount)) return {kinds[i], static_cast<uint8_t>(cc_ - bases[i])};
    }
    if ((cc_ >= kCcPlay) && (cc_ <= kCcJog)) return {static_cast<CcControl>(CCC_PLAY + (cc_ - kCcPlay)), 0};
    return {CCC_NONE, 0};
}

// Faders and knobs: the latest value wins (kept per control). Buttons and the jog: every message
// counts (queued in order).
inline bool ccIsAbsolute(CcControl c_) { return (c_ == CCC_FADER) || (c_ == CCC_KNOB); }

// Index 0-15 of an absolute control: faders 0-7, knobs 8-15.
inline uint8_t ccAbsoluteIndex(const CcInput& in_) { return static_cast<uint8_t>(((in_.control == CCC_KNOB) ? kCcStripCount : 0) + in_.strip); }
inline CcInput ccAbsoluteInput(uint8_t index_) {
    return {(index_ < kCcStripCount) ? CCC_FADER : CCC_KNOB, static_cast<uint8_t>(index_ % kCcStripCount)};
}

// Targets on the Drumboy -----------------------------------------------------------------------

enum CcTarget : uint8_t {
    CCT_NONE = 0,
    CCT_LAYER_LEVEL,   // index = layer
    CCT_LAYER_END,     // index = layer
    CCT_LAYER_MUTE,    // index = layer (button)
    CCT_LAYER_SOLO,    // index = layer (button)
    CCT_EFFECT_PARAM,  // index = effect 0/1, param = 0..5 for A..F
    CCT_REVERB_WET,
    CCT_REVERB_DECAY,
    CCT_FILTER_FREQ,   // index = filter
    CCT_FILTER_RES,    // index = filter
};

struct CcAction {
    CcTarget target;
    uint8_t index;
    uint8_t param;
};

inline bool operator==(const CcAction& a_, const CcAction& b_) { return (a_.target == b_.target) && (a_.index == b_.index) && (a_.param == b_.param); }

const uint8_t kCcBankCount = 2;
const uint8_t kCcLayerCount = 10;

enum CcEffectParam : uint8_t { CCP_A = 0, CCP_B = 1, CCP_C = 2, CCP_D = 3, CCP_E = 4, CCP_F = 5 };

// The dub filter of the design is Filter 1 (index 0).
const CcAction kCcJogAction = {CCT_FILTER_FREQ, 0, 0};
const CcAction kCcCycleJogAction = {CCT_EFFECT_PARAM, 0, CCP_C};

// What a strip control does in a bank (faders, knobs, Mute, Solo). Rec and Select do nothing yet.
inline CcAction ccResolve(uint8_t bank_, CcControl control_, uint8_t strip_) {
    const CcAction none = {CCT_NONE, 0, 0};
    if ((bank_ >= kCcBankCount) || (strip_ >= kCcStripCount)) return none;

    // Layer strips: bank A 1-8, bank B 1-2 (layers 9-10).
    int16_t layer = -1;
    if (bank_ == 0) layer = strip_;
    else if (strip_ < kCcLayerCount - kCcStripCount) layer = static_cast<int16_t>(kCcStripCount + strip_);
    if (layer >= 0) {
        const uint8_t l = static_cast<uint8_t>(layer);
        switch (control_) {
            case CCC_FADER: return {CCT_LAYER_LEVEL, l, 0};
            case CCC_KNOB: return {CCT_LAYER_END, l, 0};
            case CCC_MUTE: return {CCT_LAYER_MUTE, l, 0};
            case CCC_SOLO: return {CCT_LAYER_SOLO, l, 0};
            default: return none;
        }
    }

    // Bank B desk, strips 4-8.
    const bool fader = (control_ == CCC_FADER);
    if (!fader && (control_ != CCC_KNOB)) return none;
    switch (strip_) {
        case 3: return fader ? CcAction{CCT_EFFECT_PARAM, 1, CCP_F} : CcAction{CCT_EFFECT_PARAM, 1, CCP_A};
        case 4: return fader ? CcAction{CCT_EFFECT_PARAM, 0, CCP_C} : CcAction{CCT_EFFECT_PARAM, 0, CCP_A};
        case 5: return fader ? CcAction{CCT_EFFECT_PARAM, 0, CCP_F} : CcAction{CCT_EFFECT_PARAM, 0, CCP_D};
        case 6: return fader ? CcAction{CCT_REVERB_WET, 0, 0} : CcAction{CCT_REVERB_DECAY, 0, 0};
        case 7: return fader ? CcAction{CCT_FILTER_FREQ, 0, 0} : CcAction{CCT_FILTER_RES, 0, 0};
        default: return none;
    }
}

// Value scaling --------------------------------------------------------------------------------

// 0-127 onto min..max, rounded; 127 always reaches max.
inline uint8_t ccToRange(uint8_t cc_, uint8_t min_, uint8_t max_) {
    if (cc_ > 127) cc_ = 127;
    if (max_ <= min_) return min_;
    const uint32_t span = static_cast<uint32_t>(max_ - min_);
    return static_cast<uint8_t>(min_ + (cc_ * span + 63) / 127);
}

// min..max onto 0-127, rounded (for pickup).
inline uint8_t rangeToCc(uint8_t value_, uint8_t min_, uint8_t max_) {
    if (max_ <= min_) return 0;
    if (value_ <= min_) return 0;
    if (value_ >= max_) return 127;
    const uint32_t span = static_cast<uint32_t>(max_ - min_);
    return static_cast<uint8_t>(((value_ - min_) * 127u + span / 2) / span);
}

// Relative value of the jog: two's complement in 7 bits (1..63 up, 65..127 down, 0 and 64 none).
inline int8_t ccRelative(uint8_t value_) {
    value_ &= 0x7F;
    if ((value_ == 0) || (value_ == 64)) return 0;
    return (value_ < 64) ? static_cast<int8_t>(value_) : static_cast<int8_t>(static_cast<int16_t>(value_) - 128);
}

// A relative step applied to a value, clamped to min..max.
inline uint8_t ccStepRange(uint8_t value_, int16_t delta_, uint8_t min_, uint8_t max_) {
    int16_t v = static_cast<int16_t>(value_) + delta_;
    if (v < min_) v = min_;
    if (v > max_) v = max_;
    return static_cast<uint8_t>(v);
}

inline bool ccButtonOn(uint8_t value_) { return value_ >= 64; }

// Pickup (soft takeover) -----------------------------------------------------------------------
//
// A control drives its target only once it has caught the target's current value: when it lands
// within kCcPickupWindow of it, or moves across it between two messages. It lets go when the
// target changes from elsewhere (an encoder, a song load), and when the bank changes (reset()).
// Values here are on the 0-127 scale (rangeToCc of the target's current value).

const uint8_t kCcPickupWindow = 3;
const uint8_t kCcNoValue = 0xFF;

struct CcPickup {
    uint8_t last = kCcNoValue;      // last value the control sent
    uint8_t observed = kCcNoValue;  // target value after our last update
    bool engaged = false;

    void reset() {
        last = kCcNoValue;
        observed = kCcNoValue;
        engaged = false;
    }
};

// Returns true when the control's value should be applied to the target now.
inline bool ccPickupStep(CcPickup& p_, uint8_t value_, uint8_t current_) {
    // Someone else moved the target since our last update: catch it again.
    if (p_.engaged && (p_.observed != kCcNoValue) && (current_ != p_.observed)) p_.engaged = false;

    if (!p_.engaged) {
        const int16_t d = static_cast<int16_t>(value_) - static_cast<int16_t>(current_);
        const bool near = (d >= -kCcPickupWindow) && (d <= kCcPickupWindow);
        bool crossed = false;
        if (p_.last != kCcNoValue) {
            crossed = ((p_.last <= current_) && (value_ >= current_)) || ((p_.last >= current_) && (value_ <= current_));
        }
        if (near || crossed) p_.engaged = true;
    }
    p_.last = value_;
    return p_.engaged;
}

// After applying (or failing to apply), record the target's value as we left it.
inline void ccPickupObserved(CcPickup& p_, uint8_t current_) { p_.observed = current_; }

#endif
