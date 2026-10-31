// Host-side tests for the signal router (Core/Library/Global/Routing.h).
//
// 1. Default patch == today's fixed chain: a direct model of the summing in
//    processAudioEq/Filter/Effect/Reverb/Send (firmware V1.04) is compared with routeBlock
//    for random sources, entry points and module on/off states. The stand-in modules are
//    nonlinear, so summing before or after the wrong module shows up as a mismatch.
// 2. Parallel paths: each module multiplies by its own prime, so a source's output at the
//    master is the product of the modules on its path.
// 3. Patch validation and entry-point conversion.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

#include "Routing.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        ++checks;                                                           \
        if (!(cond)) {                                                      \
            ++failures;                                                     \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond);     \
            std::printf(__VA_ARGS__);                                       \
            std::printf("\n");                                              \
        }                                                                   \
    } while (0)

const uint16_t N = 32;                    // I2S_BLOCK_SIZE
const uint8_t kSources = 12;              // line-in, metronome, 10 layers
const uint8_t MODE_SEND = 6;              // MixerInMode value of SEND

// Stand-in DSP: module m maps x -> a*x + b*x^2 (nonlinear); the reverb also makes R.
static const float kA[6] = {0.9f, 1.1f, 0.8f, 1.2f, 0.7f, 0.6f};
static const float kB[6] = {0.05f, -0.04f, 0.03f, -0.02f, 0.06f, 0.1f};
static float mod(uint8_t m, float x) { return kA[m] * x + kB[m] * x * x; }
static float revRight(float x) { return 0.3f * x - 0.2f * x * x; }

struct State {
    float src[kSources][N];
    uint8_t mode[kSources];  // MixerInMode entry point per source
    bool active[6];          // module on/off
};

// Today's chain, transcribed from the V1.04 stage functions.
static void legacyChain(const State& s, float outL[N], float outR[N]) {
    float eq[N] = {}, f0[N] = {}, f1[N] = {}, e0[N] = {}, e1[N] = {}, rl[N] = {}, rr[N] = {};
    auto add = [&](float* bus, uint8_t mode) {
        for (uint8_t k = 0; k < kSources; k++)
            if (s.mode[k] == mode)
                for (uint16_t i = 0; i < N; i++) bus[i] += s.src[k][i];
    };
    add(eq, 0);
    if (s.active[0]) for (uint16_t i = 0; i < N; i++) eq[i] = mod(0, eq[i]);
    for (uint16_t i = 0; i < N; i++) f0[i] += eq[i];
    add(f0, 1);
    add(f1, 2);
    if (s.active[1]) for (uint16_t i = 0; i < N; i++) f0[i] = mod(1, f0[i]);
    for (uint16_t i = 0; i < N; i++) f1[i] += f0[i];
    if (s.active[2]) for (uint16_t i = 0; i < N; i++) f1[i] = mod(2, f1[i]);
    for (uint16_t i = 0; i < N; i++) e0[i] += f1[i];
    add(e0, 3);
    add(e1, 4);
    if (s.active[3]) for (uint16_t i = 0; i < N; i++) e0[i] = mod(3, e0[i]);
    for (uint16_t i = 0; i < N; i++) e1[i] += e0[i];
    if (s.active[4]) for (uint16_t i = 0; i < N; i++) e1[i] = mod(4, e1[i]);
    for (uint16_t i = 0; i < N; i++) rl[i] += e1[i];
    add(rl, 5);
    for (uint16_t i = 0; i < N; i++) {
        if (s.active[5]) {
            rr[i] = revRight(rl[i]);
            rl[i] = mod(5, rl[i]);
        } else {
            rr[i] = rl[i];
        }
    }
    float send[N] = {};
    add(send, MODE_SEND);
    for (uint16_t i = 0; i < N; i++) {
        outL[i] = rl[i] + send[i];
        outR[i] = rr[i] + send[i];
    }
}

static void routedChain(const State& s, const RoutePatch& patch, float outL[N], float outR[N]) {
    static float node[6][N], revR[N];
    RouteBuffers buf = {{node[0], node[1], node[2], node[3], node[4], node[5]}, revR, outL, outR};
    const float* src[kSources];
    uint8_t masks[kSources];
    for (uint8_t k = 0; k < kSources; k++) {
        src[k] = s.src[k];
        masks[k] = routeMaskFromEntry(s.mode[k]);
    }
    routeBlock<N>(patch, buf, src, masks, kSources, [&](uint8_t m) {
        if (m == NODE_REV) {
            for (uint16_t i = 0; i < N; i++) {
                revR[i] = s.active[m] ? revRight(node[m][i]) : node[m][i];
                if (s.active[m]) node[m][i] = mod(m, node[m][i]);
            }
        } else if (s.active[m]) {
            for (uint16_t i = 0; i < N; i++) node[m][i] = mod(m, node[m][i]);
        }
    });
}

static void testDefaultPatchMatchesLegacy() {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> sample(-0.5f, 0.5f);
    std::uniform_int_distribution<int> mode(0, 6), coin(0, 1);
    float worst = 0;
    for (int trial = 0; trial < 20000; trial++) {
        State s;
        for (uint8_t k = 0; k < kSources; k++) {
            s.mode[k] = (uint8_t)mode(rng);
            for (uint16_t i = 0; i < N; i++) s.src[k][i] = (coin(rng) || trial % 3) ? sample(rng) : 0.0f;
        }
        for (uint8_t m = 0; m < 6; m++) s.active[m] = trial % 7 == 0 ? true : coin(rng);
        float aL[N], aR[N], bL[N], bR[N];
        legacyChain(s, aL, aR);
        routedChain(s, kDefaultRoutePatch, bL, bR);
        for (uint16_t i = 0; i < N; i++) {
            float d = std::fmax(std::fabs(aL[i] - bL[i]), std::fabs(aR[i] - bR[i]));
            worst = std::fmax(worst, d);
        }
    }
    // Only float rounding from a different summation order may differ.
    CHECK(worst < 1e-5f, "default patch differs from the V1.04 chain by %g", (double)worst);
    std::printf("default patch vs V1.04 chain: 20000 random blocks, worst difference %.3g\n", (double)worst);
}

// Linear modules (x * prime): an impulse's value at the master is the product of its path.
static void testParallelPaths() {
    static const float prime[6] = {2, 3, 5, 7, 11, 13};
    // Example from the design: kick -> Filter 1 -> master, snare -> Filter 2 -> FX 1 -> Reverb -> master,
    // hat -> master directly, clap -> EQ -> FX 2 -> master.
    RoutePatch p = {{routeBit(NODE_FX2), routeBit(NODE_MASTER), routeBit(NODE_FX1), routeBit(NODE_REV), routeBit(NODE_MASTER), routeBit(NODE_MASTER)}};
    CHECK(routePatchValid(p), "example patch should be valid");
    static float node[6][N], revR[N], L[N], R[N];
    static float src[4][N];
    memset(src, 0, sizeof(src));
    const uint8_t at[4] = {1, 5, 9, 13};  // impulse positions, one per source
    for (uint8_t k = 0; k < 4; k++) src[k][at[k]] = 1.0f;
    const float* srcs[4] = {src[0], src[1], src[2], src[3]};
    const uint8_t masks[4] = {routeBit(NODE_F1), routeBit(NODE_F2), routeBit(NODE_MASTER), routeBit(NODE_EQ)};
    RouteBuffers buf = {{node[0], node[1], node[2], node[3], node[4], node[5]}, revR, L, R};
    routeBlock<N>(p, buf, srcs, masks, 4, [&](uint8_t m) {
        for (uint16_t i = 0; i < N; i++) node[m][i] *= prime[m];
        if (m == NODE_REV) for (uint16_t i = 0; i < N; i++) revR[i] = node[m][i];
    });
    const float want[4] = {3, 5 * 7 * 13, 1, 2 * 11};
    const char* name[4] = {"kick", "snare", "hat", "clap"};
    for (uint8_t k = 0; k < 4; k++) {
        CHECK(L[at[k]] == want[k] && R[at[k]] == want[k], "%s: got L=%g R=%g, want %g", name[k], (double)L[at[k]], (double)R[at[k]], (double)want[k]);
    }
    float other = 0;
    for (uint16_t i = 0; i < N; i++)
        if (i != at[0] && i != at[1] && i != at[2] && i != at[3]) other += std::fabs(L[i]) + std::fabs(R[i]);
    CHECK(other == 0, "signal leaked to other samples");

    // One source feeding two places at once: master + reverb.
    const uint8_t both[1] = {static_cast<uint8_t>(routeBit(NODE_MASTER) | routeBit(NODE_REV))};
    const float* one[1] = {src[0]};
    routeBlock<N>(kDefaultRoutePatch, buf, one, both, 1, [&](uint8_t m) {
        for (uint16_t i = 0; i < N; i++) node[m][i] *= prime[m];
        if (m == NODE_REV) for (uint16_t i = 0; i < N; i++) revR[i] = node[m][i];
    });
    CHECK(L[at[0]] == 1 + 13, "dry + reverb copy: got %g", (double)L[at[0]]);

    // Empty mask mutes.
    const uint8_t mute[1] = {0};
    routeBlock<N>(kDefaultRoutePatch, buf, one, mute, 1, [&](uint8_t) {});
    float sum = 0;
    for (uint16_t i = 0; i < N; i++) sum += std::fabs(L[i]) + std::fabs(R[i]);
    CHECK(sum == 0, "muted source reached the master");
}

static void testValidation() {
    CHECK(routePatchValid(kDefaultRoutePatch), "default patch must be valid");
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        for (unsigned mask = 0; mask < 128; mask++) {
            bool expect;
            if (m == NODE_REV) expect = (mask == routeBit(NODE_MASTER));
            else expect = (mask & ((routeBit(m) << 1) - 1)) == 0;  // nothing at or below itself
            CHECK(routeModuleDestValid(m, (uint8_t)mask) == expect, "module %u mask 0x%02x", m, mask);
        }
    }
    CHECK(!routeModuleDestValid(kRouteModuleCount, routeBit(NODE_MASTER)), "out-of-range module");
    for (uint8_t e = 0; e < 7; e++) CHECK(routeMaskFromEntry(e) == routeBit(e), "entry %u", e);
    CHECK(routeMaskFromEntry(7) == 0 && routeMaskFromEntry(255) == 0, "out-of-range entry must mute");
}

// The OUT field writes the old four settings; reading them back must give the same entry point,
// and the priority must match the old layerInst_calculateMixMode for every combination.
static void testLegacyConversion() {
    for (uint8_t e = 0; e < kRouteNodeCount; e++) {
        const uint8_t* l = kRouteLegacyFromEntry[e];
        CHECK(routeEntryFromLegacy(l[0], l[1], l[2], l[3]) == e, "entry %u does not round-trip", e);
    }
    for (uint8_t eq = 0; eq <= 1; eq++)
        for (uint8_t f = 0; f <= 2; f++)
            for (uint8_t fx = 0; fx <= 2; fx++)
                for (uint8_t rv = 0; rv <= 1; rv++) {
                    // V1.04 layerInst_calculateMixMode, transcribed
                    uint8_t old = eq ? 0 : f == 2 ? 1 : f == 1 ? 2 : fx == 2 ? 3 : fx == 1 ? 4 : rv ? 5 : 6;
                    CHECK(routeEntryFromLegacy(eq, f, fx, rv) == old, "eq %u filter %u effect %u reverb %u", eq, f, fx, rv);
                }
}

int main() {
    testLegacyConversion();
    testDefaultPatchMatchesLegacy();
    testParallelPaths();
    testValidation();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
