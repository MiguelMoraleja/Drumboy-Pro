// Host-side tests for DUB mode (DubMix.h and the send gains of Routing.h).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "DubMix.h"

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

const uint16_t N = 32;

// Each module multiplies by its own prime-ish factor, so what reaches the master tells the path.
static const float kGain[6] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f};  // EQ, F1, F2, FX1, FX2, REV

struct Rig {
    float eq[N], f1[N], f2[N], fx1[N], fx2[N], rev[N], revR[N], outL[N], outR[N];
    RouteBuffers bufs() { return {{eq, f1, f2, fx1, fx2, rev}, revR, outL, outR}; }
};

static void run(Rig& r, RouteSource* sources, uint8_t count, const RoutePatch& patch, RouteState& st) {
    routeBlock<N>(patch, st, r.bufs(), sources, count, [&](uint8_t m) {
        float* buf = (m == NODE_EQ) ? r.eq : (m == NODE_F1) ? r.f1 : (m == NODE_F2) ? r.f2 : (m == NODE_FX1) ? r.fx1 : (m == NODE_FX2) ? r.fx2 : r.rev;
        for (uint16_t i = 0; i < N; i++) buf[i] *= kGain[m];
        if (m == NODE_REV) for (uint16_t i = 0; i < N; i++) r.revR[i] = buf[i];
    });
}

int main() {
    // --- Patch ---
    CHECK(routePatchValid(kDubRoutePatch), "the DUB patch is valid");
    CHECK(kDubRoutePatch.moduleDest[NODE_EQ] == routeBit(NODE_F1) && kDubRoutePatch.moduleDest[NODE_F2] == routeBit(NODE_FX1), "bus and echo paths");
    for (uint8_t m = NODE_F1; m < kRouteModuleCount; m++) {
        if (m == NODE_F2) continue;
        CHECK(kDubRoutePatch.moduleDest[m] == routeBit(NODE_MASTER), "module %u goes to the master", m);
    }

    // --- Gains ---
    float g[kRouteNodeCount];
    DubStrip s;
    dubNodeGains(s, 0, g);
    CHECK(g[NODE_EQ] == 1.0f && g[NODE_MASTER] == 0.0f && g[NODE_F2] == 0.0f && g[NODE_REV] == 0.0f && g[NODE_FX2] == 0.0f && g[NODE_F1] == 0.0f && g[NODE_FX1] == 0.0f, "a default strip is the dry FILT path");
    s.bus = DUB_BUS_DIR;
    s.echo = 35;
    s.verb = 62;
    s.fx = 100;
    dubNodeGains(s, 0, g);
    CHECK(g[NODE_EQ] == 0.0f && g[NODE_MASTER] == 1.0f && std::fabs(g[NODE_F2] - 0.35f) < 1e-6f && std::fabs(g[NODE_REV] - 0.62f) < 1e-6f && g[NODE_FX2] == 1.0f, "DIR strip with sends");
    dubNodeGains(s, THROW_ECHO, g);
    CHECK(g[NODE_F2] == 1.0f && std::fabs(g[NODE_REV] - 0.62f) < 1e-6f, "an echo throw opens the echo send only");
    dubNodeGains(s, THROW_ECHO | THROW_VERB, g);
    CHECK(g[NODE_F2] == 1.0f && g[NODE_REV] == 1.0f, "both throws");
    s.echo = 200;
    dubNodeGains(s, 0, g);
    CHECK(g[NODE_F2] == 1.0f, "a send above 100 is capped");
    CHECK(dubDryScale(true) == 0.0f && dubDryScale(false) == 1.0f, "dry scale");

    // --- Routing with gains: the master gets the product of the modules on each path ---
    float src[N], zeros[N] = {0};
    for (uint16_t i = 0; i < N; i++) src[i] = 0.5f + 0.01f * i;
    RouteState st;
    float state[kRouteNodeCount + 1] = {0};
    DubStrip strip;
    strip.bus = DUB_BUS_FILT;
    strip.echo = 50;
    strip.verb = 20;
    strip.fx = 10;
    float target[kRouteNodeCount];
    dubNodeGains(strip, 0, target);
    Rig r;
    RouteSource sources[1] = {{src, 0, nullptr, target, state}};
    for (int block = 0; block < 3; block++) run(r, sources, 1, kDubRoutePatch, st);  // settle the ramp
    bool ok = true;
    for (uint16_t i = 0; i < N; i++) {
        // FILT: EQ * F1; echo: 0.5 * F2 * FX1; verb: 0.2 * REV; fx: 0.1 * FX2
        const float expected = src[i] * (kGain[0] * kGain[1] + 0.5f * kGain[2] * kGain[3] + 0.2f * kGain[5] + 0.1f * kGain[4]);
        ok = ok && std::fabs(r.outL[i] - expected) < 1e-5f && std::fabs(r.outR[i] - r.outL[i]) < 1e-5f;
    }
    CHECK(ok, "master sum of the four paths");
    // A silent strip adds nothing.
    for (uint8_t n = 0; n < kRouteNodeCount; n++) target[n] = 0.0f;
    for (int block = 0; block < 2; block++) run(r, sources, 1, kDubRoutePatch, st);
    float peak = 0;
    for (uint16_t i = 0; i < N; i++) peak = std::fmax(peak, std::fabs(r.outL[i]));
    CHECK(peak == 0.0f, "all gains 0: silence");
    (void)zeros;

    // --- The ramp: from the old gain to the new one over the block, linear, then steady ---
    float one[N];
    for (uint16_t i = 0; i < N; i++) one[i] = 1.0f;
    float st2[kRouteNodeCount + 1] = {0};
    float tg[kRouteNodeCount] = {0};
    tg[NODE_MASTER] = 1.0f;
    float eqB[N] = {0}, f1B[N] = {0}, f2B[N] = {0}, x1B[N] = {0}, x2B[N] = {0}, rvB[N] = {0}, rrB[N] = {0}, mL[N] = {0}, mR[N] = {0};
    RouteBuffers b = {{eqB, f1B, f2B, x1B, x2B, rvB}, rrB, mL, mR};
    routeScatterGains<N>(b, one, tg, st2);
    CHECK(std::fabs(mL[0] - 1.0f / N) < 1e-6f && std::fabs(mL[N - 1] - 1.0f) < 1e-6f && std::fabs(mL[15] - 16.0f / N) < 1e-6f && st2[NODE_MASTER] == 1.0f, "ramp up: %f %f", mL[0], mL[N - 1]);
    memset(mL, 0, sizeof mL);
    memset(mR, 0, sizeof mR);
    routeScatterGains<N>(b, one, tg, st2);
    CHECK(mL[0] == 1.0f && mL[N - 1] == 1.0f && mR[7] == 1.0f, "steady");
    tg[NODE_MASTER] = 0.0f;
    memset(mL, 0, sizeof mL);
    routeScatterGains<N>(b, one, tg, st2);
    CHECK(std::fabs(mL[0] - (1.0f - 1.0f / N)) < 1e-6f && std::fabs(mL[N - 1]) < 1e-6f, "ramp down");
    // Other nodes receive their own gain only.
    tg[NODE_F2] = 0.25f;
    routeScatterGains<N>(b, one, tg, st2);  // the ramp
    memset(f2B, 0, sizeof f2B);
    memset(eqB, 0, sizeof eqB);
    routeScatterGains<N>(b, one, tg, st2);  // steady
    CHECK(std::fabs(f2B[N - 1] - 0.25f) < 1e-6f && eqB[5] == 0.0f, "echo node");

    // --- File block ---
    DubMix d;
    d.on = true;
    for (uint8_t i = 0; i < kDubStrips; i++) {
        d.strip[i].bus = i % 2;
        d.strip[i].echo = static_cast<uint8_t>(i * 10);
        d.strip[i].verb = static_cast<uint8_t>(100 - i * 10);
        d.strip[i].fx = static_cast<uint8_t>(i);
    }
    uint8_t blob[kDubFileSize];
    dubWrite(d, blob);
    DubMix back;
    CHECK(dubRead(back, blob, kDubFileSize) && back.on && back.strip[3].bus == 1 && back.strip[3].echo == 30 && back.strip[3].verb == 70 && back.strip[9].fx == 9, "round trip");
    CHECK(!dubRead(back, blob, kDubFileSize - 1) && !back.on, "too short: default");
    // Pan: kept, defaults to the centre, validated; a version 1 block (no pan) still reads.
    d.pan[4] = 7;
    d.pan[9] = 40;
    dubWrite(d, blob);
    CHECK(dubRead(back, blob, kDubFileSize) && back.pan[4] == 7 && back.pan[9] == 40 && back.pan[0] == kPanCentre, "pan round trip");
    uint8_t v1[kDubFileSizeV1];
    memcpy(v1, blob, kDubFileSizeV1);
    v1[3] = 1;
    CHECK(dubRead(back, v1, kDubFileSizeV1) && back.on && back.strip[3].echo == 30 && back.pan[4] == kPanCentre && back.pan[9] == kPanCentre, "version 1 is read, centred");
    CHECK(!dubRead(back, blob, kDubFileSizeV1), "a version 2 block that is too short is refused");
    uint8_t badPan[kDubFileSize];
    memcpy(badPan, blob, kDubFileSize);
    badPan[kDubFileSizeV1 + 2] = 41;
    CHECK(!dubRead(back, badPan, kDubFileSize), "a pan past the end is refused");
    DubMix fresh;
    CHECK(fresh.pan[0] == kPanCentre && fresh.pan[9] == kPanCentre, "a new mix is centred");
    uint8_t bad[kDubFileSize];
    memcpy(bad, blob, kDubFileSize);
    bad[3] = 3;
    CHECK(!dubRead(back, bad, kDubFileSize), "unknown version");
    memcpy(bad, blob, kDubFileSize);
    bad[6 + 4 * 2] = 101;
    CHECK(!dubRead(back, bad, kDubFileSize) && !back.on && back.strip[2].echo == 0, "a send over 100 rejects the block");
    memcpy(bad, blob, kDubFileSize);
    bad[5] = 2;
    CHECK(!dubRead(back, bad, kDubFileSize), "unknown bus");
    memcpy(bad, blob, kDubFileSize);
    bad[0] = 'X';
    CHECK(!dubRead(back, bad, kDubFileSize), "wrong marker");
    CHECK(kDubFileSize == 55 && kDubFileSizeV1 == 45, "block sizes %u %u", kDubFileSize, kDubFileSizeV1);

    // --- Pan on the master ---
    {
        float one2[N];
        for (uint16_t i = 0; i < N; i++) one2[i] = 1.0f;
        float eq2[N], f12[N], f22[N], x12[N], x22[N], rv2[N], rr2[N], mL2[N], mR2[N];
        RouteBuffers b2 = {{eq2, f12, f22, x12, x22, rv2}, rr2, mL2, mR2};
        auto clear = [&]() {
            memset(mL2, 0, sizeof mL2);
            memset(mR2, 0, sizeof mR2);
            memset(f22, 0, sizeof f22);
        };
        // The gains path: the master pan splits the master gain, other nodes are untouched.
        float tg2[kRouteNodeCount] = {0}, state3[kRouteNodeCount + 1] = {0};
        tg2[NODE_MASTER] = 1.0f;
        tg2[NODE_F2] = 0.5f;
        float pl, pr;
        panGains(8, pl, pr);
        const float pan3[2] = {pl, pr};
        for (int k = 0; k < 2; k++) {
            clear();
            routeScatterGains<N>(b2, one2, tg2, state3, pan3);
        }
        CHECK(std::fabs(mL2[N - 1] - pl) < 1e-5f && std::fabs(mR2[N - 1] - pr) < 1e-5f && std::fabs(f22[N - 1] - 0.5f) < 1e-6f, "panned master, echo send untouched: %f %f %f", mL2[N - 1], mR2[N - 1], f22[N - 1]);
        // Without pan both sides are as before.
        float state4[kRouteNodeCount + 1] = {0};
        for (int k = 0; k < 2; k++) {
            clear();
            routeScatterGains<N>(b2, one2, tg2, state4);
        }
        CHECK(mL2[N - 1] == 1.0f && mR2[N - 1] == 1.0f, "no pan: centred");
        // A centred pan is exactly the unpanned signal.
        float state5[kRouteNodeCount + 1] = {0};
        const float centre[2] = {1.0f, 1.0f};
        for (int k = 0; k < 2; k++) {
            clear();
            routeScatterGains<N>(b2, one2, tg2, state5, centre);
        }
        CHECK(mL2[7] == 1.0f && mR2[7] == 1.0f, "centre pan is exact");
        // The pan moves with a ramp: the first block goes from centre to the new gains.
        float state6[kRouteNodeCount + 1] = {0};
        for (int k = 0; k < 2; k++) {
            clear();
            routeScatterGains<N>(b2, one2, tg2, state6, centre);
        }
        clear();
        routeScatterGains<N>(b2, one2, tg2, state6, pan3);
        CHECK(std::fabs(mL2[0] - (1.0f + (pl - 1.0f) / N)) < 1e-5f && std::fabs(mL2[N - 1] - pl) < 1e-5f && std::fabs(mR2[N - 1] - pr) < 1e-5f, "pan ramp: %f %f", mL2[0], mL2[N - 1]);
        // The mask path: a source to the master and the reverb: the reverb node as it is, the master panned.
        float ms[2] = {1.0f, 1.0f};
        const uint8_t mask = routeBit(NODE_MASTER) | routeBit(NODE_REV);
        RouteState rs;
        RouteSource sources2[1] = {{one2, mask, nullptr, nullptr, nullptr, pan3, ms}};
        Rig r2;
        run(r2, sources2, 1, kDefaultRoutePatch, rs);
        run(r2, sources2, 1, kDefaultRoutePatch, rs);
        // The reverb stand-in multiplies by its gain; what reaches the master from it is 0.4, equal on both sides.
        const float rev = kGain[NODE_REV];
        CHECK(std::fabs(r2.outL[N - 1] - (pl + rev)) < 1e-5f && std::fabs(r2.outR[N - 1] - (pr + rev)) < 1e-5f, "mask path: %f %f", r2.outL[N - 1], r2.outR[N - 1]);
        // A source with a mask that does not include the master adds nothing to it (the pan is not applied to the nodes).
        float ms2[2] = {1.0f, 1.0f};
        RouteSource sources3[1] = {{one2, routeBit(NODE_REV), nullptr, nullptr, nullptr, pan3, ms2}};
        run(r2, sources3, 1, kDefaultRoutePatch, rs);
        run(r2, sources3, 1, kDefaultRoutePatch, rs);
        CHECK(std::fabs(r2.outL[N - 1] - rev) < 1e-5f && std::fabs(r2.outR[N - 1] - rev) < 1e-5f, "a mask without the master: %f %f", r2.outL[N - 1], r2.outR[N - 1]);
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
