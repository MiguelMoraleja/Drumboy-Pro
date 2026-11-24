// The stereo stage page drawn (backlog 71): what StageView.h computes, put on the lane area. Header-only and templated on the
// LCD like the other screens (test: 02 Software/tests, test_stageview). Texts use only the characters of the firmware's font.
#ifndef __STAGESCREENS_H
#define __STAGESCREENS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GeneratorScreens.h"  // the colours, fonts, the lane area and gsDraw
#include "StageView.h"

const uint16_t kStRed = 0xF800;

// The frame: the three marks above the stage, the centre line and the quarter lines, the correlation label.
template <class G>
inline void stDrawFrame(G& g) {
    g.setBackColor(kGsBlack);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray75);
    gsDraw(g, "LEFT", kStStageX, 138);
    g.setAlignment(GS_CENTER);
    gsDraw(g, "CENTRE", static_cast<uint16_t>(kStStageX + kStStageW / 2), 138);
    g.setAlignment(GS_RIGHT);
    gsDraw(g, "RIGHT", static_cast<uint16_t>(kStStageX + kStStageW), 138);
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray50);
    gsDraw(g, "CORRELATION", kStCorrRight, 139);
    g.setAlignment(GS_LEFT);
}

// One layer's row: its name, the bar, the bass mark and the correlation. selected_ outlines it.
template <class G>
inline void stDrawRow(G& g, uint8_t layer_, const char* name_, const StRow& r_, bool selected_, bool playing_) {
    const uint16_t y = stRowY(layer_);
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(24, y, 902, kStRowH);
    // the guide lines of the stage
    for (uint8_t q = 0; q < 5; q++) {
        const uint16_t x = static_cast<uint16_t>(kStStageX + q * kStStageW / 4);
        g.setForeColor((q == 2) ? kGsGray50 : kGsGray25);
        for (uint16_t k = 0; k < kStRowH; k += (q == 2 ? 4 : 6)) g.drawPixel(x, static_cast<uint16_t>(y + k));
    }
    char t[24];
    snprintf(t, sizeof t, "%02u %.9s", static_cast<unsigned>(layer_ + 1), name_);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(selected_ ? kGsCyan : kGsGray75);
    gsDraw(g, t, 30, static_cast<uint16_t>(y + 9));
    uint16_t l, r;
    stBar(r_, l, r);
    const uint16_t c = selected_ ? kGsCyan : (playing_ ? kGsGray75 : kGsGray50);
    g.setForeColor(c);
    g.fillRect(l, static_cast<uint16_t>(y + 5), static_cast<uint32_t>(r - l + 1), 14);
    g.setForeColor(kGsBlack);
    g.fillRect(static_cast<uint16_t>(stCentreX(r_.pan) - 1), static_cast<uint16_t>(y + 9), 3, 6);  // the pan: a notch at the middle of the bar
    if (r_.bassHz > 0) {
        g.setForeColor(kGsYellow);
        g.fillRect(static_cast<uint16_t>(stCentreX(r_.pan) - 3), static_cast<uint16_t>(y + 20), 7, 3);
    }
    const bool mono = (r_.widthPct == 0) && !r_.stereoSample;
    const int8_t corr = mono ? static_cast<int8_t>(100) : r_.corr;
    const StCorrState st = mono ? ST_CORR_GOOD : stCorrState(corr);
    stCorrText(corr, mono, t, sizeof t);
    g.setAlignment(GS_RIGHT);
    g.setForeColor((st == ST_CORR_GOOD) ? kGsGreen : ((st == ST_CORR_WIDE) ? kGsYellow : ((st == ST_CORR_BAD) ? kStRed : kGsGray50)));
    gsDraw(g, t, kStCorrRight, static_cast<uint16_t>(y + 9));
    g.setAlignment(GS_LEFT);
    if (selected_) {
        g.setForeColor(kGsCyan);
        g.drawRect(24, y, 902, kStRowH);
    }
}

#endif
