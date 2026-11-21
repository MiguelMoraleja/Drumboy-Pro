// Host-side tests for the noise and tone generators (Generator.h).

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Generator.h"

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

static std::vector<float> render(uint8_t type, float ratio, float octaves, uint8_t shape, uint32_t length, uint32_t samples, uint32_t seed = 1234) {
    GenVoice v;
    genVoiceStart(v, type, seed, ratio, octaves, shape, length);
    std::vector<float> out(samples);
    for (uint32_t i = 0; i < samples; i += 32) genVoiceRender(v, out.data() + i, static_cast<uint16_t>((samples - i < 32) ? samples - i : 32));
    return out;
}

static float rms(const std::vector<float>& x, size_t from = 0, size_t to = 0) {
    if (to == 0) to = x.size();
    double s = 0;
    for (size_t i = from; i < to; i++) s += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(std::sqrt(s / static_cast<double>(to - from)));
}

static float peak(const std::vector<float>& x) {
    float p = 0;
    for (float v : x) p = std::fmax(p, std::fabs(v));
    return p;
}

// Energy of the first difference over the energy of the signal: about 2 for white noise, smaller
// the more the spectrum leans to the low end.
static float brightness(const std::vector<float>& x) {
    double d = 0, s = 0;
    for (size_t i = 1; i < x.size(); i++) {
        d += static_cast<double>(x[i] - x[i - 1]) * (x[i] - x[i - 1]);
        s += static_cast<double>(x[i]) * x[i];
    }
    return static_cast<float>(d / s);
}

// Upward zero crossings per second in [from, to).
static float crossingsHz(const std::vector<float>& x, size_t from, size_t to) {
    int n = 0;
    for (size_t i = from + 1; i < to; i++)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) n++;
    return static_cast<float>(n) * kGenSampleRate / static_cast<float>(to - from);
}

static bool finite(const std::vector<float>& x) {
    for (float v : x)
        if (!std::isfinite(v)) return false;
    return true;
}

int main() {
    // 1. FILE BLOCK
    {
        GenMix g;
        CHECK(g.type[0] == GEN_OFF && g.length[9] == kGenLengthDefault && g.sweep[3] == 0 && g.hissType == HISS_OFF, "defaults");
        g.type[2] = GEN_SIREN;
        g.length[2] = 16;
        g.sweep[2] = 75;
        g.type[9] = GEN_LASER;
        g.hissType = HISS_VINYL;
        g.hissLevel = 42;
        g.hissTone = 55;
        g.hissDuck = 80;
        uint8_t data[kGenFileSize];
        genWrite(g, data);
        CHECK(memcmp(data, "GEN", 3) == 0 && data[3] == 1, "marker and version");
        GenMix r;
        CHECK(genRead(r, data, kGenFileSize), "reads back");
        CHECK(r.type[2] == GEN_SIREN && r.length[2] == 16 && r.sweep[2] == 75 && r.type[9] == GEN_LASER, "layers");
        CHECK(r.hissType == HISS_VINYL && r.hissLevel == 42 && r.hissTone == 55 && r.hissDuck == 80, "hiss");
        CHECK(!genRead(r, data, kGenFileSize - 1) && r.type[2] == GEN_OFF, "short block: defaults");
        uint8_t bad[kGenFileSize];
        memcpy(bad, data, sizeof bad);
        bad[4] = kGenTypeCount;
        CHECK(!genRead(r, bad, kGenFileSize), "unknown type refused");
        memcpy(bad, data, sizeof bad);
        bad[4 + kGenLayers] = 0;
        CHECK(!genRead(r, bad, kGenFileSize), "length 0 refused");
        memcpy(bad, data, sizeof bad);
        bad[3] = 2;
        CHECK(!genRead(r, bad, kGenFileSize), "newer version refused");
        memcpy(bad, data, sizeof bad);
        bad[kGenFileSize - 4] = kHissTypeCount;
        CHECK(!genRead(r, bad, kGenFileSize), "unknown hiss refused");
        uint8_t zeros[kGenFileSize] = {0};
        CHECK(!genRead(r, zeros, kGenFileSize), "no block");
    }

    // 2. LENGTH AND SWEEP
    CHECK(genLengthSamples(4, 120.0f) == 22050, "a quarter at 120 BPM: %u", genLengthSamples(4, 120.0f));
    CHECK(genLengthSamples(16, 60.0f) == 176400, "a bar at 60 BPM");
    CHECK(genLengthSamples(0, 120.0f) == genLengthSamples(1, 120.0f) && genLengthSamples(200, 120.0f) == genLengthSamples(kGenLengthMax, 120.0f), "clamped");
    CHECK(genSweepOctaves(0) == 0.0f && std::fabs(genSweepOctaves(100) - 4.0f) < 1e-6f && std::fabs(genSweepOctaves(250) - 4.0f) < 1e-6f, "sweep octaves");
    CHECK(genShape(0, 0.0f) == 0.0f && genShape(0, 1.0f) == 1.0f, "up");
    CHECK(genShape(1, 0.0f) == 1.0f && genShape(1, 1.0f) == 0.0f, "down");
    CHECK(genShape(2, 0.0f) == 0.0f && genShape(2, 0.5f) == 1.0f && genShape(2, 1.0f) == 0.0f, "up-down");
    CHECK(genShape(3, 0.0f) == 1.0f && genShape(3, 0.5f) == 0.0f && genShape(3, 1.0f) == 1.0f, "down-up");
    CHECK(genShape(0, -1.0f) == 0.0f && genShape(0, 2.0f) == 1.0f, "progress clamped");
    CHECK(genSweepFactor(0.0f, 0.3f) == 1.0f, "no sweep, no change");
    CHECK(std::fabs(genSweepFactor(2.0f, 0.0f) - 0.5f) < 1e-6f && std::fabs(genSweepFactor(2.0f, 1.0f) - 2.0f) < 1e-6f, "two octaves end to end");

    // 3. EVERY TYPE: finite, bounded, audible, silent start (fade-in), reproducible
    const uint32_t second = 44100;
    for (uint8_t t = GEN_WHITE; t < kGenTypeCount; t++) {
        const std::vector<float> x = render(t, 1.0f, 2.0f, 2, second, second);
        CHECK(finite(x), "%s finite", kGenTypeName[t]);
        CHECK(peak(x) <= 1.2f, "%s peak %f", kGenTypeName[t], peak(x));
        const float level = rms(x);
        CHECK(level > 0.02f && level < 0.6f, "%s rms %f", kGenTypeName[t], level);
        CHECK(std::fabs(x[0]) < 1e-6f && std::fabs(x[10]) < 0.2f, "%s fades in", kGenTypeName[t]);
        const std::vector<float> y = render(t, 1.0f, 2.0f, 2, second, 2000);
        CHECK(memcmp(x.data(), y.data(), 2000 * sizeof(float)) == 0, "%s reproducible", kGenTypeName[t]);
        // Extreme settings stay finite and bounded.
        const std::vector<float> lo = render(t, 0.25f, 4.0f, 1, 4096, 4096), hi = render(t, 4.0f, 4.0f, 0, 4096, 4096);
        CHECK(finite(lo) && finite(hi) && peak(lo) <= 1.5f && peak(hi) <= 1.5f, "%s extremes %f %f", kGenTypeName[t], peak(lo), peak(hi));
    }
    CHECK(rms(render(GEN_OFF, 1.0f, 0.0f, 0, 1000, 1000)) == 0.0f, "OFF is silent");

    // 4. COLOURS: white brighter than pink, pink brighter than brown; PITCH darkens the tone.
    {
        const float w = brightness(render(GEN_WHITE, 4.0f, 0.0f, 0, second, second));
        const float p = brightness(render(GEN_PINK, 4.0f, 0.0f, 0, second, second));
        const float b = brightness(render(GEN_BROWN, 4.0f, 0.0f, 0, second, second));
        CHECK(w > 1.5f && w > 2.0f * p && p > 2.0f * b, "white %f pink %f brown %f", w, p, b);
        const float dark = brightness(render(GEN_WHITE, 0.25f, 0.0f, 0, second, second));
        CHECK(dark < 0.5f * w, "PITCH down closes the tone: %f vs %f", dark, w);
        // A sweep up on the tone: the end is brighter than the start.
        const std::vector<float> s = render(GEN_WHITE, 0.25f, 4.0f, 0, second, second);
        const std::vector<float> a(s.begin() + 1000, s.begin() + 10000), z(s.end() - 9000, s.end());
        CHECK(brightness(z) > 2.0f * brightness(a), "tone sweep up: %f -> %f", brightness(a), brightness(z));
    }

    // 5. SIREN: around its pitch, half an octave each way; SWEEP moves the warble rate.
    {
        const std::vector<float> x = render(GEN_SIREN, 1.0f, 0.0f, 0, second, second);
        const float f = crossingsHz(x, 0, second);
        CHECK(f > kGenSirenHz * 0.85f && f < kGenSirenHz * 1.2f, "siren average %f Hz", f);
        const std::vector<float> o = render(GEN_SIREN, 2.0f, 0.0f, 0, second, second);
        CHECK(std::fabs(crossingsHz(o, 0, second) / f - 2.0f) < 0.1f, "PITCH +12 doubles the siren");
        // Warble: short windows differ by about an octave between low and high.
        float lo = 1e9f, hi = 0.0f;
        for (uint32_t w = 0; w + 1470 <= second; w += 1470) {
            const float wf = crossingsHz(x, w, w + 1470);
            lo = std::fmin(lo, wf);
            hi = std::fmax(hi, wf);
        }
        CHECK(hi / lo > 1.5f && hi / lo < 2.4f, "warble %f..%f", lo, hi);
    }

    // 6. LASER: a sweep down falls in pitch, a sweep up rises.
    {
        const std::vector<float> d = render(GEN_LASER, 1.0f, 4.0f, 1, second, second);
        CHECK(crossingsHz(d, 0, 8000) > 3.0f * crossingsHz(d, second - 8000, second), "laser down %f -> %f", crossingsHz(d, 0, 8000), crossingsHz(d, second - 8000, second));
        const std::vector<float> u = render(GEN_LASER, 1.0f, 4.0f, 0, second, second);
        CHECK(crossingsHz(u, second - 8000, second) > 3.0f * crossingsHz(u, 0, 8000), "laser up");
        const float mid = crossingsHz(render(GEN_LASER, 1.0f, 0.0f, 0, second, second), 0, second);
        CHECK(std::fabs(mid - kGenLaserHz) < 10.0f, "laser at its pitch: %f", mid);
    }

    // 7. DUST: sparse clicks: mostly near silence, a few large values.
    {
        const std::vector<float> x = render(GEN_DUST, 1.0f, 0.0f, 0, second, 4 * second);
        int loud = 0;
        for (float v : x)
            if (std::fabs(v) > 0.1f) loud++;
        CHECK(loud > 3 && loud < 400, "dust clicks %d", loud);
    }

    // 8. HISS BED
    {
        HissBed h;
        memset(&h, 0, sizeof h);
        std::vector<float> l(second + 32), r(second + 32);  // room for the last whole block
        for (uint32_t i = 0; i < second; i += 32) hissRender(h, HISS_OFF, 100, 70, 0, 0.0f, &l[i], &r[i], 32);
        CHECK(rms(l) == 0.0f && rms(r) == 0.0f, "OFF is silent");
        for (uint8_t t = HISS_TAPE; t < kHissTypeCount; t++) {
            memset(&h, 0, sizeof h);
            for (uint32_t i = 0; i < second; i += 32) hissRender(h, t, 100, 100, 0, 0.0f, &l[i], &r[i], 32);
            const float lv = rms(l, 4410), rv = rms(r, 4410);
            CHECK(finite(l) && finite(r) && peak(l) < 0.9f, "%s finite, peak %f", kHissTypeName[t], peak(l));
            CHECK(lv > 0.03f && lv < 0.25f && rv > 0.03f && rv < 0.25f, "%s level 100: %f %f", kHissTypeName[t], lv, rv);
            double c = 0;
            for (uint32_t i = 0; i < second; i++) c += static_cast<double>(l[i]) * r[i];
            const float corr = static_cast<float>(c / second) / (rms(l) * rms(r));
            CHECK(std::fabs(corr) < 0.2f, "%s left and right differ: %f", kHissTypeName[t], corr);
        }
        // Level: 50 is 12 dB under 100.
        memset(&h, 0, sizeof h);
        for (uint32_t i = 0; i < second; i += 32) hissRender(h, HISS_PINK, 100, 70, 0, 0.0f, &l[i], &r[i], 32);
        const float full = rms(l, 4410);
        memset(&h, 0, sizeof h);
        for (uint32_t i = 0; i < second; i += 32) hissRender(h, HISS_PINK, 50, 70, 0, 0.0f, &l[i], &r[i], 32);
        const float half = rms(l, 4410);
        CHECK(std::fabs(20.0f * std::log10(full / half) - 12.0f) < 1.5f, "level 50: %f dB down", 20.0f * std::log10(full / half));
        // Ducking: a loud layer pulls the bed down, by DUCK; without DUCK nothing changes.
        memset(&h, 0, sizeof h);
        for (uint32_t i = 0; i < second; i += 32) hissRender(h, HISS_PINK, 100, 70, 100, 1.0f, &l[i], &r[i], 32);
        CHECK(rms(l, 4410) < 0.05f * full, "ducked: %f", rms(l, 4410));
        memset(&h, 0, sizeof h);
        for (uint32_t i = 0; i < second; i += 32) hissRender(h, HISS_PINK, 100, 70, 50, 1.0f, &l[i], &r[i], 32);
        CHECK(std::fabs(rms(l, 4410) / full - 0.5f) < 0.1f, "half ducked: %f", rms(l, 4410) / full);
        memset(&h, 0, sizeof h);
        for (uint32_t i = 0; i < second; i += 32) hissRender(h, HISS_PINK, 100, 70, 0, 1.0f, &l[i], &r[i], 32);
        CHECK(std::fabs(rms(l, 4410) / full - 1.0f) < 0.02f, "DUCK 0 ignores the key");
        // The bed comes back after the key stops (release about a quarter second).
        memset(&h, 0, sizeof h);
        for (uint32_t i = 0; i < second; i += 32) hissRender(h, HISS_PINK, 100, 70, 100, (i < 4410) ? 1.0f : 0.0f, &l[i], &r[i], 32);
        CHECK(rms(l, second - 8820) > 0.8f * full, "released: %f", rms(l, second - 8820) / full);
        // No step at a level change: the gain glides across a block.
        memset(&h, 0, sizeof h);
        hissRender(h, HISS_PINK, 0, 70, 0, 0.0f, &l[0], &r[0], 32);
        hissRender(h, HISS_PINK, 100, 70, 0, 0.0f, &l[32], &r[32], 32);
        CHECK(std::fabs(l[32]) < 0.02f, "glides in: %f", l[32]);
        // Set to OFF: it fades out over one block, then it is silent and its level is exactly 0.
        memset(&h, 0, sizeof h);
        for (uint32_t i = 0; i < 4410; i += 32) hissRender(h, HISS_TAPE, 100, 70, 0, 0.0f, &l[i], &r[i], 32);
        hissRender(h, HISS_OFF, 100, 70, 0, 0.0f, &l[0], &r[0], 32);
        CHECK(std::fabs(l[0]) > 0.0f && std::fabs(l[31]) < 0.01f && h.amp == 0.0f, "fades out: %f %f", l[0], l[31]);
        hissRender(h, HISS_OFF, 100, 70, 0, 0.0f, &l[0], &r[0], 32);
        CHECK(l[5] == 0.0f && r[20] == 0.0f, "then silent");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
