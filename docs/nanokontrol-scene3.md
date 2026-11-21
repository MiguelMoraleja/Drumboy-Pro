# nanoKONTROL Studio on the DUB mixer (V1.20)

The Drumboy reads MIDI **control changes on channel 3** and uses them on the DUB desk. It works on every page, with the DUB mixer ON or OFF, as long as **MIDI IN** is on (any mode). To switch it off, switch MIDI IN off or take the controller off the line; there is no other setting.

## Connection

The Studio talks USB or Bluetooth and the Drumboy has only a MIDI DIN input, so it needs a bridge:

- **Loopy Pro on the iPad** (the plan): Studio to the iPad over USB, Loopy Pro to the Scarlett's MIDI out, the Scarlett's MIDI out to the Drumboy's MIDI IN. Loopy can filter and rename the messages if needed.
- **A USB host to DIN box**: the Studio plugged into a small converter that goes to the DIN. It works without the iPad.

## Scene 3 in the Korg Kontrol Editor

MIDI channel **3** for the whole scene. Every control sends a CC number (a button sends 127 when pressed and 0 when released: set it as a momentary button, not a toggle):

| Studio control | CC |
|---|---|
| Fader 1 to 8 | 20 to 27 |
| Knob 1 to 8 | 30 to 37 |
| M button 1 to 8 (Mute) | 40 to 47 |
| TRACK ◀ | 58 |
| TRACK ▶ | 59 |

Everything else on the Studio (S, R, SELECT, the jog wheel, the X-Y pad, the transport) can stay as it is; the Drumboy ignores it. (The transport can follow MIDI start and stop, on the Drumboy's MIDI page.)

## What the controls do

The Studio has eight strips and the desk has twelve columns: the ten layers, the ECHO return and the VERB return. TRACK ◀ and ▶ slide the eight strips along the twelve columns, one at a time (layers 1 to 8, then 2 to 9, up to layers 5 to 10 with ECHO and VERB). On the DUB desk a cyan bar above the plates marks the eight columns the controller is on (it appears with the first message).

| Control | A layer column | ECHO column | VERB column |
|---|---|---|---|
| Fader | LEVEL | echo WET | reverb WET |
| Knob | ECHO send | echo FEEDBACK | reverb DECAY |
| M button | mute (a press toggles) | nothing | nothing |

(The ECHO column needs Effect 1 set to DELAY, which switching the DUB mixer on does for you.)

## Take-over

A fader does not match the screen after the strips slid, after a song was loaded, or when the Drumboy starts. So a fader takes over only when it **reaches the value on the screen** (or crosses it): move it towards the value and it takes hold; the level never jumps. If you then change the value another way (the encoder, a loaded song, undo), the fader lets go again until it reaches the new value. The same goes for the knobs.

## Later steps

Echo and reverb throws on the S and R buttons, the dub filter and the echo feedback past 100 % on the X-Y pad, and the echo time on the jog wheel need the dub mixer's next step (D2). MIDI learn (move a control, pick a function) and a settable channel can follow.
