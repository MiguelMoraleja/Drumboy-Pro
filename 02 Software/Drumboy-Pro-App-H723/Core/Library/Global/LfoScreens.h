// The LFO overview drawn (backlog 3, 35, 45): what LfoView.h computes, put on the lane area. Header-only and
// templated on the LCD like the other screens: the firmware draws with its Lcd (through the adapter of
// Controller.cpp) and a PC test draws the same code on a pixel buffer, checks it stays inside the lane area and
// writes the pictures (02 Software/tests, test_lfoscreens in test_lfoview).
//
// Texts use only the characters of the firmware's font. The % sign is not drawn in the 7x9 font.
#ifndef __LFOSCREENS_H
#define __LFOSCREENS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GeneratorScreens.h"  // the colours, fonts, the lane area and gsDraw
#include "LfoView.h"

struct LvInfo {
    uint8_t number;       // 1..10
    bool active;
    char type[12];        // "SINE", "TRIANGLE" ...
    char rate[6];         // "1/4"
    char phase[6];        // "000"
    uint8_t depth;        // 0..100
    bool invert;
    uint16_t uses;        // how many things it moves
};

// One row of the left list: the number, the small wave, and how many things the LFO moves.
template <class G>
inline void lvDrawRow(G& g, uint8_t lfo_, bool selected_, bool active_, uint16_t uses_, const float* table_, uint16_t tableSize_, bool invert_, uint8_t depth_) {
    const uint16_t y = lvRowY(lfo_);
    const uint16_t c = active_ ? (selected_ ? kGsCyan : kGsGray75) : kGsGray25;
    g.setBackColor(kGsBlack);
    g.setForeColor(selected_ ? kGsGray25 : kGsBlack);
    g.fillRect(kLvRowX, y, kLvRowW, kLvRowH);
    g.setForeColor(selected_ ? kGsCyan : kGsGray25);
    g.drawRect(kLvRowX, y, kLvRowW, kLvRowH);
    char num[4];
    snprintf(num, sizeof num, "%02u", static_cast<unsigned>(lfo_ + 1));
    g.setAlignment(GS_LEFT);
    g.setFont(GS_FONT_10x14);
    g.setForeColor(active_ ? (selected_ ? kGsCyan : kGsWhite) : kGsGray50);
    gsDraw(g, num, static_cast<uint16_t>(kLvRowX + 8), static_cast<uint16_t>(y + 7));
    g.setForeColor(c);
    const int16_t amp = 8;
    for (uint16_t x = 0; x < kLvWaveW; x++) {
        const int16_t h = active_ ? lvWaveHeight(lvTableAt(table_, tableSize_, static_cast<float>(x) / static_cast<float>(kLvWaveW)), invert_, depth_ / 100.0f, amp) : 0;
        g.drawPixel(static_cast<uint16_t>(kLvWaveX + x), static_cast<uint16_t>(y + 14 - h));
    }
    char text[8];
    if (uses_ > 0) snprintf(text, sizeof text, "X%u", static_cast<unsigned>(uses_));
    else snprintf(text, sizeof text, "--");
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_RIGHT);
    g.setForeColor((active_ && uses_ > 0) ? kGsYellow : kGsGray50);
    gsDraw(g, text, static_cast<uint16_t>(kLvRowX + kLvRowW - 8), static_cast<uint16_t>(y + 10));
    g.setAlignment(GS_LEFT);
}

// The line above the plot.
template <class G>
inline void lvDrawTitle(G& g, const LvInfo& in_) {
    char text[80];
    if (in_.active) snprintf(text, sizeof text, "LFO %02u   %s   RATE %s   PHASE %s   DEPTH %u   INVERT %s", static_cast<unsigned>(in_.number), in_.type, in_.rate, in_.phase, static_cast<unsigned>(in_.depth), in_.invert ? "ON" : "OFF");
    else snprintf(text, sizeof text, "LFO %02u   OFF", static_cast<unsigned>(in_.number));
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(kLvTitleX, kLvTitleY, 650, 10);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(in_.active ? kGsCyan : kGsGray50);
    gsDraw(g, text, kLvTitleX, kLvTitleY);
}

// Columns x0_..x1_ (inclusive, 0..kLvPlotW-1) of the plot: the background, the grid, the middle line and the wave.
template <class G>
inline void lvDrawPlotColumns(G& g, const float* table_, uint16_t tableSize_, bool active_, bool invert_, uint8_t depth_, uint16_t x0_, uint16_t x1_) {
    const uint16_t mid = static_cast<uint16_t>(kLvPlotY + kLvPlotH / 2);
    const int16_t amp = static_cast<int16_t>(kLvPlotH / 2 - 6);
    if (x1_ >= kLvPlotW) x1_ = static_cast<uint16_t>(kLvPlotW - 1);
    for (uint16_t x = x0_; x <= x1_; x++) {
        const uint16_t sx = static_cast<uint16_t>(kLvPlotX + x);
        g.setForeColor(kGsBlack);
        g.fillRect(sx, kLvPlotY, 1, kLvPlotH);
        g.setForeColor(kGsGray25);
        if ((x % (kLvPlotW / 4)) == 0) {
            for (uint16_t y = kLvPlotY; y < static_cast<uint16_t>(kLvPlotY + kLvPlotH); y += 4) g.drawPixel(sx, y);
        }
        g.setForeColor(kGsGray50);
        if ((x % 4) < 2) g.drawPixel(sx, mid);
        const float frac = static_cast<float>((static_cast<uint32_t>(x) * kLvPlotCycles) % kLvPlotW) / static_cast<float>(kLvPlotW);
        const int16_t h = active_ ? lvWaveHeight(lvTableAt(table_, tableSize_, frac), invert_, depth_ / 100.0f, amp) : 0;
        g.setForeColor(active_ ? kGsCyan : kGsGray25);
        g.fillRect(sx, static_cast<uint16_t>(mid - h), 1, 2);
    }
}

template <class G>
inline void lvDrawPlotFrame(G& g) {
    g.setForeColor(kGsGray25);
    g.drawRect(static_cast<uint16_t>(kLvPlotX - 1), static_cast<uint16_t>(kLvPlotY - 1), kLvPlotW + 2, kLvPlotH + 2);
    g.setBackColor(kGsBlack);
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray50);
    gsDraw(g, "TWO CYCLES", kLvPlotX, kLvCaptionY);
    g.setAlignment(GS_RIGHT);
    gsDraw(g, "THE DOT FOLLOWS THE SONG", static_cast<uint16_t>(kLvPlotX + kLvPlotW), kLvCaptionY);
    g.setAlignment(GS_LEFT);
}

// The phase dot with its cursor line at a plot column, over the wave (the columns around it are redrawn first).
template <class G>
inline void lvDrawDot(G& g, const float* table_, uint16_t tableSize_, bool invert_, uint8_t depth_, uint16_t column_) {
    const uint16_t mid = static_cast<uint16_t>(kLvPlotY + kLvPlotH / 2);
    const int16_t amp = static_cast<int16_t>(kLvPlotH / 2 - 6);
    const float frac = static_cast<float>((static_cast<uint32_t>(column_) * kLvPlotCycles) % kLvPlotW) / static_cast<float>(kLvPlotW);
    const int16_t h = lvWaveHeight(lvTableAt(table_, tableSize_, frac), invert_, depth_ / 100.0f, amp);
    const uint16_t sx = static_cast<uint16_t>(kLvPlotX + column_);
    g.setForeColor(kGsGray50);
    g.fillRect(sx, kLvPlotY, 1, kLvPlotH);
    g.setForeColor(kGsYellow);
    g.fillRect(static_cast<uint16_t>(sx - 3), static_cast<uint16_t>(mid - h - 2), 7, 7);
}

// The USED BY list: up to ten boxes in two columns, then "+ N MORE" when there are more.
template <class G>
inline void lvDrawUsed(G& g, const LvUse* uses_, const char (*layerNames_)[16], uint16_t total_, bool active_) {
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(kLvUsedX, kLvUsedLabelY, 652, 128);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray75);
    gsDraw(g, "USED BY", kLvUsedX, kLvUsedLabelY);
    if (total_ == 0) {
        g.setForeColor(kGsGray50);
        gsDraw(g, active_ ? "NOTHING FOLLOWS THIS LFO YET: SHIFT + TURN A FIELD" : "THIS LFO IS OFF", kLvUsedX, kLvUsedY);
        return;
    }
    const uint16_t shown = total_ < kLvShownUses ? total_ : kLvShownUses;
    for (uint16_t k = 0; k < shown; k++) {
        const uint16_t col = k % 2, row = k / 2;
        const uint16_t x = static_cast<uint16_t>(kLvUsedX + col * kLvUsedColStep), y = static_cast<uint16_t>(kLvUsedY + row * kLvUsedStep);
        g.setForeColor(kGsGray25);
        g.drawRect(x, y, kLvUsedW, kLvUsedH);
        char who[24];
        lvUseWho(uses_[k], (uses_[k].kind == 0) ? layerNames_[uses_[k].who] : "", who, sizeof who);
        g.setFont(GS_FONT_07x09);
        g.setAlignment(GS_LEFT);
        g.setForeColor(kGsWhite);
        gsDraw(g, who, static_cast<uint16_t>(x + 8), static_cast<uint16_t>(y + 6));
        g.setFont(GS_FONT_05x07);
        g.setAlignment(GS_RIGHT);
        g.setForeColor(kGsGray75);
        gsDraw(g, lvUseWhat(uses_[k]), static_cast<uint16_t>(x + kLvUsedW - 8), static_cast<uint16_t>(y + 7));
        g.setAlignment(GS_LEFT);
    }
    if (total_ > shown) {
        char more[24];
        snprintf(more, sizeof more, "+ %u MORE", static_cast<unsigned>(total_ - shown));
        g.setFont(GS_FONT_05x07);
        g.setForeColor(kGsGray50);
        gsDraw(g, more, kLvUsedX, static_cast<uint16_t>(kLvUsedY + 5 * kLvUsedStep));
    }
}

#endif
