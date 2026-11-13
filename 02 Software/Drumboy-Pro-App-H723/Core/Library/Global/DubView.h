// The dub desk (DUB page): the lane area shows one strip per layer and the two returns. Geometry
// only, hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// Strip (60 px wide): number and state, name, the ECHO and VERB send bars, a fader for LEVEL with
// a level meter beside it, and the bus (FILT or DIR). Returns: ECHO (delay) and VERB, each with a
// fader for its WET, and for the echo a FEEDBACK bar.
#ifndef __DUBVIEW_H
#define __DUBVIEW_H

#include <stdint.h>

const uint16_t kDeskX = 24, kDeskStripStep = 60, kDeskStripWidth = 57;
const uint16_t kDeskNumY = 137, kDeskNameY = 150, kDeskEchoY = 174, kDeskVerbY = 186;
const uint16_t kDeskBarOffset = 18, kDeskBarWidth = 38, kDeskBarHeight = 7;     // bar frame inside a strip
const uint16_t kDeskFaderY = 204, kDeskFaderHeight = 208, kDeskThumbHeight = 7;
const uint16_t kDeskFaderLineX = 20, kDeskThumbX = 12, kDeskThumbWidth = 17;     // from the strip's x
const uint16_t kDeskMeterX = 38, kDeskMeterWidth = 7;
const uint16_t kDeskBusY = 428;
const uint16_t kDeskReturnX[2] = {624, 700};                                     // ECHO, VERB columns
const uint16_t kDeskTitleY = 137, kDeskSubY = 150, kDeskFeedbackY = 162;

inline uint16_t deskStripX(uint8_t strip_) { return static_cast<uint16_t>(kDeskX + strip_ * kDeskStripStep); }

// Fader thumb top for a level 0..100 (100 at the top).
inline uint16_t deskThumbY(uint8_t level_) {
    if (level_ > 100) level_ = 100;
    return static_cast<uint16_t>(kDeskFaderY + ((100u - level_) * (kDeskFaderHeight - kDeskThumbHeight)) / 100u);
}

// Filled pixels of a send bar for 0..100 %.
inline uint16_t deskBarFill(uint8_t percent_) {
    if (percent_ > 100) percent_ = 100;
    if (percent_ == 0) return 0;
    const uint16_t w = static_cast<uint16_t>((static_cast<uint32_t>(percent_) * (kDeskBarWidth - 2) + 50) / 100u);
    return (w == 0) ? 1 : w;
}

// Height of the meter beside a fader from the lane meter's pixels (0..18).
inline uint16_t deskMeterHeight(uint16_t pixels_, uint16_t full_) {
    if ((full_ == 0) || (pixels_ == 0)) return 0;
    if (pixels_ > full_) pixels_ = full_;
    return static_cast<uint16_t>((static_cast<uint32_t>(pixels_) * (kDeskFaderHeight - 2)) / full_);
}

#endif
