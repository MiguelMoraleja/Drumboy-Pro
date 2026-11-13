# Dub mixer: design

Design for a mixer page in the fashion of a dub desk: a strip per layer with sends to the echo and the reverb, a filter per channel, and the big filter knob. Asked by the owner on 2026-11-13. Nothing here is built. A mockup at the device's real size is in the app's Screen ideas tab ("Dub mixer"). The cost and memory figures come from [`ux-and-signal-path-analysis.md`](ux-and-signal-path-analysis.md), section 3, and are estimates until measured (backlog 37).

## 1. What makes a mixer a dub mixer

A dub desk is played like an instrument. The design is built around the five gestures that make that possible:

| Gesture | On a desk | On the Drumboy |
|---|---|---|
| **Drop** | Mute a channel; what it already sent to the echo keeps ringing | MUTE + pad, as today. Mute stops new hits, so tails ring on |
| **Throw** | Open a channel's echo send for one hit, then close it | Hold EFFECT 1 + pad (echo) or REVERB + pad (reverb): the send is fully open while held |
| **Ride the feedback** | Turn the echo's feedback up until it builds, pull it back before it takes over | FEEDBACK is always under the same knob, and goes past 100 % |
| **Sweep the big knob** | A stepped high-pass over a group of channels | DUB FILTER: one knob, in steps, on the bus the strips feed |
| **Send a return on** | The echo return goes into the reverb | ECHO > VERB level |

## 2. Signal path in DUB mode

A song is either CHAIN (today's patch, set on the ROUTING page) or DUB. Old songs load as CHAIN and sound the same.

```
layer 1-10 ─► channel filter ─► level ─┬─ BUS: FILT ─► EQ ─► FILTER 1 = DUB FILTER ──────────────┐
line-in ────►                          ├─ BUS: DIR ──────────────────────────────────────────────┤
                                       ├─ ECHO send ─► FILTER 2 ─► EFFECT 1 (delay) ─┬─ ECHO RTN ─┤
                                       │                                             └─ > VERB ─┐ │
                                       ├─ VERB send ────────────────────────────────────────────┴─► REVERB ─ VERB RTN ─┤
                                       └─ FX send ─► EFFECT 2 ─────────────────────────────────────────────────────────┤
                                                                                              MASTER ─► limiter ─► out
```

- **It uses the modules that exist,** in the order the engine already runs them (EQ, Filter 1, Filter 2, Effect 1, Effect 2, Reverb), so the patch is loop-free and adds no latency. Their own pages keep working for the detailed settings.
- **Filter 1 is the dub filter.** Each strip chooses its bus: FILT (through the EQ and the dub filter) or DIR (straight to the master). A kick on DIR keeps its weight while the sweep thins everything else.
- **Filter 2 sits before the echo.** A high-pass there keeps the low end out of the repeats. It is off until set.
- **Effect 1 is the echo.** In DUB mode it is the delay, its DRY is off and its WET is the return level.
- **Effect 2 is a third send** (phaser, flanger, chorus or a distortion), on the second page.
- **Line-in gets a strip,** so audio from Loopy can be dubbed with the same sends and throws.
- **Sends are after the fader.** A throw opens the send fully regardless of the fader and of mute.

### What this path cannot do

- **Reverb into the echo.** The reverb runs last, so only echo into reverb is possible.
- **The dub filter over the returns.** Filter 1 runs before the echo and the reverb. On a real desk the big knob also sat on a group, not on the whole mix. A filter over everything needs the master stage of backlog 48.
- **Stereo.** The path is mono until the reverb, as today, so the first version has no pan per strip. Pan needs a stereo bus through the EQ and the dub filter (backlog 36).

## 3. The page

Reached with MIXER: MIXER → DUB → ROUTING → MIXER. While the page is open, the lane area shows the desk (the mechanism built for the Euclid rings); leaving the page brings the lanes back.

### 3.1 Screen

- **Eleven strips of 60 px:** layers 1–10 and LINE IN. From the top: number, state (MUTE, THRW), name, the channel filter (FL: off in the centre, low-pass to the left, high-pass to the right), echo send (EC), reverb send (RV), the fader with a meter beside it, and the bus (FILT or DIR).
- **ECHO return:** time, FEEDBACK (magenta past 100 %), TONE, the level into the reverb, the return fader and meter.
- **VERB return:** decay, size, the return fader and meter.
- **DUB FILTER and master:** the big knob as a dial with its steps and the current frequency, master meters L and R, limiter gain reduction.
- **Colours:** the selected strip is cyan, a muted strip is dimmed, a thrown send is yellow.
- **Drawing cost:** opening the page draws the area once. After that only the bar, fader or meter that changed is redrawn.

### 3.2 Knobs

The top row edits the selected strip. The bottom row is the desk and never changes, so the hands learn it.

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **Top: selected strip** | LEVEL | FILTER | ECHO | VERB |
| **Bottom: the desk** | FEEDBACK | ECHO RTN | VERB RTN | DUB FILTER |

- **FILTER** is one knob: centre is off, left closes a low-pass, right opens a high-pass.
- **FEEDBACK** runs 0–110 %. Past 100 % the echo grows; a soft clipper in the feedback path keeps it bounded, as tape does.
- **DUB FILTER** moves in steps: OFF, then a high-pass at 70, 100, 150, 250, 500 Hz, 1, 2, 3, 5 and 7.5 kHz (the steps usually quoted for King Tubby's filter). FILTER 1's own page still sets type, slope and resonance, so it can also be a low-pass.
- **ADD + turn** changes every strip, as on the other pages.

Second page (tap MIXER again from DUB, or a DUB 2 step in the cycle):

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **Top: selected strip** | BUS (FILT / DIR) | RESONANCE | DRIVE | FX SEND |
| **Bottom: the desk** | ECHO TIME | ECHO TONE | ECHO > VERB | FX RTN |

- **ECHO TIME** gains the dotted values dub uses most: 3/16 and 3/8, next to 1/8, 1/6, 1/4, 1/3, 1/2 and 1/1.

### 3.3 Keys and pads

| Keys | Action |
|---|---|
| Pad | Plays the layer and selects its strip |
| MUTE + pad | Drop: mute or unmute the strip (as today) |
| S (left) + pad | Solo (as today) |
| **EFFECT 1 + pad** | **Echo throw:** the strip's echo send is fully open while both are held. The pad does not play the sound |
| **REVERB + pad** | **Reverb throw,** the same way |
| EFFECT 1 or REVERB tapped alone | Opens that effect's page, as today |
| CLEAR | Resets the selected strip's sends and filter |

- **A throw on a muted strip** plays its hits into the send only: the sound is heard as echo without the dry hit.
- **Several pads at once** throw several strips.

## 4. What is new in the engine

| Part | Size | Notes |
|---|---|---|
| Send levels per source (bus, echo, reverb, FX), following the knob with a short ramp | M | Backlog 46. The router already sends one source to several nodes with a gain per sample |
| A level on one module-to-module route (ECHO > VERB) | S | Module routes are on/off today |
| DUB mode per song and its fixed patch | S | The router is table-driven |
| Throws: a momentary send that ignores fader and mute | S | The destination is latched when a note starts, which suits a throw |
| Channel filter per layer, one knob | M | Backlog 47. State in the unused DTCM RAM |
| Echo: feedback to 110 % with a soft clipper, dotted times | S | In the delay |
| Meters: a peak per strip, return and master from the audio engine | M | Backlog 41 |
| File block for the strip settings | S | After the song's `CHN` and `BTX` blocks; older firmware ignores it |

**Cost (estimates):** sends 1–2 % of the audio block, channel filters 1.6–3.2 %, the rest under 1 %. With the 34 % measured at V1.04 that lands around 40 %, under the 80 % gate. Measure first (backlog 37).

## 5. Phases

| Phase | Content | You can then |
|---|---|---|
| D1 | DUB mode, send levels, returns, the page with strips and faders (no meters), file block | Give each layer its own amount of echo and reverb, and ride FEEDBACK and the dub filter |
| D2 | Throws, feedback past 100 %, dotted echo times, ECHO > VERB | Play the desk: drops, throws, runaway echo |
| D3 | Channel filter and drive per strip, page 2 | Filter one layer without touching the others |
| D4 | Meters, line-in strip | See levels; dub external audio |
| D5 | Pan and a stereo bus; a filter over the whole mix (backlog 36, 48) | Place strips left and right |

Each phase is a release that can be tested on its own. D1 and D2 need no new DSP beyond the echo's clipper.

## 6. Open decisions for the owner

1. **"Sends to the FX and filters":** this design reads it as sends to the effects, a filter on every channel, and a bus into the dub filter. If filters should instead be send destinations with a level (a strip partly into Filter 1, partly dry), BUS becomes a level instead of a switch.
2. **Throw keys:** EFFECT 1 + pad and REVERB + pad, or another pair (for example FILL + pad for the echo, since FILL already means "more" on a layer).
3. **Does a throw pad also play the sound,** so a live hit can be thrown, or only open the send for the sequenced hits (this design)?
4. **Where the big knob acts:** on the FILT bus only (this design, possible now) or over the whole mix including the returns (needs the master stage of backlog 48).
5. **Page order:** MIXER → DUB → ROUTING, or DUB first because it is the page you perform on.
6. **Effect 2:** a third send (this design), or fixed on the echo return (a phaser or overdrive on the repeats), or on the FILT bus.
