// Host-side test of the channel strip (ChannelStrip.h): known responses and levels of every block, the fixed order,
// bypass, NaN safety and the file block.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ChannelStrip.h"

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

static const float kRate = 44100.0f;
static const uint16_t kBlock = 32;

// Runs a signal through the strip in blocks of 32.
static void run(const ChSettings& s, ChState& st, std::vector<float>& l, std::vector<float>* r = nullptr, const std::vector<float>* side = nullptr) {
    size_t done = 0;
    for (size_t i = 0; i + kBlock <= l.size(); i += kBlock, done = i) chProcess(s, st, &l[i], r ? &(*r)[i] : nullptr, side ? &(*side)[i] : nullptr, kBlock, kRate);
    l.resize(done);  // only whole blocks are processed: the tail that was not is dropped (it would sit in the measurements as a raw signal)
    if (r) r->resize(done);
}

static std::vector<float> sine(float hz, float amp, size_t n) {
    std::vector<float> v(n);
    for (size_t i = 0; i < n; i++) v[i] = static_cast<float>(amp * std::sin(6.283185307179586 * static_cast<double>(hz) * static_cast<double>(i) / 44100.0));  // double: a float phase is noisy at -50 dB
    return v;
}

static double rms(const std::vector<float>& v, size_t from) {
    double s = 0;
    for (size_t i = from; i < v.size(); i++) s += static_cast<double>(v[i]) * v[i];
    return std::sqrt(s / static_cast<double>(v.size() - from));
}

// Gain in dB of the strip for a sine (after it settles): output rms over input rms.
static double gainDb(const ChSettings& s, float hz, float amp = 0.3f) {
    ChState st;
    auto x = sine(hz, amp, 44100);
    const double in = rms(x, 20000);
    run(s, st, x);
    return 20.0 * std::log10(rms(x, 20000) / in);
}

// The magnitude of one frequency in a signal (Goertzel), for the harmonics test.
static double bin(const std::vector<float>& v, float hz, size_t from) {
    double re = 0, im = 0;
    for (size_t i = from; i < v.size(); i++) {
        const double w = 6.283185307 * hz * static_cast<double>(i) / kRate;
        re += v[i] * std::cos(w);
        im += v[i] * std::sin(w);
    }
    return std::sqrt(re * re + im * im) / static_cast<double>(v.size() - from);
}

static uint8_t idxOfHz(float hz) { return static_cast<uint8_t>(std::lround(100.0 * std::log10(hz / 20.0) / 3.0)); }

int main() {
    // The scales.
    CHECK(std::fabs(chHz(0) - 20.0f) < 0.01f && std::fabs(chHz(100) - 20000.0f) < 1.0f && std::fabs(chHz(50) - 632.5f) < 1.0f, "Hz scale");
    CHECK(std::fabs(chQ(0) - 0.5f) < 1e-4f && std::fabs(chQ(100) - 10.0f) < 1e-3f, "Q scale");
    CHECK(chAttackMs(0) == 0.0f && chAttackMs(100) == 50.0f && chDecayMs(100) == 0.0f && std::fabs(chDecayMs(0) - 10.0f) < 1e-3f && std::fabs(chDecayMs(99) - 4000.0f) < 1.0f, "ENV scales");
    CHECK(chEqDb(36) == 0.0f && chEqDb(0) == -18.0f && chEqDb(72) == 18.0f && chThreshDb(0) == -60.0f && std::fabs(chThreshDb(100)) < 1e-4f && chRatio(0) == 1.0f && chRatio(13) == 20.0f && chRatio(200) == 20.0f, "EQ and COMP scales");

    // Everything off: the buffers are untouched, bit for bit, even with odd values.
    {
        ChSettings s = chDefaults();
        ChState st;
        std::vector<float> l = sine(1000, 0.5f, 3200), copy = l;
        run(s, st, l);
        CHECK(memcmp(l.data(), copy.data(), l.size() * sizeof(float)) == 0, "bypass is bit exact");
    }

    // FILTER: a 12 dB low-pass at about 1 kHz with Q near 0.7: flat below, -3 dB at the cutoff, -24 dB two octaves up.
    {
        ChSettings s = chDefaults();
        s.on = 1u << CH_FILTER;
        s.filterType = 1;
        s.filterSlope = 1;
        s.filterCutoff = idxOfHz(1000);
        s.filterReson = 12;
        const float fc = chHz(s.filterCutoff);
        CHECK(std::fabs(gainDb(s, 100)) < 0.3, "LP 12: flat at 100 Hz: %f", gainDb(s, 100));
        CHECK(std::fabs(gainDb(s, fc) + 3.0) < 1.2, "LP 12: -3 dB at the cutoff: %f", gainDb(s, fc));
        CHECK(std::fabs(gainDb(s, fc * 4) + 24.0) < 2.5, "LP 12: -24 dB at four times the cutoff: %f", gainDb(s, fc * 4));
        s.filterSlope = 2;
        CHECK(std::fabs(gainDb(s, fc * 4) + 48.0) < 5.0, "LP 24: -48 dB at four times the cutoff: %f", gainDb(s, fc * 4));
        s.filterSlope = 0;
        CHECK(std::fabs(gainDb(s, fc * 4) + 12.0) < 1.5, "LP 6: -12 dB at four times the cutoff: %f", gainDb(s, fc * 4));
        s.filterSlope = 1;
        s.filterType = 2;
        CHECK(std::fabs(gainDb(s, 8000)) < 0.4 && std::fabs(gainDb(s, fc / 4) + 24.0) < 2.5, "HP 12: flat above, -24 dB two octaves down: %f %f", gainDb(s, 8000), gainDb(s, fc / 4));
        s.filterType = 4;
        s.filterReson = 40;
        CHECK(gainDb(s, fc) < -25.0 && std::fabs(gainDb(s, 100)) < 1.0, "notch: deep at the centre: %f", gainDb(s, fc));
        s.filterType = 3;
        CHECK(gainDb(s, fc) > gainDb(s, 100) + 10.0 && gainDb(s, fc) > gainDb(s, 10000) + 10.0, "band-pass peaks at the centre");
        // Resonance raises the peak at the cutoff of a low-pass.
        s.filterType = 1;
        s.filterReson = 12;
        const double low = gainDb(s, fc);
        s.filterReson = 70;
        CHECK(gainDb(s, fc) > low + 6.0, "resonance: the peak rises: %f vs %f", gainDb(s, fc), low);
    }
    // The extra high-pass: 12 dB below its frequency.
    {
        ChSettings s = chDefaults();
        s.on = 1u << CH_FILTER;
        s.filterType = 1;
        s.filterSlope = 1;
        s.filterCutoff = 100;  // 20 kHz (held under Nyquist): transparent
        s.filterReson = 12;
        s.filterHp = idxOfHz(1000);
        const float hp = chHz(s.filterHp);
        CHECK(std::fabs(gainDb(s, hp * 8)) < 0.5 && std::fabs(gainDb(s, hp / 10) + 40.0) < 3.0, "extra HP: %f %f", gainDb(s, hp * 8), gainDb(s, hp / 10));
        s.filterHp = 0;
        CHECK(std::fabs(gainDb(s, 50)) < 0.5, "no extra HP: 50 Hz passes");
    }
    // EQ: a bell of +6 dB at 1 kHz, and the shelves.
    {
        ChSettings s = chDefaults();
        s.on = 1u << CH_EQ;
        s.eqType = 0;
        s.eqFreq = idxOfHz(1000);
        s.eqGain = 48;  // +6 dB
        s.eqQ = 30;
        const float fc = chHz(s.eqFreq);
        CHECK(std::fabs(gainDb(s, fc) - 6.0) < 0.3 && std::fabs(gainDb(s, 80)) < 0.7 && std::fabs(gainDb(s, 12000)) < 0.7, "bell +6: %f %f %f", gainDb(s, fc), gainDb(s, 80), gainDb(s, 12000));
        s.eqGain = 24;  // -6 dB
        CHECK(std::fabs(gainDb(s, fc) + 6.0) < 0.3, "bell -6");
        s.eqType = 1;
        s.eqGain = 48;
        CHECK(std::fabs(gainDb(s, 40) - 6.0) < 0.6 && std::fabs(gainDb(s, 12000)) < 0.6, "low shelf +6: %f %f", gainDb(s, 40), gainDb(s, 12000));
        s.eqType = 2;
        CHECK(std::fabs(gainDb(s, 12000) - 6.0) < 0.6 && std::fabs(gainDb(s, 40)) < 0.6, "high shelf +6");
        s.eqGain = 36;
        CHECK(std::fabs(gainDb(s, 1000)) < 0.05, "EQ at 0 dB is transparent");
    }
    // ENV: the ramp, the tail (-60 dB at the decay time) and no decay.
    {
        ChSettings s = chDefaults();
        s.on = 1u << CH_ENV;
        s.envAttack = 40;  // 20 ms
        s.envDecay = 38;   // 100 ms
        CHECK(std::fabs(chDecayMs(38) - 100.0f) < 1.0f, "decay index 38 is 100 ms");
        ChState st;
        chTrigger(st);
        std::vector<float> x(44100, 1.0f);
        run(s, st, x);
        const size_t at10 = static_cast<size_t>(0.010 * kRate), at20 = static_cast<size_t>(0.020 * kRate), at100 = static_cast<size_t>(0.100 * kRate);
        const double d10 = std::exp(-6.9077553 * 0.010 / 0.100), d100 = std::exp(-6.9077553);
        CHECK(x[0] < 0.01f, "the ramp starts at 0: %f", x[0]);
        CHECK(std::fabs(x[at10] - 0.5 * d10) < 0.03, "half way up the ramp at 10 ms: %f vs %f", x[at10], 0.5 * d10);
        CHECK(std::fabs(x[at20 + 5] - std::exp(-6.9077553 * 0.020 / 0.100)) < 0.03, "the ramp is up at 20 ms and the tail follows its curve: %f", x[at20 + 5]);
        CHECK(std::fabs(x[at100] / d100 - 1.0) < 0.12, "-60 dB at the decay time: %f", x[at100]);
        CHECK(x[40000] < 1e-6f, "the tail ends");
        s.envDecay = 100;
        ChState st2;
        chTrigger(st2);
        std::vector<float> y(4410, 1.0f);
        run(s, st2, y);
        CHECK(std::fabs(y[4000] - 1.0f) < 1e-5f, "no decay: the level holds");
        // Triggered again: the envelope starts over.
        chTrigger(st2);
        std::vector<float> z(320, 1.0f);
        run(s, st2, z);
        CHECK(z[0] < 0.05f && z[319] > 0.2f, "a new note restarts the ramp");
    }
    // DRIVE: unity for small signals, peaks held at full scale, flat, and it aliases less than the same curve at the sample rate.
    {
        ChSettings s = chDefaults();
        s.on = 1u << CH_DRIVE;
        s.driveAmount = 0;
        for (float hz : {200.0f, 1000.0f, 4000.0f, 8000.0f}) CHECK(std::fabs(gainDb(s, hz, 0.01f)) < 0.5, "drive 0: unity gain at %.0f Hz: %f", hz, gainDb(s, hz, 0.01f));
        s.driveAmount = 100;
        ChState st;
        auto x = sine(1000, 1.0f, 8820);
        run(s, st, x);
        float peak = 0;
        for (size_t i = 3000; i < x.size(); i++) peak = std::fmax(peak, std::fabs(x[i]));
        CHECK(peak <= 1.12f && peak > 0.9f, "full drive: peaks stay within about 1 dB of full scale (the halfband filter overshoots a clipped wave): %f", peak);
        // Harmonics: a sine at 7 kHz driven hard: the 5th harmonic (35 kHz) folds to 9.1 kHz at the sample rate.
        s.driveAmount = 60;
        const float g = chDriveGain(60), norm = 1.0f / chShape(g);
        auto a = sine(7000, 0.9f, 44100), naive = a;
        for (auto& v : naive) v = chShape(v * g) * norm;
        ChState st2;
        run(s, st2, a);
        const double over = bin(a, 9100, 20000), plain = bin(naive, 9100, 20000);
        CHECK(over < plain * 0.4, "oversampling: the folded harmonic is at least 8 dB lower: %g vs %g", over, plain);
        CHECK(bin(a, 7000, 20000) > 0.3, "the fundamental is still there");
        // The delay of the oversampling filters is seven samples.
        ChSettings d = chDefaults();
        d.on = 1u << CH_DRIVE;
        d.driveAmount = 0;
        ChState st3;
        std::vector<float> imp(64, 0.0f);
        imp[0] = 0.01f;
        run(d, st3, imp);
        size_t peakAt = 0;
        for (size_t i = 0; i < imp.size(); i++)
            if (std::fabs(imp[i]) > std::fabs(imp[peakAt])) peakAt = i;
        CHECK(peakAt == 7, "the drive delays by 7 samples: %zu", peakAt);
    }
    // COMP: reduction by the threshold and ratio, nothing below the threshold, makeup, and the sidechain.
    {
        ChSettings s = chDefaults();
        s.on = 1u << CH_COMP;
        s.compThresh = 67;  // -19.8 dB
        s.compRatio = 6;    // 4:1
        s.compAttack = 20;
        s.compRelease = 30;
        s.compMakeup = 0;
        const double loud = gainDb(s, 1000, 1.0f), quiet = gainDb(s, 1000, 0.05f);
        CHECK(std::fabs(loud + 14.85) < 2.0, "4:1 over a threshold of -19.8 dB: about -14.9 dB: %f", loud);
        CHECK(std::fabs(quiet) < 0.3, "below the threshold nothing changes: %f", quiet);
        s.compMakeup = 20;
        CHECK(std::fabs(gainDb(s, 1000, 0.05f) - 6.0) < 0.3, "makeup of 6 dB: %f", gainDb(s, 1000, 0.05f));
        s.compMakeup = 0;
        s.compRatio = 0;  // 1:1
        CHECK(std::fabs(gainDb(s, 1000, 1.0f)) < 0.3, "ratio 1:1 does nothing");
        // Sidechain: a loud buffer ducks a quiet one.
        s.compRatio = 13;  // 20:1
        ChState st;
        auto main = sine(500, 0.05f, 44100);
        auto side = sine(100, 1.0f, 44100);
        const double before = rms(main, 20000);
        run(s, st, main, nullptr, &side);
        CHECK(rms(main, 20000) < before * 0.3, "the sidechain ducks the layer: %g of %g", rms(main, 20000), before);
        // The release: after the loud part the gain comes back.
        std::vector<float> quietAgain = sine(500, 0.05f, 44100 * 2);
        std::vector<float> silent(44100 * 2, 0.0f);
        run(s, st, quietAgain, nullptr, &silent);
        CHECK(rms(std::vector<float>(quietAgain.end() - 4410, quietAgain.end()), 0) > 0.05 * 0.707 * 0.9, "the gain comes back after the release");
        // Stereo: both channels get the same gain.
        ChState st2;
        std::vector<float> L = sine(1000, 1.0f, 8820), R = sine(1000, 0.5f, 8820);
        s.compRatio = 6;
        run(s, st2, L, &R);
        CHECK(std::fabs(rms(R, 6000) / rms(L, 6000) - 0.5) < 0.01, "stereo link: the ratio of the channels is kept: %f", rms(R, 6000) / rms(L, 6000));
    }
    // The order is fixed: ENV before DRIVE before FILTER (a decaying 1 kHz sine through a LP at 200 Hz is quieter than the ENV alone).
    {
        ChSettings s = chDefaults();
        s.on = (1u << CH_ENV) | (1u << CH_FILTER);
        s.filterCutoff = idxOfHz(200);
        ChState st;
        chTrigger(st);
        auto x = sine(1000, 0.5f, 4410);
        run(s, st, x);
        ChSettings e = chDefaults();
        e.on = 1u << CH_ENV;
        ChState st2;
        chTrigger(st2);
        auto y = sine(1000, 0.5f, 4410);
        run(e, st2, y);
        CHECK(rms(x, 1000) < rms(y, 1000) * 0.2, "blocks run together");
    }
    // Stereo and mono go through the same chain: each channel as the mono one.
    {
        ChSettings s = chDefaults();
        s.on = (1u << CH_FILTER) | (1u << CH_EQ) | (1u << CH_DRIVE);
        s.eqGain = 48;
        ChState a, b;
        auto m = sine(700, 0.4f, 3200), L = m, R = m;
        run(s, a, m);
        run(s, b, L, &R);
        CHECK(memcmp(m.data(), L.data(), m.size() * sizeof(float)) == 0 && memcmp(m.data(), R.data(), m.size() * sizeof(float)) == 0, "stereo channels equal the mono run");
    }
    // NaN and infinity do not stay: the chain recovers.
    {
        ChSettings s = chDefaults();
        s.on = (1u << CH_FILTER) | (1u << CH_EQ) | (1u << CH_DRIVE) | (1u << CH_COMP) | (1u << CH_ENV);
        ChState st;
        chTrigger(st);
        std::vector<float> x = sine(1000, 0.3f, 3200);
        x[100] = std::nanf("");
        x[200] = INFINITY;
        x[300] = -INFINITY;
        run(s, st, x);
        bool ok = true;
        for (float v : x) ok = ok && std::isfinite(v);
        CHECK(ok, "no non-number leaves the strip");
        std::vector<float> y = sine(1000, 0.3f, 8820);
        run(s, st, y);
        CHECK(rms(y, 4000) > 0.01 && std::isfinite(rms(y, 4000)), "and the chain still makes sound afterwards");
    }
    // Changing a setting changes the response at once (the coefficients are designed again).
    {
        ChSettings s = chDefaults();
        s.on = 1u << CH_FILTER;
        s.filterCutoff = idxOfHz(500);
        ChState st;
        auto a = sine(4000, 0.3f, 4410);
        run(s, st, a);
        const double low = rms(a, 2000);
        s.filterCutoff = 100;
        auto b = sine(4000, 0.3f, 4410);
        run(s, st, b);
        CHECK(rms(b, 2000) > low * 10.0, "opening the filter lets the 4 kHz through");
    }
    // The file block.
    {
        ChSettings set[10];
        for (int l = 0; l < 10; l++) {
            set[l] = chDefaults();
            set[l].on = static_cast<uint8_t>(l % 32);
            set[l].filterCutoff = static_cast<uint8_t>(10 * l);
            set[l].compSide = static_cast<uint8_t>(l);
        }
        uint8_t blob[kChFileSize];
        chWriteBlock(set, blob);
        ChSettings back[10];
        CHECK(chReadBlock(back, blob, kChFileSize), "reads what it wrote");
        bool same = true;
        for (int l = 0; l < 10; l++) same = same && (memcmp(&set[l], &back[l], sizeof(ChSettings)) == 0);
        CHECK(same && kChFileSize == 224, "round trip");
        CHECK(!chReadBlock(back, blob, kChFileSize - 1), "too short");
        uint8_t bad[kChFileSize];
        memcpy(bad, blob, kChFileSize);
        bad[4 + 3 * kChSettingBytes + 4] = 9;  // a filter type of 9
        CHECK(!chReadBlock(back, bad, kChFileSize) && back[3].on == chDefaults().on && back[3].filterCutoff == chDefaults().filterCutoff, "an out-of-range value rejects the block and leaves the defaults");
        memcpy(bad, blob, kChFileSize);
        bad[0] = 'X';
        CHECK(!chReadBlock(back, bad, kChFileSize), "wrong marker");
        memcpy(bad, blob, kChFileSize);
        bad[3] = 2;
        CHECK(!chReadBlock(back, bad, kChFileSize), "unknown version");
        memcpy(bad, blob, kChFileSize);
        bad[4] = 64;  // a block bit that does not exist
        CHECK(!chReadBlock(back, bad, kChFileSize), "an unknown block switch rejects the block");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
