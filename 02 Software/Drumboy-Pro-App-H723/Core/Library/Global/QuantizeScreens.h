// The quantize screen drawn (backlog 59): what QuantizeView.h computes, put on the lane area. Header-only and
// templated on the LCD like the generator and file screens: the firmware draws with its Lcd (through the
// adapter of Controller.cpp) and a PC test draws the same code on a pixel buffer, checks it stays inside the
// lane area and writes the pictures (02 Software/tests, test_quantizescreens).
//
// The screen shows while the RHYTHM page has its QUANTIZE or STRENGTH field selected:
//   left   every grid as a row of points over two beats, the chosen one lit
//   right  what the chosen grid is, and STRENGTH with a hit played a little late and where it lands
// Texts use only the characters of the firmware's font (0-9, A-Z, ! # % + , - . / : < > ? [ ] _ |).
#ifndef __QUANTIZESCREENS_H
#define __QUANTIZESCREENS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GeneratorScreens.h"  // the colours, fonts, the lane area and gsDraw
#include "QuantizeView.h"

struct QsInput {
    uint8_t grid;      // the QUANTIZE field
    uint8_t strength;  // the STRENGTH field
    bool strengthTab;  // true: the STRENGTH field is the selected one (else QUANTIZE)
};

const uint16_t kQsTitleX = 60, kQsTitleY = 134;
const uint16_t kQsHeaderY = 160;                    // "BEAT 1", "BEAT 2", "END" above the strip
const uint16_t kQsRowY0 = 178, kQsRowStep = 18, kQsTickH = 12;
const uint16_t kQsLabelRight = 100;                 // the grid names end here
const uint16_t kQsFrameX = 30, kQsFrameW = 626;     // the frame of the chosen row
const uint16_t kQsLegendY = 430;
const uint16_t kQsPanelX = 675, kQsPanelW = 245;    // the right side
const uint16_t kQsStrengthY = 262, kQsStrengthH = 170;
const uint16_t kQsDemoCentreX = 770, kQsDemoReach = 60;  // the demo picture: the point's column, and how far a late hit sits from it

inline uint16_t qsRowY(uint8_t row_) { return static_cast<uint16_t>(kQsRowY0 + row_ * kQsRowStep); }

// The tick colours: the chosen row is lit (straight yellow, triplet cyan, both white), the others dim.
inline uint16_t qsTickColor(uint8_t owner_, bool lit_) {
    if (lit_) return (owner_ == 3) ? kGsWhite : ((owner_ == 2) ? kGsCyan : kGsYellow);
    return (owner_ == 3) ? kGsGray75 : ((owner_ == 2) ? kGsCyanDim : kGsGray50);
}

template <class G>
inline void qsDrawRow(G& g, uint8_t row_, bool chosen_) {
    const uint16_t y = qsRowY(row_);
    char name[12];
    qvName(row_, name, sizeof name);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_RIGHT);
    g.setForeColor(chosen_ ? kGsYellow : kGsGray75);
    gsDraw(g, name, kQsLabelRight, static_cast<uint16_t>(y + 2));
    g.setAlignment(GS_LEFT);
    if (row_ == kQuantizeGridOff) {
        // No points: every interval is one. A solid line along the strip says so.
        g.setForeColor(chosen_ ? kGsYellow : kGsGray50);
        g.fillRect(kQvStripX, static_cast<uint16_t>(y + kQsTickH / 2 - 1), kQvStripW, 2);
        return;
    }
    QvTicks t;
    qvTicks(row_, t);
    for (uint8_t i = 0; i < t.n; i++) {
        g.setForeColor(qsTickColor(t.owner[i], chosen_));
        g.fillRect(qvStripXOf(t.at[i]), y, 2, kQsTickH);
    }
}

template <class G>
inline void qsDrawPercent(G& g, uint16_t rightEdge_, uint16_t y_, uint16_t color_) {
    // The % sign is drawn small: the 7x9 font's glyph for it is hard to read.
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_RIGHT);
    g.setForeColor(color_);
    g.drawText("%", 1, rightEdge_, static_cast<uint16_t>(y_ + 1));
}

// The whole screen: clears the lane area, then draws. (It is drawn again whenever a field changes.)
template <class G>
inline void qsDrawScreen(G& g, const QsInput& in_) {
    const uint8_t grid = (in_.grid < kQuantizeGridCount) ? in_.grid : static_cast<uint8_t>(kQuantizeGridDefault);
    const uint8_t strength = (in_.strength > 100) ? static_cast<uint8_t>(100) : in_.strength;
    g.setBackColor(kGsBlack);
    g.clearRect(kGsAreaX, kGsAreaY, kGsAreaW, kGsAreaH);
    char line[64], name[12];

    // 1. TITLE
    qvName(grid, name, sizeof name);
    snprintf(line, sizeof line, "QUANTIZE  %s", name);
    g.setFont(GS_FONT_10x14);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsCyan);
    gsDraw(g, line, kQsTitleX, kQsTitleY);

    // 2. THE STRIP: beat lines, then a row for every grid
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray75);
    gsDraw(g, "BEAT 1", static_cast<uint16_t>(qvStripXOf(0) + 3), kQsHeaderY);
    gsDraw(g, "BEAT 2", static_cast<uint16_t>(qvStripXOf(kQuantizeIntervalsPerBeat) + 3), kQsHeaderY);
    g.setAlignment(GS_RIGHT);
    gsDraw(g, "END", static_cast<uint16_t>(qvStripXOf(kQvSpan) - 3), kQsHeaderY);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray25);
    const uint16_t lineTop = static_cast<uint16_t>(kQsHeaderY + 10), lineH = static_cast<uint16_t>(qsRowY(kQuantizeGridCount - 1) + kQsTickH - lineTop + 2);
    for (uint16_t beat = 0; beat <= 2; beat++) g.fillRect(static_cast<uint16_t>(qvStripXOf(beat * kQuantizeIntervalsPerBeat) - ((beat == 2) ? 1 : 0)), lineTop, 1, lineH);
    for (uint8_t row = 0; row < kQuantizeGridCount; row++) {
        if (row == grid) continue;
        qsDrawRow(g, row, false);
    }
    qsDrawRow(g, grid, true);  // last: the lit row on top
    g.setForeColor(in_.strengthTab ? kGsGray25 : kGsYellow);
    g.drawRect(kQsFrameX, static_cast<uint16_t>(qsRowY(grid) - 2), kQsFrameW, kQsRowStep - 2);
    // The legend of the lit row's colours.
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsYellow);
    gsDraw(g, "STRAIGHT", kQvStripX, kQsLegendY);
    g.setForeColor(kGsCyan);
    gsDraw(g, "TRIPLET", static_cast<uint16_t>(kQvStripX + 70), kQsLegendY);
    g.setForeColor(kGsWhite);
    gsDraw(g, "BOTH", static_cast<uint16_t>(kQvStripX + 133), kQsLegendY);

    // 3. THE GRID: what it is
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray75);
    gsDraw(g, "GRID", kQsPanelX, 164);
    g.setFont(GS_FONT_10x14);
    g.setForeColor(kGsYellow);
    gsDraw(g, name, kQsPanelX, 176);
    g.setFont(GS_FONT_07x09);
    g.setForeColor(kGsWhite);
    gsDraw(g, qvKindName(qvKind(grid)), kQsPanelX, 200);
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray75);
    qvKindLine1(grid, line, sizeof line);
    gsDraw(g, line, kQsPanelX, 216);
    qvKindLine2(grid, line, sizeof line);
    gsDraw(g, line, kQsPanelX, 228);
    g.setForeColor(kGsGray50);
    qvChangeNote(grid, line, sizeof line);
    gsDraw(g, line, kQsPanelX, 244);

    // 4. STRENGTH: the value and a bar, then a late hit and where it lands
    g.setForeColor(in_.strengthTab ? kGsYellow : kGsGray25);
    g.drawRect(kQsPanelX, kQsStrengthY, kQsPanelW, kQsStrengthH);
    const uint16_t ix = static_cast<uint16_t>(kQsPanelX + 8), iw = static_cast<uint16_t>(kQsPanelW - 16);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsWhite);
    gsDraw(g, "STRENGTH", ix, static_cast<uint16_t>(kQsStrengthY + 8));
    snprintf(line, sizeof line, "%u", static_cast<unsigned>(strength));
    g.setAlignment(GS_RIGHT);
    g.setForeColor(in_.strengthTab ? kGsYellow : kGsWhite);
    gsDraw(g, line, static_cast<uint16_t>(kQsPanelX + kQsPanelW - 20), static_cast<uint16_t>(kQsStrengthY + 8));
    qsDrawPercent(g, static_cast<uint16_t>(kQsPanelX + kQsPanelW - 8), static_cast<uint16_t>(kQsStrengthY + 8), in_.strengthTab ? kGsYellow : kGsWhite);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray50);
    g.drawRect(ix, static_cast<uint16_t>(kQsStrengthY + 24), iw, 8);
    g.setForeColor(in_.strengthTab ? kGsYellow : kGsGray75);
    const uint16_t fill = static_cast<uint16_t>(((iw - 2u) * strength) / 100u);
    if (fill > 0) g.fillRect(static_cast<uint16_t>(ix + 1), static_cast<uint16_t>(kQsStrengthY + 25), fill, 6);
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray75);
    qvStrengthPhrase(strength, line, sizeof line);
    if (grid == kQuantizeGridOff) snprintf(line, sizeof line, "NOT USED: HITS ARE NOT SNAPPED");
    gsDraw(g, line, ix, static_cast<uint16_t>(kQsStrengthY + 38));

    const QvDemo d = qvDemo(grid, strength);
    if (!d.valid) {
        g.setForeColor(kGsGray75);
        gsDraw(g, "WITH OFF NOTHING IS MOVED:", ix, static_cast<uint16_t>(kQsStrengthY + 70));
        gsDraw(g, "STRENGTH HAS NO EFFECT", ix, static_cast<uint16_t>(kQsStrengthY + 82));
        return;
    }
    const uint16_t playedY = static_cast<uint16_t>(kQsStrengthY + 62), landedY = static_cast<uint16_t>(kQsStrengthY + 96);
    g.setForeColor(kGsGray75);
    gsDraw(g, "PLAYED", ix, static_cast<uint16_t>(playedY + 1));
    gsDraw(g, "LANDS", ix, static_cast<uint16_t>(landedY + 1));
    g.setForeColor(kGsCyan);  // the grid point
    g.fillRect(static_cast<uint16_t>(kQsDemoCentreX - 1), static_cast<uint16_t>(playedY - 6), 2, static_cast<uint16_t>(landedY - playedY + 22));
    const uint16_t playedX = static_cast<uint16_t>(kQsDemoCentreX + kQsDemoReach);
    const uint16_t landedX = static_cast<uint16_t>(kQsDemoCentreX + (static_cast<uint32_t>(d.landedBy) * kQsDemoReach) / d.lateBy);
    g.setForeColor(kGsWhite);
    g.fillRect(static_cast<uint16_t>(playedX - 2), playedY, 5, 9);
    g.setForeColor(kGsYellow);
    g.fillRect(static_cast<uint16_t>(landedX - 2), landedY, 5, 9);
    char line2[40];
    qvDemoLines(d, line, sizeof line, line2, sizeof line2);
    g.setForeColor(kGsWhite);
    gsDraw(g, line, ix, static_cast<uint16_t>(kQsStrengthY + 128));
    g.setForeColor(kGsYellow);
    gsDraw(g, line2, ix, static_cast<uint16_t>(kQsStrengthY + 140));
}

#endif
