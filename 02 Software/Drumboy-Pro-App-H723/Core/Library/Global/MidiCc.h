// MIDI control change input: the nanoKONTROL Studio (or any controller) on the DUB mixer. Hardware-free so
// it can be tested on a PC (02 Software/tests, test_midicc); the Controller applies what this decides.
//
// The controller sends on MIDI channel 3 (scene 3 of the Studio; the Drumboy's notes keep their own channel).
// Eight strips of the Studio control eight of the twelve columns of the dub desk: the ten layers, then the
// ECHO and the VERB return. TRACK < and > slide the eight strips along the twelve columns.
//
//   control            CC       on a layer column          on the ECHO column   on the VERB column
//   fader 1..8         20..27   LEVEL                      echo WET             reverb WET
//   knob 1..8          30..37   ECHO send                  echo FEEDBACK        reverb DECAY
//   M button 1..8      40..47   mute (a press toggles)     -                    -
//   TRACK <  and  >    58, 59   the strips move one column
//
// A moving fader sends hundreds of messages a second, and the receive interrupt only keeps the latest
// value of each controller (CcTable); the main loop applies it once per pass. After the strips moved, or at
// the start, a fader does not match what the screen shows: it takes over only when it reaches the shown
// value (CcTakeover), so a level never jumps.
#ifndef __MIDICC_H
#define __MIDICC_H

#include <stdint.h>

const uint8_t kCcChannel = 2;       // MIDI channel 3, counted from 0
const uint8_t kCcStrips = 8;        // the controller's strips
const uint8_t kCcColumns = 12;      // the desk's columns: ten layers, ECHO, VERB
const uint8_t kCcMaxWindow = kCcColumns - kCcStrips;
const uint8_t kCcFaderBase = 20, kCcKnobBase = 30, kCcMuteBase = 40;
const uint8_t kCcTrackPrev = 58, kCcTrackNext = 59;

enum CcKind : uint8_t { CC_NONE = 0, CC_FADER, CC_KNOB, CC_MUTE, CC_TRACK_PREV, CC_TRACK_NEXT };

struct CcControl {
    CcKind kind;
    uint8_t strip;  // 0..7 for a fader, a knob and a mute button
};

inline CcControl ccDecode(uint8_t cc_) {
    if ((cc_ >= kCcFaderBase) && (cc_ < kCcFaderBase + kCcStrips)) return CcControl{CC_FADER, static_cast<uint8_t>(cc_ - kCcFaderBase)};
    if ((cc_ >= kCcKnobBase) && (cc_ < kCcKnobBase + kCcStrips)) return CcControl{CC_KNOB, static_cast<uint8_t>(cc_ - kCcKnobBase)};
    if ((cc_ >= kCcMuteBase) && (cc_ < kCcMuteBase + kCcStrips)) return CcControl{CC_MUTE, static_cast<uint8_t>(cc_ - kCcMuteBase)};
    if (cc_ == kCcTrackPrev) return CcControl{CC_TRACK_PREV, 0};
    if (cc_ == kCcTrackNext) return CcControl{CC_TRACK_NEXT, 0};
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
    uint16_t seen[128];
    uint8_t pressSeen[128];

    CcTable() {
        for (uint8_t i = 0; i < 128; i++) {
            value[i] = 0;
            seq[i] = 0;
            presses[i] = 0;
            seen[i] = 0;
            pressSeen[i] = 0;
        }
    }

    // From the receive interrupt.
    void set(uint8_t cc_, uint8_t value_) {
        if (cc_ > 127) return;
        value[cc_] = value_;
        seq[cc_] = static_cast<uint16_t>(seq[cc_] + 1);
        if (value_ >= 64) presses[cc_] = static_cast<uint8_t>(presses[cc_] + 1);
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

// TRACK < and > slide the window along the columns, without wrapping.
inline uint8_t ccMoveWindow(uint8_t window_, int8_t by_) {
    const int w = static_cast<int>(window_) + by_;
    return static_cast<uint8_t>((w < 0) ? 0 : ((w > kCcMaxWindow) ? kCcMaxWindow : w));
}

#endif
