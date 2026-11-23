// Host-side test of the LFO overview: the numbers (LfoView.h) and the drawing (LfoScreens.h) on a pixel buffer
// with the firmware's own font tables: every pixel inside the lane area, only characters the font has, no
// overlapping texts, the wave and the phase dot where the numbers put them, and the pictures with GS_DUMP_DIR.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "LfoScreens.h"
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

static float gTable[5][3200];
static void buildTables() {
    for (int i = 0; i < 3200; i++) {
        const double x = i / 3200.0;
        gTable[0][i] = static_cast<float>(0.5 + 0.5 * std::sin(2 * M_PI * x));
        gTable[1][i] = static_cast<float>(1.0 - std::fabs(2.0 * x - 1.0));
        gTable[2][i] = static_cast<float>(x);
        gTable[3][i] = x < 0.5 ? 1.0f : 0.0f;
        gTable[4][i] = static_cast<float>(0.5 + 0.5 * std::sin(std::floor(x * 8) * 12.9898));
    }
}

int main() {
    buildTables();

    // What each LFO moves: slot numbers from 1, 0 is none.
    uint8_t layerLfo[10][kLvLayerFields];
    uint8_t filterLfo[2][kLvFilterFields];
    memset(layerLfo, 0, sizeof layerLfo);
    memset(filterLfo, 0, sizeof filterLfo);
    layerLfo[0][0] = 3;   // layer 1 LEVEL
    layerLfo[2][1] = 3;   // layer 3 PITCH
    layerLfo[2][7] = 3;   // layer 3 PROBABILITY
    layerLfo[6][4] = 3;   // layer 7 START
    layerLfo[0][1] = 1;
    filterLfo[0][0] = 3;  // filter 1 FREQ
    filterLfo[1][1] = 1;  // filter 2 RESON
    LvUse uses[kLvMaxUses];
    CHECK(lvCollect(3, layerLfo, 10, filterLfo, uses, kLvMaxUses) == 5, "LFO 3 moves five things");
    CHECK(uses[0].kind == 0 && uses[0].who == 0 && uses[0].field == 0 && uses[1].who == 2 && uses[1].field == 1 && uses[2].who == 2 && uses[2].field == 7 && uses[3].who == 6 && uses[3].field == 4 && uses[4].kind == 1 && uses[4].who == 0 && uses[4].field == 0, "in the order layers, fields, filters");
    CHECK(lvCollect(1, layerLfo, 10, filterLfo, uses, kLvMaxUses) == 2 && uses[0].field == 1 && uses[1].kind == 1 && uses[1].who == 1 && uses[1].field == 1, "LFO 1");
    CHECK(lvCollect(2, layerLfo, 10, filterLfo, uses, kLvMaxUses) == 0 && lvCollect(0, layerLfo, 10, filterLfo, uses, kLvMaxUses) == 0, "an unused LFO and slot 0");
    LvUse few[2];
    CHECK(lvCollect(3, layerLfo, 10, filterLfo, few, 2) == 5, "the count is the true one when the output is short");
    CHECK(strcmp(lvUseWhat(LvUse{0, 2, 7}), "PROBABILITY") == 0 && strcmp(lvUseWhat(LvUse{1, 1, 1}), "RESON") == 0 && strcmp(lvUseWhat(LvUse{1, 0, 0}), "FREQ") == 0, "field names");
    char who[24];
    lvUseWho(LvUse{0, 2, 1}, "HAT CLOSED", who, sizeof who);
    CHECK(strcmp(who, "L3 HAT CLOSED") == 0, "who: %s", who);
    lvUseWho(LvUse{1, 1, 0}, "", who, sizeof who);
    CHECK(strcmp(who, "F2") == 0, "who: %s", who);
    // Every layer field of every layer on one LFO: the true count (84).
    uint8_t all[10][kLvLayerFields];
    memset(all, 9, sizeof all);
    uint8_t allF[2][kLvFilterFields];
    memset(allF, 9, sizeof allF);
    CHECK(lvCollect(9, all, 10, allF, uses, kLvMaxUses) == kLvMaxUses && kLvMaxUses == 84, "everything on one LFO: %u", kLvMaxUses);

    // The wave heights: the middle of the table is the middle line, depth scales, invert flips.
    CHECK(lvWaveHeight(0.5f, false, 1.0f, 60) == 0 && lvWaveHeight(1.0f, false, 1.0f, 60) == 60 && lvWaveHeight(0.0f, false, 1.0f, 60) == -60, "full depth");
    CHECK(lvWaveHeight(1.0f, false, 0.5f, 60) == 30 && lvWaveHeight(1.0f, false, 0.0f, 60) == 0, "depth scales");
    CHECK(lvWaveHeight(1.0f, true, 1.0f, 60) == -60 && lvWaveHeight(0.25f, true, 1.0f, 60) == 30, "invert flips");
    CHECK(lvWaveHeight(7.0f, false, 1.0f, 60) == 60 && lvWaveHeight(NAN, false, 1.0f, 60) == -60 && lvWaveHeight(1.0f, false, 5.0f, 60) == 60, "out-of-range values are held");
    CHECK(lvTableAt(gTable[2], 3200, 0.0f) == 0.0f && lvTableAt(gTable[2], 3200, 0.5f) == 0.5f && lvTableAt(gTable[2], 3200, 1.0f) == gTable[2][0] && lvTableAt(nullptr, 3200, 0.3f) == 0.5f, "table lookup wraps");
    CHECK(lvDotColumn(0.0f) == 0 && lvDotColumn(0.5f) == kLvPlotW / kLvPlotCycles / 2 && lvDotColumn(1.0f) == 0 && lvDotColumn(0.999f) < kLvPlotW / kLvPlotCycles, "dot column over the first cycle");

    // Geometry.
    CHECK(lvRowY(9) + kLvRowH <= kGsAreaY + kGsAreaH && kLvRowX >= kGsAreaX && kLvRowX + kLvRowW < kLvPlotX, "rows fit and leave the plot free");
    CHECK(kLvPlotX + kLvPlotW + 1 <= kGsAreaX + kGsAreaW && kLvPlotY + kLvPlotH < kLvCaptionY && kLvCaptionY + 7 < kLvUsedLabelY, "plot and caption inside");
    CHECK(kLvUsedY + 4 * kLvUsedStep + kLvUsedH <= kGsAreaY + kGsAreaH && kLvUsedX + kLvUsedColStep + kLvUsedW <= kGsAreaX + kGsAreaW && kLvUsedW < kLvUsedColStep, "the used list fits in two columns of five");
    CHECK(kLvUsedY + 5 * kLvUsedStep + 7 <= kGsAreaY + kGsAreaH, "the MORE line fits");
    CHECK(kLvWaveX + kLvWaveW < kLvRowX + kLvRowW - 40, "a row's wave leaves room for the count");

    // The screens: inside the area, legal text, no overlap, for LFOs with no use, five uses and many.
    char layerNames[10][16] = {"KICK 808", "SNARE TIGHT", "HAT CLOSED", "HAT OPEN", "CLAP", "RIM", "TOM LOW", "SHAKER", "COWBELL", "PERC FX"};
    struct Case { const char* name; uint8_t slot, type, depth; bool invert, active; };
    const Case cases[] = {{"saw_five", 3, 2, 60, false, true}, {"sine_inv", 1, 0, 100, true, true}, {"off", 4, 1, 100, false, false}, {"random_unused", 2, 4, 35, false, true}, {"all_uses", 9, 3, 100, false, true}};
    for (const Case& k : cases) {
        FakeLcd lcd;
        lcd.dirtyArea();
        const uint8_t(*lay)[kLvLayerFields] = (k.slot == 9) ? all : layerLfo;
        const uint8_t(*fil)[kLvFilterFields] = (k.slot == 9) ? allF : filterLfo;
        for (uint8_t n = 0; n < kLvLfos; n++) {
            const uint16_t u = lvCollect(static_cast<uint8_t>(n + 1), lay, 10, fil, uses, kLvMaxUses);
            lvDrawRow(lcd, n, n + 1 == k.slot, (n == k.slot - 1) ? k.active : true, u, gTable[n % 5], 3200, (n == k.slot - 1) ? k.invert : false, (n == k.slot - 1) ? k.depth : 100);
        }
        LvInfo in;
        memset(&in, 0, sizeof in);
        in.number = k.slot;
        in.active = k.active;
        snprintf(in.type, sizeof in.type, "%s", k.type == 0 ? "SINE" : k.type == 1 ? "TRIANGLE" : k.type == 2 ? "SAWTOOTH" : k.type == 3 ? "SQUARE" : "RANDOM");
        snprintf(in.rate, sizeof in.rate, "1/4");
        snprintf(in.phase, sizeof in.phase, "045");
        in.depth = k.depth;
        in.invert = k.invert;
        lvDrawTitle(lcd, in);
        lvDrawPlotFrame(lcd);
        lvDrawPlotColumns(lcd, gTable[k.type], 3200, k.active, k.invert, k.depth, 0, kLvPlotW - 1);
        lvDrawDot(lcd, gTable[k.type], 3200, k.invert, k.depth, 120);
        const uint16_t n = lvCollect(k.slot, lay, 10, fil, uses, kLvMaxUses);
        lvDrawUsed(lcd, uses, layerNames, n, k.active);
        checkScreen(lcd, k.name);
        CHECK(lcd.ops > 100, "%s: drawn", k.name);
        char name[48];
        snprintf(name, sizeof name, "lfo_%s", k.name);
        lcd.dump(name);
    }

    // The wave is where the numbers put it: a saw at full depth runs from the top left to the bottom right of the plot.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        lvDrawPlotColumns(lcd, gTable[2], 3200, true, false, 100, 0, kLvPlotW - 1);
        const uint16_t mid = kLvPlotY + kLvPlotH / 2;
        const int16_t amp = kLvPlotH / 2 - 6;
        CHECK(pixelAt(lcd, kLvPlotX + 1, mid - lvWaveHeight(gTable[2][8], false, 1.0f, amp)) == kGsCyan, "the wave starts at the middle of the saw's range: low");
        {
            const uint16_t col = kLvPlotW / 2 - 2;  // the last columns of the first cycle: the saw is near its top
            const float frac = static_cast<float>(col * kLvPlotCycles % kLvPlotW) / kLvPlotW;
            const int16_t h = lvWaveHeight(lvTableAt(gTable[2], 3200, frac), false, 1.0f, amp);
            CHECK(h > amp - 4 && pixelAt(lcd, kLvPlotX + col, mid - h) == kGsCyan, "the saw is near the top at the end of a cycle (%d of %d)", h, amp);
        }
        // The second cycle repeats the first.
        bool same = true;
        for (uint16_t x = 0; x < kLvPlotW / 2; x += 7) {
            for (uint16_t y = kLvPlotY; y < kLvPlotY + kLvPlotH; y++) {
                const bool a = pixelAt(lcd, kLvPlotX + x, y) == kGsCyan, b = pixelAt(lcd, kLvPlotX + kLvPlotW / 2 + x, y) == kGsCyan;
                if (a != b) same = false;
            }
        }
        CHECK(same, "two identical cycles");
    }
    // The dot: yellow at its column, and redrawing the columns around it brings the wave back.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        lvDrawPlotColumns(lcd, gTable[0], 3200, true, false, 100, 0, kLvPlotW - 1);
        lvDrawDot(lcd, gTable[0], 3200, false, 100, 200);
        CHECK(pixelAt(lcd, kLvPlotX + 200, kLvPlotY + 3) == kGsGray50, "the cursor line");
        bool yellow = false;
        for (int y = kLvPlotY; y < kLvPlotY + kLvPlotH; y++) yellow = yellow || (pixelAt(lcd, kLvPlotX + 200, y) == kGsYellow);
        CHECK(yellow, "the dot");
        lvDrawPlotColumns(lcd, gTable[0], 3200, true, false, 100, 196, 204);
        bool yellowLeft = false, lineLeft = false;
        for (int y = kLvPlotY; y < kLvPlotY + kLvPlotH; y++) {
            for (int x = kLvPlotX + 196; x <= kLvPlotX + 204; x++) {
                yellowLeft = yellowLeft || (pixelAt(lcd, x, y) == kGsYellow);
                lineLeft = lineLeft || (x == kLvPlotX + 200 && pixelAt(lcd, x, kLvPlotY + 3) == kGsGray50);
            }
        }
        CHECK(!yellowLeft && !lineLeft, "redrawing the columns erases the dot and its line");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
