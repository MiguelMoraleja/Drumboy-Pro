# Drumboy Pro firmware backlog

Open work for the application firmware (`02 Software/Drumboy-Pro-App-H723`), as of **V1.09**. Each item says what is wrong or missing, what the code does today, and where to start. Function names are used instead of line numbers, which move.

Before you start, read the README's **Firmware** section: `make WERROR=1`, `make -C "02 Software/tests"`, `make PROFILE=1` for DSP load, and the release steps. The hardware-free parts of the firmware (`Fill.h`, `Routing.h`) have host tests; put new logic there when you can, so it can be tested on a PC.

This work lives in the fork **MiguelMoraleja/Drumboy-Pro**. Upstream (**Randomwaves/Drumboy-Pro**) has none of it yet.

| # | Item | Type | Size |
|---|---|---|---|
| 1 | [LFO-modulated values don't move on the display](#1-lfo-modulated-values-dont-move-on-the-display) | UX bug | S |
| 2 | [Instrument pitch should step in semitones and octaves](#2-instrument-pitch-should-step-in-semitones-and-octaves) | Feature | M |
| 3 | [LFOs can't modulate filters or effects](#3-lfos-cant-modulate-filters-or-effects) | Feature | L |
| 4 | [Routing: multiple destinations per source](#4-routing-multiple-destinations-per-source) | Feature | M |
| 5 | [Routing: send levels](#5-routing-send-levels) | Feature | M |
| 6 | [Routing: extra effect slots](#6-routing-extra-effect-slots) | Feature | L |
| 7 | [Routing: open design questions](#7-routing-open-design-questions) | Design | — |
| 8 | [LIMITER setting does nothing](#8-limiter-setting-does-nothing) | Bug | S |
| 9 | [Song/kit file format version](#9-songkit-file-format-version) | Groundwork | M |
| 10 | [Housekeeping](#10-housekeeping) | Chores | S |

Sizes are rough: S = a day, M = a few days, L = a week or more, including hardware testing.

---

## LFO and instrument

### 1. LFO-modulated values don't move on the display

**What happens.** With an LFO assigned to a layer parameter (level, pitch, start, …), the sound changes, but the field on the instrument page keeps showing the base value.

**Why.** LFO modulation is sample-and-hold at note trigger. `Controller::layer_playBeat()` reads each assigned LFO and computes `nLevel`, `nPitch`, `nStart`, `nEnd`, `nTiming`, `nProbability`, `nResolution`, `nDirection` as **local variables**, applies them to the note (`BeatPlayData`), and keeps nothing. The `lcd_drawLayerInst_*Data()` functions draw `layer.level`, `layer.pitch`, … only.

**Proposal.**
- Store the last applied values per layer, for example in `LayerPlayData` (`lastLevel`, `lastPitch`, …), written in `layer_playBeat()`. That's a few bytes per hit and nothing per sample.
- While a layer has an LFO on a parameter and the sequencer runs, show the last applied value. Either replace the value in a second colour, or show it in the sign box next to the LFO label. Refresh from the main loop at most ~20 Hz, not from the audio interrupt.
- Optional: a moving playhead on the LFO page graph (`lcd_drawLfo_GraphData()`) from `lfo.sampleCounter`.

**Done when.** Level with a sine LFO, playing: the LEVEL field visibly follows each hit. When stopped, it shows the base value again. No flicker, and no change in DSP load (check with `make PROFILE=1`).

### 2. Instrument pitch should step in semitones and octaves

**What happens.** PITCH on instrument page 0 is a playback-speed factor from 0.50 to 2.00 in steps of 0.01 (`kLayerPitchDataLibrary`, 151 entries, index 50 = 1.00). So it covers only one octave down and one up, and most values are not musical intervals. Patterning-style use (dial a pitch in semitones, jump by octaves) is not possible.

**Proposal.**
- Musical scale: index = semitones from −24 to +24 (or ±12), factor = 2^(n/12). Shown as `-12`, `+7`, `+12` and so on.
- Encoder steps one semitone; with SHIFT it steps one octave (12). SHIFT on this encoder currently picks the LFO for pitch (`encoder_incValue`, `ENC_INST_0_PITCH`), so a different modifier or a setting is needed; decide on the device.
- LFO on pitch: today it adds up to ±100 table steps. With the new scale, define depth in semitones.
- Check the sample player at factors above 2.0 (the `increment` in `BeatPlayData`, end-of-sample handling, aliasing) before allowing +24.

**Compatibility.** Kits and songs store the pitch index (layer record byte 1). Old files must keep their pitch. Either convert the old linear index to the nearest semitone on load (changes some pitches slightly) or keep both scales with a per-layer flag. Needs item 9 (format version).

**Done when.** Pitch steps in semitones and octaves on the device, old kits sound the same (or as documented), and the table and conversion have host tests.

### 3. LFOs can't modulate filters or effects

**What happens.** Confirmed in code: LFOs have exactly eight destinations, all per layer: level, pitch, direction, resolution, start, end, timing, probability (`Layer::lfoLevel … lfoProbability`, applied in `layer_playBeat()`). Nothing connects an LFO to EQ, the filters, the effects or the reverb. This is a missing feature, not a bug.

**Proposal.**
- Module destinations, e.g. filter frequency/resonance, effect parameters A–D and wet, reverb wet. Unlike layer targets these are continuous, so update them per audio block, not per hit.
- Smooth the value per block to avoid zipper noise. For filters, recomputing biquad coefficients per block is the main cost: measure with `make PROFILE=1` (V1.04 worst case: 34% of the block, so there is headroom).
- UI: the module menus have no free encoder slots. Reuse the layer pattern (SHIFT + encoder picks the LFO for that parameter, as instrument page 0 does).
- Files: one byte per assignable parameter; find spare bytes in the kit section or use item 9.

**Done when.** An LFO on filter 1 frequency sweeps audibly with no clicks, UDR/OVR stay 0 in the profile build, and assignments save and load.

---

## Routing, phase 3

Phases 0–2 of the routing design are done (V1.05–V1.09): routing engine, layer OUT, ROUTING page (MIXER twice) with overview, click-free changes, saved routing. The engine and files were built with these next steps in mind.

### 4. Routing: multiple destinations per source

E.g. a layer to MASTER and REVERB at once (dry plus reverb send).

- **Engine:** done. Destinations are bitmasks; `routeBlock()` and the crossfades handle several bits, and `test_routing` covers it.
- **UI:** OUT fields choose one destination. Needs a way to toggle several (e.g. press the encoder to add or remove the shown destination), and the overview shows the first only (`routeFirstNode`).
- **Files:** layer masks are already written (layer record byte 23) but the four old settings (bytes 16–19) are authoritative on load. With the marker present, load byte 23 instead, and keep writing the nearest single entry point to 16–19 for older firmware.

### 5. Routing: send levels

A level per route, so a layer can go to MASTER at full level and to REVERB at 30%.

- Needs a gain per (source, destination) in the scatter step. `routeScatter()` already takes an optional per-sample gain for the crossfades.
- UI: the three empty slots on instrument page 2 (tabs 5–7) were kept free for this.
- Files: bytes 175–179 and 499 of the kit section are reserved.

### 6. Routing: extra effect slots

- **Memory:** SDRAM 0xC0EA6000–0xC0FB9640 (1,128,000 bytes, between the chorus buffers and the images) is not referenced anywhere in the code (static check). Two more effect slots need 960 KB (delay + chorus buffers). Confirm at runtime that nothing writes there.
- **CPU:** measure the worst case with `make PROFILE=1` before and after. Gate: < 80% of the block, UDR/OVR = 0.
- **Routing:** the node count goes from 7 to 9, so masks need more than the 7 bits of a `uint8_t` (use `uint16_t`). Update the file layout and `kRouteDestDataLibrary`.
- **UI:** new effect menus and keys. The keypad has no free menu keys; consider a second page on EFFECT 1/2.

### 7. Routing: open design questions

Carried over from the design document; decide before building:
- User-set module order (e.g. FX 1 before Filter 1): cheap in the engine (an order array), harder to read in the UI.
- Routing presets that can be recalled across songs, or routing per song only (today: per kit/song file).
- Per-layer pan before the master: one multiply per sample per layer in the scatter step.

---

## Bugs and groundwork

### 8. LIMITER setting does nothing

The mixer's LIMITER (0–10) is saved in the system preset (byte 6), shown and editable, but no audio code reads `mixer.limiter`. The output only has the fixed soft-knee clip (`softKneeClip`, threshold 0.667) in `processAudioSend()`. Either make LIMITER set the clip threshold or a real limiter, or remove the field.

### 9. Song/kit file format version

Kit and song files have a signature but no format version. Several items above change what is stored (pitch scale, multi-destination routing, LFO module targets, fill types). Add a version byte in a spare kit-section byte (and the song section) so new firmware can convert old files and fail cleanly on newer ones. The routing marker (`0xA5` at kit byte 498) is the pattern to follow.

### 10. Housekeeping

- **Fill types:** 15 of 16 possible. The type is stored in 4 bits; a 17th type needs item 9.
- **`ENC_ROUTE_LINEIN_OUT`:** still has encoder handlers but is no longer on the ROUTING page (line-in is set with the mixer's MIXER IN). Remove it or put it back on the page.
- **Releases:** the Release workflow only runs for tags starting with lowercase `v` (`v1.10`). The `V1.04` release has no build attached for that reason.
- **Vendor warnings:** two GCC warnings remain in FatFs (`ff.c`), both known false positives. Our code is held at zero (`make WERROR=1` in CI).
- **Bootloader:** it ignores update files whose name is 32+ characters (FatFs `_MAX_LFN`). `package_firmware.py` enforces ≤ 31.
- **Upstream:** offer the fixes and the routing work to Randomwaves/Drumboy-Pro as a pull request from the fork.
