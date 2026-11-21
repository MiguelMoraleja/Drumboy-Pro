// A tiny in-memory stand-in for FatFs (ff.h) so the slot-file code (Global/SlotFile.h) runs on a
// PC. Only what that code uses. Faults can be injected: a write that fails, comes up short or stores a
// wrong byte, a close or a delete that fails, and a power cut at any step (a MockPowerCut exception
// that leaves the "card" as it was at that moment).
#ifndef MOCK_FF_H
#define MOCK_FF_H

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

typedef unsigned int UINT;
typedef uint32_t FSIZE_t;
typedef char TCHAR;

typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
} FRESULT;

#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_ALWAYS 0x08

struct FIL {
    struct {
        FSIZE_t objsize;
    } obj;
    std::string path;
    bool open = false;
    bool writing = false;
    FSIZE_t pos = 0;
};
#define f_size(fp) ((fp)->obj.objsize)

struct MockPowerCut {};

struct MockFs {
    std::map<std::string, std::vector<uint8_t>> files;
    // fault injection: counters run down over the operations of one test, -1 = off
    int cutAtOp = -1;       // the n-th changing operation (1-based) is the last thing that does not happen
    int failWriteAt = -1;   // the n-th f_write returns FR_DISK_ERR
    int shortWriteAt = -1;  // the n-th f_write returns FR_OK but writes one byte less
    int corruptWriteAt = -1;  // the n-th f_write stores a wrong byte (and reports success)
    bool failClose = false;
    std::string denyUnlinkOf;  // f_unlink of this path fails with FR_DENIED
    int failRenameAt = -1;  // the n-th f_rename returns FR_DISK_ERR
    int renames = 0;
    int ops = 0;     // changing operations so far
    int writes = 0;  // f_write calls so far

    void reset() {
        *this = MockFs();
    }
    void step() {
        ++ops;
        if ((cutAtOp > 0) && (ops == cutAtOp)) throw MockPowerCut();
    }
};

inline MockFs& mockFs() {
    static MockFs fs;
    return fs;
}

inline FRESULT f_open(FIL* fp, const TCHAR* path, uint8_t mode) {
    MockFs& fs = mockFs();
    fp->path = path;
    fp->pos = 0;
    fp->writing = (mode & FA_WRITE) != 0;
    auto it = fs.files.find(path);
    if (mode & FA_CREATE_ALWAYS) {
        fs.step();
        fs.files[path].clear();
        it = fs.files.find(path);
    } else if (it == fs.files.end()) {
        return FR_NO_FILE;
    }
    fp->open = true;
    fp->obj.objsize = static_cast<FSIZE_t>(it->second.size());
    return FR_OK;
}

inline FRESULT f_close(FIL* fp) {
    MockFs& fs = mockFs();
    if (!fp->open) return FR_OK;
    if (fp->writing) fs.step();
    fp->open = false;
    return (fp->writing && fs.failClose) ? FR_DISK_ERR : FR_OK;
}

inline FRESULT f_write(FIL* fp, const void* data, UINT size, UINT* written) {
    MockFs& fs = mockFs();
    *written = 0;
    if (!fp->open || !fp->writing) return FR_DENIED;
    fs.step();
    ++fs.writes;
    if (fs.writes == fs.failWriteAt) return FR_DISK_ERR;
    std::vector<uint8_t>& file = fs.files[fp->path];
    UINT count = size;
    if (fs.writes == fs.shortWriteAt) count = size - 1;
    if (file.size() < fp->pos + count) file.resize(fp->pos + count);
    memcpy(file.data() + fp->pos, data, count);
    if ((fs.writes == fs.corruptWriteAt) && (count > 0)) file[fp->pos] ^= 0x40;
    fp->pos += count;
    fp->obj.objsize = static_cast<FSIZE_t>(file.size());
    *written = count;
    return FR_OK;
}

inline FRESULT f_read(FIL* fp, void* data, UINT size, UINT* got) {
    MockFs& fs = mockFs();
    *got = 0;
    if (!fp->open) return FR_INT_ERR;
    const std::vector<uint8_t>& file = fs.files[fp->path];
    if (fp->pos >= file.size()) return FR_OK;
    const UINT count = (file.size() - fp->pos < size) ? static_cast<UINT>(file.size() - fp->pos) : size;
    memcpy(data, file.data() + fp->pos, count);
    fp->pos += count;
    *got = count;
    return FR_OK;
}

inline FRESULT f_lseek(FIL* fp, FSIZE_t ofs) {
    if (!fp->open) return FR_INT_ERR;
    fp->pos = ofs;
    return FR_OK;
}

inline FRESULT f_unlink(const TCHAR* path) {
    MockFs& fs = mockFs();
    fs.step();
    if (!fs.denyUnlinkOf.empty() && (fs.denyUnlinkOf == path)) return FR_DENIED;
    return (fs.files.erase(path) > 0) ? FR_OK : FR_NO_FILE;
}

inline FRESULT f_rename(const TCHAR* from, const TCHAR* to) {
    MockFs& fs = mockFs();
    fs.step();
    if (++fs.renames == fs.failRenameAt) return FR_DISK_ERR;
    auto it = fs.files.find(from);
    if (it == fs.files.end()) return FR_NO_FILE;
    if (fs.files.count(to)) return FR_EXIST;
    fs.files[to] = it->second;
    fs.files.erase(from);
    return FR_OK;
}

#endif  // MOCK_FF_H
