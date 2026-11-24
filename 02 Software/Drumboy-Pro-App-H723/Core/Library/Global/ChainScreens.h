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

// The values panel under the list: every parameter of the selected block, its name on the left and its value with the unit on the right
// (the numbers the eight fields show, here next to the pictures). Gray while the block is off.
template <class G>
inline void chvDrawValues(G& g, ChBlock b_, const ChSettings& s_) {
    const bool on = chOn(s_, b_);
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(kChvListX, kChvValuesY - 4, kChvListW, kChvValuesStep * 6 + 4);
    g.setFont(GS_FONT_07x09);
    for (uint8_t i = 0; i < chParamCount(b_); i++) {
        const uint16_t y = static_cast<uint16_t>(kChvValuesY + i * kChvValuesStep);
        char name[16], value[32];
        snprintf(name, sizeof name, "%.10s", chParam(b_, i).name);
        for (int k = static_cast<int>(strlen(name)) - 1; (k >= 0) && (name[k] == ' '); k--) name[k] = '\0';
        chvValue(b_, i, s_, value, sizeof value);
        g.setAlignment(GS_LEFT);
        g.setForeColor(on ? kGsGray75 : kChvGray35);
        gsDraw(g, name, static_cast<uint16_t>(kChvListX + 4), y);
        g.setAlignment(GS_RIGHT);
        g.setForeColor(on ? kGsWhite : kGsGray50);
        gsDraw(g, value, static_cast<uint16_t>(kChvListX + kChvListW - 4), y);
    }
    g.setAlignment(GS_LEFT);
}

// A label on the plot at a place along its x axis (0..1): to the right of that place, or to its left where it would run out of the plot.
// Its background is black, so it stays readable over the curve.
template <class G>
inline void chvPlotLabel(G& g, const char* text_, float x01_, uint16_t y_, uint16_t color_) {
    const uint16_t w = static_cast<uint16_t>(strlen(text_) * 7u);  // the 5x7 font: 5 and 2 of spacing
    const uint16_t x = static_cast<uint16_t>(kChvPlotX + static_cast<uint32_t>(chvClamp01(x01_) * (kChvPlotW - 1)));
    g.setFont(GS_FONT_05x07);
    g.setBackColor(kGsBlack);
    g.setForeColor(color_);
    if (x + 4 + w <= kChvPlotX + kChvPlotW) {
        g.setAlignment(GS_LEFT);
        gsDraw(g, text_, static_cast<uint16_t>(x + 4), y_);
    } else {
        const uint16_t right = (x >= kChvPlotX + w + 3) ? static_cast<uint16_t>(x - 3) : static_cast<uint16_t>(kChvPlotX + w);
        g.setAlignment(GS_RIGHT);
        gsDraw(g, text_, right, y_);
    }
    g.setAlignment(GS_LEFT);
}

// A dotted vertical line across the plot at a place along its x axis (0..1).
template <class G>
inline void chvPlotLine(G& g, float x01_, uint16_t color_) {
    const uint16_t x = static_cast<uint16_t>(kChvPlotX + static_cast<uint32_t>(chvClamp01(x01_) * (kChvPlotW - 1)));
    g.setForeColor(color_);
    for (uint16_t y = kChvPlotY; y < kChvPlotY + kChvPlotH; y += 3) g.drawPixel(x, y);
}

// The values written on the plot, where they belong: the cutoff and the band on their frequency, the threshold on its level, the bass corner,
// the envelope's attack and decay times at their moments. (The panel under the list has all of them.)
template <class G>
inline void chvDrawPlotValues(G& g, ChBlock b_, const ChSettings& s_, bool on_) {
    char t[96], v[32], w[32];
    const uint16_t top = static_cast<uint16_t>(kChvPlotY + 4), second = static_cast<uint16_t>(kChvPlotY + 14);
    const uint16_t c = on_ ? kGsWhite : kGsGray50;
    switch (b_) {
        case CH_ENV: {
            const float span = chvEnvSpanMs(s_), atk = chAttackMs(s_.envAttack), dec = chDecayMs(s_.envDecay);
            const float xa = (atk > 0.0f) ? chvClamp01(atk / span) : 0.0f, xd = (dec > 0.0f) ? chvClamp01(dec / span) : 0.8f;
            if (atk > 0.0f) chvPlotLine(g, xa, kGsYellow);
            if (dec > 0.0f) chvPlotLine(g, xd, kGsGray75);
            chvValue(CH_ENV, 0, s_, v, sizeof v);
            snprintf(t, sizeof t, "ATTACK %s", v);
            chvPlotLabel(g, t, xa, static_cast<uint16_t>(kChvPlotY + kChvPlotH - 34), kGsYellow);
            chvValue(CH_ENV, 1, s_, v, sizeof v);
            snprintf(t, sizeof t, "DECAY %s", v);
            chvPlotLabel(g, t, xd, static_cast<uint16_t>(kChvPlotY + kChvPlotH - 22), c);
            break;
        }
        case CH_DRIVE: {
            const float gain = 20.0f * log10f(chDriveGain(s_.driveAmount));
            chvValue(CH_DRIVE, 0, s_, v, sizeof v);
            snprintf(t, sizeof t, "AMOUNT %s  GAIN IN %+.1f DB", v, static_cast<double>(gain));
            chvPlotLabel(g, t, 1.0f, static_cast<uint16_t>(kChvPlotY + kChvPlotH - 12), c);
            break;
        }
        case CH_FILTER: {
            chvValue(CH_FILTER, 1, s_, v, sizeof v);
            snprintf(t, sizeof t, "CUTOFF %s", v);
            chvPlotLabel(g, t, chvMarker(b_, s_), top, kGsYellow);
            if (s_.filterHp > 0) {
                chvValue(CH_FILTER, 4, s_, v, sizeof v);
                snprintf(t, sizeof t, "HIGH PASS %s", v);
                const float xh = log10f(chHz(s_.filterHp) / 20.0f) / 3.0f;
                chvPlotLine(g, xh, kGsGray50);
                chvPlotLabel(g, t, xh, second, c);
            }
            break;
        }
        case CH_EQ: {
            chvValue(CH_EQ, 1, s_, v, sizeof v);
            chvValue(CH_EQ, 2, s_, w, sizeof w);
            snprintf(t, sizeof t, "FREQUENCY %s", v);
            chvPlotLabel(g, t, chvMarker(b_, s_), top, kGsYellow);
            snprintf(t, sizeof t, "GAIN %s", w);
            chvPlotLabel(g, t, chvMarker(b_, s_), second, c);
            break;
        }
        case CH_COMP: {
            chvValue(CH_COMP, 0, s_, v, sizeof v);
            snprintf(t, sizeof t, "THRESHOLD %s", v);
            chvPlotLabel(g, t, chvMarker(b_, s_), top, kGsYellow);
            chvValue(CH_COMP, 1, s_, v, sizeof v);
            snprintf(t, sizeof t, "RATIO %s", v);
            chvPlotLabel(g, t, chvMarker(b_, s_), second, c);
            break;
        }
        default: {  // IMAGE
            chvValue(CH_IMAGE, 0, s_, v, sizeof v);
            chvValue(CH_IMAGE, 1, s_, w, sizeof w);
            snprintf(t, sizeof t, "WIDTH %s", v);
            chvPlotLabel(g, t, 1.0f, top, c);
            if (s_.imgBass > 0) {
                snprintf(t, sizeof t, "BASS MONO BELOW %s", w);
                chvPlotLabel(g, t, chvMarker(b_, s_), second, kGsYellow);
            } else {
                chvPlotLabel(g, "BASS MONO OFF", 1.0f, second, c);
            }
            break;
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
    chvDrawPlotValues(g, b_, s_, on);
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
