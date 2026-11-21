// Writing and recovering slot files (song, drumkit) so a save is atomic and checked
// (AtomicSave.h explains the scheme). Only FatFs is used, so it runs on a PC against a mock
// (02 Software/tests/mock/ff.h, test_atomicsave).
#ifndef __SLOTFILE_H
#define __SLOTFILE_H

#include <stdint.h>
#include <string.h>

#include "Autosave.h"  // autosaveCrc32
#include "AtomicSave.h"
#include "ff.h"

// A slot file is complete when it holds its header, its body and the "EOF" marker the save writes
// right after the body. A file cut short (power lost during a save, a full card) is not: it is treated
// as an empty slot instead of being loaded as zeros (every level 0, an empty song) over the user's
// current work. f_ is open.
inline bool slotFileComplete(FIL* f_, uint32_t headerBytes_, uint32_t bodyBytes_) {
    const uint32_t eofAt = headerBytes_ + bodyBytes_;
    if (f_size(f_) < eofAt + 3u) return false;
    char tail[3] = {0, 0, 0};
    UINT got = 0;
    if ((f_lseek(f_, eofAt) != FR_OK) || (f_read(f_, tail, 3, &got) != FR_OK) || (got != 3)) return false;
    return (tail[0] == 'E') && (tail[1] == 'O') && (tail[2] == 'F');
}

// Writes one slot file and checks it. Every byte goes through here: a write that puts out fewer bytes
// than asked (full card, card error) is a failure, which an f_write result alone does not tell.
class SaveWriter {
  public:
    explicit SaveWriter(FIL* file_) : file(file_) {}

    void write(const void* data_, uint32_t size_) {
        if (!good || (size_ == 0)) return;
        UINT written = 0;
        if ((f_write(file, data_, size_, &written) != FR_OK) || (written != size_)) {
            good = false;
            return;
        }
        total += size_;
        crc = autosaveCrc32(crc, static_cast<const uint8_t*>(data_), size_);
    }

    // The same bytes in 512-byte blocks (a card sector), as the slot files have always been written.
    void writeBlocks(const void* data_, uint32_t size_) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data_);
        for (uint32_t at = 0; at < size_; at += 512) write(bytes + at, ((size_ - at) > 512) ? 512u : (size_ - at));
    }

    bool ok() const { return good; }
    uint32_t bytes() const { return total; }

    // Closes the file (path_ is its name) and reads it back: as long as what was written and with
    // the same CRC-32. True only when everything was written and is on the card as intended.
    bool finish(const char* path_) {
        bool whole = good && (f_size(file) == total);
        whole = (f_close(file) == FR_OK) && whole;
        if (!whole) return false;
        if (f_open(file, path_, FA_READ) != FR_OK) return false;
        uint8_t block[512];
        uint32_t remaining = total, readCrc = 0;
        whole = (f_size(file) == total);
        while (whole && (remaining > 0)) {
            const UINT chunk = (remaining > sizeof block) ? static_cast<UINT>(sizeof block) : static_cast<UINT>(remaining);
            UINT got = 0;
            if ((f_read(file, block, chunk, &got) != FR_OK) || (got != chunk)) {
                whole = false;
                break;
            }
            readCrc = autosaveCrc32(readCrc, block, chunk);
            remaining -= chunk;
        }
        f_close(file);
        return whole && (readCrc == crc);
    }

  private:
    FIL* file;
    bool good = true;
    uint32_t total = 0;
    uint32_t crc = 0;
};

// Puts the verified .new file in the place of the slot. A power cut between the two steps leaves
// the .new file, which slotRecover puts in place at the next start. If the old slot can't be deleted
// nothing has changed: the .new file is dropped.
inline bool slotReplace(const char* readyPath_, const char* finalPath_) {
    const FRESULT removed = f_unlink(finalPath_);
    if ((removed != FR_OK) && (removed != FR_NO_FILE) && (removed != FR_NO_PATH)) {
        f_unlink(readyPath_);
        return false;
    }
    return f_rename(readyPath_, finalPath_) == FR_OK;
}

// Saves a slot file. fill_(SaveWriter&) writes the content. With atomic_ the content goes to the
// .tmp file, is verified, waits as .new and replaces the slot (AtomicSave.h); otherwise (the autosave
// files, which have their own seal) it is written to finalPath_ directly. True = done.
// On a failure the slot is as it was (atomic) and the .tmp file is gone.
template <typename Fill>
inline bool slotSave(FIL* f_, const char* finalPath_, bool atomic_, Fill fill_) {
    char tempPath[64], readyPath[64];
    const bool viaTemp = atomic_ && atomicTempPath(tempPath, sizeof tempPath, finalPath_) && atomicReadyPath(readyPath, sizeof readyPath, finalPath_);
    const char* target = viaTemp ? tempPath : finalPath_;
    f_close(f_);
    if (f_open(f_, target, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        f_close(f_);
        if (viaTemp) f_unlink(target);
        return false;
    }
    SaveWriter out(f_);
    fill_(out);
    if (!out.finish(target)) {
        if (viaTemp) f_unlink(target);
        return false;
    }
    if (!viaTemp) return true;
    f_unlink(readyPath);  // a leftover of an earlier failed save
    if (f_rename(tempPath, readyPath) != FR_OK) {
        f_unlink(tempPath);
        return false;
    }
    return slotReplace(readyPath, finalPath_);
}

// The state of a slot file: whole (header with ref_, body of bodyBytes_, EOF), there but not whole, or missing.
inline SlotFileState slotFileStateOf(FIL* f_, const char* path_, const char* ref_, uint32_t bodyBytes_) {
    if (f_open(f_, path_, FA_READ) != FR_OK) return SLOTFILE_MISSING;
    SlotFileState state = SLOTFILE_INCOMPLETE;
    const uint32_t refSize = static_cast<uint32_t>(strlen(ref_));
    char head[48];
    UINT got = 0;
    if ((refSize <= sizeof head) && (f_read(f_, head, refSize, &got) == FR_OK) && (got == refSize) && (memcmp(head, ref_, refSize) == 0) &&
        slotFileComplete(f_, refSize, bodyBytes_)) {
        state = SLOTFILE_COMPLETE;
    }
    f_close(f_);
    return state;
}

// Start-up: finishes or drops the save of this slot that a power cut or a crash interrupted:
// a .tmp file is deleted, a whole .new file becomes the slot.
inline void slotRecover(FIL* f_, const char* finalPath_, const char* ref_, uint32_t bodyBytes_) {
    char tempPath[64], readyPath[64];
    if (!atomicTempPath(tempPath, sizeof tempPath, finalPath_) || !atomicReadyPath(readyPath, sizeof readyPath, finalPath_)) return;
    if (f_open(f_, tempPath, FA_READ) == FR_OK) {
        f_close(f_);
        f_unlink(tempPath);
    }
    switch (atomicRecovery(slotFileStateOf(f_, readyPath, ref_, bodyBytes_))) {
        case SAVERECOVERY_PROMOTE_READY:
            slotReplace(readyPath, finalPath_);
            break;
        case SAVERECOVERY_DELETE_READY:
            f_unlink(readyPath);
            break;
        default:
            break;
    }
}

#endif  // __SLOTFILE_H
