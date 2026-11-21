// Host-side tests for what a slot file shows (FileSummary.h): the kit block (tempo, MEASURE, BAR and
// the names), a layer's first bank as marks over the song, names cleaned for the card, and the offsets
// of a song file. The files are built the way Controller::file_encodeKit and file_encodeLayerSong write
// them (with the real Beat).

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "FileSummary.h"

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

// The kit block as file_encodeKit writes what the summary reads.
static std::vector<uint8_t> makeKit(uint8_t tempo, uint8_t measure, uint8_t bar, const char* const* inst, const char* const* sample, uint8_t layersLoaded) {
    std::vector<uint8_t> d(kFsKitBytes, 0);
    d[0] = '*';
    d[13] = tempo;
    d[14] = measure;
    d[15] = bar;
    for (uint8_t i = 0; i < 10; i++) {
        const uint16_t o = 500 + i * 100;
        if (i < layersLoaded) {
            d[o] = 'x';
            memcpy(&d[o + 1], inst[i], strlen(inst[i]));
            d[o + 50] = 'x';
            memcpy(&d[o + 51], sample[i], strlen(sample[i]));
        } else {
            d[o] = 'o';
            d[o + 50] = 'o';
        }
    }
    return d;
}

// One layer block as file_encodeLayerSong writes it, with the real Beat.
static std::vector<uint8_t> makeLayer(const std::vector<Beat>& firstBank) {
    std::vector<uint8_t> d(kFsLayerBlockBytes, 0);
    d[0] = 'l';
    for (uint8_t b = 0; b < 5; b++) {
        const uint16_t offsetBank = 1 + b * 257;
        d[offsetBank] = 'b';
        for (uint8_t k = 0; k < 64; k++) {
            Beat beat;
            if ((b == 0) && (k < firstBank.size())) beat = firstBank[k];
            const uint16_t offsetBeat = offsetBank + 1 + k * 4;
            d[offsetBeat + 0] = static_cast<uint8_t>(beat.getData() >> 8);
            d[offsetBeat + 1] = static_cast<uint8_t>(beat.getData() & 0xFF);
            d[offsetBeat + 2] = static_cast<uint8_t>(beat.getStartInterval() >> 8);
            d[offsetBeat + 3] = static_cast<uint8_t>(beat.getStartInterval() & 0xFF);
        }
    }
    return d;
}

static Beat makeBeat(uint16_t start, uint8_t type, uint8_t pattern) {
    Beat b;
    b.setActive(true);
    b.setFillType(type);
    b.setFillPattern(pattern);
    b.setStartInterval(start);
    return b;
}

static void testOffsets() {
    CHECK(fsFileLayerBankOffset(0) == 25 + 1500 + 1, "layer 0, bank 0: %u", fsFileLayerBankOffset(0));
    CHECK(fsFileLayerBankOffset(3) == 25 + 1500 + 3 * 1286 + 1, "layer 3");
    CHECK(fsFileLayerBankOffset(9) + 257 + 4 * 257 <= 25u + 1500u + 10u * 1286u, "the last layer's block ends the song data");
    CHECK(kFsAssetOffset + 9 * kFsAssetStep + 51 + kFsAssetNameSize <= kFsKitBytes, "the last asset record fits the kit block");
    CHECK(fsSongInterval(4, 4) == 12800, "800 x 4 x 4");
    CHECK(fsSongInterval(12, 12) == 115200, "the longest song");
}

static void testKit() {
    const char* inst[10] = {"808 KICK", "SNARE TIGHT", "CLAP", "HAT CLOSED", "HAT OPEN", "TOM LOW", "TOM HIGH", "RIDE", "PERC", "FX"};
    const char* sample[10] = {"bd_deep_02.wav", "sd_tight_1.WAV", "cp_wide_3.wav", "hh_c_07.wav", "hh_o_02", "tm_lo_01.wav", "tm_hi_01.wav", "rd_bell_1.wav", "pc_conga_4.wav", "fx_sweep_9.wav"};
    FsSummary s;
    CHECK(fsParseKit(makeKit(124, 4, 4, inst, sample, 10).data(), s), "a kit block");
    CHECK(s.valid && (s.tempo == 124) && (s.measure == 4) && (s.bar == 4), "tempo %u M %u B %u", s.tempo, s.measure, s.bar);
    CHECK(strcmp(s.inst[0], "808 KICK") == 0, "'%s'", s.inst[0]);
    CHECK(strcmp(s.inst[3], "HAT CLOSED") == 0, "'%s'", s.inst[3]);
    CHECK(strcmp(s.sample[0], "bd_deep_02") == 0, "the extension is cut: '%s'", s.sample[0]);
    CHECK(strcmp(s.sample[1], "sd_tight_1") == 0, "also in capitals: '%s'", s.sample[1]);
    CHECK(strcmp(s.sample[4], "hh_o_02") == 0, "no extension: '%s'", s.sample[4]);
    CHECK(strcmp(s.sample[9], "fx_sweep_9") == 0, "'%s'", s.sample[9]);
    // Layers without an instrument or a sample show nothing.
    FsSummary t;
    CHECK(fsParseKit(makeKit(90, 2, 8, inst, sample, 4).data(), t), "a kit with four layers");
    CHECK((strcmp(t.inst[3], "HAT CLOSED") == 0) && (t.inst[4][0] == 0) && (t.sample[9][0] == 0), "empty layers are empty: '%s' '%s'", t.inst[4], t.sample[9]);
    // A name is cut at 12 characters and holds no control character.
    std::vector<uint8_t> k = makeKit(100, 4, 4, inst, sample, 1);
    const char* longName = "A VERY LONG INSTRUMENT NAME";
    memset(&k[501], 0, 32);
    memcpy(&k[501], longName, strlen(longName));
    k[501 + 2] = 0x07;  // a control character in the middle
    FsSummary u;
    CHECK(fsParseKit(k.data(), u), "the long name");
    CHECK(strlen(u.inst[0]) == 12, "cut at 12: '%s'", u.inst[0]);
    CHECK(u.inst[0][2] == ' ', "a control character becomes a space");
    // Not a kit block: no marker, MEASURE or BAR out of range.
    FsSummary v;
    std::vector<uint8_t> bad = makeKit(120, 4, 4, inst, sample, 10);
    bad[0] = '-';
    CHECK(!fsParseKit(bad.data(), v) && !v.valid, "the slot marker is not '*'");
    bad = makeKit(120, 0, 4, inst, sample, 10);
    CHECK(!fsParseKit(bad.data(), v), "MEASURE 0");
    bad = makeKit(120, 4, 13, inst, sample, 10);
    CHECK(!fsParseKit(bad.data(), v), "BAR 13");
    bad = makeKit(120, 255, 255, inst, sample, 10);
    CHECK(!fsParseKit(bad.data(), v), "garbage");
}

static void testBank() {
    const uint32_t songInterval = fsSongInterval(4, 4);  // 12800
    std::vector<Beat> beats;
    for (uint8_t i = 0; i < 4; i++) beats.push_back(makeBeat(static_cast<uint16_t>(i * songInterval / 4), 0, 0));  // four flat beats a quarter apart
    beats.push_back(makeBeat(1600, 0, 1));                                                                        // a ghost
    beats.push_back(makeBeat(4800, 0, 2));                                                                        // muted
    beats.push_back(makeBeat(8000, 3, 0));                                                                        // a fill
    const std::vector<uint8_t> layer = makeLayer(beats);
    FsSummary s;
    fsParseBank(&layer[1], 2, songInterval, s);
    CHECK(s.lane[2][0] == 100, "beat at 0: %u", s.lane[2][0]);
    CHECK(s.lane[2][kFsThumbCols / 4] > 0, "beat at a quarter: col %u = %u", kFsThumbCols / 4, s.lane[2][kFsThumbCols / 4]);
    unsigned marks = 0;
    for (uint8_t c = 0; c < kFsThumbCols; c++) marks += s.lane[2][c] ? 1 : 0;
    CHECK(marks == 7, "seven beats, seven marks: %u", marks);
    CHECK(s.lane[2][(1600u * kFsThumbCols) / songInterval] == 50, "the ghost is 50");
    CHECK(s.lane[2][(4800u * kFsThumbCols) / songInterval] == 20, "the muted one is 20");
    CHECK(s.lane[2][(8000u * kFsThumbCols) / songInterval] == 70, "the fill is 70");
    // Other lanes stay as they were (empty).
    for (uint8_t c = 0; c < kFsThumbCols; c++) CHECK((s.lane[0][c] == 0) && (s.lane[5][c] == 0), "other lanes are empty (col %u)", c);
    // Two beats in one column: the louder one shows.
    std::vector<Beat> two = {makeBeat(0, 0, 1), makeBeat(5, 0, 0)};
    const std::vector<uint8_t> twoLayer = makeLayer(two);
    FsSummary t;
    fsParseBank(&twoLayer[1], 0, songInterval, t);
    CHECK(t.lane[0][0] == 100, "the louder of two beats in a column: %u", t.lane[0][0]);
    // A beat past the end of the song, and a bank that is not a bank.
    std::vector<Beat> late = {makeBeat(static_cast<uint16_t>(songInterval), 0, 0), makeBeat(60000, 0, 0)};
    const std::vector<uint8_t> lateLayer = makeLayer(late);
    FsSummary u;
    fsParseBank(&lateLayer[1], 0, songInterval, u);
    for (uint8_t c = 0; c < kFsThumbCols; c++) CHECK(u.lane[0][c] == 0, "beats past the song end are dropped (col %u)", c);
    std::vector<uint8_t> notBank = twoLayer;
    notBank[1] = 'x';
    FsSummary w;
    w.lane[0][0] = 99;
    fsParseBank(&notBank[1], 0, songInterval, w);
    CHECK(w.lane[0][0] == 0, "no 'b': empty");
    // The last column and an out-of-range lane or interval do not write outside.
    std::vector<Beat> edge = {makeBeat(static_cast<uint16_t>(songInterval - 1), 0, 0)};
    const std::vector<uint8_t> edgeLayer = makeLayer(edge);
    FsSummary e;
    fsParseBank(&edgeLayer[1], 0, songInterval, e);
    CHECK(e.lane[0][kFsThumbCols - 1] == 100, "the last column");
    fsParseBank(&edgeLayer[1], 10, songInterval, e);
    fsParseBank(&edgeLayer[1], 0, 0, e);
    // An inactive beat with data is not drawn.
    Beat off = makeBeat(100, 0, 0);
    off.setActive(false);
    const std::vector<uint8_t> offLayer = makeLayer({off});
    FsSummary o;
    fsParseBank(&offLayer[1], 0, songInterval, o);
    for (uint8_t c = 0; c < kFsThumbCols; c++) CHECK(o.lane[0][c] == 0, "inactive beats are not drawn");
}

// The factory slot files on the repository's SD card image (if the test runs from the tests folder): every
// file with the '*' marker reads, with names and beats.
static std::vector<uint8_t> readFile(const std::string& path) {
    std::vector<uint8_t> v;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return v;
    uint8_t buffer[4096];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) v.insert(v.end(), buffer, buffer + n);
    std::fclose(f);
    return v;
}

static void testFactoryFiles() {
    const std::string base = "../Sd Card/System";
    int songsUsed = 0, kitsUsed = 0;
    for (int i = 1; i <= 100; i++) {
        char name[256];
        std::snprintf(name, sizeof name, "%s/File/File_%03d.rwf", base.c_str(), i);
        std::vector<uint8_t> d = readFile(name);
        if (d.size() > kFsHeaderBytes + kFsKitBytes && d[kFsHeaderBytes] == '*') {
            songsUsed++;
            FsSummary s;
            CHECK(fsParseKit(&d[kFsHeaderBytes], s), "song %d: the kit block reads", i);
            CHECK(d.size() == kFsHeaderBytes + 14360u + 3u, "song %d: size %zu (header, 14360 bytes, EOF)", i, d.size());
            CHECK((s.tempo >= 40) && (s.tempo <= 240), "song %d: tempo %u", i, s.tempo);
            CHECK(s.inst[0][0] != 0, "song %d: the first instrument has a name", i);
            const uint32_t interval = fsSongInterval(s.measure, s.bar);
            unsigned marks = 0;
            for (uint8_t l = 0; l < kFsLanes; l++) {
                CHECK(d.size() >= fsFileLayerBankOffset(l) + kFsBankBytes, "song %d lane %u: the bank is in the file", i, l);
                fsParseBank(&d[fsFileLayerBankOffset(l)], l, interval, s);
                for (uint8_t c = 0; c < kFsThumbCols; c++) marks += s.lane[l][c] ? 1 : 0;
            }
            CHECK(marks > 0, "song %d: it has beats", i);
        }
        std::snprintf(name, sizeof name, "%s/Drumkit/Drumkit_%03d.rwd", base.c_str(), i);
        d = readFile(name);
        if (d.size() > kFsHeaderBytes + kFsKitBytes && d[kFsHeaderBytes] == '*') {
            kitsUsed++;
            FsSummary s;
            CHECK(fsParseKit(&d[kFsHeaderBytes], s), "kit %d: the kit block reads", i);
            CHECK(s.inst[0][0] != 0 && s.sample[0][0] != 0, "kit %d: the first instrument and sample have names", i);
        }
    }
    std::printf("factory files: %d songs and %d kits read\n", songsUsed, kitsUsed);
}

int main() {
    testOffsets();
    testFactoryFiles();
    testKit();
    testBank();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
