# nanoKONTROL Studio on the DUB mixer (V1.20)

The Drumboy reads MIDI **control changes** (CC) and uses them on the DUB desk. It uses the Studio's **own numbers, as they were read from the unit**, so the Korg Kontrol Editor is not needed. The MIDI channel does not matter: a control change on any channel counts. It works on every page, with the DUB mixer ON or OFF, as long as **MIDI IN** is on (any mode). To switch it off, switch MIDI IN off or take the controller off the line.

## Connection

The Studio talks USB or Bluetooth and the Drumboy has only a MIDI DIN input, so it needs a bridge:

- **Loopy Pro on the iPad** (the plan): Studio to the iPad over USB, Loopy Pro to the Scarlett's MIDI out, the Scarlett's MIDI out to the Drumboy's MIDI IN. In Loopy, route the Studio's MIDI to the Scarlett's MIDI output and make sure Loopy itself does not act on those controls.
- **A USB host to DIN box**: the Studio plugged into a small converter that goes to the DIN. It works without the iPad.

## The controls (the Studio's own CC numbers)

| Studio control | CC |
|---|---|
| Fader 1 to 8 | 2, 3, 4, 5, 6, 8, 9, 12 |
| Knob 1 to 8 | 13 to 20 |
| M button 1 to 8 (Mute) | 21 to 28 |
| TRACK ◀ | 60 |
| TRACK ▶ | 61 |

A button counts as pressed when it sends a value of 64 or more (it sends 127 when pressed). Everything else on the Studio (S, R, SELECT, the jog wheel, the X-Y pad, the transport) is ignored by the Drumboy. Other controllers that send these numbers (CC 2 to 6, 8, 9, 12 to 21 to 28, 60, 61) on any channel would act too.

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

Echo and reverb throws on the S and R buttons, the dub filter and the echo feedback past 100 % on the X-Y pad, and the echo time on the jog wheel need the dub mixer's next step (D2). MIDI learn (move a control, pick a function) can follow.
