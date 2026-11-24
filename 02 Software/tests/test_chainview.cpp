// Host-side test of the chain page's pictures: the numbers (ChainView.h) against the audio's own responses, and the drawing
// (ChainScreens.h) on a pixel buffer with the firmware's font tables: inside the lane area, legal text, no overlaps.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ChainScreens.h"
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

static uint8_t idxOfHz(float hz) { return static_cast<uint8_t>(std::lround(100.0 * std::log10(hz / 20.0) / 3.0)); }
static float dbOf(float y01, float top, float bottom) { return bottom + y01 * (top - bottom); }

int main() {
    ChSettings s = chDefaults();
    // FILTER: the picture is the response of the coefficients the audio runs: -3 dB at the cutoff of a 12 dB low-pass (Q 0.7), flat before it.
    s.filterType = 1;
    s.filterSlope = 1;
    s.filterCutoff = idxOfHz(1000);
    s.filterReson = 12;
    {
        float top, bottom;
        chvDbRange(CH_FILTER, top, bottom);
        const float mark = chvMarker(CH_FILTER, s);
        CHECK(std::fabs(dbOf(chvY01(CH_FILTER, s, mark), top, bottom) + 3.0f) < 1.2f, "-3 dB at the marker: %f", dbOf(chvY01(CH_FILTER, s, mark), top, bottom));
        CHECK(std::fabs(dbOf(chvY01(CH_FILTER, s, 0.1f), top, bottom)) < 0.5f, "flat well below the cutoff");
        CHECK(chvY01(CH_FILTER, s, 1.0f) < 0.05f, "down at 20 kHz");
        s.filterHp = idxOfHz(1000);
        CHECK(chvY01(CH_FILTER, s, 0.0f) < chvY01(CH_FILTER, s, 0.5f), "the extra high-pass lifts the curve from 20 Hz upwards");
        s.filterHp = 0;
        s.filterType = 4;
        s.filterReson = 40;
        CHECK(chvY01(CH_FILTER, s, mark) < 0.1f, "a notch is deep at its centre");
        s.filterType = 1;
        s.filterReson = 12;
    }
    // EQ: +6 dB bell at the marker.
    {
        ChSettings e = chDefaults();
        e.eqType = 0;
        e.eqFreq = idxOfHz(1000);
        e.eqGain = 48;
        float top, bottom;
        chvDbRange(CH_EQ, top, bottom);
        CHECK(std::fabs(dbOf(chvY01(CH_EQ, e, chvMarker(CH_EQ, e)), top, bottom) - 6.0f) < 0.3f, "bell +6 dB at the marker");
        CHECK(std::fabs(dbOf(chvY01(CH_EQ, e, 0.0f), top, bottom)) < 0.7f, "flat far from the band");
        e.eqGain = 36;
        CHECK(std::fabs(chvY01(CH_EQ, e, 0.3f) - 0.5f) < 0.005f, "0 dB is the middle of the plot");
    }
    // COMP: the transfer curve: 1:1 below the threshold, 1/ratio above, plus the makeup.
    {
        ChSettings cc = chDefaults();
        cc.compThresh = 67;  // -19.8 dB
        cc.compRatio = 6;    // 4:1
        cc.compMakeup = 0;
        float top, bottom;
        chvDbRange(CH_COMP, top, bottom);
        CHECK(std::fabs(dbOf(chvY01(CH_COMP, cc, 0.2f), top, bottom) - (-48.0f)) < 0.2f, "below the threshold out = in: %f", dbOf(chvY01(CH_COMP, cc, 0.2f), top, bottom));
        CHECK(std::fabs(dbOf(chvY01(CH_COMP, cc, 1.0f), top, bottom) - (-19.8f + 19.8f / 4.0f)) < 0.3f, "0 dB in comes out at about -14.9 dB");
        cc.compMakeup = 20;
        CHECK(std::fabs(dbOf(chvY01(CH_COMP, cc, 1.0f), top, bottom) - (-19.8f + 19.8f / 4.0f + 6.0f)) < 0.3f, "with 6 dB of makeup");
        CHECK(std::fabs(chvMarker(CH_COMP, cc) - (67 * 0.6f) / 60.0f) < 0.01f, "the threshold marker");
    }
    // ENV: -60 dB at the decay time, the ramp up, and hold.
    {
        ChSettings v = chDefaults();
        v.envAttack = 20;  // 10 ms
        v.envDecay = 38;   // 100 ms
        const float span = chvEnvSpanMs(v);
        CHECK(std::fabs(span - 125.0f) < 1.0f, "the axis is 1.25 times the decay: %f", span);
        CHECK(chvY01(CH_ENV, v, 0.0f) < 0.01f, "the ramp starts at 0");
        CHECK(std::fabs(chvY01(CH_ENV, v, 100.0f / span) - 0.001f) < 0.0006f, "-60 dB at the decay time: %f", chvY01(CH_ENV, v, 100.0f / span));
        v.envDecay = 100;
        CHECK(std::fabs(chvEnvSpanMs(v) - 1000.0f) < 0.1f && std::fabs(chvY01(CH_ENV, v, 0.5f) - 1.0f) < 1e-4f, "hold: the level stays");
    }
    // DRIVE: odd symmetry, hard at the ends, a line at amount 0.
    {
        ChSettings d = chDefaults();
        d.driveAmount = 80;
        CHECK(std::fabs(chvY01(CH_DRIVE, d, 0.5f) - 0.5f) < 1e-4f && std::fabs(chvY01(CH_DRIVE, d, 0.25f) + chvY01(CH_DRIVE, d, 0.75f) - 1.0f) < 1e-4f, "odd symmetry");
        CHECK(chvY01(CH_DRIVE, d, 1.0f) > 0.99f && chvY01(CH_DRIVE, d, 0.0f) < 0.01f && chvY01(CH_DRIVE, d, 0.75f) > 0.85f, "saturated: the quarter point is already near the top");
        d.driveAmount = 0;
        CHECK(std::fabs(chvY01(CH_DRIVE, d, 0.75f) - 0.75f) < 0.01f, "amount 0 is a line");
    }
    // IMAGE: the mid is flat at 0 dB; the side is the width, high-passed at the bass corner.
    {
        ChSettings m = chDefaults();
        m.imgWidth = 100;  // 200 %
        m.imgBass = 60;    // 160 Hz
        float top, bottom;
        chvDbRange(CH_IMAGE, top, bottom);
        CHECK(chvCurves(CH_IMAGE) == 2 && chvCurves(CH_FILTER) == 1, "two curves for IMAGE");
        CHECK(std::fabs(dbOf(chvY01(CH_IMAGE, m, 0.3f, 1), top, bottom)) < 1e-3f, "the mid is flat at 0 dB");
        CHECK(std::fabs(dbOf(chvY01(CH_IMAGE, m, 1.0f, 0), top, bottom) - 6.0f) < 0.3f, "the side is +6 dB for a width of 200 percent");
        CHECK(dbOf(chvY01(CH_IMAGE, m, 0.0f, 0), top, bottom) < -30.0f, "the side is cut at 20 Hz");
        const float mk = chvMarker(CH_IMAGE, m);
        CHECK(std::fabs(dbOf(chvY01(CH_IMAGE, m, mk, 0), top, bottom) - 3.0f) < 0.6f, "the side is 3 dB down at the corner (6 dB - 3 dB = 3): %f", dbOf(chvY01(CH_IMAGE, m, mk, 0), top, bottom));
        m.imgBass = 0;
        CHECK(chvMarker(CH_IMAGE, m) < 0.0f && std::fabs(dbOf(chvY01(CH_IMAGE, m, 0.0f, 0), top, bottom) - 6.0f) < 0.3f, "no bass mono: no corner, the side is flat");
        m.imgWidth = 0;
        CHECK(chvY01(CH_IMAGE, m, 0.5f, 0) == 0.0f, "width 0: the side is gone");
    }
    // The texts.
    {
        char t[32];
        chvSummary(CH_FILTER, s, t, sizeof t);
        CHECK(strcmp(t, "LP12 1.0K") == 0, "summary: '%s'", t);
        ChSettings e = chDefaults();
        e.envDecay = 100;
        chvSummary(CH_ENV, e, t, sizeof t);
        CHECK(strcmp(t, "HOLD A0") == 0, "env summary: '%s'", t);
        for (uint8_t b = 0; b < CH_BLOCKS; b++) {
            chvSummary(static_cast<ChBlock>(b), e, t, sizeof t);
            CHECK(strlen(t) <= 14 && strlen(t) > 0 && strlen(chvTitle(static_cast<ChBlock>(b))) < 60, "texts of block %u", b);
        }
        CHECK(chvAxis(CH_FILTER) == CHV_FREQ && chvAxis(CH_ENV) == CHV_TIME && chvAxis(CH_COMP) == CHV_LEVEL_IN && chvAxis(CH_DRIVE) == CHV_SIGNAL, "axes");
    }
    // Geometry: the boxes, the list and the plot are inside the lane area and clear of each other.
    CHECK(chvBoxX(5) + kChvBoxW <= kGsAreaX + kGsAreaW && chvBoxX(0) > kChvListX + kChvListW, "boxes fit and leave the list");
    CHECK(kChvBoxY + kChvBoxH < kChvTitleY && kChvPlotY + kChvPlotH < kChvCaptionY && kChvCaptionY + 7 <= kGsAreaY + kGsAreaH && kChvPlotX + kChvPlotW + 2 <= kGsAreaX + kGsAreaW, "right side in order");
    CHECK(kChvListY + 5 * kChvListStep + kChvListH <= kGsAreaY + kGsAreaH && kChvMiniX + kChvMiniW <= kChvBoxW && kChvMiniY + kChvMiniH <= kChvBoxH, "list and minis fit");

    // The screens, for every block selected, on and off.
    for (uint8_t sel = 0; sel < CH_BLOCKS; sel++) {
        for (uint8_t mask : {0x3F, 0x00, 0x15}) {
            FakeLcd lcd;
            lcd.dirtyArea();
            ChSettings x = chDefaults();
            x.on = mask;
            x.filterHp = 30;
            x.imgBass = 60;
            x.imgWidth = 80;
            chvDrawHeader(lcd, 2, "HAT CLOSED 01");
            chvDrawList(lcd, x, sel);
            chvDrawBoxes(lcd, x, sel);
            chvDrawBig(lcd, static_cast<ChBlock>(sel), x);
            char what[48];
            snprintf(what, sizeof what, "block %u mask %02X", sel, mask);
            checkScreen(lcd, what);
            CHECK(lcd.ops > 100, "%s: drawn", what);
            if (mask == 0x3F) {
                char name[48];
                snprintf(name, sizeof name, "chain_%u", sel);
                lcd.dump(name);
            }
        }
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
