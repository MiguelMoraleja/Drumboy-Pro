// "Quantize now" drawn (backlog 59): what QuantizeNow.h computes, put on the lane area. Header-only and templated on
// the LCD like the other screens (test: 02 Software/tests, test_quantizenow). Texts use only the characters of the
// firmware's font; the % sign is not used.
#ifndef __QUANTIZENOWSCREENS_H
#define __QUANTIZENOWSCREENS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GeneratorScreens.h"  // the colours, fonts, the lane area and gsDraw
#include "QuantizeNow.h"
#include "QuantizeView.h"      // qvName

const uint16_t kQnTitleX = 30, kQnTitleY = 138;
const uint16_t kQnScopeLabelY = 170, kQnScopeX = 30, kQnScopeY = 186, kQnScopeStep = 26, kQnScopeW = 236, kQnScopeH = 22;
const uint16_t kQnCountY = 276, kQnCountStep = 16, kQnCountRight = 266;
const uint16_t kQnApplyX = 30, kQnApplyY = 366, kQnApplyW = 236, kQnApplyH = 44, kQnHintY = 416;
const uint16_t kQnStripX = 300, kQnStripW = 620;
const uint16_t kQnNowY = 180, kQnAfterY = 280, kQnStripH = 34;
const uint16_t kQnCaptionY = 322, kQnBarsLabelY = 340, kQnBarBase = 420, kQnBarMax = 56, kQnBarStep = 38, kQnNumberY = 428;

inline uint16_t qnX(uint32_t interval_) { return static_cast<uint16_t>(kQnStripX + (interval_ * kQnStripW) / kQnSpan); }

template <class G>
inline void qnDrawFrame(G& g) {
    g.setBackColor(kGsBlack);
    g.setAlignment(GS_LEFT);
    g.setFont(GS_FONT_07x09);
    g.setForeColor(kGsGray75);
    gsDraw(g, "SCOPE", kQnScopeX, kQnScopeLabelY);
    gsDraw(g, "NOTES", kQnScopeX, kQnCountY);
    gsDraw(g, "MOVE", kQnScopeX, static_cast<uint16_t>(kQnCountY + kQnCountStep));
    gsDraw(g, "ON GRID", kQnScopeX, static_cast<uint16_t>(kQnCountY + 2 * kQnCountStep));
    gsDraw(g, "LARGEST MOVE", kQnScopeX, static_cast<uint16_t>(kQnCountY + 3 * kQnCountStep));
    gsDraw(g, "ALSO JOINED", kQnScopeX, static_cast<uint16_t>(kQnCountY + 4 * kQnCountStep));
    gsDraw(g, "HOW FAR EACH NOTE MOVES", kQnStripX, kQnBarsLabelY);
    gsDraw(g, "NOW", kQnStripX, static_cast<uint16_t>(kQnNowY - 14));
    gsDraw(g, "AFTER", kQnStripX, static_cast<uint16_t>(kQnAfterY - 14));
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray50);
    gsDraw(g, "TURN APPLY RIGHT TWICE", kQnScopeX, kQnHintY);
    gsDraw(g, "UNDO: MUTE + LEFT", kQnScopeX, static_cast<uint16_t>(kQnHintY + 10));
}

template <class G>
inline void qnDrawTitle(G& g, const char* text_) {
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(kQnTitleX, kQnTitleY, 700, 10);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsCyan);
    gsDraw(g, text_, kQnTitleX, kQnTitleY);
}

template <class G>
inline void qnDrawScope(G& g, uint8_t scope_) {
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    for (uint8_t k = 0; k < kQnScopes; k++) {
        const uint16_t y = static_cast<uint16_t>(kQnScopeY + k * kQnScopeStep);
        const bool on = (k == scope_);
        g.setForeColor(on ? kGsGray25 : kGsBlack);
        g.fillRect(kQnScopeX, y, kQnScopeW, kQnScopeH);
        g.setForeColor(on ? kGsCyan : kGsGray25);
        g.drawRect(kQnScopeX, y, kQnScopeW, kQnScopeH);
        g.setBackColor(on ? kGsGray25 : kGsBlack);
        g.setForeColor(on ? kGsCyan : kGsGray75);
        gsDraw(g, qnScopeName(k), static_cast<uint16_t>(kQnScopeX + 12), static_cast<uint16_t>(y + 6));
    }
    g.setBackColor(kGsBlack);
}

template <class G>
inline void qnDrawTotals(G& g, const QnTotals& t_) {
    const uint32_t v[5] = {t_.notes, t_.moves, t_.onGrid, t_.largest, t_.merged};
    const uint16_t c[5] = {kGsWhite, kGsYellow, kGsGreen, kGsWhite, (t_.merged > 0) ? kGsYellow : kGsGray50};
    const uint8_t order[5] = {0, 1, 2, 3, 4};
    g.setBackColor(kGsBlack);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_RIGHT);
    for (uint8_t k = 0; k < 5; k++) {
        char text[12];
        snprintf(text, sizeof text, "%5lu", static_cast<unsigned long>(v[order[k]] > 99999 ? 99999 : v[order[k]]));
        g.setForeColor(c[k]);
        gsDraw(g, text, kQnCountRight, static_cast<uint16_t>(kQnCountY + k * kQnCountStep));
    }
    g.setAlignment(GS_LEFT);
}

template <class G>
inline void qnDrawApply(G& g, QnApplyState s_) {
    const uint16_t c = (s_ == QN_SURE) ? kGsYellow : ((s_ == QN_DONE) ? kGsCyan : ((s_ == QN_NOGRID) ? kGsGray50 : kGsGreen));
    g.setForeColor(kGsBlack);
    g.fillRect(kQnApplyX, kQnApplyY, kQnApplyW, kQnApplyH);
    g.setForeColor(c);
    g.drawRect(kQnApplyX, kQnApplyY, kQnApplyW, kQnApplyH);
    if (s_ == QN_SURE) g.drawRect(static_cast<uint16_t>(kQnApplyX + 1), static_cast<uint16_t>(kQnApplyY + 1), kQnApplyW - 2, kQnApplyH - 2);
    g.setBackColor(kGsBlack);
    g.setFont(GS_FONT_10x14);
    g.setAlignment(GS_CENTER);
    gsDraw(g, (s_ == QN_SURE) ? "SURE?" : ((s_ == QN_DONE) ? "DONE" : ((s_ == QN_NOGRID) ? "NO GRID" : "APPLY")), static_cast<uint16_t>(kQnApplyX + kQnApplyW / 2), static_cast<uint16_t>(kQnApplyY + 15));
    g.setAlignment(GS_LEFT);
}

// The picture: the first two beats before and after, a line from each note to where it lands, and a bar for how far
// each moves (cyan when it lands later, yellow when earlier).
template <class G>
inline void qnDrawPicture(G& g, uint8_t grid_, uint8_t strength_, const QnNote* notes_, uint8_t count_, uint32_t song_) {
    g.setBackColor(kGsBlack);
    g.setForeColor(kGsBlack);
    g.fillRect(kQnStripX, static_cast<uint16_t>(kQnNowY - 4), kQnStripW + 6, 150);  // both strips and the lines between
    g.fillRect(kQnStripX, static_cast<uint16_t>(kQnCaptionY), 640, 12);
    g.fillRect(kQnStripX, static_cast<uint16_t>(kQnBarBase - kQnBarMax - 2), 640, static_cast<uint16_t>(kQnBarMax + 4));
    g.fillRect(kQnStripX, static_cast<uint16_t>(kQnNumberY), 640, 8);
    const uint16_t ys[2] = {kQnNowY, kQnAfterY};
    for (uint8_t s = 0; s < 2; s++) {
        g.setForeColor(kGsGray25);
        g.fillRect(kQnStripX, static_cast<uint16_t>(ys[s] + kQnStripH / 2), kQnStripW, 1);
        if (grid_ == kQuantizeGridOff) continue;
        QvTicks t;
        qvTicks(grid_, t);
        for (uint8_t i = 0; i < t.n; i++) {
            g.setForeColor((t.at[i] % kQuantizeIntervalsPerBeat) == 0 ? kGsGray75 : kGsGray50);
            g.fillRect(qnX(t.at[i]), static_cast<uint16_t>(ys[s] + 2), 1, static_cast<uint16_t>(kQnStripH - 4));
        }
        g.setForeColor(kGsGray75);
        g.fillRect(qnX(kQnSpan) - 1, static_cast<uint16_t>(ys[s] + 2), 1, static_cast<uint16_t>(kQnStripH - 4));
    }
    for (uint8_t i = 0; i < count_; i++) {
        const uint16_t sx = qnX(notes_[i].start), lx = qnX(notes_[i].land);
        g.setForeColor(notes_[i].start == notes_[i].land ? kGsGray25 : kGsGray50);
        g.drawLine(sx, static_cast<uint16_t>(kQnNowY + kQnStripH), lx, kQnAfterY);
        g.setForeColor(kGsYellow);
        g.fillRect(static_cast<uint16_t>(sx - 2), static_cast<uint16_t>(kQnNowY + 4), 5, static_cast<uint16_t>(kQnStripH - 8));
        const bool onPoint = (grid_ != kQuantizeGridOff) && (quantizeNearest(notes_[i].land, grid_) == notes_[i].land);
        g.setForeColor(onPoint ? kGsGreen : kGsCyan);
        g.fillRect(static_cast<uint16_t>(lx - 2), static_cast<uint16_t>(kQnAfterY + 4), 5, static_cast<uint16_t>(kQnStripH - 8));
    }
    char text[64];
    if (grid_ == kQuantizeGridOff) snprintf(text, sizeof text, "NO GRID: NOTHING MOVES    FIRST TWO BEATS");
    else {
        char name[12];
        qvName(grid_, name, sizeof name);
        snprintf(text, sizeof text, "GRID %s  STRENGTH %u    FIRST TWO BEATS", name, static_cast<unsigned>(strength_));
    }
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray50);
    gsDraw(g, text, kQnStripX, kQnCaptionY);
    for (uint8_t i = 0; i < count_; i++) {
        const uint16_t x = static_cast<uint16_t>(kQnStripX + i * kQnBarStep);
        const int32_t d = static_cast<int32_t>(notes_[i].land) - static_cast<int32_t>(notes_[i].start);
        const uint32_t dist = qnDistance(notes_[i].start, notes_[i].land, song_);
        g.setForeColor(kGsGray25);
        g.fillRect(x, kQnBarBase, 32, 1);
        if (dist > 0) {
            const uint16_t h = static_cast<uint16_t>(dist > 60 ? kQnBarMax : (dist * kQnBarMax + 59) / 60);
            const bool earlier = (d < 0) == (dist == static_cast<uint32_t>(d < 0 ? -d : d));  // a wrap to the start counts by the short way round
            g.setForeColor(earlier ? kGsYellow : kGsCyan);  // yellow: the note lands earlier, cyan: later
            g.fillRect(static_cast<uint16_t>(x + 6), static_cast<uint16_t>(kQnBarBase - h), 20, h);
        }
        snprintf(text, sizeof text, "%lu", static_cast<unsigned long>(dist));
        g.setForeColor(dist == 0 ? kGsGreen : kGsWhite);
        g.setAlignment(GS_CENTER);
        gsDraw(g, text, static_cast<uint16_t>(x + 16), kQnNumberY);
    }
    g.setAlignment(GS_LEFT);
}

#endif
