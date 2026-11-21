// Atomic saves: a slot file (song, drumkit) is never overwritten in place. A power cut, a crash
// or a card that fails half way through a SAVE must leave the slot with its old content or its
// new content, never with half of one. Hardware-free decisions, tested on a PC (see 02 Software/tests);
// the file work around them is in Controller/SlotFile.h.
//
// How a save goes (File_001.rwf is the slot):
//   1. The new content is written to File_001.tmp.
//   2. Every write must have put out all its bytes; the file must be as long as what was written;
//      reading it back must give the same CRC-32 (a card that took a write wrongly is found now,
//      not at the next load). Then File_001.tmp is renamed File_001.new: from now on the new
//      content is verified and complete.
//   3. The old slot file is deleted and File_001.new is renamed to File_001.rwf.
//   A failure in step 1 or 2 deletes the temporary file: the slot is untouched.
//
// What a power cut can leave, and what the start-up recovery does with it:
//   - File_001.tmp: the write never finished, the slot is whole: delete the .tmp file;
//   - File_001.new (with or without the slot): verified and complete, only the replacing is
//     missing: it becomes the slot.
// FAT has no journal: a cut inside the single rename of a directory entry could still leave two names
// for one file; that window is a few microseconds and is not covered.
// The autosave files are not saved this way: they alternate (A, B) and carry their own seal.
#ifndef __ATOMICSAVE_H
#define __ATOMICSAVE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum SlotFileState : uint8_t {
    SLOTFILE_MISSING,     // no such file
    SLOTFILE_INCOMPLETE,  // there, but not a whole slot file (no header, cut short, no EOF)
    SLOTFILE_COMPLETE,    // header, body and EOF marker
};

enum SaveRecovery : uint8_t {
    SAVERECOVERY_NOTHING,       // nothing to do
    SAVERECOVERY_DELETE_READY,  // the .new file is not a whole slot file: drop it
    SAVERECOVERY_PROMOTE_READY, // the .new file replaces the slot
};

// "System/File/File_001.rwf" + "tmp" -> "System/File/File_001.tmp". False when it doesn't fit.
// extension_ is without the dot.
inline bool atomicSidePath(char* out_, size_t size_, const char* finalPath_, const char* extension_) {
    const char* slash = strrchr(finalPath_, '/');
    const char* dot = strrchr(finalPath_, '.');
    const size_t stem = (dot && (!slash || (dot > slash))) ? static_cast<size_t>(dot - finalPath_) : strlen(finalPath_);
    const size_t extensionSize = strlen(extension_);
    if (stem + 1 + extensionSize + 1 > size_) return false;
    memcpy(out_, finalPath_, stem);
    out_[stem] = '.';
    memcpy(out_ + stem + 1, extension_, extensionSize + 1);
    return true;
}

// The file the new content is written to.
inline bool atomicTempPath(char* out_, size_t size_, const char* finalPath_) {
    return atomicSidePath(out_, size_, finalPath_, "tmp");
}

// The file the verified new content waits in until it replaces the slot.
inline bool atomicReadyPath(char* out_, size_t size_, const char* finalPath_) {
    return atomicSidePath(out_, size_, finalPath_, "new");
}

// What start-up does with the .new file of a slot, given its state. (A .tmp file is always deleted.)
inline SaveRecovery atomicRecovery(SlotFileState ready_) {
    if (ready_ == SLOTFILE_MISSING) return SAVERECOVERY_NOTHING;
    return (ready_ == SLOTFILE_COMPLETE) ? SAVERECOVERY_PROMOTE_READY : SAVERECOVERY_DELETE_READY;
}

#endif  // __ATOMICSAVE_H
