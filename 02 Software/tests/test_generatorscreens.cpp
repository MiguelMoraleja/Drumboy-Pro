// Host-side tests for the generator screens drawn (GeneratorScreens.h): the same template code the
// firmware draws with, here on a pixel buffer with the firmware's own font tables. Checks that every
// pixel stays inside the lane area, that texts use characters the font has, that no two texts overlap,
// that the live part's incremental drawing equals a fresh drawing, and writes the pictures when
// GS_DUMP_DIR is set (PPM files, one per screen).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Font_05x07.h"
#include "Font_07x09.h"
#include "Font_10x14.h"
#include "GeneratorScreens.h"

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

// The font's character map (Lcd::getLetter): index 52 = no glyph.
static uint8_t glyphIndex(char c) {
    static const uint8_t fontMap[128] = {
        /* 00-09 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 10-19 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 20-29 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 30-39 */ 52, 52, 52, 51, 52, 50, 52, 39, 52, 52,
        /* 40-49 */ 52, 52, 52, 38, 41, 37, 40, 36, 0, 1,
        /* 50-59 */ 2, 3, 4, 5, 6, 7, 8, 9, 42, 52,
        /* 60-69 */ 43, 52, 44, 49, 52, 10, 11, 12, 13, 14,
        /* 70-79 */ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        /* 80-89 */ 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
        /* 90-99 */ 35, 45, 52, 46, 52, 47, 52, 10, 11, 12,
        /*100-109*/ 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        /*110-119*/ 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
        /*120-129*/ 33, 34, 35, 52, 48, 52, 52, 52};
    const unsigned char u = static_cast<unsigned char>(c);
    return (u > 127) ? 52 : fontMap[u];
}

struct TextBox {
    int x, y, w, h;
    std::string text;
};

// A pixel buffer that behaves like the firmware's Lcd for what GeneratorScreens.h uses.
struct FakeLcd {
    static const int W = 960, H = 540;
    std::vector<uint16_t> pixels;
    std::vector<uint8_t> touched;
    GsFont font = GS_FONT_05x07;
    GsAlign align = GS_LEFT;
    uint16_t fore = 0xFFFF, back = 0x0000;
    int outsideArea = 0, offscreen = 0, unsupported = 0, ops = 0;
    std::string badChars;
    std::vector<TextBox> texts;

    FakeLcd() : pixels(W * H, 0x1234), touched(W * H, 0) {}

    void setFont(GsFont f) { font = f; }
    void setAlignment(GsAlign a) { align = a; }
    void setForeColor(uint16_t c) { fore = c; }
    void setBackColor(uint16_t c) { back = c; }

    void put(int x, int y, uint16_t c) {
        if ((x < 0) || (y < 0) || (x >= W) || (y >= H)) {
            offscreen++;
            return;
        }
        if ((x < kGsAreaX) || (x >= kGsAreaX + kGsAreaW) || (y < kGsAreaY) || (y >= kGsAreaY + kGsAreaH)) outsideArea++;
        pixels[y * W + x] = c;
        touched[y * W + x] = 1;
    }
    void drawPixel(uint16_t x, uint16_t y) { put(x, y, fore); }
    void drawHLine(int x, int y, int l) {
        for (int i = 0; i < l; i++) put(x + i, y, fore);
    }
    void drawVLine(uint16_t x, uint16_t y, int16_t l) {
        ops++;
        for (int i = 0; i < l; i++) put(x, y + i, fore);
    }
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
        ops++;
        int x = x0, y = y0;
        const int dx = std::abs(static_cast<int>(x1) - x), dy = -std::abs(static_cast<int>(y1) - y);
        const int sx = (x < x1) ? 1 : -1, sy = (y < y1) ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            put(x, y, fore);
            if ((x == x1) && (y == y1)) break;
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y += sy;
            }
        }
    }
    void drawRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
        ops++;
        if ((w == 0) || (h == 0)) return;
        drawHLine(x, y, static_cast<int>(w));
        drawHLine(x, y + static_cast<int>(h) - 1, static_cast<int>(w));
        for (uint32_t i = 0; i < h; i++) {
            put(x, y + static_cast<int>(i), fore);
            put(x + static_cast<int>(w) - 1, y + static_cast<int>(i), fore);
        }
    }
    void fillRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
        ops++;
        for (uint32_t j = 0; j < h; j++)
            for (uint32_t i = 0; i < w; i++) put(x + static_cast<int>(i), y + static_cast<int>(j), fore);
    }
    void clearRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
        ops++;
        for (uint32_t j = 0; j < h; j++)
            for (uint32_t i = 0; i < w; i++) put(x + static_cast<int>(i), y + static_cast<int>(j), back);
    }
    void drawText(const char* text, uint16_t length, uint16_t x, uint16_t y) {
        ops++;
        const int fw = (font == GS_FONT_05x07) ? kFontWidth_05x07 : ((font == GS_FONT_07x09) ? kFontWidth_07x09 : kFontWidth_10x14);
        const int fh = (font == GS_FONT_05x07) ? kFontHeight_05x07 : ((font == GS_FONT_07x09) ? kFontHeight_07x09 : kFontHeight_10x14);
        const int fs = (font == GS_FONT_05x07) ? kFontSpacing_05x07 : ((font == GS_FONT_07x09) ? kFontSpacing_07x09 : kFontSpacing_10x14);
        const int w = length * (fw + fs);
        int xPos = x;
        if (align == GS_CENTER) xPos = x - w / 2;
        if (align == GS_RIGHT) xPos = x - w;
        std::string s(text, text + length);
        texts.push_back(TextBox{xPos, y, w, fh, s});
        for (int i = 0; i < length; i++) {
            const uint8_t idx = glyphIndex(text[i]);
            if ((idx == 52) && (text[i] != ' ')) {
                unsupported++;
                badChars += std::string("'") + text[i] + "' in '" + s + "'  ";
            }
            for (int row = 0; row < fh; row++) {
                uint16_t bits = 0;
                if (font == GS_FONT_05x07) bits = kFontData_05x07[idx][row];
                if (font == GS_FONT_07x09) bits = kFontData_07x09[idx][row];
                if (font == GS_FONT_10x14) bits = kFontData_10x14[idx][row];
                for (int col = 0; col < fw + fs; col++) {
                    const bool on = (col < fw) && (bits & (1u << (fw - 1 - col)));
                    put(xPos + i * (fw + fs) + col, y + row, on ? fore : back);
                }
            }
        }
    }

    // The area before a screen is drawn: a pattern, so pixels the screen never touches are visible.
    void dirtyArea() {
        for (int y = kGsAreaY; y < kGsAreaY + kGsAreaH; y++)
            for (int x = kGsAreaX; x < kGsAreaX + kGsAreaW; x++) pixels[y * W + x] = 0x1234;
    }
    void dump(const char* name) const {
        const char* dir = std::getenv("GS_DUMP_DIR");
        if (!dir) return;
        std::string path = std::string(dir) + "/" + name + ".ppm";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return;
        std::fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (int i = 0; i < W * H; i++) {
            const uint16_t c = pixels[i];
            const uint8_t r = static_cast<uint8_t>(((c >> 11) & 31) * 255 / 31), g = static_cast<uint8_t>(((c >> 5) & 63) * 255 / 63), b = static_cast<uint8_t>((c & 31) * 255 / 31);
            std::fputc(r, f);
            std::fputc(g, f);
            std::fputc(b, f);
        }
        std::fclose(f);
    }
};

static bool overlap(const TextBox& a, const TextBox& b) {
    return (a.x < b.x + b.w) && (b.x < a.x + a.w) && (a.y < b.y + b.h) && (b.y < a.y + a.h);
}

// Inside the lane area, only legal characters, no two texts on top of each other, every pixel of the area painted.
static void checkScreen(const FakeLcd& lcd, const char* what, bool everyPixel = true) {
    CHECK(lcd.offscreen == 0, "%s: %d pixels off the screen", what, lcd.offscreen);
    CHECK(lcd.outsideArea == 0, "%s: %d pixels outside the lane area", what, lcd.outsideArea);
    CHECK(lcd.unsupported == 0, "%s: %d characters the font has no glyph for: %s", what, lcd.unsupported, lcd.badChars.c_str());
    for (size_t i = 0; i < lcd.texts.size(); i++) {
        const TextBox& t = lcd.texts[i];
        CHECK((t.x >= kGsAreaX) && (t.x + t.w <= kGsAreaX + kGsAreaW) && (t.y >= kGsAreaY) && (t.y + t.h <= kGsAreaY + kGsAreaH), "%s: text '%s' at %d,%d %dx%d leaves the area", what, t.text.c_str(), t.x, t.y, t.w, t.h);
        for (size_t j = i + 1; j < lcd.texts.size(); j++) CHECK(!overlap(t, lcd.texts[j]), "%s: texts '%s' and '%s' overlap", what, t.text.c_str(), lcd.texts[j].text.c_str());
    }
    if (everyPixel) {
        int untouched = 0;
        for (int y = kGsAreaY; y < kGsAreaY + kGsAreaH; y++)
            for (int x = kGsAreaX; x < kGsAreaX + kGsAreaW; x++) untouched += lcd.touched[y * FakeLcd::W + x] ? 0 : 1;
        CHECK(untouched == 0, "%s: %d pixels of the area never painted", what, untouched);
    }
    // Nothing outside the area was touched.
    int stray = 0;
    for (int y = 0; y < FakeLcd::H; y++)
        for (int x = 0; x < FakeLcd::W; x++) {
            const bool inside = (x >= kGsAreaX) && (x < kGsAreaX + kGsAreaW) && (y >= kGsAreaY) && (y < kGsAreaY + kGsAreaH);
            if (!inside && lcd.touched[y * FakeLcd::W + x]) stray++;
        }
    CHECK(stray == 0, "%s: %d pixels outside the area touched", what, stray);
}

static GsGen makeGen(uint8_t type, float ratio, uint8_t sweepPercent, uint8_t shape, uint8_t length16, float tempo, bool plain = false) {
    GsGen in;
    memset(&in, 0, sizeof in);
    snprintf(in.layerNum, sizeof in.layerNum, "%02u", 3u);
    in.type = type;
    in.plain = plain;
    in.ratio = plain ? 1.0f : ratio;
    in.sweepPercent = plain ? 0 : sweepPercent;
    in.octaves = genSweepOctaves(in.sweepPercent);
    in.shape = plain ? 0 : shape;
    in.length16 = length16;
    in.length = genLengthSamples(length16, tempo);
    return in;
}

static void drawGen(FakeLcd& lcd, const GsGen& in) {
    std::vector<uint8_t> peaks(kGvNoteCols), rms(kGvNoteCols);
    genViewPeaks(in.type, in.ratio, in.octaves, in.shape, in.length, peaks.data(), kGvNoteCols, static_cast<uint8_t>(kGvNoteHalf), rms.data());
    gsDrawGen(lcd, in, peaks.data(), rms.data());
}

static void testGenScreens() {
    // Every type, with the default, a full sweep in each direction, plain, and the extremes of length and PITCH.
    struct Case {
        float ratio;
        uint8_t sweep, shape, length16;
        float tempo;
        bool plain;
    };
    const Case cases[] = {{1.0f, 0, 0, 4, 120.0f, false},  {1.0f, 50, 0, 4, 120.0f, false}, {1.0f, 100, 1, 16, 90.0f, false}, {1.0f, 100, 2, 64, 20.0f, false},
                          {2.0f, 75, 3, 1, 240.0f, false}, {0.25f, 100, 2, 8, 120.0f, false}, {4.0f, 100, 0, 8, 120.0f, false},  {1.0f, 80, 1, 4, 120.0f, true}};
    for (uint8_t type = GEN_WHITE; type < kGenTypeCount; type++) {
        for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
            FakeLcd lcd;
            lcd.dirtyArea();
            const Case& c = cases[k];
            drawGen(lcd, makeGen(type, c.ratio, c.sweep, c.shape, c.length16, c.tempo, c.plain));
            char what[80];
            snprintf(what, sizeof what, "GEN %s case %zu", kGenTypeName[type], k);
            checkScreen(lcd, what);
        }
    }
    // The pictures: one per interesting type.
    struct Pic {
        const char* name;
        GsGen in;
    };
    const Pic pics[] = {
        {"gen_tape_updown", makeGen(GEN_TAPE, 1.0f, 60, 2, 8, 120.0f)},
        {"gen_white_down", makeGen(GEN_WHITE, 1.0f, 100, 1, 16, 120.0f)},
        {"gen_pink_plain", makeGen(GEN_PINK, 1.0f, 0, 0, 4, 120.0f, true)},
        {"gen_brown", makeGen(GEN_BROWN, 1.0f, 30, 0, 8, 120.0f)},
        {"gen_dust", makeGen(GEN_DUST, 1.0f, 50, 0, 16, 120.0f)},
        {"gen_siren", makeGen(GEN_SIREN, 1.0f, 70, 2, 8, 120.0f)},
        {"gen_riser_up", makeGen(GEN_RISER, 1.0f, 100, 0, 16, 100.0f)},
        {"gen_laser_down", makeGen(GEN_LASER, 1.0f, 100, 1, 4, 140.0f)},
    };
    for (const Pic& p : pics) {
        FakeLcd lcd;
        lcd.dirtyArea();
        drawGen(lcd, p.in);
        lcd.dump(p.name);
    }
    // A sweep shows three curves (start, middle, end), no sweep one: the picture differs.
    FakeLcd a, b;
    a.dirtyArea();
    b.dirtyArea();
    drawGen(a, makeGen(GEN_RISER, 1.0f, 100, 0, 8, 120.0f));
    drawGen(b, makeGen(GEN_RISER, 1.0f, 0, 0, 8, 120.0f));
    CHECK(a.pixels != b.pixels, "a sweep changes the picture");
    // Redrawing is the same picture (deterministic, and clears what was there).
    FakeLcd c1, c2;
    c1.dirtyArea();
    c2.dirtyArea();
    const GsGen g = makeGen(GEN_TAPE, 1.0f, 60, 2, 8, 120.0f);
    drawGen(c1, g);
    drawGen(c2, g);
    drawGen(c2, g);
    CHECK(c1.pixels == c2.pixels, "drawing twice gives the picture of drawing once");
}

static GsHiss makeHiss(uint8_t type, uint8_t level, uint8_t tone, uint8_t duck) { return GsHiss{type, level, tone, duck}; }

struct HissFrame {
    std::vector<uint8_t> l, r, rl, rr;
    HissFrame(const GsHiss& in) : l(kHvTextureCols), r(kHvTextureCols), rl(kHvTextureCols), rr(kHvTextureCols) {
        hissViewTexture(in.type, in.level, in.tone, l.data(), r.data(), kHvTextureCols, static_cast<uint8_t>(kHvTextureHalf), rl.data(), rr.data());
    }
};

static void testHissScreens() {
    const GsHiss cases[] = {makeHiss(HISS_TAPE, 30, 70, 0),   makeHiss(HISS_VINYL, 60, 50, 40), makeHiss(HISS_PINK, 100, 100, 100), makeHiss(HISS_WHITE, 10, 0, 25),
                            makeHiss(HISS_OFF, 30, 70, 0),     makeHiss(HISS_VINYL, 0, 70, 60),  makeHiss(HISS_TAPE, 100, 100, 1)};
    int n = 0;
    for (const GsHiss& in : cases) {
        for (int state = 0; state < 3; state++) {
            FakeLcd lcd;
            lcd.dirtyArea();
            GsHissLive live;
            HissFrame f(in);
            const bool bedOn = (state != 2) && (in.type != HISS_OFF);
            gsDrawHiss(lcd, in, f.l.data(), f.r.data(), f.rl.data(), f.rr.data(), live, bedOn, (state == 1) ? 0.45f : 1.0f, 0.04f, 0.03f);
            char what[80];
            snprintf(what, sizeof what, "HISS %s case %d state %d", kHissTypeName[in.type], n, state);
            checkScreen(lcd, what);
            if (state == 0) {
                char name[48];
                snprintf(name, sizeof name, "hiss_%s_%d", kHissTypeName[in.type], n);
                lcd.dump(name);
            }
        }
        n++;
    }
}

// The live part draws only what changed: after any sequence of updates the picture is the one a fresh
// drawing with the last values gives.
static void testLiveIncremental() {
    const GsHiss in = makeHiss(HISS_VINYL, 50, 60, 40);
    HissFrame f(in);
    const float gains[] = {1.0f, 0.9f, 0.55f, 0.7f, 0.6f, 1.0f, 0.6f, 0.0f, 0.3f};
    const float peaksL[] = {0.0f, 0.05f, 0.2f, 0.01f, 0.5f, 0.001f, 0.0f, 0.3f, 0.02f};
    FakeLcd live;
    live.dirtyArea();
    GsHissLive st;
    gsDrawHiss(live, in, f.l.data(), f.r.data(), f.rl.data(), f.rr.data(), st, true, gains[0], peaksL[0], peaksL[0] * 0.5f);
    for (size_t i = 1; i < sizeof gains / sizeof gains[0]; i++) {
        gsDrawHissLive(live, in, st, true, gains[i], peaksL[i], peaksL[i] * 0.5f);
        FakeLcd fresh;
        fresh.dirtyArea();
        GsHissLive st2;
        gsDrawHiss(fresh, in, f.l.data(), f.r.data(), f.rl.data(), f.rr.data(), st2, true, gains[i], peaksL[i], peaksL[i] * 0.5f);
        CHECK(live.pixels == fresh.pixels, "step %zu (gain %.2f): the incremental picture differs from a fresh one", i, gains[i]);
    }
    // The bed goes off: not ducked, bars empty.
    gsDrawHissLive(live, in, st, false, 0.3f, 0.3f, 0.3f);
    FakeLcd off;
    off.dirtyArea();
    GsHissLive st3;
    gsDrawHiss(off, in, f.l.data(), f.r.data(), f.rl.data(), f.rr.data(), st3, false, 0.3f, 0.3f, 0.3f);
    CHECK(live.pixels == off.pixels, "the bed switched off: bars empty, gain full");
    // And back on: the colour and the gain are drawn again.
    gsDrawHissLive(live, in, st, true, 0.5f, 0.1f, 0.2f);
    FakeLcd on;
    on.dirtyArea();
    GsHissLive st5;
    gsDrawHiss(on, in, f.l.data(), f.r.data(), f.rl.data(), f.rr.data(), st5, true, 0.5f, 0.1f, 0.2f);
    CHECK(live.pixels == on.pixels, "the bed switched on again");
    gsDrawHissLive(live, in, st, false, 0.5f, 0.1f, 0.2f);
    // Nothing to draw when nothing changed.
    const int before = live.ops;
    gsDrawHissLive(live, in, st, false, 0.3f, 0.3f, 0.3f);
    CHECK(live.ops == before, "an unchanged live state draws nothing (%d new operations)", live.ops - before);
    // NaN from the audio side draws an empty bar, not garbage.
    FakeLcd nan;
    nan.dirtyArea();
    GsHissLive st4;
    gsDrawHiss(nan, in, f.l.data(), f.r.data(), f.rl.data(), f.rr.data(), st4, true, NAN, NAN, NAN);
    checkScreen(nan, "NaN live values");
}

static void testTextsAreLegal() {
    // The texts of every screen use only characters the font has (no '=', '(' or ')').
    FakeLcd lcd;
    lcd.dirtyArea();
    drawGen(lcd, makeGen(GEN_SIREN, 1.0f, 70, 2, 8, 120.0f));
    bool sawKhz = false;
    for (const TextBox& t : lcd.texts) {
        for (char ch : t.text) CHECK((glyphIndex(ch) != 52) || (ch == ' '), "text '%s' has the character '%c' the font lacks", t.text.c_str(), ch);
        if (t.text.find("kHz") != std::string::npos) sawKhz = true;
    }
    (void)sawKhz;
}

int main() {
    testGenScreens();
    testHissScreens();
    testLiveIncremental();
    testTextsAreLegal();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
