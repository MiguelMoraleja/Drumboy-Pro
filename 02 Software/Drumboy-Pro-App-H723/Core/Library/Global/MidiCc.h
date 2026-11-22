// MIDI control change input: the nanoKONTROL Studio (or any controller) on the DUB mixer. Hardware-free so
// it can be tested on a PC (02 Software/tests, test_midicc); the Controller applies what this decides.
//
// The numbers are the nanoKONTROL Studio's own (read from the unit, no editor needed) and the channel does not
// matter: a control change on any channel counts (notes keep the channel set on the MIDI page).
// Eight strips of the Studio control eight of the twelve columns of the dub desk: the ten layers, then the
// ECHO and the VERB return. TRACK < and > slide the eight strips along the twelve columns.
//
//   control            CC                  on a layer column          on the ECHO column   on the VERB column
//   fader 1..8         2 3 4 5 6 8 9 12    LEVEL                      echo WET             reverb WET
//   knob 1..8          13..20              ECHO send                  echo FEEDBACK        reverb DECAY
//   M button 1..8      21..28              mute (a press toggles)     -                    -
//   TRACK <  and  >    60, 61              the strips move one column
//   S button 1..8      29..36 (see below)  ECHO throw while held   -                    -
//   R button 1..8      37..44 (see below)  VERB throw while held   -                    -
//   X-Y pad X, Y       45, 46 (see below)  Filter 1 FREQ (X), echo FEEDBACK (Y, past 100 % at the top)
//   jog wheel          47 (see below)      echo TIME, a step per click
// The S, R, X-Y and jog numbers (29 to 47) are a first guess that continues the Studio's own run (the owner reads
// them on the unit and the numbers are corrected at one place, here); the DUB desk shows the last controller
// message ("CC 37 127") for a few seconds so they can be read off the Drumboy itself.
//
// A moving fader sends hundreds of messages a second, and the receive interrupt only keeps the latest
// value of each controller (CcTable); the main loop applies it once per pass. After the strips moved, or at
// the start, a fader does not match what the screen shows: it takes over only when it reaches the shown
// value (CcTakeover), so a level never jumps.
#ifndef __MIDICC_H
#define __MIDICC_H

#include <stdint.h>

const uint8_t kCcStrips = 8;        // the controller's strips
const uint8_t kCcColumns = 13;      // the desk's columns: ten layers, ECHO, VERB, LINE IN
const uint8_t kCcMaxWindow = kCcColumns - kCcStrips;
const uint8_t kCcFader[kCcStrips] = {2, 3, 4, 5, 6, 8, 9, 12};  // the faders are not consecutive
const uint8_t kCcKnobBase = 13, kCcMuteBase = 21;               // knobs 13..20, M buttons 21..28
const uint8_t kCcTrackPrev = 60, kCcTrackNext = 61;
const uint8_t kCcSoloBase = 29, kCcRecBase = 37;                // S buttons 29..36, R buttons 37..44 (throws)
const uint8_t kCcPadX = 45, kCcPadY = 46, kCcJog = 47;          // the X-Y pad and the jog wheel

enum CcKind : uint8_t { CC_NONE = 0, CC_FADER, CC_KNOB, CC_MUTE, CC_TRACK_PREV, CC_TRACK_NEXT, CC_SOLO, CC_REC, CC_PAD_X, CC_PAD_Y, CC_JOG };

struct CcControl {
    CcKind kind;
    uint8_t strip;  // 0..7 for a fader, a knob and a mute button
};

inline CcControl ccDecode(uint8_t cc_) {
    for (uint8_t s = 0; s < kCcStrips; s++) {
        if (cc_ == kCcFader[s]) return CcControl{CC_FADER, s};
    }
    if ((cc_ >= kCcKnobBase) && (cc_ < kCcKnobBase + kCcStrips)) return CcControl{CC_KNOB, static_cast<uint8_t>(cc_ - kCcKnobBase)};
    if ((cc_ >= kCcMuteBase) && (cc_ < kCcMuteBase + kCcStrips)) return CcControl{CC_MUTE, static_cast<uint8_t>(cc_ - kCcMuteBase)};
    if (cc_ == kCcTrackPrev) return CcControl{CC_TRACK_PREV, 0};
    if (cc_ == kCcTrackNext) return CcControl{CC_TRACK_NEXT, 0};
    if ((cc_ >= kCcSoloBase) && (cc_ < kCcSoloBase + kCcStrips)) return CcControl{CC_SOLO, static_cast<uint8_t>(cc_ - kCcSoloBase)};
    if ((cc_ >= kCcRecBase) && (cc_ < kCcRecBase + kCcStrips)) return CcControl{CC_REC, static_cast<uint8_t>(cc_ - kCcRecBase)};
    if (cc_ == kCcPadX) return CcControl{CC_PAD_X, 0};
    if (cc_ == kCcPadY) return CcControl{CC_PAD_Y, 0};
    if (cc_ == kCcJog) return CcControl{CC_JOG, 0};
    return CcControl{CC_NONE, 0};
}

// The column of the desk a strip controls, for a window position 0..kCcMaxWindow.
inline uint8_t ccColumn(uint8_t window_, uint8_t strip_) { return static_cast<uint8_t>(window_ + strip_); }

// 0..127 as the 0..100 scale of the dub mixer's levels and sends (127 = 100).
inline uint8_t ccTo100(uint8_t value_) { return static_cast<uint8_t>(((value_ > 127 ? 127 : value_) * 100u + 63u) / 127u); }

// A 0..100 position inside a parameter's range [min_, max_], and back.
inline uint8_t ccToRange(uint8_t percent_, uint8_t min_, uint8_t max_) {
    const uint8_t p = (percent_ > 100) ? 100 : percent_;
    return static_cast<uint8_t>(min_ + ((static_cast<uint32_t>(max_ - min_) * p + 50u) / 100u));
}
inline uint8_t ccFromRange(uint8_t value_, uint8_t min_, uint8_t max_) {
    if (max_ <= min_) return 0;
    const uint8_t v = (value_ < min_) ? min_ : ((value_ > max_) ? max_ : value_);
    return static_cast<uint8_t>(((static_cast<uint32_t>(v - min_) * 100u) + (max_ - min_) / 2u) / (max_ - min_));
}

// The latest value of each controller, and how many times each was pressed (value 64 or more). The receive
// interrupt is the only writer of value, seq and presses; the main loop only reads them and keeps its own
// copies (seen), so nothing is shared for writing and no lock is needed. A value read together with a newer
// sequence than it belongs to is simply read again at the next pass. The sequence is 16 bits (a single aligned
// access on the Cortex-M7): a stall of the main loop that lets exactly 65536 messages of one controller go by
// would hide the last one, and a fader sends a few hundred a second.
struct CcTable {
    volatile uint8_t value[128];
    volatile uint16_t seq[128];
    volatile uint8_t presses[128];
    volatile uint16_t ups[128];    // messages of 1 to 63: a relative control (the jog wheel) turned one way
    volatile uint16_t downs[128];  // messages of 65 to 127: the other way
    uint16_t seen[128];
    uint8_t pressSeen[128];
    uint16_t upSeen[128];
    uint16_t downSeen[128];

    CcTable() {
        for (uint8_t i = 0; i < 128; i++) {
            value[i] = 0;
            seq[i] = 0;
            presses[i] = 0;
            ups[i] = 0;
            downs[i] = 0;
            seen[i] = 0;
            pressSeen[i] = 0;
            upSeen[i] = 0;
            downSeen[i] = 0;
        }
    }

    // From the receive interrupt.
    void set(uint8_t cc_, uint8_t value_) {
        if (cc_ > 127) return;
        value[cc_] = value_;
        seq[cc_] = static_cast<uint16_t>(seq[cc_] + 1);
        if (value_ >= 64) presses[cc_] = static_cast<uint8_t>(presses[cc_] + 1);
        if ((value_ >= 1) && (value_ <= 63)) ups[cc_] = static_cast<uint16_t>(ups[cc_] + 1);
        if ((value_ >= 65) && (value_ <= 127)) downs[cc_] = static_cast<uint16_t>(downs[cc_] + 1);
    }

    // From the main loop: true when the controller sent something since the last call, with its latest value.
    // (anySince: any controller at all, for "the controller is there".)
    bool poll(uint8_t cc_, uint8_t& value_) {
        const uint16_t s = seq[cc_ & 127];
        if (s == seen[cc_ & 127]) return false;
        value_ = value[cc_ & 127];
        seen[cc_ & 127] = s;
        return true;
    }

    // From the main loop: the clicks of a relative control since the last call, one per message (up minus down).
    int16_t takeRelative(uint8_t cc_) {
        const uint16_t u = ups[cc_ & 127], d = downs[cc_ & 127];
        const int32_t n32 = static_cast<int32_t>(static_cast<uint16_t>(u - upSeen[cc_ & 127])) - static_cast<int32_t>(static_cast<uint16_t>(d - downSeen[cc_ & 127]));
        const int16_t n = static_cast<int16_t>((n32 > 32000) ? 32000 : ((n32 < -32000) ? -32000 : n32));
        upSeen[cc_ & 127] = u;
        downSeen[cc_ & 127] = d;
        return n;
    }

    // Is a button held now (its last value is 64 or more)?
    bool held(uint8_t cc_) const { return value[cc_ & 127] >= 64; }

    // From the main loop: how many presses came since the last call (a press that came and went between two
    // passes still counts).
    uint8_t takePresses(uint8_t cc_) {
        const uint8_t p = presses[cc_ & 127];
        const uint8_t n = static_cast<uint8_t>(p - pressSeen[cc_ & 127]);
        pressSeen[cc_ & 127] = p;
        return n;
    }
};

// Take-over of one fader or knob: it applies only once it has reached the value the screen shows (within 2
// percent), or has moved across it since the last message. It lets go again when the value changed by
// another route (the encoder, a loaded song, undo): what it set last no longer matches what is shown.
const uint8_t kCcExternalTolerance = 4;  // percent: more than this between what the controller set and what is shown = changed elsewhere

struct CcTakeover {
    bool engaged = false;
    bool hasLast = false;
    uint8_t last = 0;
    bool hasApplied = false;
    uint8_t applied = 0;  // what the controller set last, on the 0..100 scale
};

inline void ccDisengage(CcTakeover& t_) {
    t_.engaged = false;
    t_.hasLast = false;
    t_.hasApplied = false;
}

// scaled_: the controller's value on the 0..100 scale; current_: what the parameter is now (0..100).
// True when the value should be applied.
inline bool ccTakeoverStep(CcTakeover& t_, uint8_t scaled_, uint8_t current_) {
    if (t_.engaged && t_.hasApplied) {
        const int moved = static_cast<int>(current_) - static_cast<int>(t_.applied);
        if ((moved > kCcExternalTolerance) || (moved < -kCcExternalTolerance)) {
            t_.engaged = false;  // changed elsewhere: the fader has to reach the new value first
            t_.hasApplied = false;
        }
    }
    if (!t_.engaged) {
        const int diff = static_cast<int>(scaled_) - static_cast<int>(current_);
        if ((diff >= -2) && (diff <= 2)) {
            t_.engaged = true;
        } else if (t_.hasLast && ((t_.last <= current_) != (scaled_ <= current_))) {
            t_.engaged = true;  // it crossed the shown value between two messages
        }
    }
    t_.last = scaled_;
    t_.hasLast = true;
    if (t_.engaged) {
        t_.applied = scaled_;
        t_.hasApplied = true;
    }
    return t_.engaged;
}

// The X-Y pad's Y axis as the echo's FEEDBACK. 0 to 80 % of the travel is the echo's normal range (index 0 to
// kMax, the feedback gain 0 to 0.8); the last fifth adds a boost on top of the largest normal feedback, up to
// kCcFeedbackBoost, so the loop gain goes past 1 (100 %) at the very top and the echo builds up and
// self-oscillates (the loop is soft-clipped while the boost is on, so it stays bounded).
const float kCcFeedbackBoost = 0.35f;
inline void ccFeedbackFromPad(uint8_t percent_, uint8_t maxIndex_, uint8_t& index_, float& boost_) {
    const uint8_t p = (percent_ > 100) ? 100 : percent_;
    if (p <= 80) {
        index_ = static_cast<uint8_t>((static_cast<uint32_t>(maxIndex_) * p + 40u) / 80u);
        boost_ = 0.0f;
    } else {
        index_ = maxIndex_;
        boost_ = kCcFeedbackBoost * static_cast<float>(p - 80) / 20.0f;
    }
}

// TRACK < and > slide the window along the columns, without wrapping.
inline uint8_t ccMoveWindow(uint8_t window_, int8_t by_) {
    const int w = static_cast<int>(window_) + by_;
    return static_cast<uint8_t>((w < 0) ? 0 : ((w > kCcMaxWindow) ? kCcMaxWindow : w));
}

#endif
