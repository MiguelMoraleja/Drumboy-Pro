// Host-side test of "Quantize now": the numbers (QuantizeNow.h) and the drawing (QuantizeNowScreens.h) on a pixel
// buffer with the firmware's own font tables. Checks the totals against Quantize.h's own snapping, the apply
// state machine, that every pixel stays inside the lane area, legal text, no overlapping texts, and the picture.

#include <cstdio>
#include <cstring>

#include "QuantizeNowScreens.h"
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

static uint8_t gridOfStep(uint16_t step_) {  // the straight grid whose points are step_ intervals apart
    for (uint8_t g = 0; g < kQuantizeGridCount - 1; g++) {
        if ((quantizeGrid(g).periodThirds == step_ * 3u) && (quantizeGrid(g).periodThirdsB == 0)) return g;
    }
    return 255;
}

int main() {
    const uint32_t song = 12800;
    const uint8_t g16 = gridOfStep(200);
    CHECK(g16 != 255, "found the 1:16 grid");

    // The distance round a loop.
    CHECK(qnDistance(100, 130, song) == 30 && qnDistance(130, 100, song) == 30 && qnDistance(12700, 0, song) == 100 && qnDistance(0, 12700, song) == 100 && qnDistance(5, 5, song) == 0, "distances");

    // Totals: nine notes of the picture on a 1:16 grid, full strength.
    const uint16_t notes[9] = {0, 212, 395, 610, 805, 1003, 1190, 1385, 1590};
    QnTotals t;
    memset(&t, 0, sizeof t);
    qnAddBank(t, notes, 9, g16, 100, song);
    uint32_t moves = 0, onGrid = 0, largest = 0;
    for (uint8_t i = 0; i < 9; i++) {
        const uint16_t l = quantizeSnap(notes[i], g16, 100, song);
        if (l != notes[i]) moves++;
        if (quantizeNearest(notes[i], g16) == notes[i]) onGrid++;
        if (qnDistance(notes[i], l, song) > largest) largest = qnDistance(notes[i], l, song);
    }
    CHECK(t.notes == 9 && t.moves == moves && t.onGrid == onGrid && t.largest == largest && t.merged == 0, "totals %u %u %u %u %u", (unsigned)t.notes, (unsigned)t.moves, (unsigned)t.onGrid, (unsigned)t.largest, (unsigned)t.merged);
    CHECK(t.onGrid == 1 && t.moves == 8 && t.largest > 0 && t.largest <= 100, "the demo: one note on the grid, eight move, none farther than half a step");
    // Partial strength moves less; strength 0 moves nothing.
    QnTotals half;
    memset(&half, 0, sizeof half);
    qnAddBank(half, notes, 9, g16, 50, song);
    QnTotals none;
    memset(&none, 0, sizeof none);
    qnAddBank(none, notes, 9, g16, 0, song);
    CHECK(half.largest <= t.largest / 2 + 1 && half.largest > 0 && none.moves == 0 && none.largest == 0, "strength 50 moves half as far, 0 nothing");
    // OFF: nothing moves, everything is on the grid.
    QnTotals off;
    memset(&off, 0, sizeof off);
    qnAddBank(off, notes, 9, kQuantizeGridOff, 100, song);
    CHECK(off.moves == 0 && off.onGrid == 9 && off.merged == 0, "OFF moves nothing");
    // Two notes that land on the same point are joined.
    const uint16_t pair[3] = {190, 210, 600};
    QnTotals j;
    memset(&j, 0, sizeof j);
    qnAddBank(j, pair, 3, g16, 100, song);
    CHECK(j.merged == 1 && j.moves == 2 && j.onGrid == 1, "joined notes: %u merged, %u move", (unsigned)j.merged, (unsigned)j.moves);
    // A note just before the end of the loop lands on the start (the song wraps) and moves only a little.
    const uint16_t endNote[1] = {static_cast<uint16_t>(song - 40)};
    QnTotals e;
    memset(&e, 0, sizeof e);
    qnAddBank(e, endNote, 1, g16, 100, song);
    CHECK(e.moves == 1 && e.largest == 40, "a wrap to the start moves it 40: %u", (unsigned)e.largest);
    // Totals add up across banks.
    qnAddBank(t, notes, 9, g16, 100, song);
    CHECK(t.notes == 18 && t.moves == 16, "two banks add");
    // The most beats of a bank the counting holds.
    uint16_t many[kQnMaxBeats + 40];
    for (uint16_t i = 0; i < kQnMaxBeats + 40; i++) many[i] = static_cast<uint16_t>(i * 7);
    QnTotals m;
    memset(&m, 0, sizeof m);
    qnAddBank(m, many, kQnMaxBeats + 40, g16, 100, 60000);
    CHECK(m.notes == kQnMaxBeats, "more beats than the limit are not counted past it");

    // The preview: the first two beats only, at most sixteen notes.
    QnNote pv[kQnMaxNotes];
    const uint16_t spread[12] = {0, 300, 700, 1599, 1600, 1700, 2400, 3000, 5000, 6400, 9000, 12000};
    CHECK(qnPreview(spread, 12, g16, 100, song, pv) == 4 && pv[3].start == 1599 && pv[0].land == 0 && pv[1].land == quantizeSnap(300, g16, 100, song), "the preview holds the notes before 1600");
    uint16_t dense[40];
    for (int i = 0; i < 40; i++) dense[i] = static_cast<uint16_t>(i * 30);
    CHECK(qnPreview(dense, 40, g16, 100, song, pv) == kQnMaxNotes, "at most sixteen notes");
    CHECK(qnPreview(spread, 4, kQuantizeGridOff, 100, song, pv) == 4 && pv[1].land == pv[1].start, "OFF: the notes stay");

    // The APPLY field: two turns to the right.
    bool apply = true;
    QnApplyState s = qnApplyTurn(QN_READY, true, apply);
    CHECK(s == QN_SURE && !apply, "the first turn arms");
    s = qnApplyTurn(s, true, apply);
    CHECK(s == QN_DONE && apply, "the second turn applies");
    s = qnApplyTurn(s, true, apply);
    CHECK(s == QN_SURE && !apply, "after DONE a turn arms again");
    s = qnApplyTurn(s, false, apply);
    CHECK(s == QN_READY && !apply, "a turn left disarms");
    s = qnApplyTurn(QN_READY, false, apply);
    CHECK(s == QN_READY && !apply, "left on READY stays");
    s = qnApplyTurn(QN_NOGRID, true, apply);
    CHECK(s == QN_NOGRID && !apply, "no grid: nothing to apply");
    CHECK(strcmp(qnApplyName(QN_SURE), "SURE?") == 0 && strcmp(qnScopeName(2), "ALL LAYERS") == 0 && strcmp(qnScopeName(9), "THIS LAYER") == 0, "names");

    // The screens: inside the area, legal text, no overlap.
    struct Case { const char* name; uint8_t grid, strength, scope; QnApplyState apply; };
    const Case cases[] = {{"demo", g16, 75, 0, QN_READY}, {"sure", g16, 100, 1, QN_SURE}, {"done", g16, 50, 2, QN_DONE}, {"off", kQuantizeGridOff, 100, 0, QN_NOGRID}, {"fine", gridOfStep(25), 100, 0, QN_READY}, {"triplet", 7, 100, 1, QN_READY}};
    for (const Case& k : cases) {
        FakeLcd lcd;
        lcd.dirtyArea();
        QnTotals tt;
        memset(&tt, 0, sizeof tt);
        qnAddBank(tt, notes, 9, k.grid, k.strength, song);
        qnAddBank(tt, pair, 3, k.grid, k.strength, song);
        QnNote pn[kQnMaxNotes];
        const uint8_t pc = qnPreview(notes, 9, k.grid, k.strength, song, pn);
        qnDrawFrame(lcd);
        qnDrawTitle(lcd, "QUANTIZE NOW   LAYER 03  HAT CLOSED   BANK 2");
        qnDrawScope(lcd, k.scope);
        qnDrawTotals(lcd, tt);
        qnDrawApply(lcd, k.apply);
        qnDrawPicture(lcd, k.grid, k.strength, pn, pc, song);
        checkScreen(lcd, k.name);
        CHECK(lcd.ops > 50, "%s: drawn", k.name);
        char name[48];
        snprintf(name, sizeof name, "qnow_%s", k.name);
        lcd.dump(name);
    }
    // Sixteen notes in the picture still fit.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        QnNote pn[kQnMaxNotes];
        const uint8_t pc = qnPreview(dense, 40, g16, 100, song, pn);
        qnDrawPicture(lcd, g16, 100, pn, pc, song);
        checkScreen(lcd, "sixteen notes");
    }
    // The picture is where the numbers put it: a note is a yellow mark on the NOW strip and a cyan or green one on AFTER.
    {
        FakeLcd lcd;
        lcd.dirtyArea();
        const QnNote one[1] = {{212, 200}};
        qnDrawPicture(lcd, g16, 100, one, 1, song);
        CHECK(pixelAt(lcd, qnX(212), kQnNowY + kQnStripH / 2 - 3) == kGsYellow, "the note on the NOW strip");
        CHECK(pixelAt(lcd, qnX(200), kQnAfterY + kQnStripH / 2 - 3) == kGsGreen, "a note on a grid point is green on the AFTER strip");
        const QnNote off2[1] = {{212, 212}};
        qnDrawPicture(lcd, g16, 0, off2, 1, song);
        CHECK(pixelAt(lcd, qnX(212), kQnAfterY + kQnStripH / 2 - 3) == kGsCyan, "a note off the grid is cyan");
        // A bar is yellow for an earlier landing and cyan for a later one.
        const QnNote two[2] = {{212, 200}, {195, 200}};
        qnDrawPicture(lcd, g16, 100, two, 2, song);
        CHECK(pixelAt(lcd, kQnStripX + 16, kQnBarBase - 2) == kGsYellow, "an earlier landing: the bar is yellow");
        CHECK(pixelAt(lcd, kQnStripX + kQnBarStep + 16, kQnBarBase - 2) == kGsCyan, "a later landing: the bar is cyan");
    }
    // The geometry.
    CHECK(kQnScopeY + 2 * kQnScopeStep + kQnScopeH < kQnCountY && kQnCountY + 5 * kQnCountStep <= kQnApplyY && kQnApplyY + kQnApplyH < kQnHintY && kQnHintY + 10 + 7 <= kGsAreaY + kGsAreaH && kQnScopeX + 22 * 7 < kQnStripX, "left column in order");
    CHECK(kQnStripX + kQnStripW + 6 <= kGsAreaX + kGsAreaW && kQnNumberY + 7 <= kGsAreaY + kGsAreaH && kQnStripX + kQnMaxNotes * kQnBarStep <= kGsAreaX + kGsAreaW, "right side inside");
    CHECK(kQnAfterY + kQnStripH < kQnCaptionY && kQnCaptionY + 7 < kQnBarsLabelY && kQnBarsLabelY + 9 < kQnBarBase - kQnBarMax && kQnBarBase < kQnNumberY, "right side in order");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
