# Drumkit and sample specifications

For anyone (a person, a script or another AI session) who prepares sample packs and drumkits for the Drumboy Pro. Facts come from the firmware source (`02 Software/Drumboy-Pro-App-H723`), as of **V1.16**. Checks are automated by [`02 Software/tools/check_drumkit.py`](../02%20Software/tools/check_drumkit.py). Features that are not in a released firmware are marked with their version.

## 1. Summary for a pack

| | Do this |
|---|---|
| Format | WAV, PCM or float, **44.1 kHz, 24-bit** (16-bit is fine), mono for most hits |
| Length | at most **20 s** of playback, mono or stereo (V1.17); V1.16 and older: 5.44 s mono, 2.72 s stereo, longer files are cut |
| Stereo | only for sounds that need width; V1.15 and older play the left channel only |
| File name | `.wav`, whole name (with `.wav`) **under 32 characters**; first 10 characters distinctive |
| Folder | one folder per instrument directly in `Sample/`, name under 32 characters, no subfolders |
| Level | peak below 0 dBFS; no need to normalise (the firmware can) |
| After building kits | never rename a file or folder: kits find samples by exact name |

## 2. Card layout

```
Sample/<Instrument>/<SampleName>.wav     the library; an instrument is a folder
System/Drumkit/Drumkit_001.rwd ... _100  100 kit slots
System/File/File_001.rwf ... _100        100 song slots
System/Data/                             index files, rebuilt at every start-up (do not copy)
System/Preset/System.rwp, Encoder.rwp    required settings files (a folder, with both files)
System/Image/, System/Firmware/          the firmware's own
```

`02 Software/Sd Card` in the repository is the factory card image (2,500 samples in 19 instrument folders).

## 3. Sample files

- **Container and format:** RIFF/WAVE with a `fmt ` chunk of 16, 18 or 40 bytes; audio format 1 (PCM) or 3 (IEEE float); **1 or 2 channels**; **8, 16, 24 or 32 bits**. Other chunks are skipped. The data chunk is capped by the real file size.
- **Sample rates accepted:** 8000, 11025, 16000, 22050, 24000, 44100, 48000, 88200, 96000, 176400, 192000 Hz. Others are refused.
- **What the engine does with the rate.** The engine runs at **44.1 kHz** and plays the frames it is given at that clock:
  - 44.1 kHz: true pitch. **Use this.**
  - 48 kHz: plays about 1.5 semitones low (the factory library is 48 kHz and tuned that way).
  - 88.2 and 96 kHz: every second frame is used, with no filter (aliasing); 176.4 and 192 kHz: every fourth.
  - 8, 11.025, 16, 22.05, 24 kHz: each frame is repeated 6, 4, 3, 2, 2 times (no interpolation).
  - There is no proper resampler yet. **Convert to 44.1 kHz before importing.**
- **Length limit.** **V1.17 and later:** at most **20 s of playback** (882,000 frames at 44.1 kHz, whatever the file's rate) for a mono or a stereo sample; a longer file is **ignored** (marked unsupported, it does not load). All samples share one memory pool of 14.4 MB: a 20 s mono sample takes 3.5 MB (7 MB stereo), so four fit at once, or about 80 s of mono in total (40 s stereo). If a sample does not fit, the load fails with SAMPLE MEMORY FULL and the layer stays empty. **V1.16 and older:** fixed slots of 240,000 frames (5.44 s mono, 2.72 s stereo); a longer file is cut at the limit and still loads.
- **Stereo:** both channels are kept from V1.16 (32-bit float, left plane and right plane in one slot). Its PAN is a balance (centre untouched). Older firmware keeps only the left channel.
- **Storage:** every sample is converted to 32-bit float in the SDRAM; 8-, 16-, 24- and 32-bit sources keep their full precision. The loader notes the peak of the sample so the per-layer NORMALIZE option can scale it.

## 4. Names

- **Instrument folder:** a directory directly in `Sample/`, not hidden, name shorter than 32 characters. At most 500 instruments. Subfolders inside an instrument are ignored.
- **Sample file:** a file in the instrument folder, not hidden, extension `.wav` (any case), **total name length including `.wav` under 32 characters**, so the stem is at most 27 characters. At most 5,000 samples per instrument (the memory index, in practice far fewer).
- **Order:** samples and instruments are sorted alphabetically. Kits refer by name, so order does not matter to them.
- **Identity:** a kit stores the instrument folder name and the sample file name (with `.wav`) as **exact, case-sensitive strings**. A renamed file or folder, or a copy that changed a character, makes the layer empty and shows MISSING SAMPLE (V1.16 shows the layer, instrument and sample).
- **Screen:** names are shown shortened to about 10 characters, so keep the first 10 distinct.
- **Index:** `System/Data` is deleted and rebuilt at every start-up; add samples, then restart.

## 5. Drumkit files (`.rwd`)

A kit is one file per slot: `System/Drumkit/Drumkit_001.rwd` to `Drumkit_100.rwd`. It stores everything a drumkit is: the ten layers' settings, the mixer, EQ, filters, effects, reverb, LFOs and the routing, plus **references** (names) to the samples. It never contains audio.

Layout (little-endian bytes; offsets from the start of the file; the 25-byte header is `RW_DRUMBOYPRO_DRUMKIT    `; "payload" offsets below are counted after the header, so file offset = payload offset + 25):

| Payload offset | Size | Content |
|---|---|---|
| 0 | 1 | `*` slot is in use (a blank slot has `-`) |
| 1-12 | 12 | mixer: master, headphone, line-out, line-in volume, line-in mixer input, pan, limiter, normalise, mutes |
| 13-24 | 12 | tempo, measure, bar, quantize; metronome; sync and MIDI settings |
| 25-38 | 14 | EQ |
| 39-54 | 16 | Filter 1 and Filter 2 (active, type, frequency, resonance, slope, drive, dry, wet) |
| 55-166 | 112 | Effect 1 and 2 (active, type) and their sub-effect parameters |
| 167-174 | 8 | reverb |
| 180-239 | 60 | ten LFOs, 6 bytes each |
| 240-489 | 250 | ten layer records of **25 bytes** each (see below) |
| 490-495 | 6 | routing: destination mask of each module |
| 496-497 | 2 | file format version (1) and its check byte (version XOR 0x5A) |
| 498 | 1 | routing marker 0xA5 |
| 500-1499 | 1000 | ten asset records of **100 bytes** each (see below) |

Layer record (payload offset 240 + 25 × layer): 0 level, 1 pitch (older firmware's byte), 2 direction, 3 resolution, 4 start, 5 end, 6 timing, 7 probability, 8-15 LFO assignments, 16-19 EQ, filter, effect and reverb sends, 20 mute, 21 fill, 22 style, 23 routing destination mask, 24 pitch in semitones.

Asset record (payload offset 500 + 100 × layer): byte 0 is `x` when the layer has an instrument (`o` when empty); bytes 1-32 the instrument folder name (zero padded); byte 50 is `x` when the layer has a sample; bytes 51-82 the sample file name **with `.wav`**. In file offsets: instrument flag at `525 + 100 × layer`, sample flag at `575 + 100 × layer`.

Not in the kit: **PAN, the DUB mixer settings and the echo width** are saved in songs (`File_nnn.rwf`), not in kits. Kits made by older firmware load in newer firmware; a kit with a format version newer than the firmware knows is refused.

The simplest way to make a kit: build it on the Drumboy (load samples into the ten layers, set levels, save to a kit slot) and copy the `.rwd`. Writing `.rwd` files by script is possible with the table above; run the checker on the result.

## 6. Checking a pack

```
python3 "02 Software/tools/check_drumkit.py" /path/to/Sample
python3 "02 Software/tools/check_drumkit.py" /path/to/Sample --kit Drumkit_001.rwd
python3 "02 Software/tools/check_drumkit.py" /path/to/Sample --strict                       # V1.17 rules: a sample over 20 s is an error
```

It reports errors (unsupported format, names too long, kit references that are not on the card) and warnings (not 44.1 kHz, sample over the limit and cut, subfolders). Exit status 1 when there is an error.

## 7. Copying to the card

- Copy **one instrument folder at a time** and compare the file counts afterwards (a large copy from an iPad can silently drop files, which looks like MISSING SAMPLE).
- Use **V1.16 or later** firmware: V1.15 could damage the card when saving (backlog 56).
- Keep a backup of the card before copying, and eject it properly.
