// Host-side tests for atomic saves (AtomicSave.h, Global/SlotFile.h) against an in-memory
// stand-in for FatFs (mock/ff.h): a failed or cut-short save never damages the slot, a power
// cut at any step leaves the old or the new file, and the start-up recovery sorts out what is left.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "SlotFile.h"

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

static const char kRef[] = "RW_TEST_SLOT   ";  // header
static const uint32_t kBody = 1500;            // body, then "EOF", then a tail like the chain/notes/DUB blocks
static const uint32_t kTail = 120;
static const char kSlot[] = "System/File/File_001.rwf";
static const char kTemp[] = "System/File/File_001.tmp";
static const char kReady[] = "System/File/File_001.new";

// A slot file's content: header, body filled with `seed`, EOF, tail.
static std::vector<uint8_t> makeContent(uint8_t seed) {
    std::vector<uint8_t> v(kRef, kRef + strlen(kRef));
    for (uint32_t i = 0; i < kBody; i++) v.push_back(static_cast<uint8_t>(seed + i * 7));
    v.push_back('E');
    v.push_back('O');
    v.push_back('F');
    for (uint32_t i = 0; i < kTail; i++) v.push_back(static_cast<uint8_t>(seed ^ i));
    return v;
}

static int ticks = 0;
static void countTick() { ticks++; }

// The save the firmware does: the content in pieces, the body in 512-byte blocks.
static bool save(const std::vector<uint8_t>& content, bool atomic = true) {
    FIL f;
    const uint32_t head = static_cast<uint32_t>(strlen(kRef));
    return slotSave(
        &f, kSlot, atomic,
        [&](SaveWriter& out) {
            out.write(content.data(), head);
            out.writeBlocks(content.data() + head, kBody);
            out.write(content.data() + head + kBody, 3);
            out.write(content.data() + head + kBody + 3, static_cast<uint32_t>(content.size()) - head - kBody - 3);
        },
        countTick);
}

static void putFile(const char* path, const std::vector<uint8_t>& v) {
    mockFs().files[path] = v;
}
static bool hasFile(const char* path) {
    return mockFs().files.count(path) > 0;
}
static bool fileIs(const char* path, const std::vector<uint8_t>& v) {
    return hasFile(path) && (mockFs().files[path] == v);
}
static bool noTemp() {
    return !hasFile(kTemp) && !hasFile(kReady);
}

static void testPaths() {
    char out[40];
    CHECK(atomicTempPath(out, sizeof out, "System/File/File_001.rwf") && (strcmp(out, "System/File/File_001.tmp") == 0), "%s", out);
    CHECK(atomicTempPath(out, sizeof out, "System/Drumkit/Drumkit_099.rwd") && (strcmp(out, "System/Drumkit/Drumkit_099.tmp") == 0), "%s", out);
    CHECK(atomicTempPath(out, sizeof out, "NoExtension") && (strcmp(out, "NoExtension.tmp") == 0), "%s", out);
    CHECK(atomicTempPath(out, sizeof out, "a.b/File") && (strcmp(out, "a.b/File.tmp") == 0), "a dot in a folder is not an extension: %s", out);
    CHECK(atomicReadyPath(out, sizeof out, "System/File/File_001.rwf") && (strcmp(out, "System/File/File_001.new") == 0), "%s", out);
    char small[9];
    CHECK(!atomicTempPath(small, 8, "System/File/File_001.rwf"), "does not fit");
    CHECK(atomicTempPath(small, sizeof small, "abcd.rwf") && (strcmp(small, "abcd.tmp") == 0), "fits exactly");
    CHECK(!atomicTempPath(small, 8, "abcd.rwf"), "one short");
}

static void testRecoveryTable() {
    CHECK(atomicRecovery(SLOTFILE_MISSING) == SAVERECOVERY_NOTHING, "no .new: nothing");
    CHECK(atomicRecovery(SLOTFILE_INCOMPLETE) == SAVERECOVERY_DELETE_READY, "a .new that is not whole is dropped");
    CHECK(atomicRecovery(SLOTFILE_COMPLETE) == SAVERECOVERY_PROMOTE_READY, "a whole .new replaces the slot");
}

static void testNormalSave() {
    mockFs().reset();
    const auto oldContent = makeContent(1), newContent = makeContent(2);
    putFile(kSlot, oldContent);
    CHECK(save(newContent), "save over an existing slot");
    CHECK(fileIs(kSlot, newContent), "new content in the slot");
    CHECK(noTemp(), "no temporary file left");

    mockFs().reset();
    CHECK(save(newContent), "save into a missing slot");
    CHECK(fileIs(kSlot, newContent) && noTemp(), "created");

    // The autosave way: straight into the file.
    mockFs().reset();
    putFile(kSlot, oldContent);
    CHECK(save(newContent, false), "direct save");
    CHECK(fileIs(kSlot, newContent) && noTemp(), "direct: in place, no temp");
}

// Every kind of failure: the save says no, the slot keeps the old content, no temporary file stays.
static void testFailuresKeepOldSlot() {
    const auto oldContent = makeContent(1), newContent = makeContent(2);
    // How many writes does one save make?
    mockFs().reset();
    putFile(kSlot, oldContent);
    save(newContent);
    const int writesPerSave = mockFs().writes;
    CHECK(writesPerSave > 5, "writes: %d", writesPerSave);

    for (int n = 1; n <= writesPerSave; n++) {
        for (int kind = 0; kind < 3; kind++) {
            mockFs().reset();
            putFile(kSlot, oldContent);
            if (kind == 0) mockFs().failWriteAt = n;
            if (kind == 1) mockFs().shortWriteAt = n;
            if (kind == 2) mockFs().corruptWriteAt = n;
            const bool ok = save(newContent);
            CHECK(!ok, "write %d kind %d must fail", n, kind);
            CHECK(fileIs(kSlot, oldContent), "write %d kind %d: slot damaged", n, kind);
            CHECK(noTemp(), "write %d kind %d: temp left", n, kind);
        }
    }

    mockFs().reset();
    putFile(kSlot, oldContent);
    mockFs().failClose = true;
    CHECK(!save(newContent) && fileIs(kSlot, oldContent) && noTemp(), "failed close");

    mockFs().reset();
    putFile(kSlot, oldContent);
    mockFs().denyUnlinkOf = kSlot;
    CHECK(!save(newContent) && fileIs(kSlot, oldContent) && noTemp(), "old slot can't be deleted: unchanged");

    // The first rename (.tmp to .new) fails: nothing has changed.
    mockFs().reset();
    putFile(kSlot, oldContent);
    mockFs().failRenameAt = 1;
    CHECK(!save(newContent), "first rename failed");
    CHECK(fileIs(kSlot, oldContent) && noTemp(), "the first rename failed: the slot is untouched");

    // The second rename fails after the old slot was deleted: the verified .new is kept for the recovery.
    mockFs().reset();
    putFile(kSlot, oldContent);
    mockFs().failRenameAt = 2;
    CHECK(!save(newContent), "second rename failed");
    CHECK(!hasFile(kSlot) && fileIs(kReady, newContent) && !hasFile(kTemp), "new content is safe in the .new file");
    mockFs().failRenameAt = -1;
    FIL f;
    slotRecover(&f, kSlot, kRef, kBody);
    CHECK(fileIs(kSlot, newContent) && noTemp(), "recovered");

    // A full card: the write puts out fewer bytes than asked.
    mockFs().reset();
    putFile(kSlot, oldContent);
    mockFs().shortWriteAt = 1;
    CHECK(!save(newContent) && fileIs(kSlot, oldContent) && noTemp(), "first write short");
}

// A power cut at every changing step of a save, then the start-up recovery: the slot holds the old
// or the new file, whole, and no temporary file remains.
static void testPowerCutAnywhere() {
    const auto oldContent = makeContent(1), newContent = makeContent(2);
    for (int existing = 0; existing < 2; existing++) {
        mockFs().reset();
        if (existing) putFile(kSlot, oldContent);
        save(newContent);
        const int totalOps = mockFs().ops;
        CHECK(totalOps > 8, "ops: %d", totalOps);
        int sawOld = 0, sawNew = 0;
        for (int cut = 1; cut <= totalOps; cut++) {
            mockFs().reset();
            if (existing) putFile(kSlot, oldContent);
            mockFs().cutAtOp = cut;
            bool cutHappened = false;
            try {
                save(newContent);
            } catch (const MockPowerCut&) {
                cutHappened = true;
            }
            CHECK(cutHappened, "cut %d did not happen", cut);
            mockFs().cutAtOp = -1;  // power is back
            FIL f;
            slotRecover(&f, kSlot, kRef, kBody);
            CHECK(noTemp(), "cut %d: temp left after recovery", cut);
            const bool isOld = existing && fileIs(kSlot, oldContent);
            const bool isNew = fileIs(kSlot, newContent);
            CHECK(isOld || isNew || (!existing && !hasFile(kSlot)), "cut %d (existing %d): slot is neither old nor new", cut, existing);
            if (isOld) sawOld++;
            if (isNew) sawNew++;
            // A slot that exists is always a whole one.
            if (hasFile(kSlot)) {
                FIL g;
                CHECK(slotFileStateOf(&g, kSlot, kRef, kBody) == SLOTFILE_COMPLETE, "cut %d: slot not whole", cut);
            }
        }
        CHECK(sawOld + sawNew > 0, "some outcome");
        CHECK(sawNew >= 1, "late cuts keep the new content (existing %d)", existing);
        if (existing) CHECK(sawOld >= 1, "early cuts keep the old content");
    }
}

static void testRecoveryCases() {
    const auto oldContent = makeContent(1), newContent = makeContent(2);
    FIL f;

    // Nothing to do.
    mockFs().reset();
    putFile(kSlot, oldContent);
    slotRecover(&f, kSlot, kRef, kBody);
    CHECK(fileIs(kSlot, oldContent), "untouched");
    CHECK(mockFs().ops == 0, "no changes: %d", mockFs().ops);

    // A .tmp file (a write that never finished), however much of it: always dropped, the slot stays.
    for (size_t size : {size_t(0), size_t(10), size_t(700), newContent.size() - 1, newContent.size()}) {
        mockFs().reset();
        putFile(kSlot, oldContent);
        putFile(kTemp, std::vector<uint8_t>(newContent.begin(), newContent.begin() + size));
        slotRecover(&f, kSlot, kRef, kBody);
        CHECK(fileIs(kSlot, oldContent) && noTemp(), "tmp of %zu bytes dropped", size);
    }
    mockFs().reset();
    putFile(kTemp, newContent);
    slotRecover(&f, kSlot, kRef, kBody);
    CHECK(!hasFile(kSlot) && noTemp(), "a .tmp is never promoted, even a whole one");

    // A .new that is not a whole slot file: dropped.
    auto badHeader = newContent;
    badHeader[0] = 'X';
    auto noEof = newContent;
    noEof[strlen(kRef) + kBody] = 0;
    for (const auto& bad : {badHeader, noEof, std::vector<uint8_t>(newContent.begin(), newContent.begin() + 700)}) {
        mockFs().reset();
        putFile(kSlot, oldContent);
        putFile(kReady, bad);
        slotRecover(&f, kSlot, kRef, kBody);
        CHECK(fileIs(kSlot, oldContent) && noTemp(), "bad .new dropped");
    }

    // A whole .new, slot deleted (cut between delete and rename): promoted.
    mockFs().reset();
    putFile(kReady, newContent);
    slotRecover(&f, kSlot, kRef, kBody);
    CHECK(fileIs(kSlot, newContent) && noTemp(), "promoted");

    // A whole .new next to the old slot (cut before the delete): the save finishes.
    mockFs().reset();
    putFile(kSlot, oldContent);
    putFile(kReady, newContent);
    slotRecover(&f, kSlot, kRef, kBody);
    CHECK(fileIs(kSlot, newContent) && noTemp(), "the verified save is finished");

    // Both leftovers at once.
    mockFs().reset();
    putFile(kSlot, oldContent);
    putFile(kTemp, std::vector<uint8_t>(newContent.begin(), newContent.begin() + 300));
    putFile(kReady, newContent);
    slotRecover(&f, kSlot, kRef, kBody);
    CHECK(fileIs(kSlot, newContent) && noTemp(), "both leftovers");
}

// A cut during the recovery itself (power fails again at start-up): running it again finishes the job.
static void testPowerCutDuringRecovery() {
    const auto oldContent = makeContent(1), newContent = makeContent(2);
    for (int withSlot = 0; withSlot < 2; withSlot++) {
        for (int cut = 1; cut <= 4; cut++) {
            mockFs().reset();
            if (withSlot) putFile(kSlot, oldContent);
            putFile(kTemp, std::vector<uint8_t>(newContent.begin(), newContent.begin() + 300));
            putFile(kReady, newContent);
            mockFs().cutAtOp = cut;
            try {
                FIL f;
                slotRecover(&f, kSlot, kRef, kBody);
            } catch (const MockPowerCut&) {
            }
            mockFs().cutAtOp = -1;
            FIL f;
            slotRecover(&f, kSlot, kRef, kBody);
            CHECK(fileIs(kSlot, newContent) && noTemp(), "cut %d during recovery (slot %d), then recovered", cut, withSlot);
        }
    }
}

// A save that fails at its last step leaves the verified .new file with the slot deleted: the next save
// puts it back before it starts, so a second failure does not lose the only good copy.
static void testLeftoverNewIsKept() {
    const auto oldContent = makeContent(1), newContent = makeContent(2), thirdContent = makeContent(3);
    mockFs().reset();
    putFile(kSlot, oldContent);
    mockFs().failRenameAt = 2;
    CHECK(!save(newContent), "the last step failed");
    CHECK(!hasFile(kSlot) && fileIs(kReady, newContent), "the slot is gone, the verified copy waits");
    mockFs().failRenameAt = -1;
    // The next save fails early (a write error): the leftover became the slot again first.
    mockFs().writes = 0;
    mockFs().failWriteAt = 1;
    CHECK(!save(thirdContent), "the second save fails");
    mockFs().failWriteAt = -1;
    CHECK(fileIs(kSlot, newContent) && noTemp(), "the first save's verified copy is the slot again");
    // And a save that works replaces it as usual.
    CHECK(save(thirdContent) && fileIs(kSlot, thirdContent) && noTemp(), "a later save works");

    // The same through a power cut anywhere in the second save: the slot is the verified copy or the third content, whole.
    mockFs().reset();
    putFile(kSlot, oldContent);
    mockFs().failRenameAt = 2;
    save(newContent);
    mockFs().failRenameAt = -1;
    const MockFs saved = mockFs();  // the card after the failed save
    int totalOps = 0;
    mockFs().ops = 0;
    save(thirdContent);
    totalOps = mockFs().ops;
    for (int cut = 1; cut <= totalOps; cut++) {
        mockFs() = saved;
        mockFs().ops = 0;
        mockFs().cutAtOp = cut;
        try {
            save(thirdContent);
        } catch (const MockPowerCut&) {
        }
        mockFs().cutAtOp = -1;
        FIL f;
        slotRecover(&f, kSlot, kRef, kBody);
        CHECK(noTemp(), "cut %d: temp left", cut);
        CHECK(fileIs(kSlot, newContent) || fileIs(kSlot, thirdContent), "cut %d: the slot is neither of the two good copies", cut);
    }
}

static void testWatchdogTick() {
    mockFs().reset();
    ticks = 0;
    putFile(kSlot, makeContent(1));
    CHECK(save(makeContent(2)), "save");
    // Every write and every read-back chunk ticks: 6 writes, 3 read chunks of the 1.6 KB file.
    CHECK(ticks >= 9, "ticks during a save: %d", ticks);
    // Without a tick function nothing is called (the default).
    FIL f;
    ticks = 0;
    const auto c = makeContent(3);
    CHECK(slotSave(&f, kSlot, true, [&](SaveWriter& out) { out.write(c.data(), static_cast<uint32_t>(c.size())); }), "save without a tick (the content is not a whole slot file, but the write is)");
    CHECK(ticks == 0, "no tick function, no ticks");
}

// A slot path whose .tmp or .new name does not fit the 64-byte buffers: an atomic save refuses (the slot is
// untouched), it does not fall back to writing the slot in place (review). A direct save still works.
static void testLongPathRefused() {
    mockFs().reset();
    char longPath[120];
    snprintf(longPath, sizeof longPath, "System/File/%s/File_001.rwf", "a_folder_name_that_makes_the_side_paths_too_long_for_the_buffer");
    const auto old = makeContent(1), fresh = makeContent(2);
    putFile(longPath, old);
    FIL f;
    const uint32_t head = static_cast<uint32_t>(strlen(kRef));
    auto fill = [&](SaveWriter& out) {
        out.write(fresh.data(), head);
        out.writeBlocks(fresh.data() + head, kBody);
        out.write(fresh.data() + head + kBody, static_cast<uint32_t>(fresh.size()) - head - kBody);
    };
    CHECK(!slotSave(&f, longPath, true, fill), "an atomic save with side paths that do not fit is refused");
    CHECK(fileIs(longPath, old), "the slot is untouched");
    CHECK(mockFs().files.size() == 1, "no side file was left");
    CHECK(slotSave(&f, longPath, false, fill), "a direct save (autosave) does not need the side paths");
    CHECK(fileIs(longPath, fresh), "the direct save wrote the file");
}

static void testWriterCounting() {
    mockFs().reset();
    FIL f;
    f_open(&f, "x.bin", FA_WRITE | FA_CREATE_ALWAYS);
    SaveWriter w(&f);
    std::vector<uint8_t> data(1286, 0x5A);
    w.writeBlocks(data.data(), static_cast<uint32_t>(data.size()));
    CHECK(w.ok() && (w.bytes() == 1286), "bytes %u", w.bytes());
    w.write(nullptr, 0);
    CHECK(w.ok() && (w.bytes() == 1286), "an empty write is nothing");
    CHECK(w.finish("x.bin"), "finish");
    CHECK(mockFs().files["x.bin"].size() == 1286, "size");

    // After a failure nothing more is written.
    mockFs().reset();
    f_open(&f, "y.bin", FA_WRITE | FA_CREATE_ALWAYS);
    SaveWriter v(&f);
    mockFs().failWriteAt = 2;
    v.writeBlocks(data.data(), static_cast<uint32_t>(data.size()));
    CHECK(!v.ok(), "failed");
    const int writesSoFar = mockFs().writes;
    v.write(data.data(), 10);
    CHECK(mockFs().writes == writesSoFar, "no writes after a failure");
    CHECK(!v.finish("y.bin"), "finish says no");
}

int main() {
    testPaths();
    testRecoveryTable();
    testNormalSave();
    testFailuresKeepOldSlot();
    testPowerCutAnywhere();
    testRecoveryCases();
    testPowerCutDuringRecovery();
    testWriterCounting();
    testLeftoverNewIsKept();
    testWatchdogTick();
    testLongPathRefused();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
