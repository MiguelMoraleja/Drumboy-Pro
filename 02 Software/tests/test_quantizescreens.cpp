// Host-side test of the quantize screen (QuantizeScreens.h): the same template code the firmware draws with,
// here on a pixel buffer with the firmware's own font tables. Checks every pixel stays inside the lane area,
// the texts use characters the font has and do not overlap, that the points are drawn where QuantizeView.h
// puts them, and writes the pictures when GS_DUMP_DIR is set.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "QuantizeScreens.h"
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
        CHECK((t.x >= kGsAreaX) && (t.x + t.w <= kGsAreaX + kGsAreaW) && (t.y >= kGsAreaY) && (t.y + t.h <= kGsAreaY + kGsAreaH), "%s: text '%s' at %d,%d %dx%d leaves the area", what, t.text.c_str(), t.x, t.y, t.w, t.h);
        for (size_t j = i + 1; j < lcd.texts.size(); j++) CHECK(!fakeLcdOverlap(t, lcd.texts[j]), "%s: texts '%s' and '%s' overlap", what, t.text.c_str(), lcd.texts[j].text.c_str());
    }
}

static uint16_t pixelAt(const FakeLcd& lcd, int x, int y) { return lcd.pixels[y * FakeLcd::W + x]; }

static bool hasText(const FakeLcd& lcd, const char* text) {
    for (const TextBox& t : lcd.texts)
        if (t.text == text) return true;
    return false;
}

int main() {
    // Every grid, several strengths, both fields: inside the area, legal text, no overlaps. The pictures of a few.
    for (uint8_t grid = 0; grid < kQuantizeGridCount; grid++) {
        for (uint8_t strength : {0, 25, 50, 100}) {
            for (bool strengthTab : {false, true}) {
                FakeLcd lcd;
                lcd.dirtyArea();
                qsDrawScreen(lcd, QsInput{grid, strength, strengthTab});
                char what[64];
                snprintf(what, sizeof what, "grid %u strength %u %s", grid, strength, strengthTab ? "STRENGTH tab" : "QUANTIZE tab");
                checkScreen(lcd, what);
                CHECK(lcd.ops > 20, "%s: drawn", what);
                if ((strength == 50) && (grid == 9 || grid == 11 || grid == kQuantizeGridOff || grid == 2)) {
                    char name[48];
                    snprintf(name, sizeof name, "quantize_g%u_%s", grid, strengthTab ? "strength" : "grid");
                    lcd.dump(name);
                }
            }
        }
    }

    // The points are where QuantizeView.h puts them: the lit row's ticks have the colours of their owners, the
    // columns in between are black.
    for (uint8_t grid = 0; grid < kQuantizeGridCount - 1; grid++) {
        FakeLcd lcd;
        lcd.dirtyArea();
        qsDrawScreen(lcd, QsInput{grid, 100, false});
        QvTicks t;
        qvTicks(grid, t);
        const uint16_t y = static_cast<uint16_t>(qsRowY(grid) + kQsTickH / 2);
        std::vector<bool> tick(kQvStripW + 8, false);
        for (uint8_t i = 0; i < t.n; i++) {
            const uint16_t x = qvStripXOf(t.at[i]);
            CHECK(pixelAt(lcd, x, y) == qsTickColor(t.owner[i], true), "grid %u tick %u at %u has the colour %04X, expected %04X", grid, i, x, pixelAt(lcd, x, y), qsTickColor(t.owner[i], true));
            CHECK(pixelAt(lcd, x + 1, y) == qsTickColor(t.owner[i], true), "grid %u tick %u is two columns wide", grid, i);
            tick[x - kQvStripX] = tick[x + 1 - kQvStripX] = true;
        }
        for (uint16_t x = kQvStripX + 2; x < kQvStripX + kQvStripW; x++) {
            if (tick[x - kQvStripX]) continue;
            const uint16_t c = pixelAt(lcd, x, y);
            // Between the ticks of the lit row: black, but the beat line at the middle and the song end may show.
            CHECK((c == kGsBlack) || (c == kGsGray25), "grid %u: column %u between the ticks is %04X", grid, x, c);
        }
        // A row of another grid is dim, never the lit colours.
        const uint8_t other = static_cast<uint8_t>((grid + 3) % (kQuantizeGridCount - 1));
        QvTicks to;
        qvTicks(other, to);
        const uint16_t oy = static_cast<uint16_t>(qsRowY(other) + kQsTickH / 2);
        for (uint8_t i = 0; i < to.n; i++) CHECK(pixelAt(lcd, qvStripXOf(to.at[i]), oy) == qsTickColor(to.owner[i], false), "grid %u: row %u is dim", grid, other);
    }

    // The words: the title, the kind, the strength and the demo lines for a few cases.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        qsDrawScreen(lcd, QsInput{9, 50, false});
        CHECK(hasText(lcd, "QUANTIZE  1:8T") && hasText(lcd, "TRIPLET GRID") && hasText(lcd, "3 NOTES IN THE TIME OF 2") && hasText(lcd, "3 POINTS IN A BEAT"), "1:8T texts");
        CHECK(hasText(lcd, "A HIT MOVES 50 % OF THE WAY") && hasText(lcd, "PLAYED 30 LATE") && hasText(lcd, "LANDS 15 LATE"), "strength 50 texts");
        CHECK(hasText(lcd, "STRENGTH") && hasText(lcd, "50") && hasText(lcd, "%"), "strength value");
    }
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        qsDrawScreen(lcd, QsInput{kQuantizeGridOff, 100, false});
        CHECK(hasText(lcd, "QUANTIZE  OFF") && hasText(lcd, "NO GRID") && hasText(lcd, "WITH OFF NOTHING IS MOVED:"), "OFF texts");
        CHECK(!hasText(lcd, "PLAYED"), "OFF has no demo picture");
    }
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        qsDrawScreen(lcd, QsInput{2, 100, true});
        CHECK(hasText(lcd, "A HIT SNAPS TO ITS POINT") && hasText(lcd, "LANDS ON THE POINT"), "strength 100 texts");
    }
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        qsDrawScreen(lcd, QsInput{kQuantizeGridOff, 50, true});
        CHECK(hasText(lcd, "NOT USED: HITS ARE NOT SNAPPED") && !hasText(lcd, "A HIT MOVES 50 % OF THE WAY"), "OFF does not claim a strength");
    }
    // Out-of-range input is drawn as the default grid and 100 %: no crash, still inside the area.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        qsDrawScreen(lcd, QsInput{200, 250, false});
        checkScreen(lcd, "out of range");
        CHECK(hasText(lcd, "QUANTIZE  1:16"), "an unknown grid shows as the default");
    }
    // The frame says which field is selected: yellow around the lit row on QUANTIZE, around the strength box on STRENGTH.
    {
        FakeLcd a, b;
        a.dirtyArea();
        b.dirtyArea();
        qsDrawScreen(a, QsInput{9, 50, false});
        qsDrawScreen(b, QsInput{9, 50, true});
        const int frameY = qsRowY(9) - 2;
        CHECK(pixelAt(a, kQsFrameX, frameY) == kGsYellow && pixelAt(b, kQsFrameX, frameY) == kGsGray25, "the row frame is lit on the QUANTIZE tab only");
        CHECK(pixelAt(a, kQsPanelX, kQsStrengthY) == kGsGray25 && pixelAt(b, kQsPanelX, kQsStrengthY) == kGsYellow, "the strength frame is lit on the STRENGTH tab only");
    }
    // Drawing twice gives the picture of drawing once (the screen clears its own area).
    {
        FakeLcd once, twice;
        once.dirtyArea();
        twice.dirtyArea();
        qsDrawScreen(once, QsInput{11, 75, true});
        qsDrawScreen(twice, QsInput{4, 10, false});
        qsDrawScreen(twice, QsInput{11, 75, true});
        CHECK(once.pixels == twice.pixels, "a redraw replaces the old picture completely");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
