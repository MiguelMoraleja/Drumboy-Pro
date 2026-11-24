// Host-side test of the Performance page's numbers (PerformView.h) and of its drawing (PerformScreens.h) on a
// pixel buffer with the firmware's own font tables: every pixel inside the lane area, only characters the font has,
// no overlapping texts, and the pictures when GS_DUMP_DIR is set.

#include <cstdio>
#include <cstring>

#include "PerformScreens.h"
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

static void checkScreen(const FakeLcd& lcd, const char* what) {
    CHECK(lcd.offscreen == 0, "%s: %d pixels off the screen", what, lcd.offscreen);
    CHECK(lcd.outsideArea == 0, "%s: %d pixels outside the lane area", what, lcd.outsideArea);
    CHECK(lcd.unsupported == 0, "%s: %d characters the font has no glyph for: %s", what, lcd.unsupported, lcd.badChars.c_str());
    for (size_t i = 0; i < lcd.texts.size(); i++) {
        const TextBox& t = lcd.texts[i];
        CHECK((t.x >= kGsAreaX) && (t.x + t.w <= kGsAreaX + kGsAreaW) && (t.y >= kGsAreaY) && (t.y + t.h <= kGsAreaY + kGsAreaH), "%s: text '%s' leaves the area", what, t.text.c_str());
        for (size_t j = i + 1; j < lcd.texts.size(); j++) CHECK(!fakeLcdOverlap(t, lcd.texts[j]), "%s: texts '%s' and '%s' overlap", what, t.text.c_str(), lcd.texts[j].text.c_str());
    }
}

static uint16_t pixelAt(const FakeLcd& lcd, int x, int y) { return lcd.pixels[y * FakeLcd::W + x]; }

static PfChain demoChain(bool active) {
    PfChain c;
    memset(&c, 0, sizeof c);
    c.count = 4;
    const uint8_t bank[4] = {0, 1, 0, 2}, rep[4] = {4, 2, 4, 2};
    for (int i = 0; i < 4; i++) {
        c.bank[i] = bank[i];
        c.repeats[i] = rep[i];
    }
    c.active = active;
    c.loop = true;
    c.step = 1;
    c.loopsDone = 0;
    return c;
}

static void drawAll(FakeLcd& lcd, const PfChain& chain, uint16_t soloMask, uint16_t muteMask, uint16_t tempo) {
    lcd.dirtyArea();
    pfDrawFrame(lcd);
    pfDrawBarBeat(lcd, 3, 2, 4, 4, true);
    pfDrawTempo(lcd, tempo);
    pfDrawChain(lcd, chain);
    static const char* const kName[10] = {"KICK 808", "SNARE TIGHT", "HAT CLOSED", "HAT OPEN", "CLAP", "RIM", "TOM LOW", "SHAKER", "COWBELL", "PERC FX"};
    for (uint8_t i = 0; i < kPfLayers; i++) {
        char num[4];
        snprintf(num, sizeof num, "%02u", i + 1u);
        char name[10];
        snprintf(name, sizeof name, "%.9s", kName[i]);
        const PfState st = pfState((muteMask & (1u << i)) != 0, soloMask, i);
        pfDrawTile(lcd, i, num, name, st);
        pfDrawPattern(lcd, i, static_cast<uint16_t>(0x1111u << (i % 4)), static_cast<int8_t>(6), st);
        pfDrawMeter(lcd, i, static_cast<uint8_t>((i * 3) % 14), st == PF_ON || st == PF_SOLO);
    }
}

int main() {
    // The states: MUTE wins, a solo silences the others.
    CHECK(pfState(false, 0, 3) == PF_ON && pfState(true, 0, 3) == PF_MUTE, "on and mute");
    CHECK(pfState(false, 1u << 3, 3) == PF_SOLO && pfState(false, 1u << 3, 4) == PF_SILENT, "solo and silenced by a solo");
    CHECK(pfState(true, 1u << 3, 3) == PF_MUTE && pfState(true, 1u << 3, 4) == PF_MUTE, "a muted layer shows MUTE even in a solo");
    CHECK(strcmp(pfStateName(PF_MUTE), "MUTE") == 0 && strcmp(pfStateName(PF_SILENT), "SOLO OFF") == 0, "names");

    // The pattern row and the playhead.
    const uint16_t starts[4] = {0, 3200, 6400, 9600};
    CHECK(pfPattern(starts, 4, 12800) == ((1u << 0) | (1u << 4) | (1u << 8) | (1u << 12)), "four hits a beat apart in a 4 beat loop of 16 cells: %04X", pfPattern(starts, 4, 12800));
    const uint16_t late[2] = {12799, 12800};
    CHECK(pfPattern(late, 2, 12800) == (1u << 15), "the last interval is cell 15; a start past the loop is dropped");
    CHECK(pfPattern(starts, 4, 0) == 0 && pfPattern(starts, 0, 12800) == 0, "no loop, no beats");
    CHECK(pfPlayCell(0, 12800) == 0 && pfPlayCell(12799, 12800) == 15 && pfPlayCell(800, 12800) == 1 && pfPlayCell(13600, 12800) == 1 && pfPlayCell(5, 0) == 0, "playhead cells");

    // Bar and beat: 4 beats of 800 intervals a bar, 4 bars.
    uint8_t bar, beat;
    pfBarBeat(0, 4, 800, 4, bar, beat);
    CHECK(bar == 1 && beat == 1, "start");
    pfBarBeat(2 * 4 * 800 + 800 + 5, 4, 800, 4, bar, beat);
    CHECK(bar == 3 && beat == 2, "bar 3 beat 2: %u %u", bar, beat);
    pfBarBeat(4 * 4 * 800 - 1, 4, 800, 4, bar, beat);
    CHECK(bar == 4 && beat == 4, "the last interval of the loop is bar 4 beat 4");
    pfBarBeat(60000, 4, 800, 4, bar, beat);
    CHECK(bar == 4, "a position past the loop stays on the last bar");
    pfBarBeat(100, 0, 800, 4, bar, beat);
    CHECK(bar == 1 && beat == 1, "no measure: 1.1");

    // The level bar.
    CHECK(pfMeterLit(0, 20) == 0 && pfMeterLit(20, 20) == kPfMeterSegments && pfMeterLit(100, 20) == kPfMeterSegments && pfMeterLit(1, 20) == 1 && pfMeterLit(5, 0) == 0, "meter segments");
    CHECK(pfSegmentY(0) > pfSegmentY(12) && pfSegmentY(0) + kPfMeterH <= kPfTileY + kPfTileH, "segment 0 is the bottom one and inside the tile");

    // The chain strip.
    PfChain c = demoChain(true);
    CHECK(pfChainTotal(c) == 12 && pfChainUnit(c) == 40, "12 loops at 40 px");
    c.count = 16;
    for (int i = 0; i < 16; i++) c.repeats[i] = 16;
    CHECK(pfChainTotal(c) == 256 && pfChainUnit(c) == 2, "a very long chain: 2 px a loop (%u)", pfChainUnit(c));
    c = demoChain(true);
    CHECK(pfChainNext(c) == 2, "next step");
    c.step = 3;
    CHECK(pfChainNext(c) == 0, "a looping chain goes back to step 1");
    c.loop = false;
    CHECK(pfChainNext(c) == -1, "a stopping chain has no next step");

    // The seven segments.
    CHECK(pfSegments(8) == 0x7F && pfSegments(1) == 0x06 && pfSegments(7) == 0x07, "segment masks");

    // Geometry: ten tiles side by side inside the lane area.
    CHECK(pfTileX(0) >= kGsAreaX && pfTileX(9) + kPfTileW <= kGsAreaX + kGsAreaW, "tiles inside the area");
    CHECK(kPfTileY + kPfTileH <= kGsAreaY + kGsAreaH && kPfTileStep > kPfTileW, "tiles inside the area vertically, with a gap");
    CHECK(kPfPatternY + kPfCellH < kPfHeadY && kPfHeadY + 2 < kPfMeterY && kPfStateY + kPfStateH < kPfPatternY, "tile rows in order");
    CHECK(kPfCells * kPfCellStep <= kPfTileW - 4, "16 cells fit in a tile");

    // Drawing: inside the area, legal text, no overlap, in several situations.
    struct Case { const char* name; bool chain; uint16_t solo, mute, tempo; };
    const Case cases[] = {{"idle", false, 0, 0, 120}, {"mutes", true, 0, (1u << 3) | (1u << 6), 120}, {"solo", true, (1u << 2) | (1u << 4), 0, 87}, {"solo_mute", true, 1u << 2, 1u << 2, 240}, {"all", true, 0x3FF, 0, 40}};
    for (const Case& k : cases) {
        FakeLcd lcd;
        drawAll(lcd, demoChain(k.chain), k.solo, k.mute, k.tempo);
        checkScreen(lcd, k.name);
        CHECK(lcd.ops > 100, "%s: drawn", k.name);
        char name[48];
        snprintf(name, sizeof name, "perform_%s", k.name);
        lcd.dump(name);
    }
    // A very long chain still fits the strip.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        PfChain big = demoChain(true);
        big.count = 16;
        for (int i = 0; i < 16; i++) {
            big.bank[i] = static_cast<uint8_t>(i % 5);
            big.repeats[i] = 16;
        }
        pfDrawChain(lcd, big);
        checkScreen(lcd, "long chain");
    }
    // A MUTE tile is red under its text, a SOLO tile yellow, an ON tile outlined.
    {
        FakeLcd lcd;
        drawAll(lcd, demoChain(true), 0, 1u << 3, 120);
        CHECK(pixelAt(lcd, pfTileX(3) + 8, kPfStateY + 2) == kPfRed, "MUTE box is red: %04X", pixelAt(lcd, pfTileX(3) + 8, kPfStateY + 2));
        CHECK(pixelAt(lcd, pfTileX(0) + 8, kPfStateY + 2) == kGsBlack, "an ON tile has no fill");
        CHECK(pixelAt(lcd, pfTileX(0) + 6, kPfStateY) == kGsGray25, "an ON tile has an outline");
        // The playhead cell is marked, the others are not.
        CHECK(pixelAt(lcd, pfTileX(0) + 4 + 6 * kPfCellStep, kPfHeadY) == kGsYellow && pixelAt(lcd, pfTileX(0) + 4 + 5 * kPfCellStep, kPfHeadY) == kGsBlack, "playhead marker");
        // A lit meter segment is green at the bottom, dark above its level.
        CHECK(pixelAt(lcd, pfTileX(9) + 34, pfSegmentY(0) + 1) != kGsGray25, "the loudest tile's lowest segment is lit");
    }
    // The digits: 88 lights every segment, 11 only the right side.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        pfDrawDigit(lcd, 100, 150, 8, kGsWhite, 0x1082);
        CHECK(pixelAt(lcd, 100 + kPfDigitW / 2, 150 + 1) == kGsWhite && pixelAt(lcd, 100 + 1, 150 + 10) == kGsWhite && pixelAt(lcd, 100 + kPfDigitW - 2, 150 + 10) == kGsWhite && pixelAt(lcd, 100 + 1, 150 + 34) == kGsWhite && pixelAt(lcd, 100 + kPfDigitW - 2, 150 + 34) == kGsWhite && pixelAt(lcd, 100 + kPfDigitW / 2, 150 + kPfDigitH - 2) == kGsWhite && pixelAt(lcd, 100 + kPfDigitW / 2, 150 + kPfDigitH / 2) == kGsWhite, "an 8 lights every segment");
        pfDrawDigit(lcd, 200, 150, 1, kGsWhite, 0x1082);
        CHECK(pixelAt(lcd, 200 + 1, 150 + 10) == 0x1082 && pixelAt(lcd, 200 + kPfDigitW - 2, 150 + 10) == kGsWhite, "a 1 lights only the right side");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
