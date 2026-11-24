// Host-side test of the stereo stage page: the numbers (StageView.h), the IMAGE switch that follows its settings (ChannelStrip.h)
// and the drawing (StageScreens.h) on a pixel buffer with the firmware's font tables.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "StageScreens.h"
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

int main() {
    // The IMAGE switch follows the settings.
    {
        ChSettings s = chDefaults();
        CHECK(chImageNeutral(s) && !chOn(s, CH_IMAGE), "defaults are neutral");
        s.imgWidth = 60;
        chImageSync(s);
        CHECK(!chImageNeutral(s) && chOn(s, CH_IMAGE), "a width of 120 percent switches the image on");
        s.imgWidth = 50;
        chImageSync(s);
        CHECK(!chOn(s, CH_IMAGE), "neutral again: off");
        s.imgBass = 5;
        chImageSync(s);
        CHECK(chOn(s, CH_IMAGE), "bass mono switches it on");
        s.imgBass = 0;
        s.imgSpread = 10;
        s.on = static_cast<uint8_t>(s.on | (1u << CH_FILTER));
        chImageSync(s);
        CHECK(chOn(s, CH_IMAGE) && chOn(s, CH_FILTER), "spread switches it on and leaves the other blocks");
        s.imgSpread = 0;
        chImageSync(s);
        CHECK(!chOn(s, CH_IMAGE) && chOn(s, CH_FILTER), "and off again without touching the others");
    }
    // The width in percent.
    {
        ChSettings s = chDefaults();
        CHECK(stWidthPct(s, false) == 0 && stWidthPct(s, true) == 100, "no image: a mono layer is a point, a stereo sample 100 percent");
        s.imgSpread = 60;
        s.imgWidth = 50;
        chImageSync(s);
        CHECK(stWidthPct(s, false) == 60 && stWidthPct(s, true) == 100, "spread 60: a mono layer is 60 percent wide");
        s.imgWidth = 100;
        CHECK(stWidthPct(s, false) == 120 && stWidthPct(s, true) == 200, "width 200 percent doubles it");
        s.imgWidth = 0;
        CHECK(stWidthPct(s, false) == 0 && stWidthPct(s, true) == 0, "width 0: mono");
        s.imgWidth = 50;
        s.imgSpread = 0;
        s.imgBass = 60;
        chImageSync(s);
        CHECK(stBassHz(s) == 159 && stWidthPct(s, false) == 0, "bass mono 159 Hz");
        s.on = 0;
        CHECK(stBassHz(s) == 0, "the image off: no bass corner");
    }
    // The bar on the stage.
    {
        StRow r;
        memset(&r, 0, sizeof r);
        r.pan = kPanCentre;
        r.widthPct = 100;
        uint16_t l, rr;
        stBar(r, l, rr);
        CHECK(stCentreX(kPanCentre) == kStStageX + kStStageW / 2 && stCentreX(0) == kStStageX && stCentreX(kPanMax) == kStStageX + kStStageW, "centres");
        CHECK(rr - l == kStStageW / 4 && l + (rr - l) / 2 == stCentreX(kPanCentre), "100 percent is a quarter of the stage, centred: %u %u", l, rr);
        r.widthPct = 0;
        stBar(r, l, rr);
        CHECK(rr - l == 4, "a point is 4 px wide: %u", rr - l);
        r.widthPct = 200;
        r.pan = 0;
        stBar(r, l, rr);
        CHECK(l == kStStageX && rr <= kStStageX + kStStageW && rr > l, "a wide layer panned hard left is held inside the stage");
        r.pan = kPanMax;
        stBar(r, l, rr);
        CHECK(rr == kStStageX + kStStageW && l >= kStStageX, "and hard right");
        r.pan = 200;
        stBar(r, l, rr);
        CHECK(rr <= kStStageX + kStStageW, "a pan past the end is held");
    }
    // The correlation.
    {
        CHECK(stCorrOf(1, 1, 1) == 100 && stCorrOf(1, 1, -1) == -100 && stCorrOf(1, 1, 0) == 0, "identical, opposite, unrelated");
        CHECK(stCorrOf(1, 4, 1) == 50 && stCorrOf(0, 0, 0) == kStNoCorr && stCorrOf(1, 0, 0) == kStNoCorr, "scaled copy, silence, one side silent");
        CHECK(stCorrOf(1, 1, 3) == 100 && stCorrOf(1, 1, -3) == -100, "held to +-1");
        CHECK(stCorrSmooth(kStNoCorr, 40) == 40 && stCorrSmooth(50, kStNoCorr) == 50 && stCorrSmooth(100, 0) == 70 && stCorrSmooth(0, 100) == 30, "smoothing");
        CHECK(stCorrState(kStNoCorr) == ST_CORR_NONE && stCorrState(100) == ST_CORR_GOOD && stCorrState(60) == ST_CORR_GOOD && stCorrState(59) == ST_CORR_WIDE && stCorrState(10) == ST_CORR_WIDE && stCorrState(9) == ST_CORR_BAD && stCorrState(-100) == ST_CORR_BAD, "states");
        char t[16];
        stCorrText(55, false, t, sizeof t);
        CHECK(strcmp(t, "+0.55") == 0, "text %s", t);
        stCorrText(-20, false, t, sizeof t);
        CHECK(strcmp(t, "-0.20") == 0, "text %s", t);
        stCorrText(100, false, t, sizeof t);
        CHECK(strcmp(t, "+1.00") == 0, "text %s", t);
        stCorrText(kStNoCorr, false, t, sizeof t);
        CHECK(strcmp(t, "--") == 0, "text %s", t);
        stCorrText(0, true, t, sizeof t);
        CHECK(strcmp(t, "MONO") == 0, "text %s", t);
        // A measurement through the real block: spread decorrelates a mono layer.
        ChSettings s = chDefaults();
        s.imgSpread = 100;
        chImageSync(s);
        ChState st;
        std::vector<float> L(44096), R(44096, 0.0f);
        uint32_t seed = 99;
        for (auto& v : L) {
            seed = seed * 1664525u + 1013904223u;
            v = (static_cast<float>(seed >> 8) / 8388608.0f - 1.0f) * 0.3f;
        }
        for (size_t i = 0; i + 32 <= L.size(); i += 32) chProcess(s, st, &L[i], &R[i], false, nullptr, 32, 44100.0f);
        double ll = 0, rr = 0, lr = 0;
        for (size_t i = 8000; i < L.size(); i++) {
            ll += static_cast<double>(L[i]) * L[i];
            rr += static_cast<double>(R[i]) * R[i];
            lr += static_cast<double>(L[i]) * R[i];
        }
        const int8_t c = stCorrOf(static_cast<float>(ll), static_cast<float>(rr), static_cast<float>(lr));
        CHECK(c != kStNoCorr && std::abs(c) < 60, "the measured correlation of a spread mono layer: %d", c);
        s.imgSpread = 0;
        s.imgWidth = 0;
        chImageSync(s);
        ChState st2;
        std::vector<float> L2(4096, 0.1f), R2(4096, 0.1f);
        for (size_t i = 0; i + 32 <= L2.size(); i += 32) chProcess(s, st2, &L2[i], &R2[i], true, nullptr, 32, 44100.0f);
        CHECK(stCorrOf(1.0f, 1.0f, 1.0f) == 100 && L2[100] == R2[100], "width 0: both sides the same");
    }
    // The geometry.
    CHECK(stRowY(9) + kStRowH <= kGsAreaY + kGsAreaH && stRowY(0) > 148, "rows fit under the marks");
    CHECK(kStStageX + kStStageW + 20 < kStCorrRight - 5 * 7 && kStStageX > 24 + 11 * 7, "the stage leaves room for the names and the correlation");

    // The screens: inside the area, legal text, no overlap; many situations.
    static const char* const kName[10] = {"KICK 808", "SNARE TIGHT", "HAT CLOSED", "HAT OPEN", "CLAP", "RIM", "TOM LOW", "SHAKER", "COWBELL", "PERC FX"};
    const uint8_t pan[10] = {20, 14, 8, 28, 20, 34, 6, 38, 26, 0};
    const uint16_t wid[10] = {0, 0, 120, 80, 100, 60, 40, 90, 70, 200};
    const int8_t cor[10] = {100, 100, 55, 70, 90, 80, 90, 30, 60, -20};
    for (int variant = 0; variant < 3; variant++) {
        FakeLcd lcd;
        lcd.dirtyArea();
        stDrawFrame(lcd);
        for (uint8_t i = 0; i < 10; i++) {
            StRow r;
            memset(&r, 0, sizeof r);
            r.pan = variant == 2 ? static_cast<uint8_t>(40 - pan[i]) : pan[i];
            r.widthPct = variant == 1 ? static_cast<uint16_t>(wid[i] * 2) : wid[i];
            r.bassHz = (i % 3 == 0) ? 120 : 0;
            r.corr = variant == 1 ? kStNoCorr : cor[i];
            r.stereoSample = wid[i] > 0;
            stDrawRow(lcd, i, kName[i], r, i == 2, variant != 1);
        }
        char what[32];
        snprintf(what, sizeof what, "stage %d", variant);
        checkScreen(lcd, what);
        CHECK(lcd.ops > 50, "%s: drawn", what);
        if (variant == 0) lcd.dump("stage_0");
    }
    // The pictures say what the numbers say: a green bass mark, a red correlation, the selected row outlined.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        StRow r;
        memset(&r, 0, sizeof r);
        r.pan = 20;
        r.widthPct = 100;
        r.bassHz = 120;
        r.corr = -30;
        r.stereoSample = true;
        stDrawRow(lcd, 3, "HAT OPEN", r, true, true);
        const uint16_t y = stRowY(3);
        CHECK(pixelAt(lcd, stCentreX(20), y + 21) == kGsYellow, "the bass mark is yellow below the bar");
        CHECK(pixelAt(lcd, stCentreX(20) + 20, y + 8) == kGsCyan, "the selected layer's bar is cyan");
        CHECK(pixelAt(lcd, 24, y) == kGsCyan, "the selected row is outlined");
        bool red = false;
        for (int x = kStCorrRight - 40; x < kStCorrRight; x++)
            for (int yy = y; yy < y + kStRowH; yy++) red = red || (pixelAt(lcd, x, yy) == kStRed);
        CHECK(red, "a negative correlation is red");
        r.bassHz = 0;
        stDrawRow(lcd, 3, "HAT OPEN", r, false, true);
        CHECK(pixelAt(lcd, stCentreX(20), y + 21) == kGsBlack && pixelAt(lcd, 24, y) == kGsBlack, "no bass mark, no outline");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
