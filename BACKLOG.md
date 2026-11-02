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
| 18 | [MIDI in: velocity, running status, note queue](#18-midi-in-velocity-running-status-note-queue) | Bug | S |
| 19 | [MIDI out velocity and CC control](#19-midi-out-velocity-and-cc-control) | Feature | M |
| 20 | [Choke groups](#20-choke-groups) | Feature | S |
| 21 | [Roll / note repeat](#21-roll--note-repeat) | Feature | S–M |
| 22 | [Velocity as a modulation source](#22-velocity-as-a-modulation-source) | Feature | M |
| 23 | [Kit morph](#23-kit-morph) | Feature | M |
| 24 | [Beat parameter locks](#24-beat-parameter-locks) | Feature | L |
| 25 | [LFO retrigger and shapes](#25-lfo-retrigger-and-shapes) | Feature | S |

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

**Sprint 3, "MIDI and performance":** [18](#18-midi-in-velocity-running-status-note-queue), [20](#20-choke-groups), [21](#21-roll--note-repeat). Small, and 18 matters for playing and recording from Loopy Pro or the BeatStep Pro.

**Sprint 4, "Routing phase 3":** decide [7](#7-routing-open-design-questions), then [4](#4-routing-multiple-destinations-per-source) and [5](#5-routing-send-levels).

**To schedule after sprint 2:** [15](#15-song-mode-chain-the-banks) song mode (needs 9, which sprint 2 delivers).

**Ideas from LXR (later):** [19](#19-midi-out-velocity-and-cc-control), [22](#22-velocity-as-a-modulation-source) (a cheap first step toward 3), [23](#23-kit-morph), [24](#24-beat-parameter-locks) (needs 9), [25](#25-lfo-retrigger-and-shapes). A RANDOM chain step for [15](#15-song-mode-chain-the-banks) also comes from LXR, see the end of this file.

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

**Status: part A done in V1.11** (ACCENTS on EUCLID tab 7, `euclidAccents()` in `Euclid.h`). Part B open.

**Proposal**, in two parts:

- **A. ACCENTS field (S, no file change).** A second Euclidean pattern over the hits: ACCENTS 0–HITS spreads that many accents evenly over the pattern's hits (and follows ROTATE). Accented hits stay FLAT (100); the others become GHOST (50). Uses the beat states that already exist, so songs load on any firmware. Tab 7 on the EUCLID page is free for it. Example: E(5,8) with 2 accents gives `X.xX.xx.` (X loud, x ghost; the accents are E(2,5) over the five hits). The device shows ghost hits as `+` (`X.+X.++.`): its font has no lowercase.
- **B. VELOCITY field (M).** Apply the fill VELOCITY modes across the Euclidean hits: FLAT, STEP (100/75 alternating), DESCEND and ASCEND over the cycle, and RANDOM (40–100) for variance; optionally a SPREAD amount (± random per hit, drawn once and stored, so the pattern stays the same on every loop). This needs a velocity per plain beat. The beat's 4-bit level field is unused on plain beats and is already saved and loaded, but the loader resets any level above 4 (`setBeatFill` range check), and older firmware plays all plain beats at FLAT/GHOST anyway. Decide the encoding together with [9](#9-songkit-file-format-version) (file format version), then play plain beats at their stored velocity in the sequencer (`kNumberDataLibrary[...]` multiplier instead of the fixed 100/50/0).
- Put the accent and velocity distribution in `Euclid.h` with host tests (accent spacing, rotation, RANDOM stored per hit, values stay within 0–100).
- The app's Euclid tab gets the same fields, so the result can be heard before building.

**Done when.** On the device, E(5,8) with 2 accents plays two loud and three ghost hits on every loop; with VELOCITY RANDOM the levels vary per hit but repeat identically each loop and survive save/load; older kits and songs sound the same.

### 15. Song mode: chain the banks

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

## Ideas from the LXR firmware

Items 18–25 come from reading Erica Synths' open-source LXR V2 firmware (fork: MiguelMoraleja/lxr-desktop-app, STM32F765). **Licence:** LXR is GPL-3.0, this repo is MIT. Don't copy LXR code here: that would make the fork GPL and it could no longer go upstream to Randomwaves (item 10). Take the idea, write our own code, and put the logic in a hardware-free header with host tests as usual. LXR file names are given only so the behaviour can be looked up.

Not worth bringing over: LXR's analog-style voices and wavetables (a synthesis engine, not a sample player), its trigger outputs (no matching hardware), and most of its master FX (we already have compressor, expander, overdrive, distortion, bitcrusher). A one-knob tilt EQ on the master is a small optional extra.

### 18. MIDI in: velocity, running status, note queue

**What happens.** Notes from a controller or DAW sometimes go missing or play at the wrong level:
- **Velocity is ignored.** `receiveMidiCommand()` stores `rxNoteOnVelocity`, but the main loop calls `layer_playBeat(layerNum, 1.0f)`: every hit is full level, and recorded beats are plain FLAT beats.
- **Note-on with velocity 0 plays a hit.** By the MIDI spec it means note-off, and many devices send it instead of 0x80. Each note then plays twice.
- **No running status.** The note parser only starts on a status byte (`rxNoteOnReadStage == 0` needs `0x90 + channel`). A sender that leaves out repeated status bytes loses every note after the first.
- **One pending note.** `rxNoteOnKey` / `rxNoteOnWriteFlag` is a single slot, cleared in the main loop. Two notes before the loop runs (a chord, or a kick and hat on the same step) lose one.

**Proposal.**
- A hardware-free `MidiIn.h` parser: running status (remember the last channel status byte; real-time bytes 0xF8–0xFF don't cancel it, SysEx and other system common bytes do), note-on velocity 0 → note-off, and a small ring buffer of events (e.g. 16) filled in the UART ISR and drained in the main loop.
- Velocity scales the hit: `layer_playBeat(layerNum, velocity / 127.0f)`, or through a curve (LINEAR / SOFT / FIXED) in the IO menu.
- Recording: a hit below a threshold (e.g. velocity < 64) records as GHOST, the rest FLAT, until item 14 B gives plain beats a real velocity.
- Host tests: running status across real-time bytes, note-on 0, a burst of 8 notes, channel filter.

**Done when.** A chord of three layers from Loopy Pro plays all three; a sender using running status and note-on 0 plays each note once; soft and hard hits are audibly different; recorded soft hits come back as ghosts.

### 19. MIDI out velocity and CC control

**Today.** MIDI out sends notes at a fixed velocity 100 (`0x64` in the TX block of the main loop). MIDI in has no control change (0xB0) parser: nothing on the device can be set from a controller. LXR maps every sound parameter to a CC or NRPN (`ParameterArray.c`, `MidiParser.c`) and sends them out when a knob moves.

**Proposal.**
- **Out:** send the velocity the hit actually played at (beat state × fill velocity × level, scaled to 1–127).
- **In:** a CC map for the parameters most worth playing live: per-layer level, pitch, start, LFO depth; filter frequency/resonance; effect wet; reverb wet; master. One fixed, documented map first (e.g. CC 16–27 = layer 1–12 level), editable later. Values arrive in the ISR; apply them from the main loop, smoothed like an encoder move.
- Optionally send the same CCs out when an encoder moves, so a DAW can record knob moves.
- Uses the parser and queue from 18.

**Done when.** Turning a BeatStep Pro knob mapped to a layer's level changes it on the device and on the screen; MIDI out velocities follow ghosts and fills.

### 20. Choke groups

**Today.** Every layer plays on its own. An open hat keeps ringing under the closed hat that follows it. LXR has this for its hats: the closed hat stops the open one (`sequencer.c`).

**Proposal.**
- A CHOKE field per layer: OFF or group A–D. A hit in a group fades out (a few ms, no click) every other layer playing in the same group.
- Applies to sequencer hits, pads and MIDI in.
- Files: one byte per layer; needs a format version step ([9](#9-songkit-file-format-version)). Old files load with CHOKE OFF.
- Put "which layers to cut" in a hardware-free function with a host test; the fade is in the voice.

**Done when.** Open hat in group A, closed hat in group A: the closed hat cuts the open one without a click; layers with CHOKE OFF are unchanged; the setting saves and loads.

### 21. Roll / note repeat

**Today.** A held pad plays once. LXR has a roll mode: while a pad is held, the voice retriggers at a set rate (1/16, 1/32, …) locked to the sequencer clock, and roll hits go through the step probability (`seq_setRoll`, `seq_rollRate`).

**Proposal.**
- Hold a modifier (or a ROLL key combination, to be decided on the device) and press pads: they retrigger at the ROLL RATE while held, on the sequencer grid (`kQuantizeInterval`), only while playing (or at the set tempo when stopped, decide).
- ROLL RATE: 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32. Optional ROLL FADE: each repeat a little quieter, or a ramp up.
- With RECORD on, rolls are recorded as beats (or as a fill, which already describes repeated hits inside a beat: try that first).
- Performance only; nothing new is saved unless recorded.

**Done when.** Holding a pad while playing gives tight 1/16 repeats in time with the sequencer and external clock; releasing stops at once; recorded rolls play back the same.

### 22. Velocity as a modulation source

**Today.** A hit's level only scales the output. In LXR, each voice's velocity can be routed to one other parameter with an amount (`modulationNode.c`): harder hits can open a filter, shorten a decay or raise a pitch.

**Proposal.**
- Per layer: VEL DEST (OFF, PITCH, START, END, FILTER of the layer's route, …) and VEL AMOUNT (±). Applied per hit in `layer_playBeat()`, like the LFO values today (sample and hold at trigger), so it costs nothing per sample.
- Velocity comes from the beat (FLAT/GHOST, fills, item 14 B) and from MIDI in (item 18).
- A cheap first step toward [3](#3-lfos-cant-modulate-filters-or-effects): same destination list and file layout ideas.
- Files: two bytes per layer, with a format version step.

**Done when.** VEL → PITCH +2 semitones: ghost hits and full hits play at audibly different pitches; with VEL DEST OFF everything plays as before.

### 23. Kit morph

**Today.** Changing kits is a hard switch. LXR has MORPH: one knob crossfades every sound parameter from the active kit to a second loaded kit, and spreads the work by updating one parameter per audio block so the CPU never spikes (`preset_morph_tick()` in `presetManager.c`). Audio routing is not morphed; it stays with the first kit.

**Proposal.**
- Load a second kit as the MORPH target (parameters only; samples stay those of the active kit, since morphing samples isn't possible).
- One encoder (e.g. on the mixer or a new page) goes 0–100 %: level, pitch (in semitones), start/end, filter and effect parameters interpolate; discrete fields (filter type, effect type, routing, sample) switch at 50 % or stay with the active kit (decide per field).
- Interpolate in the main loop, a few parameters per pass, and let the existing click-free paths apply them.
- Optional: drive MORPH from a CC (item 19).

**Done when.** Two kits differing in pitch and filter: sweeping MORPH moves smoothly between them with no clicks or DSP spikes (`make PROFILE=1`), and MORPH 0 % sounds exactly like the active kit.

### 24. Beat parameter locks

**Today.** A beat has a position, a state and a fill; all hits of a layer use the layer's parameters (plus LFO). LXR steps carry up to two parameter overrides each (`param1Nr/param1Val`, `param2Nr/param2Val` in its step struct, recorded with `seq_recordAutomation()`).

**Proposal.**
- Up to two locks per beat: a parameter (pitch, level, start, end, filter frequency, effect wet, …) and a value. Applied at trigger in `layer_playBeat()` instead of the layer value; the LFO then modulates around the locked value.
- Editing: select a beat on Layer Song (item 12 already does selection), hold the beat's key and turn the parameter's encoder; the beat shows a lock mark. Clearing: a long press, or CLEAR LOCKS for the layer.
- Memory and files: two (param, value) pairs per beat is 4 bytes × 64 beats × 5 banks × 12 layers ≈ 15 KB per song. Check against the song file layout; needs a format version step ([9](#9-songkit-file-format-version)).
- Put the lock lookup and merge (lock vs LFO vs base) in a hardware-free header with host tests.

**Done when.** A hat pattern with a pitch lock on beat 3 and a filter lock on beat 7 plays them on every loop, survives save/load and MOVE/SHIFT ALL, and older songs load with no locks.

### 25. LFO retrigger and shapes

**Today.** LFOs run free (or from `sampleStart`) with five shapes: SIN, TRI, SAW, SQR, RND. LXR also has saw down, exponential up/down, and per-voice retrigger: the LFO restarts at its phase on each note (`lfo_retrigger()`), which makes it act like a per-hit envelope.

**Proposal.**
- RETRIG field per LFO: FREE, BAR (restart at each bar, as now if that's what it does), NOTE (restart on each hit of the layer it modulates).
- Add shapes: SAW DOWN (if INVERT doesn't already cover it, check), EXP UP, EXP DOWN.
- Files: one byte per LFO, with a format version step.

**Done when.** An EXP DOWN LFO on PITCH with RETRIG NOTE gives every hit the same falling pitch; FREE behaves as today.

### Related: RANDOM step for song mode (item 15)

LXR lets each pattern name the next one, including "random among the previous patterns" and "random of all", and change every N bars. If [15](#15-song-mode-chain-the-banks) gets built, a RANDOM step type (pick a bank at random, optionally from a set) costs almost nothing and makes long jams less static.
