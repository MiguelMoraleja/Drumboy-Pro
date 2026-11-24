// The chain page's pictures drawn (backlog 71): what ChainView.h computes, put on the lane area. Header-only and templated
// on the LCD like the other screens (test: 02 Software/tests, test_chainview). Texts use only the characters of the firmware's
// font; the % sign is not used.
#ifndef __CHAINSCREENS_H
#define __CHAINSCREENS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ChainView.h"
#include "GeneratorScreens.h"  // the colours, fonts, the lane area and gsDraw

const uint16_t kChvGray35 = 0x5ACB;  // a dark gray for what is switched off

// The layer's number and sample (the top of the left column).
template <class G>
inline void chvDrawHeader(G& g, uint8_t layer_, const char* sample_) {
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(24, 134, 240, 44);
    char t[16];
    snprintf(t, sizeof t, "LAYER %02u", static_cast<unsigned>(layer_ + 1));
    g.setFont(GS_FONT_10x14);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsCyan);
    gsDraw(g, t, 24, 138);
    g.setFont(GS_FONT_07x09);
    g.setForeColor(kGsGray75);
    gsDraw(g, sample_, 24, 160);
}

// The list: one row per block, the selected one lit, the blocks that are on in white.
template <class G>
inline void chvDrawList(G& g, const ChSettings& s_, uint8_t sel_) {
    g.setBackColor(kGsBlack);
    for (uint8_t k = 0; k < CH_BLOCKS; k++) {
        const uint16_t y = static_cast<uint16_t>(kChvListY + k * kChvListStep);
        const bool on = chOn(s_, static_cast<ChBlock>(k)), sel = (k == sel_);
        g.setForeColor(sel ? kGsGray25 : kGsBlack);
        g.fillRect(kChvListX, y, kChvListW, kChvListH);
        g.setForeColor(sel ? kGsCyan : kGsGray25);
        g.drawRect(kChvListX, y, kChvListW, kChvListH);
        g.setForeColor(on ? kGsGreen : kGsGray25);
        g.fillRect(static_cast<uint16_t>(kChvListX + 8), static_cast<uint16_t>(y + 9), 8, 8);
        g.setBackColor(sel ? kGsGray25 : kGsBlack);
        g.setFont(GS_FONT_07x09);
        g.setAlignment(GS_LEFT);
        g.setForeColor(on ? (sel ? kGsCyan : kGsWhite) : kGsGray50);
        gsDraw(g, chBlockName(static_cast<ChBlock>(k)), static_cast<uint16_t>(kChvListX + 24), static_cast<uint16_t>(y + 9));
        char t[24];
        chvSummary(static_cast<ChBlock>(k), s_, t, sizeof t);
        g.setForeColor(on ? kGsGray75 : kChvGray35);
        g.setAlignment(GS_RIGHT);
        gsDraw(g, t, static_cast<uint16_t>(kChvListX + kChvListW - 8), static_cast<uint16_t>(y + 9));
        g.setAlignment(GS_LEFT);
        g.setBackColor(kGsBlack);
    }
}

// One curve of a block across `width_` columns from (x_, y_) down `height_` rows, as 1 px wide steps joined by lines.
template <class G>
inline void chvDrawCurve(G& g, ChBlock b_, const ChSettings& s_, uint8_t curve_, uint16_t x_, uint16_t y_, uint16_t width_, uint16_t height_, uint16_t color_) {
    int32_t prev = -1;
    g.setForeColor(color_);
    for (uint16_t i = 0; i < width_; i++) {
        const float v = chvY01(b_, s_, static_cast<float>(i) / static_cast<float>(width_ > 1 ? width_ - 1 : 1), curve_);
        const int32_t py = static_cast<int32_t>(y_) + static_cast<int32_t>(height_ - 1) - static_cast<int32_t>(v * static_cast<float>(height_ - 1) + 0.5f);
        if ((prev >= 0) && (i > 0) && ((py - prev > 1) || (prev - py > 1))) {
            const int32_t a = (py < prev) ? py : prev, b = (py < prev) ? prev : py;
            g.fillRect(static_cast<uint16_t>(x_ + i), static_cast<uint16_t>(a), 1, static_cast<uint32_t>(b - a + 1));
        } else {
            g.fillRect(static_cast<uint16_t>(x_ + i), static_cast<uint16_t>(py), 1, 2);
        }
        prev = py;
    }
}

// The six boxes with their small pictures.
template <class G>
inline void chvDrawBoxes(G& g, const ChSettings& s_, uint8_t sel_) {
    for (uint8_t k = 0; k < CH_BLOCKS; k++) {
        const ChBlock b = static_cast<ChBlock>(k);
        const uint16_t x = chvBoxX(k);
        const bool on = chOn(s_, b), sel = (k == sel_);
        const uint16_t c = on ? (sel ? kGsCyan : kGsGray75) : kChvGray35;
        g.setBackColor(kGsBlack);
        g.setForeColor(kGsBlack);
        g.fillRect(x, kChvBoxY, kChvBoxW, kChvBoxH);
        g.setForeColor(sel ? kGsCyan : kGsGray25);
        g.drawRect(x, kChvBoxY, kChvBoxW, kChvBoxH);
        if (sel) g.drawRect(static_cast<uint16_t>(x + 1), static_cast<uint16_t>(kChvBoxY + 1), kChvBoxW - 2, kChvBoxH - 2);
        g.setFont(GS_FONT_07x09);
        g.setAlignment(GS_LEFT);
        g.setForeColor(c);
        gsDraw(g, chBlockName(b), static_cast<uint16_t>(x + 8), static_cast<uint16_t>(kChvBoxY + 8));
        g.setForeColor(on ? kGsGreen : kGsGray25);
        g.fillRect(static_cast<uint16_t>(x + kChvBoxW - 18), static_cast<uint16_t>(kChvBoxY + 8), 9, 9);
        g.setForeColor(kGsGray25);
        g.fillRect(static_cast<uint16_t>(x + kChvMiniX), static_cast<uint16_t>(kChvBoxY + kChvMiniY + kChvMiniH - 1), kChvMiniW, 1);
        for (uint8_t cv = chvCurves(b); cv > 0; cv--) {
            chvDrawCurve(g, b, s_, static_cast<uint8_t>(cv - 1), static_cast<uint16_t>(x + kChvMiniX), static_cast<uint16_t>(kChvBoxY + kChvMiniY), kChvMiniW, kChvMiniH, (cv == 1) ? c : (on ? kGsGray50 : kGsGray25));
        }
        if (k + 1 < CH_BLOCKS) {
            g.setForeColor(kGsGray50);
            g.fillRect(static_cast<uint16_t>(x + kChvBoxW), static_cast<uint16_t>(kChvBoxY + kChvBoxH / 2), kChvBoxGap, 1);
        }
    }
}

// The selected block, big: title, plot with its grid, the curve(s), the marker, the axis texts.
template <class G>
inline void chvDrawBig(G& g, ChBlock b_, const ChSettings& s_) {
    const bool on = chOn(s_, b_);
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(kChvPlotX, kChvTitleY, kChvPlotW + 4, kChvCaptionY + 10 - kChvTitleY);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(on ? kGsCyan : kGsGray50);
    gsDraw(g, chvTitle(b_), kChvPlotX, kChvTitleY);
    g.setAlignment(GS_RIGHT);
    g.setForeColor(on ? kGsGreen : kGsGray50);
    gsDraw(g, on ? "ON" : "OFF", static_cast<uint16_t>(kChvPlotX + kChvPlotW), kChvTitleY);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray25);
    g.drawRect(static_cast<uint16_t>(kChvPlotX - 1), static_cast<uint16_t>(kChvPlotY - 1), kChvPlotW + 2, kChvPlotH + 2);
    const ChvAxis axis = chvAxis(b_);
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray50);
    if (axis == CHV_FREQ) {
        static const float kHz[3] = {100.0f, 1000.0f, 10000.0f};
        static const char* const kName[3] = {"100", "1K", "10K"};
        for (uint8_t k = 0; k < 3; k++) {
            const uint16_t x = static_cast<uint16_t>(kChvPlotX + static_cast<uint32_t>(log10f(kHz[k] / 20.0f) / 3.0f * (kChvPlotW - 1)));
            g.setForeColor(kGsGray25);
            for (uint16_t y = kChvPlotY; y < kChvPlotY + kChvPlotH; y += 4) g.drawPixel(x, y);
            g.setForeColor(kGsGray50);
            gsDraw(g, kName[k], static_cast<uint16_t>(x + 3), static_cast<uint16_t>(kChvPlotY + kChvPlotH - 9));
        }
        float top, bottom;
        chvDbRange(b_, top, bottom);
        for (float db = ceilf(bottom / 12.0f) * 12.0f; db <= top; db += 12.0f) {
            const uint16_t y = static_cast<uint16_t>(kChvPlotY + (top - db) / (top - bottom) * (kChvPlotH - 1));
            g.setForeColor((db == 0.0f) ? kGsGray50 : kGsGray25);
            for (uint16_t x = kChvPlotX; x < kChvPlotX + kChvPlotW; x += 4) g.drawPixel(x, y);
        }
    } else {
        g.setForeColor(kGsGray25);
        for (uint8_t q = 1; q < 4; q++) {
            const uint16_t x = static_cast<uint16_t>(kChvPlotX + q * kChvPlotW / 4);
            for (uint16_t y = kChvPlotY; y < kChvPlotY + kChvPlotH; y += 4) g.drawPixel(x, y);
            const uint16_t yy = static_cast<uint16_t>(kChvPlotY + q * kChvPlotH / 4);
            for (uint16_t xx = kChvPlotX; xx < kChvPlotX + kChvPlotW; xx += 4) g.drawPixel(xx, yy);
        }
    }
    chvDrawCurve(g, b_, s_, 0, kChvPlotX, kChvPlotY, kChvPlotW, kChvPlotH, on ? kGsCyan : kGsGray50);
    if (chvCurves(b_) == 2) chvDrawCurve(g, b_, s_, 1, kChvPlotX, kChvPlotY, kChvPlotW, kChvPlotH, on ? kGsWhite : kGsGray25);
    const float marker = chvMarker(b_, s_);
    if (marker >= 0.0f) {
        const uint16_t x = static_cast<uint16_t>(kChvPlotX + static_cast<uint32_t>(chvClamp01(marker) * (kChvPlotW - 1)));
        g.setForeColor(kGsYellow);
        for (uint16_t y = kChvPlotY; y < kChvPlotY + kChvPlotH; y += 3) g.drawPixel(x, y);
    }
    // The axis texts under the plot.
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray50);
    char t[96];
    switch (axis) {
        case CHV_TIME:
            snprintf(t, sizeof t, "TIME FROM 0 TO %.0f MS ACROSS, THE LEVEL OF THE NOTE UP", static_cast<double>(chvEnvSpanMs(s_)));
            break;
        case CHV_SIGNAL: snprintf(t, sizeof t, "LEVEL IN FROM -1 TO +1 ACROSS, LEVEL OUT UP"); break;
        case CHV_LEVEL_IN: snprintf(t, sizeof t, "LEVEL IN FROM -60 TO 0 DB ACROSS, LEVEL OUT UP, THRESHOLD MARKED"); break;
        default: snprintf(t, sizeof t, (b_ == CH_IMAGE) ? "SIDE IN CYAN AND MID IN WHITE, DB, BASS MONO CORNER MARKED" : "RESPONSE IN DB, THE MAIN FREQUENCY MARKED"); break;
    }
    gsDraw(g, t, kChvPlotX, kChvCaptionY);
}

#endif
