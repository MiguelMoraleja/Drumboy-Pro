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
| 11 | [Euclidean rhythm page (EUCLID)](#11-euclidean-rhythm-page-euclid) | Feature | M |
| 12 | [Free beat placement on Layer Song](#12-free-beat-placement-on-layer-song) | Feature | M |
| 13 | [Sample preview while browsing](#13-sample-preview-while-browsing) | Feature | L |

Sizes are rough: S = a day, M = a few days, L = a week or more, including hardware testing.

## Plan

**Sprint 1, V1.10 "Sequencer"** (built, waiting for the hardware test), one hardware test session after step 5:
1. ✅ Beat-list editing in a hardware-free header with host tests (`BeatList.h`, `test_beatlist`): rebuild, move, rotate, insert. Items 11 and 12 both use it.
2. ✅ [12](#12-free-beat-placement-on-layer-song) Free beat placement.
3. ✅ [11](#11-euclidean-rhythm-page-euclid) EUCLID page.
4. ✅ [1](#1-lfo-modulated-values-dont-move-on-the-display) LFO values on the display.
5. ✅ [8](#8-limiter-setting-does-nothing) LIMITER.
6. ✅ Reproducible release builds (from [10](#10-housekeeping)), before tagging V1.10.

**Sprint 2, "Formats and sound":** [9](#9-songkit-file-format-version) file format version first, then [2](#2-instrument-pitch-should-step-in-semitones-and-octaves) pitch in semitones and [13](#13-sample-preview-while-browsing) sample preview (largest risk: SD streaming while playing, larger system preset).

**Sprint 3, "Routing phase 3":** decide [7](#7-routing-open-design-questions), then [4](#4-routing-multiple-destinations-per-source) and [5](#5-routing-send-levels).

**Later:** [3](#3-lfos-cant-modulate-filters-or-effects) (needs 9), [6](#6-routing-extra-effect-slots) (needs CPU measurement), the rest of [10](#10-housekeeping) alongside other work.

---

## LFO and instrument

### 1. LFO-modulated values don't move on the display

**Status: done in V1.10.** While playing, a field with an active LFO (and the layer's STYLE on) shows what the last hit used, in yellow; when stopped it shows the set value again. `layer_playBeat()` stores the values in `LayerPlayData::shownValue`, and `lcd_drawLayerInstLfoValues()` redraws at most 20 times a second. The LFO page playhead is not done.

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

**Status: done in V1.10.** LIMITER is now the output ceiling: the soft-knee clip in `processAudioSend()` is scaled to it. At +0 dB (the default) the output is unchanged.

The mixer's LIMITER (0–10) is saved in the system preset (byte 6), shown and editable, but no audio code reads `mixer.limiter`. The output only has the fixed soft-knee clip (`softKneeClip`, threshold 0.667) in `processAudioSend()`. Either make LIMITER set the clip threshold or a real limiter, or remove the field.

### 9. Song/kit file format version

Kit and song files have a signature but no format version. Several items above change what is stored (pitch scale, multi-destination routing, LFO module targets, fill types). Add a version byte in a spare kit-section byte (and the song section) so new firmware can convert old files and fail cleanly on newer ones. The routing marker (`0xA5` at kit byte 498) is the pattern to follow.

### 10. Housekeeping

- **Fill types:** 15 of 16 possible. The type is stored in 4 bits; a 17th type needs item 9.
- **`ENC_ROUTE_LINEIN_OUT`:** still has encoder handlers but is no longer on the ROUTING page (line-in is set with the mixer's MIXER IN). Remove it or put it back on the page.
- **Releases:** the Release workflow only runs for tags starting with lowercase `v` (`v1.10`). The `V1.04` release has no build attached for that reason.
- ~~**Reproducible release builds**~~ (done in V1.10): the v1.09 release file differed from the local build by 4 bytes of `.data`. Same toolchain packages; the cause was link order, because the Makefile listed sources with `find`, whose order depends on the file system. The lists are now sorted. Rebuilding the v1.09 commit locally gives exactly the file tested on hardware.
- **Vendor warnings:** two GCC warnings remain in FatFs (`ff.c`), both known false positives. Our code is held at zero (`make WERROR=1` in CI).
- **Bootloader:** it ignores update files whose name is 32+ characters (FatFs `_MAX_LFN`). `package_firmware.py` enforces ≤ 31.
- **Upstream:** offer the fixes and the routing work to Randomwaves/Drumboy-Pro as a pull request from the fork.

---

## Sequencer and samples

### 11. Euclidean rhythm page (EUCLID)

**Status: done in V1.10** (`Euclid.h`, `test_euclid`, `euclid_*` in `Controller.cpp`). Tap LAYER SONG on its own on the Layer Song page. Fields STEPS, HITS, ROTATE, GRID, MODE; CLEAR restores the layer.

**Today.** The only beat generator is a long ADD press on the Layer Song page, and only when the layer's bank is empty (`layerBeatKeyFlag` is set only when `lastActiveBeatNum == -1`). Each step of the hold runs a fixed preset from `layerSong_generateBeat()`: one beat per bar, two off-beat layouts, then BASIC_4 and BASIC_8 fills on every beat. Nothing is adjustable, and an existing pattern can't be extended. (The EUCLID *fill type* in `Fill.h` is different: it places hits inside one beat, not beats across the bar.)

**Proposal.** A EUCLID page for the selected layer and bank, opened the same way as ROUTING (press the Layer Song key again):
- **STEPS** 1–32, **HITS** 0–STEPS, **ROTATE** 0–STEPS−1, **GRID** (a quantize value from `kQuantizeInterval`, e.g. 1/16 = 200 ticks, since `kMeasureInterval` = 800 per quarter note). The pattern repeats to the end of the song (`songInterval`).
- Changes apply live while the sequencer plays: rebuild the bank's beats and call `layerSong_calculatePlayBeatNum()`, as `layerSong_generateBeat()` does now.
- **MERGE** adds the Euclidean hits to the beats already there. A hit that lands on an existing beat keeps that beat and its fill.
- **CLEAR** restores the bank as it was when the page was opened. Keep a snapshot in a `LayerSongCopy`-style buffer; don't use the copy/paste clipboard.
- Put the distribution (Bjorklund / Bresenham) in a hardware-free header (e.g. `Euclid.h`) with host tests in `02 Software/tests`: hit counts, rotation, known patterns such as E(3,8) = `x..x..x.` and E(5,8).

**Limits to respect.** A bank holds 64 beats (`kBeatLibrarySize`) and 32 fills (`kBeatFillLibrarySize`). 32 steps over several bars can exceed 64, so stop at the limit and show it. Every rebuild must keep beats sorted with correct end intervals (`layerSong_arrangeActiveBeats()`).

**Done when.** E(3,8) and E(5,16) sound right on the device while playing; MERGE keeps the beats and fills already there; CLEAR restores exactly what was there; songs save and load unchanged (the result is ordinary beats, so no file format change).

### 12. Free beat placement on Layer Song

**Status: done in V1.10** (`BeatList.h`, `test_beatlist`). MOVE on tab 6 (or ADD + LEFT/RIGHT), SHIFT ALL on tab 7, short ADD press inserts. Also fixed: recording into a full bank wrote past the beat list, and the old sort skipped the last beat.

**Today.** SHIFT + LEFT/RIGHT (`layerSong_shiftSelectedBeat()`) moves the selected beat one quantize step, but it stops at the neighbouring beat and resets the fill of the moved beat and of the one before it (`setBeatFill(…, 0, 0, 0, 0)`). There is no way to shift a whole layer, and the only way to add a single beat is live recording (`record()`).

**Proposal.**
- **MOVE:** the selected beat steps one quantize step and can pass its neighbours. Take it out, insert it at the new position, re-sort and fix the end intervals (`layerSong_arrangeActiveBeats()`), and keep the selection on the moved beat. Its fill is kept; recompute it for the new length with `layerSong_calculateBeatFill()` rather than clearing it. Show the exact position as bar.beat.16th (e.g. `2.3.4`) while moving.
- **SHIFT ALL:** rotate every beat of the layer's bank by one quantize step, wrapping at `songInterval`. Reversible: rotating back returns the identical pattern, fills included.
- **Insert:** a short ADD press adds one beat in the middle of the gap after the selected beat, snapped to the quantize grid (`layerSong_setBeat()`). A long ADD press keeps its current meaning.
- Put the move, rotate and insert arithmetic on the beat list in a hardware-free header with host tests (wrap-around, passing neighbours, full bank, rotation followed by its inverse).

**Done when.** A beat can move anywhere in the bar while keeping its fill; SHIFT ALL left then right gives back the identical bank; inserting into a full bank (64 beats) is refused with a message; all of it works while playing without glitches.

### 13. Sample preview while browsing

**Today.** On the instrument page, turning SAMPLE only selects a file (`layerInst_setSampleSelected()` reads its WAV header). To hear it, you have to load it with ADD (`layerInst_setSampleLoaded()`), which replaces the layer's sample, copies the whole file from SD to SDRAM and calls `keyboard_disable()` until it's done. The sample is cut at 5.44 s (`kSampleSize` = 240,000 samples).

**Proposal.**
- When the SAMPLE encoder stops (e.g. 250 ms after the last step), play the selected sample on a separate preview voice. The layer key replays it. The layer's loaded sample doesn't change until ADD.
- Stream the preview from the SD card through a small ring buffer in SDRAM, filled from the main loop in chunks, so even long files start at once and nothing locks the keyboard. Stop the preview when another sample is selected, the menu is left or ADD loads it.
- The audio interrupt only reads the ring buffer. FatFs is not reentrant, so every SD read stays in the main loop. When the buffer runs dry, fade out instead of clicking.
- **PREVIEW** on/off and **PREVIEW LEVEL** in the Metronome menu (which already has its own voice and level). The system preset has no free bytes (all 21 are used), so it needs to grow, and older system files (21 bytes) must still load.
- Measure with `make PROFILE=1`: one more voice plus SD reads while the sequencer plays. UDR/OVR must stay 0.

**Done when.** Turning through samples plays each one after a short pause, including files longer than 5 s; the keyboard never locks while browsing; the loaded sample is untouched until ADD; the preview setting survives a restart; no I2S errors in the profile build.
