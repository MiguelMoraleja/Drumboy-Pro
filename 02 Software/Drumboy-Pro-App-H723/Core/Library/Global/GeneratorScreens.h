// The generator screens drawn (backlog 66): what GeneratorView.h computes, put on the lane area.
// Header-only and templated on the LCD, so the firmware draws with its Lcd (through a thin adapter)
// and a PC test draws the same code on a pixel buffer, checks it stays inside the lane area and
// writes the pictures (02 Software/tests, test_generatorscreens).
//
// Needs from the LCD type G: setFont(GsFont), setAlignment(GsAlign), setForeColor, setBackColor,
// drawPixel, drawVLine, drawLine, drawRect, fillRect, clearRect, drawText (as the firmware's Lcd).
// The font has no '=', '(' or ')': texts use only 0-9, A-Z (case does not matter) and ! # % + , - . / : < > ? [ ] _ |
#ifndef __GENERATORSCREENS_H
#define __GENERATORSCREENS_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GeneratorView.h"

enum GsFont : uint8_t { GS_FONT_05x07, GS_FONT_07x09, GS_FONT_10x14 };
enum GsAlign : uint8_t { GS_LEFT, GS_CENTER, GS_RIGHT };

// RGB565, the values of Global.h (checked there).
const uint16_t kGsBlack = 0x0000, kGsWhite = 0xFFFF, kGsYellow = 0xFFE0, kGsGreen = 0x07E0, kGsCyan = 0x07FF, kGsMagenta = 0xF81F;
const uint16_t kGsGray25 = 0x4208, kGsGray50 = 0x8410, kGsGray75 = 0xBDF7;
const uint16_t kGsCyanDim = 0x02CB, kGsMagentaDim = 0x580B;  // about 35 % of cyan and magenta: the peaks around the body of a waveform

// The lane area the views replace (Global.h: kLayerLineX, kImageLayerY[0], kLayerLineWidth, 309).
const uint16_t kGsAreaX = 20, kGsAreaY = 131, kGsAreaW = 920, kGsAreaH = 309;
const uint16_t kGsRightEdge = 900;  // right-aligned texts end here (as the sample view's)

const uint16_t kGsStartColor = kGsMagenta, kGsMidColor = kGsWhite, kGsEndColor = kGsCyan;  // the three moments of a note

// Text with its length taken from the string (a hand-counted length once drew a NUL).
template <class G>
inline void gsDraw(G& g, const char* text_, uint16_t x_, uint16_t y_) {
    g.drawText(text_, static_cast<uint16_t>(strlen(text_)), x_, y_);
}

/* Shared: the spectrum panel ---------------------------------------------------------*/

// The frame: 20 Hz to 20 kHz across (grid at 100 Hz, 1 kHz, 10 kHz), 0 dB at the top to -48 dB at the
// bottom (grid every 12 dB).
template <class G>
inline void gsSpectrumFrame(G& g, uint16_t x_, uint16_t top_, uint16_t w_, uint16_t h_, uint16_t scaleY_) {
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsGray25);
    g.drawRect(static_cast<uint16_t>(x_ - 1), static_cast<uint16_t>(top_ - 1), static_cast<uint32_t>(w_ + 2), static_cast<uint32_t>(h_ + 2));
    for (uint8_t k = 1; k < 4; k++) {
        const uint16_t y = gvYOfDb(-12.0f * static_cast<float>(k), top_, h_);
        for (uint16_t x = x_; x < static_cast<uint16_t>(x_ + w_); x += 4) g.drawPixel(x, y);
    }
    static const float kGridHz[3] = {100.0f, 1000.0f, 10000.0f};
    static const char* const kGridName[3] = {"100", "1K", "10K"};
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    for (uint8_t k = 0; k < 3; k++) {
        const uint16_t x = gvXOfHz(kGridHz[k], x_, w_);
        g.setForeColor(kGsGray25);
        for (uint16_t y = top_; y < static_cast<uint16_t>(top_ + h_); y += 4) g.drawPixel(x, y);
        g.setForeColor(kGsGray75);
        const uint16_t len = static_cast<uint16_t>(strlen(kGridName[k]));
        g.drawText(kGridName[k], len, static_cast<uint16_t>(x - (len * 7) / 2), scaleY_);
    }
    g.setForeColor(kGsGray75);
    gsDraw(g, "20", x_, scaleY_);  // the right end is 20 kHz: it has no label (10K stands next to it)
    static const char* const kDbName[4] = {"0", "-12", "-24", "-36"};
    for (uint8_t k = 0; k < 4; k++) g.drawText(kDbName[k], static_cast<uint16_t>(strlen(kDbName[k])), static_cast<uint16_t>(x_ + 2), static_cast<uint16_t>(gvYOfDb(-12.0f * static_cast<float>(k), top_, h_) + 2));
}

// One curve (dB per column) relative to reference_ (the loudest point of the panel); points more than
// 48 dB down are not drawn.
template <class G>
inline void gsSpectrumCurve(G& g, const float* db_, uint16_t cols_, uint16_t x_, uint16_t top_, uint16_t h_, float reference_, uint16_t color_) {
    g.setForeColor(color_);
    bool havePrevious = false;
    uint16_t px = 0, py = 0;
    for (uint16_t c = 0; c < cols_; c++) {
        const float rel = db_[c] - reference_;
        if (!(rel > -kGvSpecRangeDb)) {  // also NaN
            havePrevious = false;
            continue;
        }
        const uint16_t x = static_cast<uint16_t>(x_ + c), y = gvYOfDb(rel, top_, h_);
        if (havePrevious) {
            g.drawLine(px, py, x, y);
        } else {
            g.drawPixel(x, y);
        }
        px = x;
        py = y;
        havePrevious = true;
    }
}

/* The GEN view -----------------------------------------------------------------------*/

struct GsGen {
    char layerNum[4];       // "03"
    uint8_t type;           // GenType
    float ratio;            // the PITCH factor the note plays with (1 when STYLE is off)
    float octaves;          // SWEEP in octaves (0 when STYLE is off)
    uint8_t sweepPercent;   // the SWEEP setting, 0..100, for the text
    uint8_t shape;          // DIRECTION: 0 up, 1 down, 2 up-down, 3 down-up
    uint8_t length16;       // LENGTH in 16ths
    uint32_t length;        // LENGTH in samples at the tempo
    bool plain;             // STYLE off: the note plays plain
};

// Draws the whole GEN view. peaks_ and rms_ = genViewPeaks for this note (kGvNoteCols columns).
template <class G>
inline void gsDrawGen(G& g, const GsGen& in_, const uint8_t* peaks_, const uint8_t* rms_) {
    g.setBackColor(kGsBlack);
    g.clearRect(kGsAreaX, kGsAreaY, kGsAreaW, kGsAreaH);
    const uint8_t type = (in_.type < kGenTypeCount) ? in_.type : static_cast<uint8_t>(GEN_OFF);
    char line[80];

    // 1. TITLE AND SUMMARY
    snprintf(line, sizeof line, "LAYER %.2s  GEN %s", in_.layerNum, kGenTypeName[type]);
    g.setFont(GS_FONT_10x14);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsCyan);
    g.drawText(line, static_cast<uint16_t>(strlen(line)), kGvTitleX, kGvTitleY);
    const unsigned ms = static_cast<unsigned>((static_cast<uint64_t>(in_.length) * 1000u) / 44100u);
    const int len = in_.plain ? snprintf(line, sizeof line, "%u/16  %u MS  PLAIN: STYLE OFF", static_cast<unsigned>(in_.length16), ms)
                              : snprintf(line, sizeof line, "%u/16  %u MS  %s", static_cast<unsigned>(in_.length16), ms, genViewShapeName(in_.shape));
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_RIGHT);
    g.setForeColor(kGsWhite);
    g.drawText(line, static_cast<uint16_t>(len > 0 ? len : 0), kGsRightEdge, kGvTitleY + 3);

    // 2. NOTE: the waveform, mirrored about its centre line, and a tick for every 16th
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray75);
    gsDraw(g, "NOTE", kGvNoteX, kGvNoteLabelY);
    g.setForeColor(kGsGray25);
    g.drawRect(static_cast<uint16_t>(kGvNoteX - 1), static_cast<uint16_t>(kGvNoteCenterY - kGvNoteHalf - 1), static_cast<uint32_t>(kGvNoteCols + 2), static_cast<uint32_t>(2 * kGvNoteHalf + 3));
    for (uint16_t x = kGvNoteX; x < kGvNoteX + kGvNoteCols; x += 4) g.drawPixel(x, kGvNoteCenterY);
    // The peaks dim, the RMS body bright inside them.
    g.setForeColor(kGsCyanDim);
    for (uint16_t c = 0; c < kGvNoteCols; c++) {
        const uint8_t h = peaks_[c];
        if (h > 0) g.drawVLine(static_cast<uint16_t>(kGvNoteX + c), static_cast<uint16_t>(kGvNoteCenterY - h), static_cast<int16_t>(2 * h + 1));
    }
    g.setForeColor(kGsCyan);
    for (uint16_t c = 0; c < kGvNoteCols; c++) {
        const uint16_t x = static_cast<uint16_t>(kGvNoteX + c);
        const uint8_t h = rms_[c];
        if (h > 0) {
            g.drawVLine(x, static_cast<uint16_t>(kGvNoteCenterY - h), static_cast<int16_t>(2 * h + 1));
        } else {
            g.drawPixel(x, kGvNoteCenterY);
        }
    }
    const uint8_t n16 = (in_.length16 > 0) ? in_.length16 : static_cast<uint8_t>(1);
    for (uint8_t i = 0; i <= n16; i++) {
        const uint16_t x = static_cast<uint16_t>(kGvNoteX + (static_cast<uint32_t>(i) * (kGvNoteCols - 1)) / n16);
        const bool beat = ((i % 4) == 0);
        g.setForeColor(beat ? kGsGray75 : kGsGray50);
        g.drawVLine(x, kGvNoteTickY, beat ? 5 : 3);
    }
    g.setForeColor(kGsGray75);
    gsDraw(g, "0", kGvNoteX, kGvNoteTickY + 8);
    char text[24];
    snprintf(text, sizeof text, "%u/16", static_cast<unsigned>(n16));
    g.setAlignment(GS_RIGHT);
    g.drawText(text, static_cast<uint16_t>(strlen(text)), kGvNoteX + kGvNoteCols, kGvNoteTickY + 8);
    g.setAlignment(GS_LEFT);

    // 3. SWEEP: the frequency's path over the note, +-2 octaves around what PITCH sets
    g.setForeColor(kGsGray75);
    gsDraw(g, "SWEEP", kGvSweepX, kGvSweepLabelY);
    g.setForeColor(kGsGray25);
    g.drawRect(static_cast<uint16_t>(kGvSweepX - 1), static_cast<uint16_t>(kGvSweepTop - 1), static_cast<uint32_t>(kGvSweepW + 2), static_cast<uint32_t>(kGvSweepH + 2));
    const uint16_t halfH = static_cast<uint16_t>(kGvSweepH / 2), centerY = static_cast<uint16_t>(kGvSweepTop + halfH);
    for (int8_t oct = -1; oct <= 1; oct++) {
        const uint16_t y = static_cast<uint16_t>(centerY - static_cast<int16_t>((static_cast<float>(oct) / kGvSweepRangeOct) * static_cast<float>(halfH - 1)));
        g.setForeColor(kGsGray25);
        for (uint16_t x = kGvSweepX; x < kGvSweepX + kGvSweepW; x += 4) g.drawPixel(x, y);
        g.setForeColor(kGsGray75);
        g.setAlignment(GS_RIGHT);
        const char* name = (oct < 0) ? "-1 OCT" : ((oct == 0) ? genViewFreqName(type) : "+1 OCT");
        g.drawText(name, static_cast<uint16_t>(strlen(name)), kGvSweepX + kGvSweepW - 3, static_cast<uint16_t>(y - 8));
    }
    g.setAlignment(GS_LEFT);
    auto sweepY = [&](float p) {
        const float o = genViewSweepOctave(in_.shape, in_.octaves, p);
        return static_cast<uint16_t>(centerY - static_cast<int16_t>(lroundf((o / kGvSweepRangeOct) * static_cast<float>(halfH - 1))));
    };
    g.setForeColor(kGsYellow);
    uint16_t px = kGvSweepX, py = sweepY(0.0f);
    for (uint16_t c = 1; c < kGvSweepW; c++) {
        const uint16_t x = static_cast<uint16_t>(kGvSweepX + c), y = sweepY(static_cast<float>(c) / static_cast<float>(kGvSweepW - 1));
        g.drawLine(px, py, x, y);
        px = x;
        py = y;
    }
    const float moments[3] = {0.0f, 0.5f, 1.0f};
    const uint16_t momentColor[3] = {kGsStartColor, kGsMidColor, kGsEndColor};
    for (uint8_t k = 0; k < 3; k++) {
        const uint16_t x = static_cast<uint16_t>(kGvSweepX + static_cast<uint16_t>(moments[k] * static_cast<float>(kGvSweepW - 1)));
        const uint16_t y = sweepY(moments[k]);
        g.setForeColor(momentColor[k]);
        g.fillRect(static_cast<uint16_t>((k == 0) ? x : ((k == 2) ? x - 4 : x - 2)), static_cast<uint16_t>(y - 2), 5, 5);
    }
    char f0[16], f1[16], f2[16];
    genViewFormatFreq(f0, sizeof f0, type, genViewFreq(type, in_.ratio, exp2f(genViewSweepOctave(in_.shape, in_.octaves, 0.0f))));
    genViewFormatFreq(f1, sizeof f1, type, genViewFreq(type, in_.ratio, exp2f(genViewSweepOctave(in_.shape, in_.octaves, 0.5f))));
    genViewFormatFreq(f2, sizeof f2, type, genViewFreq(type, in_.ratio, exp2f(genViewSweepOctave(in_.shape, in_.octaves, 1.0f))));
    if (in_.octaves > 0.0f) {
        snprintf(line, sizeof line, "%s %s > %s > %s   %u.%02u OCT", genViewFreqName(type), f0, f1, f2, static_cast<unsigned>(in_.sweepPercent * 4u / 100u), static_cast<unsigned>((in_.sweepPercent * 4u) % 100u));
    } else {
        snprintf(line, sizeof line, "%s %s   NO SWEEP", genViewFreqName(type), f0);
    }
    g.setFont(GS_FONT_07x09);
    g.setForeColor(kGsWhite);
    g.drawText(line, static_cast<uint16_t>(strlen(line)), kGvSweepX, kGvSweepTextY);

    // 4. SPECTRUM: the noises as curves (start, middle, end of the note), SIREN and LASER as lines
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray75);
    gsDraw(g, "SPECTRUM", kGvSpecX, kGvSpecLabelY);
    gsSpectrumFrame(g, kGvSpecX, kGvSpecTop, kGvSpecW, kGvSpecH, kGvSpecScaleY);
    const bool swept = (in_.octaves > 0.0f);
    const uint8_t order[3] = {0, 2, 1};  // start and end first, the middle on top
    float mul[3];
    for (uint8_t k = 0; k < 3; k++) mul[k] = exp2f(genViewSweepOctave(in_.shape, in_.octaves, moments[k]));
    float lineHz[3][24], lineDb[3][24];
    uint8_t lineCount[3] = {0, 0, 0};
    float reference = -1000.0f;
    float curve[kGvSpecW];  // one curve at a time (the firmware's RAM has no room for three): the reference is found first
    for (uint8_t k = 0; k < 3; k++) {
        if (!swept && (k != 1)) continue;
        lineCount[k] = genViewLines(type, in_.ratio, mul[k], lineHz[k], lineDb[k], 24);
        if (lineCount[k] > 0) {
            for (uint8_t n = 0; n < lineCount[k]; n++) reference = fmaxf(reference, lineDb[k][n]);
        } else {
            for (uint16_t c = 0; c < kGvSpecW; c++) reference = fmaxf(reference, genViewSpectrumDb(type, in_.ratio, mul[k], gvHzAtColumn(c, kGvSpecW)));
        }
    }
    for (uint8_t o = 0; o < 3; o++) {
        const uint8_t k = order[o];
        if (!swept && (k != 1)) continue;
        const uint16_t color = swept ? momentColor[k] : kGsMidColor;
        if (lineCount[k] > 0) {
            g.setForeColor(color);
            for (uint8_t n = 0; n < lineCount[k]; n++) {
                const uint16_t x = gvXOfHz(lineHz[k][n], kGvSpecX, kGvSpecW), y = gvYOfDb(lineDb[k][n] - reference, kGvSpecTop, kGvSpecH);
                g.drawVLine(x, y, static_cast<int16_t>(kGvSpecTop + kGvSpecH - y));
            }
        } else {
            for (uint16_t c = 0; c < kGvSpecW; c++) curve[c] = genViewSpectrumDb(type, in_.ratio, mul[k], gvHzAtColumn(c, kGvSpecW));
            gsSpectrumCurve(g, curve, kGvSpecW, kGvSpecX, kGvSpecTop, kGvSpecH, reference, color);
        }
    }
    if (type == GEN_SIREN) {
        // The warble: the pitch moves over this range, the lines above move with it.
        float lo, hi;
        genViewWarbleRange(in_.ratio, lo, hi);
        g.setForeColor(kGsYellow);
        const uint16_t xl = gvXOfHz(lo, kGvSpecX, kGvSpecW), xh = gvXOfHz(hi, kGvSpecX, kGvSpecW);
        g.fillRect(xl, static_cast<uint16_t>(kGvSpecTop + kGvSpecH - 3), static_cast<uint32_t>(xh - xl + 1), 3);
    }
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    if (swept) {
        g.setForeColor(kGsStartColor);
        gsDraw(g, "START", kGvSpecX, kGvSpecTextY);
        g.setForeColor(kGsMidColor);
        gsDraw(g, "MIDDLE", kGvSpecX + 60, kGvSpecTextY);
        g.setForeColor(kGsEndColor);
        gsDraw(g, "END", kGvSpecX + 130, kGvSpecTextY);
    } else if (type == GEN_SIREN) {
        g.setForeColor(kGsYellow);
        gsDraw(g, "WARBLE RANGE", kGvSpecX, kGvSpecTextY);
    }
}

/* The HISS page ------------------------------------------------------------------------*/

struct GsHiss {
    uint8_t type, level, tone, duck;  // HissType, then LEVEL, TONE, DUCK 0..100
};

// What the live part has drawn (so only changes are drawn): the duck bar in pixels, the two level bars.
struct GsHissLive {
    uint16_t gainDrawn = 0xFFFF;
    uint8_t meterDrawn[2] = {0xFF, 0xFF};
    bool bedOnDrawn = true;
};

const uint16_t kHvMeterX[2] = {590, 604}, kHvMeterW = 10, kHvMeterTop = 172, kHvMeterH = 170;  // the bed's level bars between texture and spectrum

// The live part, 25 times a second: the bed's left and right level bars (peaks of the last block) and the
// duck bar (the bed's gain: full = not ducked). bedOn_: the bed makes sound or is fading out.
template <class G>
inline void gsDrawHissLive(G& g, const GsHiss& in_, GsHissLive& st_, bool bedOn_, float gain_, float peakL_, float peakR_) {
    const float peaks[2] = {bedOn_ ? peakL_ : 0.0f, bedOn_ ? peakR_ : 0.0f};
    const uint16_t colors[2] = {kGsCyan, kGsMagenta};
    for (uint8_t ch = 0; ch < 2; ch++) {
        float f = (gvDb(peaks[ch]) + kHvTextureFloorDb) / kHvTextureFloorDb;
        if (!(f > 0.0f)) f = 0.0f;
        if (f > 1.0f) f = 1.0f;
        const uint8_t h = static_cast<uint8_t>(f * static_cast<float>(kHvMeterH) + 0.5f);
        const uint8_t old = st_.meterDrawn[ch];
        if (h == old) continue;
        const uint16_t bottom = static_cast<uint16_t>(kHvMeterTop + kHvMeterH);
        if (old == 0xFF) {
            g.setForeColor(kGsGray25);
            g.drawRect(static_cast<uint16_t>(kHvMeterX[ch] - 1), static_cast<uint16_t>(kHvMeterTop - 1), static_cast<uint32_t>(kHvMeterW + 2), static_cast<uint32_t>(kHvMeterH + 2));
            g.setBackColor(kGsBlack);
            g.clearRect(kHvMeterX[ch], kHvMeterTop, kHvMeterW, kHvMeterH);
            if (h > 0) {
                g.setForeColor(colors[ch]);
                g.fillRect(kHvMeterX[ch], static_cast<uint16_t>(bottom - h), kHvMeterW, h);
            }
        } else if (h > old) {
            g.setForeColor(colors[ch]);
            g.fillRect(kHvMeterX[ch], static_cast<uint16_t>(bottom - h), kHvMeterW, static_cast<uint32_t>(h - old));
        } else {
            g.setBackColor(kGsBlack);
            g.clearRect(kHvMeterX[ch], static_cast<uint16_t>(bottom - old), kHvMeterW, static_cast<uint32_t>(old - h));
        }
        st_.meterDrawn[ch] = h;
    }

    // The duck bar (a bed that is off is not ducked: its bar is grey).
    const uint16_t px = bedOn_ ? hissViewGainPx(gain_, kHvDuckW) : kHvDuckW;
    if (bedOn_ != st_.bedOnDrawn) {
        st_.bedOnDrawn = bedOn_;
        st_.gainDrawn = 0xFFFF;  // another colour: the bar is drawn again
    }
    if (px == st_.gainDrawn) return;
    const uint16_t barColor = bedOn_ ? kGsGreen : kGsGray25;
    if (st_.gainDrawn == 0xFFFF) {
        g.setBackColor(kGsBlack);
        g.clearRect(kHvDuckX, kHvDuckY, kHvDuckW, kHvDuckH);
        g.setForeColor(barColor);
        if (px > 0) g.fillRect(kHvDuckX, kHvDuckY, px, kHvDuckH);
    } else if (px > st_.gainDrawn) {
        g.setForeColor(barColor);
        g.fillRect(static_cast<uint16_t>(kHvDuckX + st_.gainDrawn), kHvDuckY, static_cast<uint32_t>(px - st_.gainDrawn), kHvDuckH);
    } else {
        g.setBackColor(kGsBlack);
        g.clearRect(static_cast<uint16_t>(kHvDuckX + px), kHvDuckY, static_cast<uint32_t>(st_.gainDrawn - px), kHvDuckH);
    }
    if (in_.duck > 0) {  // the floor marker was drawn over by the bar
        g.setForeColor(kGsYellow);
        g.drawVLine(static_cast<uint16_t>(kHvDuckX + hissViewFloorPx(in_.duck, kHvDuckW)), static_cast<uint16_t>(kHvDuckY - 4), static_cast<int16_t>(kHvDuckH + 8));
    }
    st_.gainDrawn = px;
    char text[24];
    const unsigned percent = static_cast<unsigned>((static_cast<uint32_t>(px) * 100u + kHvDuckW / 2u) / kHvDuckW);
    if (bedOn_) {
        snprintf(text, sizeof text, "BED GAIN %3u", percent);
    } else {
        snprintf(text, sizeof text, "BED OFF     ");  // the same width: it overwrites the number
    }
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setBackColor(kGsBlack);
    g.setForeColor(bedOn_ ? kGsWhite : kGsGray50);
    g.drawText(text, static_cast<uint16_t>(strlen(text)), kHvDuckX, kHvDuckTextY);
    // The % sign is drawn small: the 7x9 font's glyph for it is hard to read.
    g.setFont(GS_FONT_05x07);
    g.setForeColor(bedOn_ ? kGsWhite : kGsBlack);
    g.drawText("%", 1, static_cast<uint16_t>(kHvDuckX + static_cast<uint16_t>(strlen(text)) * 9u + 2), static_cast<uint16_t>(kHvDuckTextY + 1));
}

// The whole HISS screen, then the live part once. peaks*_ and rms*_ = hissViewTexture (kHvTextureCols columns).
template <class G>
inline void gsDrawHiss(G& g, const GsHiss& in_, const uint8_t* peaksL_, const uint8_t* peaksR_, const uint8_t* rmsL_, const uint8_t* rmsR_, GsHissLive& st_, bool bedOn_, float gain_, float peakL_,
                       float peakR_) {
    g.setBackColor(kGsBlack);
    g.clearRect(kGsAreaX, kGsAreaY, kGsAreaW, kGsAreaH);
    st_ = GsHissLive();  // the live bars are drawn again
    const uint8_t type = (in_.type < kHissTypeCount) ? in_.type : static_cast<uint8_t>(HISS_OFF);
    char line[80];

    // 1. TITLE AND SUMMARY
    snprintf(line, sizeof line, "HISS BED  %s", kHissTypeName[type]);
    g.setFont(GS_FONT_10x14);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsCyan);
    g.drawText(line, static_cast<uint16_t>(strlen(line)), kGvTitleX, kGvTitleY);
    char tone[16];
    gvFormatHz(tone, sizeof tone, hissToneHz(in_.tone));
    const float levelDb = hissViewLevelDb(in_.level);
    if (levelDb > -100.0f) {
        snprintf(line, sizeof line, "LEVEL %u  %d DBFS  TONE %s  DUCK %u", static_cast<unsigned>(in_.level), static_cast<int>(lroundf(levelDb)), tone, static_cast<unsigned>(in_.duck));
    } else {
        snprintf(line, sizeof line, "LEVEL 0  SILENT  TONE %s  DUCK %u", tone, static_cast<unsigned>(in_.duck));
    }
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_RIGHT);
    g.setForeColor(kGsWhite);
    g.drawText(line, static_cast<uint16_t>(strlen(line)), kGsRightEdge, kGvTitleY + 3);

    // 2. TEXTURE: left channel above, right channel below, height = dBFS from -60 (nothing) to 0
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray75);
    gsDraw(g, "TEXTURE  ONE SECOND  LINES EVERY 12 DBFS", kHvTextureX, kHvTextureLabelY);
    const uint16_t tops[2] = {kHvTextureLTop, kHvTextureRTop};
    const uint8_t* const peaks[2] = {peaksL_, peaksR_};
    const uint8_t* const rms[2] = {rmsL_, rmsR_};
    const uint16_t colors[2] = {kGsCyan, kGsMagenta};
    const uint16_t dimColors[2] = {kGsCyanDim, kGsMagentaDim};
    for (uint8_t ch = 0; ch < 2; ch++) {
        const uint16_t centre = static_cast<uint16_t>(tops[ch] + kHvTextureHalf);
        g.setForeColor(kGsGray25);
        g.drawRect(static_cast<uint16_t>(kHvTextureX - 1), static_cast<uint16_t>(tops[ch] - 1), static_cast<uint32_t>(kHvTextureCols + 2), static_cast<uint32_t>(2 * kHvTextureHalf + 3));
        for (uint8_t k = 1; k < 5; k++) {  // -12, -24, -36, -48 dBFS
            const uint16_t h = static_cast<uint16_t>(((kHvTextureFloorDb - 12.0f * static_cast<float>(k)) / kHvTextureFloorDb) * static_cast<float>(kHvTextureHalf));
            for (uint16_t x = kHvTextureX; x < kHvTextureX + kHvTextureCols; x += 4) {
                g.drawPixel(x, static_cast<uint16_t>(centre - h));
                g.drawPixel(x, static_cast<uint16_t>(centre + h));
            }
        }
        g.setForeColor(dimColors[ch]);  // the peaks (the crackle's pops stand out), the RMS body bright inside them
        for (uint16_t c = 0; c < kHvTextureCols; c++) {
            const uint8_t h = peaks[ch][c];
            if (h > 0) g.drawVLine(static_cast<uint16_t>(kHvTextureX + c), static_cast<uint16_t>(centre - h), static_cast<int16_t>(2 * h + 1));
        }
        g.setForeColor(colors[ch]);
        for (uint16_t c = 0; c < kHvTextureCols; c++) {
            const uint8_t h = rms[ch][c];
            if (h > 0) g.drawVLine(static_cast<uint16_t>(kHvTextureX + c), static_cast<uint16_t>(centre - h), static_cast<int16_t>(2 * h + 1));
        }
        g.setForeColor(kGsGray75);
        gsDraw(g, ch == 0 ? "L" : "R", static_cast<uint16_t>(kHvTextureX + 4), static_cast<uint16_t>(tops[ch] + 3));
    }
    if (type == HISS_OFF) {
        g.setForeColor(kGsYellow);
        g.setAlignment(GS_CENTER);
        gsDraw(g, "HISS IS OFF: TURN THE FIRST KNOB TO TAPE, VINYL, PINK OR WHITE", kHvTextureX + kHvTextureCols / 2, kHvTextureRTop + kHvTextureHalf + 20);
        g.setAlignment(GS_LEFT);
    }

    // 3. SPECTRUM, with the TONE low-pass marked
    g.setForeColor(kGsGray75);
    gsDraw(g, "SPECTRUM", kGvSpecX, kGvSpecLabelY);
    gsSpectrumFrame(g, kGvSpecX, kGvSpecTop, kGvSpecW, kGvSpecH, kGvSpecScaleY);
    if (type != HISS_OFF) {
        float curve[kGvSpecW];
        float reference = -1000.0f;
        for (uint16_t c = 0; c < kGvSpecW; c++) {
            curve[c] = hissViewSpectrumDb(type, in_.tone, gvHzAtColumn(c, kGvSpecW));
            reference = fmaxf(reference, curve[c]);
        }
        gsSpectrumCurve(g, curve, kGvSpecW, kGvSpecX, kGvSpecTop, kGvSpecH, reference, kGsWhite);
        const uint16_t xt = gvXOfHz(hissToneHz(in_.tone), kGvSpecX, kGvSpecW);
        g.setForeColor(kGsYellow);
        for (uint16_t y = kGvSpecTop; y < kGvSpecTop + kGvSpecH; y += 3) g.drawPixel(xt, y);
    }
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsYellow);
    g.setAlignment(GS_LEFT);
    snprintf(line, sizeof line, "TONE %s", tone);
    g.drawText(line, static_cast<uint16_t>(strlen(line)), kGvSpecX, kGvSpecTextY);

    // 4. DUCK: the frame and the floor the bed can fall to; the live gain follows
    g.setForeColor(kGsGray75);
    gsDraw(g, "DUCK  THE BED FALLS WHEN A LAYER PLAYS", kHvDuckX, kHvDuckLabelY);
    g.setForeColor(kGsGray50);
    g.drawRect(static_cast<uint16_t>(kHvDuckX - 1), static_cast<uint16_t>(kHvDuckY - 1), static_cast<uint32_t>(kHvDuckW + 2), static_cast<uint32_t>(kHvDuckH + 2));
    if (in_.duck > 0) {
        const uint16_t xf = static_cast<uint16_t>(kHvDuckX + hissViewFloorPx(in_.duck, kHvDuckW));
        g.setForeColor(kGsYellow);
        g.drawVLine(xf, static_cast<uint16_t>(kHvDuckY - 4), static_cast<int16_t>(kHvDuckH + 8));
        snprintf(line, sizeof line, "FLOOR %u %%", static_cast<unsigned>(100u - in_.duck));
        g.setAlignment(GS_RIGHT);
        g.drawText(line, static_cast<uint16_t>(strlen(line)), kHvDuckX + kHvDuckW, kHvDuckLabelY);
        g.setAlignment(GS_LEFT);
    }
    gsDrawHissLive(g, in_, st_, bedOn_, gain_, peakL_, peakR_);
}

#endif
