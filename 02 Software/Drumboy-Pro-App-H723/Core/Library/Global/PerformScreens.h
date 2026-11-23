// The Performance page drawn (backlog 45): what PerformView.h computes, put on the lane area. Header-only and
// templated on the LCD like the other screens: the firmware draws with its Lcd (through the adapter of
// Controller.cpp) and a PC test draws the same code on a pixel buffer, checks it stays inside the lane area
// and writes the pictures (02 Software/tests, test_performscreens).
//
// The big digits are seven segments drawn with filled rectangles: no new font, and the same on the firmware
// and in the test. Texts use only the characters of the firmware's font.
#ifndef __PERFORMSCREENS_H
#define __PERFORMSCREENS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GeneratorScreens.h"  // the colours, fonts, the lane area and gsDraw
#include "PerformView.h"

const uint16_t kPfRed = 0xF800;
const uint16_t kPfRedDim = 0x4000;

// The three blocks of the top band.
const uint16_t kPfBarX = 30, kPfTempoX = 176, kPfChainX = 346;
const uint16_t kPfBandLabelY = 136, kPfDigitY = 150, kPfDigitW = 28, kPfDigitH = 46, kPfDigitStep = 36, kPfSegT = 6;
const uint16_t kPfBandSubY = 202, kPfDividerY = 212;
const uint16_t kPfChainBoxY = 156, kPfChainBoxH = 34;

// Seven segments, a b c d e f g, for the digits 0-9 (bit 0 = a).
inline uint8_t pfSegments(uint8_t digit_) {
    static const uint8_t kSeg[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    return kSeg[digit_ > 9 ? 0 : digit_];
}

template <class G>
inline void pfDrawDigit(G& g, uint16_t x_, uint16_t y_, uint8_t digit_, uint16_t color_, uint16_t off_) {
    const uint8_t s = pfSegments(digit_);
    const uint16_t t = kPfSegT, w = kPfDigitW, h = kPfDigitH, mid = static_cast<uint16_t>(y_ + (h - t) / 2);
    struct Seg { uint16_t x, y, w, h; };
    const Seg seg[7] = {
        {static_cast<uint16_t>(x_ + t), y_, static_cast<uint16_t>(w - 2 * t), t},                                                         // a
        {static_cast<uint16_t>(x_ + w - t), static_cast<uint16_t>(y_ + t), t, static_cast<uint16_t>((h - 3 * t) / 2)},                    // b
        {static_cast<uint16_t>(x_ + w - t), static_cast<uint16_t>(mid + t), t, static_cast<uint16_t>((h - 3 * t) / 2)},                   // c
        {static_cast<uint16_t>(x_ + t), static_cast<uint16_t>(y_ + h - t), static_cast<uint16_t>(w - 2 * t), t},                          // d
        {x_, static_cast<uint16_t>(mid + t), t, static_cast<uint16_t>((h - 3 * t) / 2)},                                                  // e
        {x_, static_cast<uint16_t>(y_ + t), t, static_cast<uint16_t>((h - 3 * t) / 2)},                                                   // f
        {static_cast<uint16_t>(x_ + t), mid, static_cast<uint16_t>(w - 2 * t), t},                                                        // g
    };
    for (uint8_t k = 0; k < 7; k++) {
        g.setForeColor((s & (1u << k)) ? color_ : off_);
        g.fillRect(seg[k].x, seg[k].y, seg[k].w, seg[k].h);
    }
}

// A number of `digits_` digits, leading zeros kept (03), unlit segments barely visible.
template <class G>
inline void pfDrawNumber(G& g, uint16_t x_, uint16_t y_, uint16_t value_, uint8_t digits_, uint16_t color_) {
    uint16_t v = value_;
    for (int8_t i = static_cast<int8_t>(digits_) - 1; i >= 0; i--) {
        pfDrawDigit(g, static_cast<uint16_t>(x_ + i * kPfDigitStep), y_, static_cast<uint8_t>(v % 10), color_, 0x1082);
        v = static_cast<uint16_t>(v / 10);
    }
}

// The three fixed texts and the dividers of the top band (drawn once, with the page).
template <class G>
inline void pfDrawFrame(G& g) {
    g.setBackColor(kGsBlack);
    g.setAlignment(GS_LEFT);
    g.setFont(GS_FONT_07x09);
    g.setForeColor(kGsGray75);
    gsDraw(g, "BAR", kPfBarX, kPfBandLabelY);
    gsDraw(g, "TEMPO", kPfTempoX, kPfBandLabelY);
    g.setFont(GS_FONT_05x07);
    gsDraw(g, "BPM", static_cast<uint16_t>(kPfTempoX + 3 * kPfDigitStep + 4), static_cast<uint16_t>(kPfDigitY + kPfDigitH - 7));
    g.setForeColor(kGsGray25);
    g.fillRect(static_cast<uint16_t>(kPfTempoX - 14), 138, 1, 68);
    g.fillRect(static_cast<uint16_t>(kPfChainX - 14), 138, 1, 68);
    g.fillRect(24, kPfDividerY, 912, 1);
}

template <class G>
inline void pfDrawBarBeat(G& g, uint8_t bar_, uint8_t beat_, uint8_t measure_, uint8_t barCount_, bool playing_) {
    pfDrawNumber(g, kPfBarX, kPfDigitY, bar_, 2, playing_ ? kGsWhite : kGsGray50);
    char text[24];
    snprintf(text, sizeof text, "BEAT %u OF %u  BAR %u", static_cast<unsigned>(beat_), static_cast<unsigned>(measure_), static_cast<unsigned>(barCount_));
    g.setBackColor(kGsBlack);
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray75);
    gsDraw(g, text, kPfBarX, kPfBandSubY);
}

template <class G>
inline void pfDrawTempo(G& g, uint16_t tempo_) {
    pfDrawNumber(g, kPfTempoX, kPfDigitY, tempo_, 3, kGsWhite);
}

// The chain: boxes as wide as their repeats (the playing step lit, filled up to the loop), the step and loop,
// and what comes next.
template <class G>
inline void pfDrawChain(G& g, const PfChain& c_) {
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(kPfChainX, 132, 584, 78);  // clears the block (the band's right part)
    g.setForeColor(kGsGray75);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    gsDraw(g, "CHAIN", kPfChainX, kPfBandLabelY);
    char text[40];
    const uint8_t step = c_.step < c_.count ? c_.step : 0;
    const uint8_t reps = c_.repeats[step] ? c_.repeats[step] : 1;
    if (c_.active) snprintf(text, sizeof text, "STEP %u OF %u   LOOP %u OF %u", static_cast<unsigned>(step + 1), static_cast<unsigned>(c_.count), static_cast<unsigned>(c_.loopsDone + 1), static_cast<unsigned>(reps));
    else snprintf(text, sizeof text, "CHAIN OFF   %u STEPS", static_cast<unsigned>(c_.count));
    g.setForeColor(c_.active ? kGsCyan : kGsGray50);
    gsDraw(g, text, static_cast<uint16_t>(kPfChainX + 70), kPfBandLabelY);
    const uint16_t unit = pfChainUnit(c_);
    const uint16_t gap = (unit >= 6) ? 3 : 1;  // between two boxes
    uint16_t x = kPfChainX;
    for (uint8_t i = 0; i < c_.count && i < kPfMaxSteps; i++) {
        const uint8_t r = c_.repeats[i] ? c_.repeats[i] : 1;
        const uint16_t w = static_cast<uint16_t>(r * unit - gap);
        const bool here = (i == step);
        const uint16_t c = here && c_.active ? kGsCyan : kGsGray75;
        if (here && c_.active) {
            g.setForeColor(kGsGray25);
            g.fillRect(x, kPfChainBoxY, static_cast<uint16_t>((c_.loopsDone < r ? c_.loopsDone + 1 : r) * unit - gap), kPfChainBoxH);
        }
        g.setForeColor(c);
        g.drawRect(x, kPfChainBoxY, w, kPfChainBoxH);
        if (w >= 20) {  // a box too narrow for its bank number (a very long chain) stays empty
            char b[4];
            snprintf(b, sizeof b, "%u", static_cast<unsigned>(c_.bank[i] + 1));
            g.setFont(GS_FONT_10x14);
            gsDraw(g, b, static_cast<uint16_t>(x + 6), static_cast<uint16_t>(kPfChainBoxY + 10));
        }
        x = static_cast<uint16_t>(x + r * unit);
    }
    const int8_t next = pfChainNext(c_);
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray50);
    if (c_.active && next >= 0) snprintf(text, sizeof text, "NEXT: STEP %u  BANK %u", static_cast<unsigned>(next + 1), static_cast<unsigned>(c_.bank[next] + 1));
    else if (c_.active) snprintf(text, sizeof text, "NEXT: STOP");
    else snprintf(text, sizeof text, "TURN STEP, REPEATS: EDIT");
    gsDraw(g, text, kPfChainX, kPfBandSubY);
}

// A layer's tile from nothing: frame, number, name and state.
template <class G>
inline void pfDrawTile(G& g, uint8_t layer_, const char* number_, const char* name_, PfState state_) {
    const uint16_t x = pfTileX(layer_);
    const bool dim = (state_ == PF_MUTE) || (state_ == PF_SILENT);
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(x, kPfTileY, kPfTileW, kPfTileH);
    g.setForeColor(dim ? kGsGray25 : kGsGray50);
    g.drawRect(x, kPfTileY, kPfTileW, kPfTileH);
    g.setAlignment(GS_LEFT);
    g.setFont(GS_FONT_10x14);
    g.setForeColor(dim ? kGsGray50 : kGsWhite);
    gsDraw(g, number_, static_cast<uint16_t>(x + 6), kPfNumberY);
    g.setFont(GS_FONT_07x09);
    g.setForeColor(dim ? kGsGray50 : kGsGray75);
    gsDraw(g, name_, static_cast<uint16_t>(x + 6), kPfNameY);
    // The state: a filled box in colour for MUTE and SOLO, an outline for the rest.
    const uint16_t bx = static_cast<uint16_t>(x + 6), bw = kPfTileW - 12;
    if ((state_ == PF_MUTE) || (state_ == PF_SOLO)) {
        const uint16_t c = (state_ == PF_MUTE) ? kPfRed : kGsYellow;
        g.setForeColor(c);
        g.fillRect(bx, kPfStateY, bw, kPfStateH);
        g.setBackColor(c);
        g.setForeColor(state_ == PF_MUTE ? kGsWhite : kGsBlack);
    } else {
        g.setForeColor(kGsGray25);
        g.drawRect(bx, kPfStateY, bw, kPfStateH);
        g.setForeColor(state_ == PF_ON ? kGsGreen : kGsGray50);
    }
    const char* name = pfStateName(state_);
    g.setFont(state_ == PF_SILENT ? GS_FONT_07x09 : GS_FONT_10x14);
    g.setAlignment(GS_CENTER);
    gsDraw(g, name, static_cast<uint16_t>(bx + bw / 2), static_cast<uint16_t>(kPfStateY + (state_ == PF_SILENT ? 10 : 8)));
    g.setAlignment(GS_LEFT);
    g.setBackColor(kGsBlack);
}

// The pattern row of a tile: hit cells in cyan, the playhead cell marked in yellow below the row.
template <class G>
inline void pfDrawPattern(G& g, uint8_t layer_, uint16_t pattern_, int8_t playCell_, PfState state_) {
    const uint16_t x = pfTileX(layer_);
    const bool dim = (state_ == PF_MUTE) || (state_ == PF_SILENT);
    for (uint8_t c = 0; c < kPfCells; c++) {
        const bool on = (pattern_ & (1u << c)) != 0;
        g.setForeColor(on ? (dim ? kGsGray50 : kGsCyan) : kGsGray25);
        g.fillRect(static_cast<uint16_t>(x + 4 + c * kPfCellStep), kPfPatternY, kPfCellW, kPfCellH);
        g.setForeColor(c == playCell_ ? kGsYellow : kGsBlack);
        g.fillRect(static_cast<uint16_t>(x + 4 + c * kPfCellStep), kPfHeadY, kPfCellW, 2);
    }
}

// The level bar of a tile: 13 segments, green then yellow then red (dark when the layer is not heard).
template <class G>
inline void pfDrawMeter(G& g, uint8_t layer_, uint8_t lit_, bool heard_) {
    const uint16_t x = static_cast<uint16_t>(pfTileX(layer_) + (kPfTileW - kPfMeterW) / 2);
    for (uint8_t k = 0; k < kPfMeterSegments; k++) {
        uint16_t c = kGsGray25;
        if (heard_ && (k < lit_)) c = (k < kPfMeterGreen) ? kGsGreen : ((k < kPfMeterYellow) ? kGsYellow : kPfRed);
        g.setForeColor(c);
        g.fillRect(x, pfSegmentY(k), kPfMeterW, kPfMeterH);
    }
}

#endif
