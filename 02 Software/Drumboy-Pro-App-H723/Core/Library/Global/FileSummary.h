// What a slot file holds, for the load and save screens (FileScreens.h): the numbers the card of a
// slot shows, read from the file's own blocks. Hardware-free so it can be tested on a PC
// (test_filesummary); the Controller reads the bytes from the card and calls these.
//
// Song file (File_NNN.rwf): 25 bytes of header, the kit block (1500 bytes), then one block per layer
// (1286 bytes: 'l', then 5 banks of 257 bytes: 'b' and 64 beats of 4 bytes: the packed beat, the start
// interval). The kit block holds the tempo, MEASURE and BAR (bytes 13 to 15) and, from byte 500, one
// record per layer of 100 bytes: an 'x' or 'o' and the instrument's name (32 bytes), and at +50 the same
// for its sample. A drumkit file (Drumkit_NNN.rwd) is the header and the same kit block.
#ifndef __FILESUMMARY_H
#define __FILESUMMARY_H

#include <stdint.h>
#include <string.h>

#include "FileScreens.h"
#include "Fill.h"  // Beat: the packed beat

const uint16_t kFsHeaderBytes = 25;        // strlen(kFileRef) = strlen(kDrumkitRef)
const uint16_t kFsKitBytes = 1500;         // kDrumkitByteSize
const uint16_t kFsLayerBlockBytes = 1286;  // kSongByteSize
const uint16_t kFsBankOffset = 1;          // the first bank inside a layer block
const uint16_t kFsBankBytes = 257;         // 'b' and 64 beats of 4 bytes
const uint16_t kFsBeatsPerBank = 64;       // kBeatLibrarySize
const uint16_t kFsAssetOffset = 500, kFsAssetStep = 100, kFsAssetNameSize = 32;
const uint16_t kFsMeasureInterval = 800;   // kMeasureInterval: a song is this long times MEASURE times BAR
static_assert(kFsBeatsPerBank == kBeatLibrarySize, "the bank holds kBeatLibrarySize beats");

// Where a layer's first bank starts in a song file.
inline uint32_t fsFileLayerBankOffset(uint8_t layer_) { return kFsHeaderBytes + kFsKitBytes + static_cast<uint32_t>(layer_) * kFsLayerBlockBytes + kFsBankOffset; }

inline uint32_t fsSongInterval(uint8_t measure_, uint8_t bar_) { return static_cast<uint32_t>(kFsMeasureInterval) * measure_ * bar_; }

// A name from a file or from memory as the card shows it: printable characters only, the extension of a
// sample file cut, 12 characters. (The font has no lowercase: it draws capitals for both.)
inline void fsCleanName(const uint8_t* in_, uint8_t inSize_, char* out_, uint8_t outSize_) {
    uint8_t n = 0;
    uint8_t end = 0;
    while ((end < inSize_) && (in_[end] != 0)) end++;
    for (uint8_t i = end; i > 0; i--) {  // ".wav" and the like: cut at the last dot
        if (in_[i - 1] == '.') {
            if (end - i <= 4) end = static_cast<uint8_t>(i - 1);
            break;
        }
    }
    for (uint8_t i = 0; (i < end) && (n + 1 < outSize_); i++) {
        const uint8_t c = in_[i];
        out_[n++] = ((c >= 32) && (c < 127)) ? static_cast<char>(c) : ' ';
    }
    out_[n] = 0;
}

// The kit block: tempo, MEASURE, BAR and the instrument (and, for a kit, the sample) names. Returns false
// when the block is not a kit block that this firmware wrote ('*' marker, values in range).
inline bool fsParseKit(const uint8_t* kit_, FsSummary& s_) {
    if (kit_[0] != '*') return false;
    const uint8_t tempo = kit_[13], measure = kit_[14], bar = kit_[15];
    if ((measure < 1) || (measure > 12) || (bar < 1) || (bar > 12)) return false;
    s_.tempo = tempo;
    s_.measure = measure;
    s_.bar = bar;
    for (uint8_t l = 0; l < kFsLanes; l++) {
        const uint8_t* record = kit_ + kFsAssetOffset + l * kFsAssetStep;
        s_.inst[l][0] = 0;
        s_.sample[l][0] = 0;
        if (record[0] == 'x') fsCleanName(record + 1, static_cast<uint8_t>(kFsAssetNameSize), s_.inst[l], kFsNameSize);
        if (record[50] == 'x') fsCleanName(record + 51, static_cast<uint8_t>(kFsAssetNameSize), s_.sample[l], kFsNameSize);
    }
    s_.valid = true;
    return true;
}

// One layer's first bank: the beats as marks over kFsThumbCols columns of the whole song. level: FLAT 100,
// GHOST 50, MUTED 20, a fill 70.
inline void fsParseBank(const uint8_t* bank_, uint8_t lane_, uint32_t songInterval_, FsSummary& s_) {
    if ((lane_ >= kFsLanes) || (songInterval_ == 0)) return;
    for (uint8_t c = 0; c < kFsThumbCols; c++) s_.lane[lane_][c] = 0;
    if (bank_[0] != 'b') return;
    for (uint16_t k = 0; k < kFsBeatsPerBank; k++) {
        const uint8_t* beat = bank_ + 1 + k * 4;
        const uint16_t packed = static_cast<uint16_t>((beat[0] << 8) | beat[1]);
        const uint16_t start = static_cast<uint16_t>((beat[2] << 8) | beat[3]);
        if (!(packed & Beat::ACTIVE_MASK)) continue;
        if (start >= songInterval_) continue;  // past the end of the song (a damaged file)
        const uint16_t type = (packed & Beat::TYPE_MASK) >> Beat::TYPE_SHIFT;
        const uint16_t pattern = (packed & Beat::PATTERN_MASK) >> Beat::PATTERN_SHIFT;
        uint8_t level = 100;
        if (type != 0) {
            level = 70;
        } else if (pattern == 1) {
            level = 50;
        } else if (pattern == 2) {
            level = 20;
        }
        const uint8_t col = static_cast<uint8_t>((static_cast<uint64_t>(start) * kFsThumbCols) / songInterval_);
        if (col < kFsThumbCols) s_.lane[lane_][col] = (level > s_.lane[lane_][col]) ? level : s_.lane[lane_][col];
    }
}

#endif
