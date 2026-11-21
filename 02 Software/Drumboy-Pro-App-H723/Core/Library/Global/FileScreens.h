// The load and save screens (backlog 60), drawn in the lane area like the other full-area views:
// header-only and templated on the LCD (as GeneratorScreens.h), so the firmware draws with its Lcd and a
// PC test draws the same code on a pixel buffer and writes the pictures (test_filescreens).
//
// GRID: all 100 slots as cells (used, empty, unreadable, the one selected, the one last loaded or
// saved), and a card for the selected slot: tempo, length, the ten lanes as a thumbnail and the
// instruments. SAVE shows the song in memory next to what the slot holds. (A list of ten rows was drawn
// as the alternative and not chosen.)
// A song slot shows what its file holds (no name is stored): tempo, MEASURE and BAR, the beats of the
// first bank, the instrument names. A kit slot shows its ten instruments and samples.
#ifndef __FILESCREENS_H
#define __FILESCREENS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GeneratorScreens.h"  // GsFont, GsAlign, the colours and the lane area

const uint8_t kFsSlots = 100;
const uint8_t kFsLanes = 10;
const uint8_t kFsThumbCols = 105;  // the first bank, the whole song across (4 px each in the card)
const uint8_t kFsNameSize = 13;

enum FsState : uint8_t { FS_EMPTY = 0, FS_USED = 1, FS_BAD = 2 };  // BAD: not a readable slot file (cut short, newer firmware)
enum FsMode : uint8_t { FS_LOAD = 0, FS_SAVE = 1, FS_CLEAR = 2 };
enum FsKind : uint8_t { FS_SONG = 0, FS_KIT = 1 };

// What one slot shows.
struct FsSummary {
    bool valid = false;                      // there is something to show (a used slot, or the song in memory)
    uint16_t tempo = 0;                      // BPM
    uint8_t measure = 0, bar = 0;            // MEASURE and BAR of the file
    uint8_t lane[kFsLanes][kFsThumbCols];    // songs: 0 = no beat here, else the level 1..100
    char inst[kFsLanes][kFsNameSize];        // the ten instruments' short names
    char sample[kFsLanes][kFsNameSize];      // kits: the ten samples' short names
    FsSummary() {
        memset(lane, 0, sizeof lane);
        memset(inst, 0, sizeof inst);
        memset(sample, 0, sizeof sample);
    }
};

struct FsView {
    FsKind kind = FS_SONG;
    FsMode mode = FS_LOAD;
    uint8_t selected = 0;      // 0..99
    int16_t current = -1;      // the slot last loaded or saved, -1 = none
    uint8_t state[kFsSlots];   // FsState per slot
    FsSummary sel;             // the selected slot
    FsSummary now;             // the song or kit in memory (SAVE)
    FsView() { memset(state, 0, sizeof state); }
};

inline uint8_t fsUsedCount(const FsView& v_) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < kFsSlots; i++) n = static_cast<uint8_t>(n + (v_.state[i] == FS_USED ? 1 : 0));
    return n;
}

const uint16_t kFsTitleX = 40, kFsTitleY = 134;
const uint16_t kFsRightEdge = 920;

// The words of the screen. The font has no lowercase, '=' or parentheses.
inline const char* fsModeName(FsMode m_) { return (m_ == FS_LOAD) ? "LOAD" : ((m_ == FS_SAVE) ? "SAVE" : "CLEAR"); }
inline const char* fsKindName(FsKind k_) { return (k_ == FS_SONG) ? "SONG" : "DRUMKIT"; }

template <class G>
inline void fsTitle(G& g, const FsView& v_) {
    char line[64];
    snprintf(line, sizeof line, "%s %s", fsModeName(v_.mode), fsKindName(v_.kind));
    g.setFont(GS_FONT_10x14);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsCyan);
    gsDraw(g, line, kFsTitleX, kFsTitleY);
    const unsigned used = fsUsedCount(v_);
    snprintf(line, sizeof line, "%u OF %u SLOTS USED   %u FREE", used, static_cast<unsigned>(kFsSlots), static_cast<unsigned>(kFsSlots - used));
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_RIGHT);
    g.setForeColor(kGsWhite);
    g.drawText(line, static_cast<uint16_t>(strlen(line)), kFsRightEdge, kFsTitleY + 3);
    g.setAlignment(GS_LEFT);
}

// A thumbnail of a song: the ten lanes, the whole first bank across; marks as tall as their level.
// x_, y_ = top left; the width is cols_ * colW_, each lane laneH_ px with gapH_ between.
template <class G>
inline void fsThumb(G& g, const FsSummary& s_, uint16_t x_, uint16_t y_, uint16_t colW_, uint16_t laneH_, uint16_t gapH_, uint8_t cols_, uint16_t frame_) {
    g.setForeColor(frame_);
    g.drawRect(static_cast<uint16_t>(x_ - 2), static_cast<uint16_t>(y_ - 2), static_cast<uint32_t>(cols_ * colW_ + 4), static_cast<uint32_t>(kFsLanes * (laneH_ + gapH_) - gapH_ + 4));
    for (uint8_t l = 0; l < kFsLanes; l++) {
        const uint16_t top = static_cast<uint16_t>(y_ + l * (laneH_ + gapH_));
        g.setForeColor(kGsGray25);
        g.drawPixel(x_, static_cast<uint16_t>(top + laneH_ - 1));
        g.setForeColor((l % 2 == 0) ? kGsWhite : kGsGray75);
        for (uint8_t c = 0; c < cols_; c++) {
            const uint8_t level = s_.lane[l][c];
            if (level == 0) continue;
            uint16_t h = static_cast<uint16_t>((level * laneH_ + 99) / 100);
            if (h < 1) h = 1;
            if (h > laneH_) h = laneH_;
            const uint16_t w = (colW_ > 1) ? static_cast<uint16_t>(colW_ - 1) : static_cast<uint16_t>(1);
            g.fillRect(static_cast<uint16_t>(x_ + c * colW_), static_cast<uint16_t>(top + laneH_ - h), w, h);
        }
    }
}

/* GRID: all slots at a glance and a card for the selected one ---------------------------*/

const uint16_t kFsGridX = 40, kFsGridY = 162, kFsCellW = 40, kFsCellH = 24, kFsCellGapX = 3, kFsCellGapY = 3;  // 10 x 10 cells: x 40..466, y 162..428
const uint16_t kFsCardX = 500, kFsCardW = 420;

template <class G>
inline void fsGridCell(G& g, const FsView& v_, uint8_t slot_) {
    const uint16_t x = static_cast<uint16_t>(kFsGridX + (slot_ % 10) * (kFsCellW + kFsCellGapX));
    const uint16_t y = static_cast<uint16_t>(kFsGridY + (slot_ / 10) * (kFsCellH + kFsCellGapY));
    const bool selected = (slot_ == v_.selected);
    const uint8_t state = v_.state[slot_];
    // The cell: filled for a used slot, an outline for an empty one, red for an unreadable one.
    g.setBackColor(kGsBlack);
    g.clearRect(static_cast<uint16_t>(x - 2), static_cast<uint16_t>(y - 2), kFsCellW + 4, kFsCellH + 4);  // the cell and the gap around it, where the cursor's frame is
    uint16_t number = kGsGray50;
    if (state == FS_USED) {
        g.setForeColor(kGsCyanDim);
        g.fillRect(x, y, kFsCellW, kFsCellH);
        number = kGsWhite;
    } else if (state == FS_BAD) {
        g.setForeColor(0xF800);
        g.drawRect(x, y, kFsCellW, kFsCellH);
        g.drawLine(x, y, static_cast<uint16_t>(x + kFsCellW - 1), static_cast<uint16_t>(y + kFsCellH - 1));
        number = 0xF800;
    } else {
        g.setForeColor(kGsGray25);
        g.drawRect(x, y, kFsCellW, kFsCellH);
    }
    char num[8];
    snprintf(num, sizeof num, "%03u", static_cast<unsigned>(slot_ + 1));
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(number);
    g.setBackColor((state == FS_USED) ? kGsCyanDim : kGsBlack);
    gsDraw(g, num, static_cast<uint16_t>(x + 9), static_cast<uint16_t>(y + 8));
    g.setBackColor(kGsBlack);
    if (static_cast<int16_t>(slot_) == v_.current) {  // the slot last loaded or saved: a magenta corner
        g.setForeColor(kGsMagenta);
        g.fillRect(static_cast<uint16_t>(x + kFsCellW - 6), y, 6, 6);
    }
    if (selected) {  // the cursor: a yellow frame around the cell
        g.setForeColor(kGsYellow);
        g.drawRect(static_cast<uint16_t>(x - 2), static_cast<uint16_t>(y - 2), kFsCellW + 4, kFsCellH + 4);
        g.drawRect(static_cast<uint16_t>(x - 1), static_cast<uint16_t>(y - 1), kFsCellW + 2, kFsCellH + 2);
    }
}

// The card of a song slot: its tempo and length, thumbnail and instruments.
template <class G>
inline void fsSongCard(G& g, const FsSummary& s_, uint16_t x_, uint16_t y_, uint16_t colW_, uint8_t cols_, bool withInstruments_) {
    char line[64];
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsWhite);
    snprintf(line, sizeof line, "%u BPM   MEASURE %u   BAR %u", static_cast<unsigned>(s_.tempo), static_cast<unsigned>(s_.measure), static_cast<unsigned>(s_.bar));
    gsDraw(g, line, x_, y_);
    fsThumb(g, s_, x_ + 2, y_ + 20, colW_, 9, 1, cols_, kGsGray50);
    if (!withInstruments_) return;
    g.setFont(GS_FONT_05x07);
    for (uint8_t l = 0; l < kFsLanes; l++) {
        const uint16_t cx = static_cast<uint16_t>(x_ + (l / 5) * 210), cy = static_cast<uint16_t>(y_ + 20 + 108 + 6 + (l % 5) * 11);
        g.setForeColor((l % 2 == 0) ? kGsWhite : kGsGray75);
        snprintf(line, sizeof line, "%02u %s", static_cast<unsigned>(l + 1), s_.inst[l]);
        gsDraw(g, line, cx, cy);
    }
}

// The card of a kit slot: the ten instruments with their samples.
template <class G>
inline void fsKitCard(G& g, const FsSummary& s_, uint16_t x_, uint16_t y_) {
    char line[64];
    g.setFont(GS_FONT_05x07);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsGray75);
    gsDraw(g, "LAYER  INSTRUMENT     SAMPLE", x_, y_);
    for (uint8_t l = 0; l < kFsLanes; l++) {
        const uint16_t y = static_cast<uint16_t>(y_ + 16 + l * 15);
        g.setForeColor((l % 2 == 0) ? kGsWhite : kGsGray75);
        snprintf(line, sizeof line, "%02u     %-12.12s  %-12.12s", static_cast<unsigned>(l + 1), s_.inst[l], s_.sample[l]);
        gsDraw(g, line, x_, y);
    }
}

// The line that says what ADD will do.
template <class G>
inline void fsBanner(G& g, uint16_t x_, uint16_t y_, uint16_t w_, uint16_t color_, const char* line1_, const char* line2_) {
    g.setForeColor(color_);
    g.drawRect(x_, y_, w_, 32);
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_LEFT);
    gsDraw(g, line1_, static_cast<uint16_t>(x_ + 8), static_cast<uint16_t>(y_ + 5));
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray75);
    gsDraw(g, line2_, static_cast<uint16_t>(x_ + 8), static_cast<uint16_t>(y_ + 19));
}

// The right side: the selected slot's number and state, its card, and the banner. Clears its own area.
template <class G>
inline void fsDrawCard(G& g, const FsView& v_) {
    g.setBackColor(kGsBlack);
    g.clearRect(kFsCardX - 4, kFsGridY - 2, 436, 432 - (kFsGridY - 2));  // x 496..931: the second thumbnail's frame reaches 926
    char line[64];
    const bool used = (v_.state[v_.selected] == FS_USED);
    g.setFont(GS_FONT_10x14);
    g.setAlignment(GS_LEFT);
    g.setForeColor(kGsYellow);
    snprintf(line, sizeof line, "FILE %03u", static_cast<unsigned>(v_.selected + 1));
    if (v_.kind == FS_KIT) snprintf(line, sizeof line, "DRUMKIT %03u", static_cast<unsigned>(v_.selected + 1));
    gsDraw(g, line, kFsCardX, kFsGridY);
    const char* stateText = (v_.state[v_.selected] == FS_USED) ? "USED" : ((v_.state[v_.selected] == FS_BAD) ? "NOT READABLE" : "EMPTY");
    g.setFont(GS_FONT_07x09);
    g.setAlignment(GS_RIGHT);
    g.setForeColor((v_.state[v_.selected] == FS_BAD) ? static_cast<uint16_t>(0xF800) : (used ? kGsCyan : kGsGray50));
    g.drawText(stateText, static_cast<uint16_t>(strlen(stateText)), kFsRightEdge, kFsGridY + 3);
    g.setAlignment(GS_LEFT);

    const uint16_t cardY = kFsGridY + 28;
    if (v_.kind == FS_KIT) {
        if (used && v_.sel.valid) fsKitCard(g, v_.sel, kFsCardX, cardY);
    } else if (v_.mode == FS_SAVE) {
        // Two thumbnails side by side: what is in memory, and what the slot holds now.
        g.setFont(GS_FONT_05x07);
        g.setForeColor(kGsGray75);
        gsDraw(g, "IN MEMORY", kFsCardX, cardY);
        gsDraw(g, used ? "IN THE SLOT NOW" : ((v_.state[v_.selected] == FS_BAD) ? "THE SLOT: NOT READABLE" : "THE SLOT IS FREE"), kFsCardX + 212, cardY);
        fsThumb(g, v_.now, kFsCardX + 2, cardY + 14, 2, 9, 1, kFsThumbCols, kGsGray50);
        if (used && v_.sel.valid) fsThumb(g, v_.sel, kFsCardX + 214, cardY + 14, 2, 9, 1, kFsThumbCols, kGsGray50);
        g.setFont(GS_FONT_05x07);
        g.setForeColor(kGsWhite);
        snprintf(line, sizeof line, "%u BPM  M %u  B %u", static_cast<unsigned>(v_.now.tempo), static_cast<unsigned>(v_.now.measure), static_cast<unsigned>(v_.now.bar));
        gsDraw(g, line, kFsCardX, cardY + 14 + 108);
        if (used && v_.sel.valid) {
            snprintf(line, sizeof line, "%u BPM  M %u  B %u", static_cast<unsigned>(v_.sel.tempo), static_cast<unsigned>(v_.sel.measure), static_cast<unsigned>(v_.sel.bar));
            gsDraw(g, line, kFsCardX + 212, cardY + 14 + 108);
        }
    } else if (used && v_.sel.valid) {
        fsSongCard(g, v_.sel, kFsCardX, cardY, 4, kFsThumbCols, true);
    }

    // What ADD will do (ADD opens the confirmation alert, ADD again confirms, CLEAR cancels).
    const uint16_t by = 396;
    const bool song = (v_.kind == FS_SONG);
    if (v_.mode == FS_LOAD) {
        if (used) {
            fsBanner(g, kFsCardX, by, kFsCardW, kGsCyan, song ? "ADD LOADS THIS SONG, THEN ADD AGAIN" : "ADD LOADS THIS KIT, THEN ADD AGAIN",
                     song ? "UNSAVED WORK IS AUTOSAVED FIRST IF AUTOSAVE IS ON" : "THE KIT REPLACES THE INSTRUMENTS OF THE SONG");
        } else if (v_.state[v_.selected] == FS_BAD) {
            fsBanner(g, kFsCardX, by, kFsCardW, 0xF800, "THIS FILE CAN NOT BE LOADED", "CUT SHORT, DAMAGED OR FROM NEWER FIRMWARE");
        } else {
            fsBanner(g, kFsCardX, by, kFsCardW, kGsGray50, "NOTHING TO LOAD", "THIS SLOT IS EMPTY");
        }
    } else if (v_.mode == FS_SAVE) {
        if (used) {
            fsBanner(g, kFsCardX, by, kFsCardW, kGsYellow, "ADD REPLACES THIS SLOT, THEN ADD AGAIN", "THE OLD FILE STAYS UNTIL THE NEW ONE IS CHECKED");
        } else if (v_.state[v_.selected] == FS_BAD) {
            fsBanner(g, kFsCardX, by, kFsCardW, kGsYellow, "ADD REPLACES THIS FILE, THEN ADD AGAIN", "IT CAN NOT BE READ. THE NEW FILE IS CHECKED AFTER WRITING");
        } else {
            fsBanner(g, kFsCardX, by, kFsCardW, kGsGreen, "ADD SAVES HERE, THEN ADD AGAIN", "THE NEW FILE IS CHECKED AFTER WRITING");
        }
    } else {
        if (used || (v_.state[v_.selected] == FS_BAD)) {
            fsBanner(g, kFsCardX, by, kFsCardW, 0xF800, "ADD CLEARS THIS SLOT, THEN ADD AGAIN", "THERE IS NO UNDO");
        } else {
            fsBanner(g, kFsCardX, by, kFsCardW, kGsGray50, "THIS SLOT IS ALREADY EMPTY", "NOTHING TO CLEAR");
        }
    }
}

template <class G>
inline void fsDrawGrid(G& g, const FsView& v_) {
    g.setBackColor(kGsBlack);
    g.clearRect(kGsAreaX, kGsAreaY, kGsAreaW, kGsAreaH);
    fsTitle(g, v_);
    for (uint8_t i = 0; i < kFsSlots; i++) {
        if (i != v_.selected) fsGridCell(g, v_, i);
    }
    fsGridCell(g, v_, v_.selected);  // last: a neighbour's clear would cut the cursor's frame
    fsDrawCard(g, v_);
    g.setFont(GS_FONT_05x07);
    g.setForeColor(kGsGray50);
    g.setAlignment(GS_LEFT);
    gsDraw(g, "UP AND DOWN: NEXT SLOT   HOLD: TEN AT A TIME   LEFT AND RIGHT: NEW LOAD SAVE CLEAR", kFsGridX, 433);
}

// The cursor moved from oldSelected_ to v_.selected and nothing else changed: two cells and the card.
template <class G>
inline void fsMoveSelection(G& g, const FsView& v_, uint8_t oldSelected_) {
    if (oldSelected_ < kFsSlots) fsGridCell(g, v_, oldSelected_);
    fsGridCell(g, v_, v_.selected);
    fsDrawCard(g, v_);
}

#endif
