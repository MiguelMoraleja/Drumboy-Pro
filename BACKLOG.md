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
| 14 | [EUCLID: velocity and variance](#14-euclid-velocity-and-variance) | Feature | S–M |
| 15 | [Song mode: chain the banks](#15-song-mode-chain-the-banks) | Feature | M–L |
| 16 | [Kids firmware](#16-kids-firmware) | Feature, on hold | L |
| 17 | [Sample rate conversion (48 kHz and others)](#17-sample-rate-conversion-48-khz-and-others) | Bug | M |
| 18 | [Swing / shuffle (LXR)](#18-swing--shuffle-lxr) | Feature | S–M |
| 19 | [Choke groups (LXR)](#19-choke-groups-lxr) | Feature | S |
| 20 | [Parameter locks per beat (LXR)](#20-parameter-locks-per-beat-lxr) | Feature | L |
| 21 | [Probability per beat (LXR)](#21-probability-per-beat-lxr) | Feature | M |
| 22 | [Live roll (LXR)](#22-live-roll-lxr) | Feature | S–M |
| 23 | [Per-layer length (polymeter) (LXR)](#23-per-layer-length-polymeter-lxr) | Feature | L |
| 24 | [Velocity modulates a parameter (LXR)](#24-velocity-modulates-a-parameter-lxr) | Feature | M |
| 25 | [LFO retrigger and free-running (LXR)](#25-lfo-retrigger-and-free-running-lxr) | Feature | S |
| 26 | [Random step in the song chain (LXR)](#26-random-step-in-the-song-chain-lxr) | Feature | S |
| 27 | [Erase while recording (LXR)](#27-erase-while-recording-lxr) | Feature | S |
| 28 | [Undo for bank edits (LXR)](#28-undo-for-bank-edits-lxr) | Feature | S–M |
| 29 | [New effects: ring modulator, tilt EQ, master saturation (LXR)](#29-new-effects-ring-modulator-tilt-eq-master-saturation-lxr) | Feature | M each |
| 30 | [MIDI in/out: velocity, running status, note queue, CC](#30-midi-inout-velocity-running-status-note-queue-cc) | Bug + feature | S (CC: M) |
| 31 | [Kit morph (LXR)](#31-kit-morph-lxr) | Feature | M |
| 32 | [Autosave](#32-autosave) | Feature | A: M, B: M–L |
| 33 | [Per-note editing: velocity, probability and more](#33-per-note-editing-velocity-probability-and-more) | Feature | M–L |
| 34 | [MIDI clock slave: drift and 200 BPM runaway](#34-midi-clock-slave-drift-and-200-bpm-runaway) | Bug | M |

Sizes are rough: S = a day, M = a few days, L = a week or more, including hardware testing.

## Plan

**Sprint 1, V1.10 "Sequencer"** (done, tested on hardware):
1. ✅ Beat-list editing in a hardware-free header with host tests (`BeatList.h`, `test_beatlist`): rebuild, move, rotate, insert. Items 11 and 12 both use it.
2. ✅ [12](#12-free-beat-placement-on-layer-song) Free beat placement.
3. ✅ [11](#11-euclidean-rhythm-page-euclid) EUCLID page.
4. ✅ [1](#1-lfo-modulated-values-dont-move-on-the-display) LFO values on the display.
5. ✅ [8](#8-limiter-setting-does-nothing) LIMITER.
6. ✅ Reproducible release builds (from [10](#10-housekeeping)), before tagging V1.10.

**Sprint 2, V1.11 "Formats and sound"** (done, tested on hardware): ✅ [14](#14-euclid-velocity-and-variance) part A ACCENTS, ✅ [9](#9-songkit-file-format-version) file format version, ✅ [2](#2-instrument-pitch-should-step-in-semitones-and-octaves) pitch in semitones, ✅ [13](#13-sample-preview-while-browsing) sample preview.

**Sprint 3, "Routing phase 3":** decide [7](#7-routing-open-design-questions), then [4](#4-routing-multiple-destinations-per-source) and [5](#5-routing-send-levels).

**V1.12 "Song mode"** (done, tested on hardware, released as v1.12): ✅ [15](#15-song-mode-chain-the-banks) song mode.

**Owner's priorities (2026-11-07):** [34](#34-midi-clock-slave-drift-and-200-bpm-runaway) MIDI clock slave drift (with 30, same setup), [32](#32-autosave) autosave (work gets lost today) and [33](#33-per-note-editing-velocity-probability-and-more) per-note velocity and probability (it absorbs 14 part B and 21, and gives 30's incoming velocity somewhere to go).

**Suggested next sprint (ideas from the LXR firmware):** [30](#30-midi-inout-velocity-running-status-note-queue-cc) MIDI in/out fixes first (they matter for the owner's Loopy Pro → Scarlett setup), then [18](#18-swing--shuffle-lxr) swing, [19](#19-choke-groups-lxr) choke groups, [22](#22-live-roll-lxr) live roll, [25](#25-lfo-retrigger-and-free-running-lxr) LFO retrigger, [27](#27-erase-while-recording-lxr) erase while recording. Later, together with [14](#14-euclid-velocity-and-variance) part B (they share one per-beat data extension and one file format bump): [20](#20-parameter-locks-per-beat-lxr), [21](#21-probability-per-beat-lxr), [24](#24-velocity-modulates-a-parameter-lxr).

**On hold:** [16](#16-kids-firmware) kids firmware (shape decided, waiting for the owner's go).

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

**Status: done in V1.11** (`Pitch.h`, `test_pitch`). −24 to +24 semitones; a fast turn steps to the next octave line; SHIFT still picks the pitch LFO; LFO depth ±12 semitones. Layers from older files keep their exact old pitch (shown as before, e.g. `1.37`) until PITCH is turned, then snap to the nearest semitone. Not done: anti-aliasing above 2× (linear interpolation only, so pitching up more than an octave can sound harsh on bright samples).

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

**Status: done in V1.11** (`FileFormat.h`). Kit section byte 496 = version, 497 = check byte; covers kit and song files. Old files are version 0; a newer file is refused before anything changes. Version 1 = semitone pitch in layer record byte 24.

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

**Status: done in V1.10** (`Euclid.h`, `test_euclid`, `euclid_*` in `Controller.cpp`). On the song page, tap S (right side, L I S row) on its own, or hold S and press the same layer pad again. Fields STEPS, HITS, ROTATE, GRID, MODE; CLEAR restores the layer.

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

**Status: done in V1.11** (`Preview.h`, `test_preview`). PREVIEW and PREV LEVEL on the Metronome page (tabs 4–5); 256 KB ring in the SDRAM gap at 0xC0EA6000; FatFs `_FS_LOCK` raised to 3. The SDRAM gap was only checked statically (nothing in the code references it); watch for any glitch in the preview that could mean something else writes there.

**Today.** On the instrument page, turning SAMPLE only selects a file (`layerInst_setSampleSelected()` reads its WAV header). To hear it, you have to load it with ADD (`layerInst_setSampleLoaded()`), which replaces the layer's sample, copies the whole file from SD to SDRAM and calls `keyboard_disable()` until it's done. The sample is cut at 5.44 s (`kSampleSize` = 240,000 samples).

**Proposal.**
- When the SAMPLE encoder stops (e.g. 250 ms after the last step), play the selected sample on a separate preview voice. The layer key replays it. The layer's loaded sample doesn't change until ADD.
- Stream the preview from the SD card through a small ring buffer in SDRAM, filled from the main loop in chunks, so even long files start at once and nothing locks the keyboard. Stop the preview when another sample is selected, the menu is left or ADD loads it.
- The audio interrupt only reads the ring buffer. FatFs is not reentrant, so every SD read stays in the main loop. When the buffer runs dry, fade out instead of clicking.
- **PREVIEW** on/off and **PREVIEW LEVEL** in the Metronome menu (which already has its own voice and level). The system preset has no free bytes (all 21 are used), so it needs to grow, and older system files (21 bytes) must still load.
- Measure with `make PROFILE=1`: one more voice plus SD reads while the sequencer plays. UDR/OVR must stay 0.

**Done when.** Turning through samples plays each one after a short pause, including files longer than 5 s; the keyboard never locks while browsing; the loaded sample is untouched until ADD; the preview setting survives a restart; no I2S errors in the profile build.

### 14. EUCLID: velocity and variance

**Today.** Every hit the EUCLID page makes is a plain beat at full velocity (BASIC / FLAT), so the pattern sounds mechanical. Plain beats have only three velocities: FLAT (100), GHOST (50) and MUTED (0), set by the beat's fill PATTERN; the VELOCITY field (FLAT, STEP, DESCEND, ASCEND, RANDOM) only shapes the micro-hits of a complex fill, not single beats. Variance is possible today only indirectly: an LFO on the layer's LEVEL (RANDOM type for humanising) with STYLE on changes each hit, and since V1.10 the instrument page shows the value used.

**Status: part A done in V1.11** (ACCENTS on EUCLID tab 7, `euclidAccents()` in `Euclid.h`). Part B open; its per-beat velocity storage is now planned in [33](#33-per-note-editing-velocity-probability-and-more), and the VELOCITY field here becomes one way to fill it.

**Proposal**, in two parts:

- **A. ACCENTS field (S, no file change).** A second Euclidean pattern over the hits: ACCENTS 0–HITS spreads that many accents evenly over the pattern's hits (and follows ROTATE). Accented hits stay FLAT (100); the others become GHOST (50). Uses the beat states that already exist, so songs load on any firmware. Tab 7 on the EUCLID page is free for it. Example: E(5,8) with 2 accents gives `X.xX.xx.` (X loud, x ghost; the accents are E(2,5) over the five hits). The device shows ghost hits as `+` (`X.+X.++.`): its font has no lowercase.
- **B. VELOCITY field (M).** Apply the fill VELOCITY modes across the Euclidean hits: FLAT, STEP (100/75 alternating), DESCEND and ASCEND over the cycle, and RANDOM (40–100) for variance; optionally a SPREAD amount (± random per hit, drawn once and stored, so the pattern stays the same on every loop). This needs a velocity per plain beat. The beat's 4-bit level field is unused on plain beats and is already saved and loaded, but the loader resets any level above 4 (`setBeatFill` range check), and older firmware plays all plain beats at FLAT/GHOST anyway. Decide the encoding together with [9](#9-songkit-file-format-version) (file format version), then play plain beats at their stored velocity in the sequencer (`kNumberDataLibrary[...]` multiplier instead of the fixed 100/50/0).
- Put the accent and velocity distribution in `Euclid.h` with host tests (accent spacing, rotation, RANDOM stored per hit, values stay within 0–100).
- The app's Euclid tab gets the same fields, so the result can be heard before building.

**Done when.** On the device, E(5,8) with 2 accents plays two loud and three ghost hits on every loop; with VELOCITY RANDOM the levels vary per hit but repeat identically each loop and survive save/load; older kits and songs sound the same.

### 15. Song mode: chain the banks

**Status: done in V1.12** (`SongChain.h`, `test_songchain`). SONG page: tap the SONG key (the one held to pick banks); fields STEP, BANK, REPEATS, LENGTH, END (LOOP/STOP), CHAIN. Decided: a manual bank pick jumps the chain to the step using that bank (a bank not in the chain turns chain play off); STOP keeps the position, RESET goes to step 1. The chain is saved after the song file's EOF (marker `CHN`), so older firmware ignores it and no format version change was needed. Also in V1.12: whole-bank copy/paste on the SONG page (long COPY / PASTE, all 10 layers; clipboard in SDRAM after the preview ring).

**Today.** Each layer has 5 song banks (`kBankLibrarySize`), and the active bank (`activeSongBank`) is the same for all layers. Holding the SONG key and pressing a pair of layer pads picks a bank (pads 1–2 = bank 1 … 9–10 = bank 5). While playing, `layerSong_triggerBank()` arms the change (`bankShiftFlag`, `targetSongBank`) and the sequencer switches at the next loop end; when stopped it switches at once (`layerSong_selectBank()`). Nothing plays the banks in sequence: every change is by hand.

**Proposal.**
- **Chain:** a list of up to 16 steps, each a bank and a repeat count (1–16 loops), for example `1×4, 2×2, 1×4, 3×1`. Optionally an END step that stops, or the chain loops.
- **Playback:** with chain play on, count loops at the loop end (where `bankShiftFlag` is handled today) and, when a step's repeats are done, switch to the next step's bank exactly like a manual trigger. A manual bank press while the chain plays either jumps the chain to that bank's first step or pauses the chain (decide on the device).
- **Page:** a SONG page (e.g. tap the SONG key, the same tap pattern as I and S) with the chain as a list: STEP, BANK, REPEATS, plus INSERT/DELETE step and CHAIN ON/OFF. The top-right area shows the chain with the current step and the repeats left; the bank tabs keep showing the active bank.
- **Transport and sync:** RESET goes back to step 1; STOP keeps the position (or resets, decide). With external sync, follow the incoming clock; loop ends are already counted by the sequencer, so nothing new is needed for sync.
- **Files:** the chain is saved with the song (about 33 bytes: count + 16 × bank/repeats). Needs [9](#9-songkit-file-format-version) so older firmware ignores it and older songs load with chain play off.
- Put the chain stepping (count, advance, END, jump) in a hardware-free header with host tests.

**Done when.** A chain such as `1×4, 2×2, 3×1` plays back by itself with bank changes exactly on loop ends, survives save/load, RESET restarts it, manual bank presses behave as decided, and songs without a chain play as before.

### 16. Kids firmware

**Idea (open, "we will see").** A separate, much simpler firmware for children on the same hardware: few pages, big obvious actions, nothing that can break a kit or song, safe volume.

**Approach to decide first.**
- **A build variant of this firmware** (e.g. `make KIDS=1`), sharing the audio engine, sample player, sequencer, files and bootloader, with a different, reduced UI layer. Recommended: every engine fix reaches both, and the hardware-free headers (`Fill.h`, `BeatList.h`, `Euclid.h`, `Pitch.h`, `Preview.h`) are reused as they are.
- **Or a KIDS MODE inside the normal firmware** (a locked mode entered with a key combination). Simpler to ship (one file), but the full UI code stays reachable, and every new feature has to be hidden from it.
- **Or a firmware written from scratch.** Most freedom, most work; only worth it if the kids version shares little.
- The update file must still match the bootloader's `Drumboy_Pro_Fw?*.bin` pattern and stay within 31 characters (e.g. `Drumboy_Pro_Fw_01_11_kids.bin`, 29). Switching between the normal and the kids firmware is then just flashing the other file.

**First ideas for what it does** (to try with a child before building much):
- Pads play sounds straight away; a fixed set of kits (animals, instruments, voices) chosen with one knob, no instrument/sample browsing.
- A simple loop: press record, play the pads, it repeats; CLEAR erases the loop (with a confirm). Maybe the EUCLID idea as one "make a rhythm" knob.
- One knob each for tempo, volume, and one fun effect (e.g. pitch in octaves, echo).
- Big, colourful screen; no numbers or menus deeper than one level.
- **Safety:** a hard cap on master and headphone volume (`kMaxMixerHpVolume` is 20 steps today), no file deletion, no firmware or SD maintenance actions, settings that can't get stuck.

**Decided (owner, 2026-11-02).** On hold until the owner says go.
- Ages 9–11.
- All the ideas above are in.
- A separate firmware file (the build variant above; switching = flashing the other file).
- Saving is not needed for the proof of concept.

**Still open.** Which kits ship with it, and whether 9–11-year-olds want more than one effect knob (try the POC with a child first).

**Done when.** To be defined once the shape is chosen; a first milestone could be a kids build with pads, kit knob, volume cap and a one-button loop.

### 17. Sample rate conversion (48 kHz and others)

**What happens.** The device plays at 44.1 kHz, but samples at several common rates are not converted, only repeated or thinned out by whole frames, so they play about 8.8% slow (roughly 1.5 semitones low, and 9% longer):

| File rate | What the loader does | Plays as |
|---|---|---|
| 8, 16, 24 kHz | repeats each frame 6, 3, 2 times | 48 kHz content at 44.1 kHz: 8.8% slow |
| 48 kHz | as is | 8.8% slow |
| 96, 192 kHz | keeps every 2nd / 4th frame, no filter | 8.8% slow, and can alias |
| 11.025, 22.05 kHz | repeats 4, 2 times | correct (but stair-stepped, no smoothing) |
| 44.1, 88.2, 176.4 kHz | as is / every 2nd / every 4th | correct (the last two can alias) |

Same rule in the layer loader (`layerInst_setSampleLoaded()`) and the sample preview (`Preview.h`, which copies it on purpose so the preview sounds like the loaded sample).

**Proposal.**
- A hardware-free `Resample.h`: convert any accepted rate to 44.1 kHz with a fractional step (ratio = file rate / 44100) and interpolation (linear at least; a short windowed-sinc or polyphase filter for downsampling 88.2–192 kHz so it doesn't alias). Used by both the loader and the preview, streaming-friendly (keeps its phase between chunks).
- Check the cost: the loader converts once at load (fine); the preview converts in the main loop per chunk (measure, it must still keep up; see the V1.11 throughput fix).
- The 5.44 s limit (`kSampleSize`) then counts in 44.1 kHz samples, so a 48 kHz file holds 8.8% less of its length than today; document it.
- **Compatibility to decide:** kits made with 48 kHz samples were tuned by ear to the slow playback and will sound about 1.5 semitones higher after the fix. Options: accept it (it's the correct pitch), or for files saved before the change (format version 1 or older, `FileFormat.h`) add -1.5 semitones back. Semitone pitch can't express that exactly, so an old-file flag would be needed.
- Related, optional: stereo files use only the left channel today; mixing to mono would keep both sides.

**Done when.** A 48 kHz and a 44.1 kHz recording of the same sound play at the same pitch and length, loaded and previewed; 96/192 kHz files don't alias; host tests for the converter (pitch of a sine at each rate, no clicks at chunk boundaries); the preview still never underruns on 48 kHz 24-bit stereo.

---

## Ideas from the LXR firmware (items 18–29)

Reviewed from the owner's fork of the Erica Synths LXR V2 desktop firmware (MiguelMoraleja/lxr-desktop-app, 2026-11-05). **Licence:** that firmware is GPL-3.0 and Drumboy-Pro is MIT, so its code must not be copied here (it would put the Drumboy firmware under the GPL), and "LXR" / "Erica Synths" are trademarks. Take the ideas only and write our own code. Not taken: the LXR's synthesised drum voices (the Drumboy is sample-based), its second stereo output (no such hardware), a kit per pattern (the Drumboy keeps one kit per song).

### 18. Swing / shuffle (LXR)

**LXR:** one shuffle amount per pattern that delays the off-beat steps. **Today:** only the SWING fill type, inside one beat; nothing swings the whole groove. **Proposal:** a SWING amount per song (or per bank) on the RHYTHM page that delays beats on off-beat 16ths (or 8ths) by a fraction of a step at playback time, without moving the stored beats, so it can be changed live and saved. Applies to fills' micro-hits too. Host tests for the timing.

### 19. Choke groups (LXR)

**LXR:** open and closed hi-hats cut each other off. **Today:** every layer rings over the others; cheap to add per layer. **Proposal:** a CHOKE group per layer (OFF, 1–4) on an instrument page; a hit fades out (a few ms, no click) any other layer in the same group still sounding. Saved per kit (needs a spare byte; see item 9).

### 20. Parameter locks per beat (LXR)

**LXR:** each step can set up to 2 parameters (e.g. pitch, decay), recorded by turning a knob while recording. The biggest creative gain of the list. On the Drumboy it is per beat, built on the beat list (`BeatList.h`, item 12), and needs a format version bump (item 9 makes that possible). **Today:** parameters only per layer or through an LFO. **Proposal:** up to 2 locks per beat (parameter + value) applied at the hit like the LFO values in `layer_playBeat()`; set by turning an instrument knob while a beat is selected (or while recording). Needs per-beat storage beyond `Beat` (16-bit data is full): extend the song section with item 9; plan together with 21, 24 and 14B.

### 21. Probability per beat (LXR)

**LXR:** a 0–100% probability per step. **Today:** one PROB per layer. **Proposal:** a probability per beat (and per fill micro-hit?) on the song page, rolled at each hit. Same per-beat storage as 20. Now part of [33](#33-per-note-editing-velocity-probability-and-more).

### 22. Live roll (LXR)

**LXR:** hold ROLL and a pad: the pad repeats at a set rate (1/16, 1/32, …) in time with the sequencer, with the step probability applied. **Today:** rolls only as fill types. A performance feature that sits next to fills; apply the layer's PROB to each repeat. **Proposal:** a roll modifier (key combination to choose) + pad repeats the layer at a rate (1/8, 1/16, 1/32, triplets) locked to the tempo while held; while recording, the repeats are written as beats (or as one beat with a ROLL fill).

### 23. Per-layer length / polymeter (LXR)

**LXR:** each track has its own length (e.g. 5 steps against 16). **Today:** every layer loops over the song length. **Proposal:** a LENGTH per layer and bank (in 16ths, up to the song length); the layer's beats wrap at its own length. Larger change for the free-placement beat model (playback pointers per layer, drawing the lane); host-test the wrap.

### 24. Velocity modulates a parameter (LXR)

**LXR:** `modulationNode` sends each hit's velocity to any parameter. **Today:** velocity only changes volume. Pairs with the MIDI velocity fix in 30 (incoming velocity becomes a real value) and is a cheaper first step toward 3 (LFO → filters/FX): the same "modulate a module parameter per hit" plumbing. **Proposal:** per layer, a VELO → destination (pitch, start, filter, ...) and amount, applied at the hit. Needs per-beat velocity (14B).

### 25. LFO retrigger and free-running (LXR)

**LXR:** an LFO can restart on each note of a chosen voice, can run synced or free, and has exponential up/down shapes. **Today:** LFOs are tempo-synced, never restart, and have SIN, TRI, SAW, SQR, RND. **Proposal:** per LFO a RETRIG source (OFF or a layer): a hit on that layer resets the LFO phase; optionally a FREE mode in Hz; add EXP UP and EXP DOWN shapes (`kLfoDataLibrary`, Lfo.h). Small; Lfo.h + `layer_playBeat()`.

### 26. Random step in the song chain (LXR)

**LXR:** each pattern says which pattern comes next, including "random among the previous ones", and switches every N bars. **Today:** song mode is a fixed chain (item 15) whose REPEATS already cover "every N bars". Adding RANDOM as a step type costs almost nothing. **Proposal:** a step's BANK can be RANDOM (any bank, or not the same as the last); `SongChain.h` with a seeded random for tests; file block version 2.

### 27. Erase while recording (LXR)

**LXR:** while recording, hold a key and the hits that pass the play position are erased. **Today:** only CLEAR (one beat or the whole layer). **Proposal:** while recording, hold CLEAR + a layer pad: beats of that layer under the play position are removed as it passes.

### 28. Undo for bank edits (LXR)

**LXR:** a backup buffer to return a pattern to how it was. **Today:** only EUCLID CLEAR. **Proposal:** one level of undo per bank for destructive edits (paste, EUCLID apply, CLEAR all, SHIFT ALL): snapshot before, a key combination to swap back. The SDRAM gap has room (one bank of 10 layers is ~28 KB, like the bank clipboard).

### 29. New effects: ring modulator, tilt EQ, master saturation (LXR)

**LXR:** ring modulator and compressor on the FX bus, a tilt EQ, and a tube-style saturation on the master. **Today:** 9 effect types in the two effect slots (no ring mod), a 6-band EQ, a soft-knee clip at the LIMITER ceiling. **Proposal:** ring modulator as a 10th effect type; a TILT control (one knob, dark ↔ bright) on the mixer; an optional warm saturation before the output clip. Measure DSP load for each (`make PROFILE=1`).

### 30. MIDI in/out: velocity, running status, note queue, CC

Matters for playing the Drumboy from Loopy Pro (through a Scarlett interface). Checked in the code (`Controller.cpp`: the MIDI byte parser and the inbound processing in the main loop):

- **Velocity ignored:** the parser stores the note-on velocity, but the main loop plays every received note at full level (`layer_playBeat(layerNum, 1.0f)`) and records it as a FLAT beat.
- **Note On with velocity 0** (sent by many controllers instead of Note Off) plays a hit at full level.
- **No running status:** each note must start with its own status byte (`rxNoteOnReadStage` only leaves stage 0 on a status byte), so when a controller omits repeated status bytes, the second and later notes of a run are dropped.
- **One pending note:** a single slot (`rxNoteOnKey` / `rxNoteOnWriteFlag`); a second note arriving before the main loop handles the first overwrites it, so two notes landing together lose one.
- **MIDI out velocity** is always 100 (0x64) in Note On and Note Off.
- **No CC input** at all (the LXR maps its parameters to CC and NRPN).

**Proposal.**
- A hardware-free MIDI parser (e.g. `MidiParser.h`) with running status (data bytes reuse the last channel status; real-time bytes 0xF8–0xFF don't cancel it, other system messages do), note-on velocity 0 = note off, and a small queue (e.g. 16 events) between the UART interrupt and the main loop. Host tests with byte streams: running status runs, interleaved clock bytes, chords, velocity 0.
- Play received notes at their velocity (cubed like the other levels, or as is; decide by ear), and record them with it once per-beat velocity exists (14B); until then, map low velocities to GHOST.
- MIDI out sends the velocity the hit actually played with (fill level × FLAT/GHOST).
- CC input (M): a fixed CC map for the main per-layer parameters (level, pitch, start, end, …) and the mixer, on the receive channel; learn mode later. NRPN not needed at first.

**Done when.** From Loopy Pro: soft and hard notes play at different levels, a fast run with running status plays every note, two simultaneous notes both play, velocity-0 notes are silent, and the MIDI monitor shows the real velocity on MIDI out.

### 31. Kit morph (LXR)

**LXR:** one knob crossfades every sound parameter of the kit toward a second kit; the interpolation is spread over audio ticks, one parameter per tick, so it never spikes the CPU; routing is excluded. **Today:** nothing; a kit is loaded whole. **Proposal:** load a second kit as the morph target (parameters only, not samples: a layer whose sample differs keeps its own), a MORPH knob 0–100% that interpolates level, pitch, start/end, filter, EQ, effect and reverb parameters, updated a few parameters per audio block; routing (OUT and the ROUTING page) and samples stay as they are, which matches the routing design. Host-test the interpolation (endpoints exact, no parameter left half-updated across a full sweep).

### 32. Autosave

**Today.** Nothing is saved unless SAVE is used on the FILE page (`file_saveAction()` → `sd_saveFile()`), so a power cut, a crash or loading another song without saving loses the work. A song file is small (kit 1,500 bytes + 10 layers × 1,286 bytes + the 38-byte chain block, about 14 KB) and is written in one go with the keys disabled (`keyboard_disable()`), so a save takes tens of milliseconds. The system and encoder presets already use a "dirty flag, save later" pattern (`systemPresetSaveFlag`).

**Proposal**, in two parts:

- **A. Simple autosave (M).**
  - **When something changed:** instead of hooking every edit, keep a checksum of the song state (kit block, the song banks, routing, chain) and compare it every minute; the STM32H723 has a hardware CRC unit, and 14 KB takes well under a millisecond. Changed since the last save (manual or auto) = dirty.
  - **When to write:** an AUTOSAVE setting on the Metronome/system page: OFF, 2, 5, 10 (default), 20 minutes, stored in the system preset (byte 23, growing it to 24 bytes the way the preview bytes did). Write only when dirty and safe: not recording, no precount, no load or save in progress; while playing, at a loop end, so the short SD write never lands in the middle of the user's gesture. Decide on the device whether writing during playback is inaudible (the audio runs in the interrupt, so it should be); if not, autosave only while stopped plus once on STOP.
  - **Where:** never over the user's numbered slots. A reserved file outside the 1–99 library (e.g. `Song/Autosave/AUTO_A.rwf` and `AUTO_B.rwf`), written alternately, each first to a `.tmp` name and then renamed, so a power cut during the write always leaves the previous good copy. Each file also records the slot it came from and a running sequence number (the board has no battery-backed clock, so no wall-clock time).
  - **Restore:** at power-on, if the newest autosave is newer than the last manual save, show `RESTORE AUTOSAVE?` (YES loads it like a song file, NO keeps the normal start). It also appears as an AUTOSAVE entry on the FILE page, so it can be loaded and then saved into any slot.
  - **Safety net for LOAD:** loading another song while the current one is dirty writes an autosave first.
  - Hardware-free logic (`Autosave.h`): the timer and dirty decision, which of A/B to write, which is newest, the restore decision, with host tests (power cut mid-write simulated by an unrenamed `.tmp`, sequence wraparound, setting OFF).
- **B. History and branching (M–L), later.** Keep the last N autosaves per song slot (e.g. 8, as a ring in `Song/Autosave/NN/`), browsable on the FILE page with their age in minutes and the slot they belong to. Loading one and saving it into a new slot is the "branch"; the autosave history then follows the new slot. Pairs with [28](#28-undo-for-bank-edits-lxr) (undo inside a session); autosave covers the time between sessions.

**Done when.** With AUTOSAVE 10, editing and then pulling the power loses at most 10 minutes of work; the next power-on offers the restore; pulling the power during an autosave write still leaves a loadable autosave; manual slots are never touched by autosave.

### 33. Per-note editing: velocity, probability and more

**Today.** A beat (a note on Layer Song) has a start, a fill (type, pattern, time, level) and an active flag, stored in 4 bytes per beat in the song file (257 bytes per bank per layer). Plain beats only have FLAT (100), GHOST (50) and MUTED (0). Probability exists only per layer (PROB). Incoming MIDI velocity is ignored ([30](#30-midi-inout-velocity-running-status-note-queue-cc)), and MIDI out always sends 100.

**Proposal.** One per-beat extension that several items need, done once:

- **Data:** two extra bytes per beat: VELOCITY (0–127, or 1–100 to match the other levels) and PROBABILITY (0–100%), plus room for the extras below. In memory next to `beatLibrary`; in the file as a block after the song's "EOF" with its own marker (e.g. `BTX`), the same trick as the song chain: older firmware stops at "EOF" and ignores it, and a file without the block loads with velocity from FLAT/GHOST/MUTED and probability 100%. About 6.4 KB per song for 10 layers × 5 banks × 64 beats × 2 bytes. Bank copy/paste, the bank clipboard, BeatList moves and rebuilds, EUCLID and record must carry the new fields along with the beat.
- **Editing on the device:** on Layer Song, with a beat selected (the MOVE cursor from 12), a NOTE tab with VEL and PROB fields for that beat; ADD + turning the encoder changes all beats of the bank at once (relative), for quick humanising. The pattern line can show low-probability beats differently (the font has no lowercase, like the `+` ghost hits).
- **Playback:** the sequencer plays a plain beat at its stored velocity, rolls its probability at each loop (combined with the layer's PROB), and MIDI out sends the velocity actually played.
- **Recording:** live and MIDI-in recording store the velocity that came in (needs 30's velocity fix); pads record at FLAT.
- **More, per beat, in the same block (pick after A works):** a play condition (every loop, only loop 1 of N, every 2nd, first time only, not the first time, Elektron-style "A:B" trig conditions); a semitone offset (a first, small step toward the parameter locks of [20](#20-parameter-locks-per-beat-lxr)); a micro-timing nudge finer than the quantize step; a ratchet count (repeat the hit 2–4 times inside the beat, like a small fill).
- **Ties to other items:** this is the storage that [14](#14-euclid-velocity-and-variance) part B (EUCLID VELOCITY / RANDOM spread) and [21](#21-probability-per-beat-lxr) (probability per beat) were waiting for, and the base of [20](#20-parameter-locks-per-beat-lxr) (parameter locks). EUCLID can then fill VEL and PROB with its own patterns.
- Hardware-free code (`BeatExt.h` or inside `BeatList.h`): the block's read/write with checks, carrying the fields through move/rebuild/copy, the probability roll and the condition logic, with host tests (old file without the block, round trip, a move keeps each beat's velocity, probability 0/100 exact, condition "1:4" plays exactly once in four loops).
- The app gets a tab to try velocity and probability on a pattern before building.

**Done when.** On the device a single beat can be set to velocity 30 and probability 50%, it plays softly about half the time, its settings survive move, copy/paste and save/load, MIDI recording keeps the played velocity, and the same song still loads (and plays at FLAT/100%) on V1.12.

### 34. MIDI clock slave: drift and 200 BPM runaway

**Reported (V1.12).** Setup: Loopy Pro → Scarlett → MIDI IN, sending clock, transport and notes. (1) The Drumboy ran away at 200 BPM; the owner traced it to a MIDI loop sending signals back into the system. (2) Even without the loop it slowly drifts out of sync, "as if the signal did not arrive continuously".

**What the code does (checked).**
- **The clock only sets the tempo; it never steers the playhead.** On 0xF8 the UART handler (`receiveMidiCommand()`) counts 48 ticks, measures them with `HAL_GetTick()` (1 ms) and computes an **integer** BPM (`rxClockBpm`). The main loop applies it with `rhythm_setTempo()`, which only changes the period of the sequencer timer TIM15. After START the sequencer runs free on its own timer; the incoming ticks never move the play position again (`rxClockTickPhase` is only used to start on a beat). So any difference between the two tempos accumulates until the next STOP/START.
- **Integer tempo.** `rhythm.tempo` is a `uint8_t`. Loopy Pro's tempo is often fractional (it comes from the length of the first loop, e.g. 97.43 BPM). Running at 97 instead of 97.43 is 0.44% slow: a 16th note behind after about 57 beats, well under a minute. When the real tempo sits near .5, the measurement flips between two values every 2 beats.
- **Timer rounding.** `playTimerPeriod = 60,000,000 / (tempo × 800)` in whole microseconds (TIM15 at 1 MHz): exact at 120 or 125 BPM, but e.g. 133 BPM gives 563 µs instead of 563.9 (0.16% fast). This adds to the drift even when the master's tempo is a whole number.
- **Lost clock bytes would make it worse.** MIDI IN (USART1) receives one byte per DMA transfer and re-arms in the completion callback, with the UART FIFO disabled. That interrupt has the same priority as the sequencer timer (preempt 2) and below audio and SD (preempt 0). If it is held off for longer than about two byte times (640 µs), the UART overruns and bytes are lost; `HAL_UART_ErrorCallback` re-arms it, so it recovers silently. Lost 0xF8 bytes read as a slower tempo; lost note bytes drop notes. Not confirmed on the device yet. Count the overruns to check: add a counter in the error callback and show it on the test page.
- **Clock out while slaved enables the loop.** MIDI OUT clock (`txClock`, TIM4 at the Drumboy's own tempo) is independent of MIDI IN clock (`rxClock`), so a slaved Drumboy keeps sending its own clock. If anything echoes MIDI OUT back to MIDI IN (MIDI thru in Loopy Pro or the interface routing), the input sees roughly twice the ticks, measures about twice the tempo, clamps at `kMaxTempo` = 200 and sends 200 BPM clock back: the runaway.

**Proposal.**
1. **Lock the playhead to the incoming clock (the real fix).** While slaved to MIDI clock, the sequencer may advance only as far as the clock allows: tick k since START corresponds to position k × 800 / 24 sequencer ticks (33⅓, so keep the fraction). The timer runs about 1% faster than the measured tempo and waits at the allowed position until the next 0xF8. It can never get ahead, and it is never more than one clock tick (1/24 beat) behind. A clock tick that arrives early lets it catch up at once. Song position after CONTINUE: keep the counter. Measure the tempo with microsecond timestamps (DWT cycle counter or a free-running timer) averaged over 24–48 ticks. Keep a fractional tempo internally for the timer, the delay effects and the LFOs; the screen can still show whole BPM.
2. **No echo loop.** While MIDI IN clock is on, MIDI OUT clock either forwards the incoming ticks (true thru) or is off. It never sends a second, independent clock. Ignore 0xF8 that arrive impossibly fast (more than 2× the measured rate) as a safety net.
3. **Robust MIDI IN.** Enable the USART1 RX FIFO, or move to a circular DMA ring read by the parser (this fits the parser and queue planned in 30), so no byte is lost while higher-priority interrupts run. Count overruns for diagnostics.
4. Hardware-free logic (`MidiClock.h`): tempo estimation from timestamps, the allowed-position rule, start/stop/continue, rejection of doubled ticks. Host tests: a fractional tempo (97.43 BPM) for 10 minutes stays within 1/24 beat; jittered ticks (±1 ms); one dropped tick; doubled ticks from an echo; tempo changes.

**Done when.** Slaved to Loopy Pro at a fractional tempo for 30 minutes, the Drumboy stays in time (by ear and on a recording, against a click from Loopy). With a MIDI loop deliberately set up, it neither runs away nor doubles its tempo. The overrun counter stays at 0 with clock and dense notes coming in together.

**Workarounds until then.** Turn the Drumboy's MIDI OUT clock off while it is slaved (or turn MIDI thru off in Loopy Pro) to avoid the loop. Set Loopy Pro to a whole-number tempo, preferably one that divides evenly into the timer (120, 125, 100, 150…); this reduces the drift but does not remove it. STOP/START from Loopy realigns it.
