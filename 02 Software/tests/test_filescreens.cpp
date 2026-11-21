// Host-side test of the load and save screens (FileScreens.h): the same template code the firmware
// will draw with, here on a pixel buffer with the firmware's own font tables. Checks every pixel stays
// inside the lane area, the texts use characters the font has and do not overlap, and writes the
// pictures when GS_DUMP_DIR is set.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "FileScreens.h"
#include "fakelcd.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                \
    do {                                                                \
        ++checks;                                                       \
        if (!(cond)) {                                                  \
            ++failures;                                                 \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                   \
            std::printf("\n");                                          \
        }                                                               \
    } while (0)

// A song as a slot file would hold it: a plausible pattern per lane, repeated over the song.
static const char* kInst[kFsLanes] = {"KICK 808", "SNARE 01", "CLAP 03", "HAT CLOSED", "HAT OPEN", "TOM LOW", "TOM HIGH", "RIDE", "PERC 04", "FX RISER"};
static const char* kSample[kFsLanes] = {"BD_DEEP_02", "SD_TIGHT_1", "CP_WIDE_3", "HH_C_07", "HH_O_02", "TM_LO_01", "TM_HI_01", "RD_BELL_1", "PC_CONGA_4", "FX_SWEEP_9"};
static const char* kPattern[kFsLanes] = {"x...x...x...x...", "....x.......x...", "....x.......x..x", "x.x.x.x.x.x.x.x.", "..............x.", "..........x.....", "............x.x.", "x...............", "..x...x..x......", "................"};

static FsSummary makeSong(unsigned seed, uint16_t tempo, uint8_t measure, uint8_t bar, uint8_t lanesUsed) {
    FsSummary s;
    s.valid = true;
    s.tempo = tempo;
    s.measure = measure;
    s.bar = bar;
    const unsigned steps16 = static_cast<unsigned>(measure) * bar * 4;  // sixteenths in the song
    for (uint8_t l = 0; l < kFsLanes; l++) {
        snprintf(s.inst[l], sizeof s.inst[l], "%.12s", kInst[l]);
        snprintf(s.sample[l], sizeof s.sample[l], "%.12s", kSample[l]);
        if (l >= lanesUsed) continue;
        for (uint8_t c = 0; c < kFsThumbCols; c++) {
            const unsigned sixteenth = (static_cast<unsigned>(c) * steps16) / kFsThumbCols;
            const bool first = (c == 0) || (sixteenth != ((static_cast<unsigned>(c) - 1) * steps16) / kFsThumbCols);  // the first column of a sixteenth
            if (first && (kPattern[(l + seed) % kFsLanes][sixteenth % 16] == 'x')) s.lane[l][c] = static_cast<uint8_t>(55 + ((sixteenth * 7 + l * 13 + seed) % 46));
        }
    }
    return s;
}

static FsView makeView(FsKind kind, FsMode mode, uint8_t selected, int16_t current) {
    FsView v;
    v.kind = kind;
    v.mode = mode;
    v.selected = selected;
    v.current = current;
    for (uint8_t i = 0; i < kFsSlots; i++) {
        const unsigned h = (i * 2654435761u) >> 8;
        v.state[i] = ((i < 31) || (h % 5 == 0)) ? FS_USED : FS_EMPTY;
    }
    v.state[57] = FS_BAD;
    if ((mode == FS_SAVE) && (selected == 62)) v.state[selected] = FS_EMPTY;
    v.sel = makeSong(selected, static_cast<uint16_t>(90 + (selected * 7) % 60), static_cast<uint8_t>(2 + selected % 4 * 2), 4, static_cast<uint8_t>(5 + selected % 6));
    v.now = makeSong(3, 124, 4, 4, 8);
    return v;
}

static void checkScreen(const FakeLcd& lcd, const char* what) {
    CHECK(lcd.offscreen == 0, "%s: %d pixels off the screen", what, lcd.offscreen);
    CHECK(lcd.outsideArea == 0, "%s: %d pixels outside the lane area", what, lcd.outsideArea);
    CHECK(lcd.unsupported == 0, "%s: %d characters the font has no glyph for: %s", what, lcd.unsupported, lcd.badChars.c_str());
    for (size_t i = 0; i < lcd.texts.size(); i++) {
        const TextBox& t = lcd.texts[i];
        CHECK((t.x >= kGsAreaX) && (t.x + t.w <= kGsAreaX + kGsAreaW) && (t.y >= kGsAreaY) && (t.y + t.h <= kGsAreaY + kGsAreaH), "%s: text '%s' at %d,%d %dx%d leaves the area", what, t.text.c_str(), t.x, t.y, t.w, t.h);
        for (size_t j = i + 1; j < lcd.texts.size(); j++) CHECK(!fakeLcdOverlap(t, lcd.texts[j]), "%s: texts '%s' and '%s' overlap", what, t.text.c_str(), lcd.texts[j].text.c_str());
    }
}

int main() {
    // GRID: load, save (a slot with a song, a free slot), clear, an empty and an unreadable slot, the kit.
    struct Case {
        const char* name;
        FsKind kind;
        FsMode mode;
        uint8_t selected;
        int16_t current;
    };
    const Case cases[] = {{"grid_load_song", FS_SONG, FS_LOAD, 12, 17},       {"grid_save_over", FS_SONG, FS_SAVE, 12, 12},     {"grid_save_free", FS_SONG, FS_SAVE, 62, 12},
                          {"grid_clear", FS_SONG, FS_CLEAR, 12, -1},          {"grid_load_empty", FS_SONG, FS_LOAD, 63, 17},    {"grid_load_bad", FS_SONG, FS_LOAD, 57, 17},
                          {"grid_load_kit", FS_KIT, FS_LOAD, 7, 3},           {"grid_save_kit", FS_KIT, FS_SAVE, 7, 3},          {"grid_first", FS_SONG, FS_LOAD, 0, 0},
                          {"grid_last", FS_SONG, FS_LOAD, 99, 99}};
    for (const Case& c : cases) {
        FakeLcd lcd;
        lcd.dirtyArea();
        const FsView v = makeView(c.kind, c.mode, c.selected, c.current);
        fsDrawGrid(lcd, v);
        checkScreen(lcd, c.name);
        lcd.dump(c.name);
    }
    // Every slot as the selected one, every mode and kind: always inside the area, always legal text.
    for (int kind = 0; kind < 2; kind++)
        for (int mode = 0; mode < 3; mode++)
            for (int sel = 0; sel < kFsSlots; sel += 7) {
                FakeLcd lcd;
                lcd.dirtyArea();
                fsDrawGrid(lcd, makeView(static_cast<FsKind>(kind), static_cast<FsMode>(mode), static_cast<uint8_t>(sel), static_cast<int16_t>(sel / 2)));
                checkScreen(lcd, "grid sweep");
            }
    // LIST: pages 0, 3 and the last, selected row at the top, the middle and the bottom.
    for (uint8_t first : {0, 30, 90}) {  // the first page is the busiest
        for (uint8_t sel : {static_cast<uint8_t>(first), static_cast<uint8_t>(first + 4), static_cast<uint8_t>(first + 9)}) {
            FakeLcd lcd;
            lcd.dirtyArea();
            const FsView v = makeView(FS_SONG, FS_LOAD, sel, 17);
            FsSummary summaries[10];
            for (uint8_t i = 0; i < 10; i++) summaries[i] = makeSong(first + i, static_cast<uint16_t>(90 + ((first + i) * 7) % 60), static_cast<uint8_t>(2 + (first + i) % 4 * 2), 4, static_cast<uint8_t>(5 + (first + i) % 6));
            fsDrawList(lcd, v, first, summaries);
            char name[40];
            snprintf(name, sizeof name, "list_page%u_sel%u", first, sel);
            checkScreen(lcd, name);
            if ((first == 0) && (sel == 4)) lcd.dump("list_load_song");
        }
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
