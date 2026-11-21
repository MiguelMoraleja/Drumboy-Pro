// Host-side tests for the sample memory pool (SamplePool.h).

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "SamplePool.h"

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

int main() {
    // Sizes and the 20 s limit.
    CHECK(kSampleMaxFrames == 882000 && kSamplePoolBytes == 14400000, "constants");
    CHECK(samplePoolRound(1) == 64 && samplePoolRound(64) == 64 && samplePoolRound(65) == 128 && samplePoolRound(0) == 0, "round");
    CHECK(samplePlaneBytes(1000) == 4032 && sampleBytes(1000, true) == 8064 && sampleBytes(1000, false) == 4032, "plane bytes");
    CHECK(sampleBytes(kSampleMaxFrames, false) == 3528000 && sampleBytes(kSampleMaxFrames, true) == 7056000, "a 20 s sample: 3.5 MB mono, 7 MB stereo");
    CHECK(sampleFramesToTenthSeconds(441000) == 100 && sampleFramesToTenthSeconds(kSampleMaxFrames) == 200, "tenths of a second");

    // An empty pool.
    CHECK(samplePoolFind(nullptr, 0, kSamplePoolBytes, 1000) == 0, "empty pool: offset 0");
    CHECK(samplePoolFind(nullptr, 0, kSamplePoolBytes, kSamplePoolBytes) == 0, "the whole pool fits");
    CHECK(samplePoolFind(nullptr, 0, kSamplePoolBytes, kSamplePoolBytes + 1) == -1, "one byte more does not");
    CHECK(samplePoolFind(nullptr, 0, kSamplePoolBytes, 0) == -1, "nothing to place");
    CHECK(samplePoolLargestFree(nullptr, 0, kSamplePoolBytes) == 14400000, "largest free: all (a multiple of 64)");

    // Four full-length mono samples fit, a fifth does not.
    {
        PoolBlock used[kSamplePoolMaxBlocks];
        uint8_t n = 0;
        const uint32_t big = sampleBytes(kSampleMaxFrames, false);
        for (int i = 0; i < 4; i++) {
            const int64_t at = samplePoolFind(used, n, kSamplePoolBytes, big);
            CHECK(at == static_cast<int64_t>(i) * big, "sample %d at %lld", i, (long long)at);
            used[n++] = {static_cast<uint32_t>(at), big};
        }
        CHECK(samplePoolFind(used, n, kSamplePoolBytes, big) == -1, "a fifth 20 s sample does not fit");
        CHECK(samplePoolFind(used, n, kSamplePoolBytes, 100000) >= 0, "but a short one does");
        CHECK(samplePoolUsed(used, n) == 4 * big, "used bytes");
        CHECK(samplePoolLargestFree(used, n, kSamplePoolBytes) == (kSamplePoolBytes - 4 * big) / 64 * 64, "largest free after four");
    }

    // A hole is reused (first fit), in any order of the list.
    {
        PoolBlock used[4] = {{4096, 4096}, {0, 4096}, {12288, 4096}, {8192, 100}};  // holes: none until 8192+128, then 8320..12288
        CHECK(samplePoolFind(used, 4, 100000, 4096) == 16384, "the hole behind the small block (3968 bytes) is too small for 4096");
        CHECK(samplePoolFind(used, 4, 100000, 3968) == 8320, "exactly the hole");
        CHECK(samplePoolFind(used, 4, 100000, 64) == 8320, "a small block takes the first hole");
        CHECK(samplePoolFind(used, 4, 100000, 4032 + 1) == 16384, "too big for the hole: after the last block");
        // A zero-size block is ignored.
        PoolBlock z[2] = {{0, 0}, {64, 64}};
        CHECK(samplePoolFind(z, 2, 100000, 64) == 0, "an empty block takes no room");
    }

    // The end of the pool: a block that would run past it does not fit.
    {
        PoolBlock used[1] = {{0, 1000 * 64}};
        CHECK(samplePoolFind(used, 1, 2000 * 64, 1000 * 64) == 1000 * 64, "fits exactly at the end");
        CHECK(samplePoolFind(used, 1, 2000 * 64, 1000 * 64 + 1) == -1, "one alignment unit too many");
    }

    // The MEM bar.
    CHECK(samplePoolPermille(0, kSamplePoolBytes) == 0 && samplePoolPermille(kSamplePoolBytes, kSamplePoolBytes) == 1000 && samplePoolPermille(kSamplePoolBytes / 2, kSamplePoolBytes) == 500, "permille");
    CHECK(samplePoolPermille(kSamplePoolBytes * 2u, kSamplePoolBytes) == 1000 && samplePoolPermille(1, 0) == 0, "capped, and no pool");

    // Random use: blocks never overlap and stay inside the pool.
    {
        std::srand(7);
        PoolBlock used[kSamplePoolMaxBlocks];
        uint8_t n = 0;
        for (int step = 0; step < 20000; step++) {
            if ((n > 0) && ((std::rand() % 3 == 0) || (n == kSamplePoolMaxBlocks))) {
                const uint8_t k = static_cast<uint8_t>(std::rand() % n);
                used[k] = used[--n];
            } else {
                const uint32_t bytes = 64u * static_cast<uint32_t>(1 + std::rand() % 60000);
                const int64_t at = samplePoolFind(used, n, kSamplePoolBytes, bytes);
                if (at >= 0) used[n++] = {static_cast<uint32_t>(at), bytes};
            }
            for (uint8_t i = 0; i < n; i++) {
                CHECK(used[i].offset + used[i].bytes <= kSamplePoolBytes && (used[i].offset % 64) == 0, "inside, aligned");
                for (uint8_t j = i + 1; j < n; j++) CHECK((used[i].offset + used[i].bytes <= used[j].offset) || (used[j].offset + used[j].bytes <= used[i].offset), "no overlap");
            }
            if (failures > 5) break;
        }
    }

    // Packing: the data moves with its block, the order stays, and the free space becomes one gap at the end.
    {
        std::srand(11);
        static unsigned char mem[400000];
        for (int round = 0; round < 300; round++) {
            for (unsigned i = 0; i < sizeof mem; i++) mem[i] = 0;
            PoolBlock used[kSamplePoolMaxBlocks];
            uint8_t n = 0;
            for (int tries = 0; tries < 12; tries++) {
                const uint32_t bytes = 64u * static_cast<uint32_t>(1 + std::rand() % 400);
                const int64_t at = samplePoolFind(used, n, sizeof mem, bytes);
                if (at >= 0) {
                    used[n] = {static_cast<uint32_t>(at), bytes};
                    for (uint32_t k = 0; k < bytes; k++) mem[at + k] = static_cast<unsigned char>(n + 1);
                    n++;
                }
                if ((n > 2) && (std::rand() % 3 == 0)) {  // free a random block: a hole
                    const uint8_t f = static_cast<uint8_t>(std::rand() % n);
                    for (uint32_t k = 0; k < used[f].bytes; k++) mem[used[f].offset + k] = 0;
                    used[f] = used[--n];
                    for (uint8_t j = 0; j < n; j++)  // the pattern follows the index: rewrite it
                        for (uint32_t k = 0; k < used[j].bytes; k++) mem[used[j].offset + k] = static_cast<unsigned char>(j + 1);
                }
            }
            const uint32_t usedBefore = samplePoolUsed(used, n);
            PoolMove moves[kSamplePoolMaxBlocks];
            const uint8_t m = samplePoolPlanCompact(used, n, moves);
            for (uint8_t k = 0; k < m; k++) {
                std::memmove(mem + moves[k].to, mem + moves[k].from, used[moves[k].block].bytes);
                used[moves[k].block].offset = moves[k].to;
            }
            // The blocks now follow each other from 0, with their own data.
            uint32_t end = 0;
            bool packed = true;
            for (uint8_t j = 0; j < n; j++) end = (used[j].offset + samplePoolRound(used[j].bytes) > end) ? used[j].offset + samplePoolRound(used[j].bytes) : end;
            packed = (end == usedBefore);
            CHECK(packed, "packed: end %u used %u", end, usedBefore);
            bool intact = true;
            for (uint8_t j = 0; j < n; j++)
                for (uint32_t k = 0; k < used[j].bytes; k++) intact = intact && (mem[used[j].offset + k] == j + 1);
            CHECK(intact, "data intact after packing");
            CHECK(samplePoolFind(used, n, sizeof mem, sizeof mem - usedBefore) == static_cast<int64_t>(usedBefore) || (sizeof mem - usedBefore) == 0, "all the free space is one gap");
            if (failures > 5) break;
        }
        // A packed pool has no moves.
        PoolBlock tight[2] = {{0, 128}, {128, 64}};
        PoolMove mv[2];
        CHECK(samplePoolPlanCompact(tight, 2, mv) == 0, "nothing to move when packed");
        PoolBlock gap[2] = {{256, 128}, {1024, 64}};
        CHECK(samplePoolPlanCompact(gap, 2, mv) == 2 && mv[0].from == 256 && mv[0].to == 0 && mv[1].from == 1024 && mv[1].to == 128, "two blocks move down");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
