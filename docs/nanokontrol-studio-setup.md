# nanoKONTROL Studio → Drumboy: setup (dub mixer phase D0)

From V1.15 the Drumboy reads MIDI CC on its MIDI IN channel with a fixed map (`MidiCcMap.h`) laid out like a Korg nanoKONTROL Studio. Any controller that sends the same CC numbers works too (BeatStep Pro, Launch Control XL 3, ...).

## 1. On the Drumboy

I/O page → MIDI IN. The four new modes add CC to the existing ones:

| Mode | Receives |
|---|---|
| `CC` | controllers only |
| `NOTE+CC` | notes and controllers |
| `SYNC+CC` | clock, start/stop and controllers |
| `N+SYNC+CC` | everything |

ADD + turn sets the channel, as before. Notes and controllers use the same channel. Older firmware doesn't know modes 4–7 and falls back to OFF.

## 2. On the nanoKONTROL Studio (KORG KONTROL Editor, once)

Use one scene (for example scene 2) for the Drumboy, so scene 1 can stay on the Loopy desk.

- **Scene MIDI channel:** the Drumboy's MIDI IN channel (for example 16).
- **Assign type:** Control Change for every control.

| Control | CC, strips 1–8 | Button behaviour |
|---|---|---|
| Fader | 20–27 | |
| Knob | 30–37 | |
| Mute | 40–47 | Toggle = latched mute; Momentary = mute while held (a drop) |
| Solo | 50–57 | Toggle (or Momentary for a held solo) |
| Rec | 60–67 | Momentary (reserved: echo throw, phase D2) |
| Select | 70–77 | Momentary (reserved: reverb throw, phase D2) |

| Control | CC | Button behaviour |
|---|---|---|
| Play | 80 | Momentary |
| Stop | 81 | Momentary |
| Rec (transport) | 82 | Momentary |
| Rew / FF | 83 / 84 | (unused) |
| Cycle | 85 | Momentary (held = jog rides FEEDBACK) |
| Track ◀ / ▶ | 86 / 87 | Momentary (bank A / B) |
| Marker Set / ◀ / ▶ | 88 / 89 / 90 | (unused) |
| Jog wheel | 91 | Relative: 1 per click clockwise, 127 per click counter-clockwise (two's complement) |

Write the scene to the controller. From then on no editor is needed.

## 3. What the controls do

**Bank A** (Track ◀): strips 1–8 = layers 1–8.

| | Fader | Knob | Mute | Solo |
|---|---|---|---|---|
| Strip n | LEVEL | END (shortens the hit) | MUTE | SOLO |

**Bank B** (Track ▶):

| Strip | Fader | Knob |
|---|---|---|
| 1–2 | layers 9–10 LEVEL (Mute, Solo as in A) | END |
| 4 | EFFECT 2 WET | EFFECT 2 parameter A |
| 5 | EFFECT 1 C = delay FEEDBACK | EFFECT 1 A = delay TIME |
| 6 | EFFECT 1 F = delay WET (the echo return) | EFFECT 1 D = delay DAMPING |
| 7 | REVERB WET | REVERB DECAY |
| 8 | FILTER 1 FREQUENCY (the dub filter) | FILTER 1 RESONANCE |

The effect parameters follow whatever type the effect slot has; the names above are for the delay. Put the delay in Effect 1 and turn Filter 1 on (high-pass) to play it as a dub desk.

**Both banks:** jog = FILTER 1 FREQUENCY (two steps per click), Cycle held + jog = delay FEEDBACK, Play / Stop / Rec = the Drumboy's keys (when the Drumboy follows a clock they arm, as the keys do).

**Pickup:** after a song load, a bank change or an edit on the Drumboy itself, a fader or knob takes over only once it reaches the current value. Nothing jumps.

## 4. Connections

- **Studio (Loopy Pro on the iPad):** nanoKONTROL Studio → iPad (USB or Bluetooth). In Loopy, add a MIDI channel to the mixer with the nanoKONTROL as source and the Scarlett's MIDI OUT as destination. The Scarlett's MIDI OUT already carries Loopy's clock to the Drumboy (MIDI IN `SYNC+CC` or `N+SYNC+CC`). Loopy's own mappings stay on the other scene's channel.
- **Without the iPad:** a CME WIDI Jack on the Drumboy's MIDI IN (TRS type switch on the Jack, USB power because a MIDI IN supplies none), the nanoKONTROL Studio on batteries paired over Bluetooth. MIDI IN mode `CC` or `NOTE+CC`; the Drumboy is the clock master.
- **BeatStep Pro:** in MIDI Control Center, set its encoders to absolute CC 20–27 and 30–37 on the Drumboy's channel (Control Mode), pads as gate CC for Mute 40–47 and Solo 50–57. MIDI OUT → TRS-DIN adapter → Drumboy MIDI IN.
