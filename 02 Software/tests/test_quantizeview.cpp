// Host-side tests for the numbers of the quantize screen (QuantizeView.h): the points of every grid on the
// strip, who owns them, the counts per beat, the kinds and the strength demo.

#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#include "QuantizeView.h"
#include "fakelcd.h"  // glyphIndex: the font's character map

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

static void testKinds() {
    for (uint8_t g = 0; g < 7; g++) CHECK(qvKind(g) == QV_STRAIGHT, "grid %u straight", g);
    for (uint8_t g = 7; g <= 10; g++) CHECK(qvKind(g) == QV_TRIPLET, "grid %u triplet", g);
    CHECK(qvKind(11) == QV_MIXED && qvKind(12) == QV_MIXED && qvKind(kQuantizeGridOff) == QV_OFF, "mixed and OFF");
}

static void testTicks() {
    for (uint8_t g = 0; g < kQuantizeGridCount; g++) {
        QvTicks t;
        qvTicks(g, t);
        if (g == kQuantizeGridOff) {
            CHECK(t.n == 0, "OFF has no points");
            continue;
        }
        // The same points as Quantize.h's closed formulas give, in order, inside the strip, starting at 0.
        const QuantizeSpec spec = quantizeSpec(g);
        std::set<uint32_t> want;
        for (uint32_t p3 : {spec.p3, spec.p3b}) {
            if (p3 == 0) continue;
            for (uint32_t k = 0; quantizePointOf(p3, k) < kQvSpan; k++) want.insert(quantizePointOf(p3, k));
        }
        CHECK(t.n == want.size() && t.n <= kQvMaxTicks, "grid %u: %u ticks, expected %zu", g, t.n, want.size());
        uint8_t i = 0;
        for (uint32_t p : want) {
            CHECK(i < t.n && t.at[i] == p, "grid %u tick %u", g, i);
            i++;
        }
        CHECK(t.n > 0 && t.at[0] == 0, "grid %u starts at 0", g);
        for (uint8_t k = 0; k < t.n; k++) {
            CHECK(t.owner[k] >= 1 && t.owner[k] <= 3, "grid %u owner", g);
            CHECK(qvStripXOf(t.at[k]) >= kQvStripX && qvStripXOf(t.at[k]) < kQvStripX + kQvStripW, "grid %u tick %u on the strip", g, k);
            if (k > 0) CHECK(qvStripXOf(t.at[k]) > qvStripXOf(t.at[k - 1]), "grid %u: ticks %u and %u on the same column", g, k - 1, k);
        }
    }
    // A few exact ones.
    QvTicks t;
    qvTicks(9, t);  // 1:8T: 0, 267, 533, 800, 1067, 1333
    CHECK(t.n == 6 && t.at[1] == 267 && t.at[2] == 533 && t.at[3] == 800 && t.at[5] == 1333 && t.owner[1] == 2, "1:8T points");
    qvTicks(11, t);  // 1:16+T: both at 0, 400, 800 ...; straight at 200, 600; triplet at 133, 267
    CHECK(t.at[0] == 0 && t.owner[0] == 3 && t.at[1] == 133 && t.owner[1] == 2 && t.at[2] == 200 && t.owner[2] == 1 && t.at[4] == 400 && t.owner[4] == 3, "1:16+T points and owners");
    qvTicks(2, t);
    CHECK(t.n == 8 && t.owner[0] == 1 && t.owner[7] == 1, "1:16 has 8 straight points in two beats");
    qvTicks(6, t);
    CHECK(t.n == 1 && t.at[0] == 0, "1:1 has only the start in two beats");
    // The strip's edges: 0 at its first column, the song end at its last.
    CHECK(qvStripXOf(0) == kQvStripX && qvStripXOf(kQvSpan) == kQvStripX + kQvStripW, "strip edges");
    CHECK(qvPointsPerBeat(2) == 4 && qvPointsPerBeat(9) == 3 && qvPointsPerBeat(8) == 6 && qvPointsPerBeat(11) == 8 && qvPointsPerBeat(12) == 4 && qvPointsPerBeat(kQuantizeGridOff) == 0 && qvPointsPerBeat(0) == 16, "points per beat");
}

static void testDemo() {
    CHECK(!qvDemo(kQuantizeGridOff, 100).valid, "OFF has no demo");
    for (uint8_t g = 0; g < kQuantizeGridCount - 1; g++) {
        for (uint8_t strength : {0, 5, 25, 50, 75, 95, 100}) {
            const QvDemo d = qvDemo(g, strength);
            CHECK(d.valid && d.lateBy >= 1 && d.lateBy <= 30, "grid %u: lateBy %u", g, d.lateBy);
            CHECK(d.played == d.point + d.lateBy, "grid %u: played", g);
            // The point is a point of the grid, the late hit is nearer to it than to any other point, and the landing is the firmware's.
            CHECK(quantizeNearest(d.played, g) == d.point, "grid %u: the late hit snaps to the point (%u -> %u, point %u)", g, d.played, static_cast<unsigned>(quantizeNearest(d.played, g)), d.point);
            CHECK(d.landed == quantizeSnap(d.played, g, strength, kQvSpan), "grid %u strength %u: landing", g, strength);
            CHECK(d.landed >= d.point && d.landed <= d.played, "grid %u strength %u: lands between the point and where it was played", g, strength);
            CHECK(d.landedBy == static_cast<int>(d.landed) - d.point, "grid %u: landedBy", g);
            if (strength == 100) CHECK(d.landedBy == 0, "grid %u: full strength lands on the point", g);
            if (strength == 0) CHECK(d.landed == d.played, "grid %u: strength 0 leaves the hit", g);
        }
    }
    // The grids' demos: 1:16 plays 30 late, 1:64 less (the gap is 50).
    CHECK(qvDemo(2, 100).lateBy == 30 && qvDemo(0, 100).lateBy == 16 && qvDemo(1, 100).lateBy == 30, "late by");
    const QvDemo half = qvDemo(2, 50);
    CHECK(half.landedBy == 15, "1:16 at 50 %%: 30 late lands 15 late (%d)", half.landedBy);
}

static void testTexts() {
    char a[64], b[64], c[64];
    for (uint8_t g = 0; g < kQuantizeGridCount; g++) {
        qvName(g, a, sizeof a);
        CHECK(a[0] != ' ' && strlen(a) >= 3 && strlen(a) <= 6, "name '%s'", a);
        qvKindLine1(g, a, sizeof a);
        qvKindLine2(g, b, sizeof b);
        const char* kind = qvKindName(qvKind(g));
        for (const char* text : {static_cast<const char*>(a), static_cast<const char*>(b), kind}) {
            for (const char* p = text; *p; p++) CHECK((glyphIndex(*p) != 52) || (*p == ' '), "grid %u text '%s' has '%c' the font lacks", g, text, *p);
        }
        CHECK(strlen(a) <= 35 && strlen(b) <= 35, "grid %u: the grid texts fit 35 characters of the 5x7 font", g);
    }
    // The second line: grammar and truth for every kind of grid.
    struct { uint8_t grid; const char* text; } line2[] = {
        {0, "16 POINTS IN A BEAT"}, {1, "8 POINTS IN A BEAT"}, {2, "4 POINTS IN A BEAT"}, {3, "2 POINTS IN A BEAT"}, {4, "1 POINT IN A BEAT"},
        {5, "A POINT EVERY 2 BEATS"}, {6, "A POINT EVERY 4 BEATS"}, {7, "12 POINTS IN A BEAT"}, {8, "6 POINTS IN A BEAT"}, {9, "3 POINTS IN A BEAT"},
        {10, "3 POINTS IN 2 BEATS"}, {11, "THE NEARER ONE WINS: 8 PER BEAT"}, {12, "THE NEARER ONE WINS: 4 PER BEAT"}, {13, "EVERY INTERVAL IS A POINT"}};
    for (const auto& l : line2) {
        qvKindLine2(l.grid, a, sizeof a);
        CHECK(!strcmp(a, l.text), "grid %u line 2 is '%s', expected '%s'", l.grid, a, l.text);
    }
    qvChangeNote(2, a, sizeof a);
    qvChangeNote(kQuantizeGridOff, b, sizeof b);
    CHECK(!strcmp(a, "A NEW GRID MOVES ALL BEATS ONTO IT") && !strcmp(b, "OFF KEEPS THE BEATS WHERE THEY ARE") && strlen(a) <= 35 && strlen(b) <= 35, "change notes");
    qvKindLine1(2, a, sizeof a);
    qvKindLine1(0, b, sizeof b);
    CHECK(!strcmp(a, "A POINT EVERY 200 INTERVALS") && !strcmp(b, "A POINT EVERY 50 INTERVALS"), "straight text");
    for (uint8_t s : {0, 1, 50, 99, 100}) {
        qvStrengthPhrase(s, a, sizeof a);
        for (const char* p = a; *p; p++) CHECK((glyphIndex(*p) != 52) || (*p == ' ') || (*p == '%'), "strength phrase '%s'", a);
        CHECK(strlen(a) <= 35, "phrase '%s' fits", a);
    }
    qvStrengthPhrase(50, a, sizeof a);
    CHECK(!strcmp(a, "A HIT MOVES 50 % OF THE WAY"), "phrase at 50: '%s'", a);
    qvDemoLines(qvDemo(2, 50), a, sizeof a, b, sizeof b);
    qvDemoLines(qvDemo(2, 100), a, sizeof a, c, sizeof c);
    CHECK(!strcmp(a, "PLAYED 30 LATE") && !strcmp(c, "LANDS ON THE POINT"), "demo lines");
    qvDemoLines(qvDemo(2, 50), a, sizeof a, b, sizeof b);
    CHECK(!strcmp(b, "LANDS 15 LATE"), "demo line at 50: '%s'", b);
}

int main() {
    testKinds();
    testTicks();
    testDemo();
    testTexts();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
