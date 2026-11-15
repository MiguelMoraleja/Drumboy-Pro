#!/usr/bin/env python3
"""Checks sample folders and drumkit files against docs/drumkit-specs.md.

  check_drumkit.py SAMPLE_ROOT                 check every instrument folder under SAMPLE_ROOT
  check_drumkit.py SAMPLE_ROOT --kit K.rwd ... also check that the kit's samples exist there
  options: --max-seconds S   per-sample limit in seconds of playback (default: the firmware's, 5.44 s mono, 2.72 s stereo)
           --strict          a sample over the limit is an error, not a warning

Exit status 1 when an error is found (warnings do not count). Standard library only.
"""
import argparse
import os
import struct
import sys

NAME_LIMIT = 32             # kFileNameSize: a name must be shorter than this, ".wav" included
MONO_FRAMES = 240000        # kSampleSize (the firmware's limit up to V1.16)
STEREO_FRAMES = 120000      # half of it for a stereo sample (V1.16 and later)
RATES = {8000, 11025, 16000, 22050, 24000, 44100, 48000, 88200, 96000, 176400, 192000}
KIT_HEADER = b"RW_DRUMBOYPRO_DRUMKIT    "
LAYER_REF_BASE = 25 + 500   # file offset of layer 0's instrument flag
LAYERS = 10


def read_wav(path):
    """(format, channels, rate, bits, frames) or raises ValueError."""
    with open(path, "rb") as f:
        head = f.read(12)
        if head[:4] != b"RIFF" or head[8:12] != b"WAVE":
            raise ValueError("not a RIFF/WAVE file")
        fmt = None
        frames = None
        while True:
            chunk = f.read(8)
            if len(chunk) < 8:
                break
            cid, size = struct.unpack("<4sI", chunk)
            if cid == b"fmt ":
                data = f.read(size)
                fmt = struct.unpack("<HHIIHH", data[:16])
                if size % 2:
                    f.read(1)
            elif cid == b"data":
                if fmt is None:
                    raise ValueError("data chunk before fmt chunk")
                real = os.path.getsize(path) - f.tell()
                size = min(size, real)
                frames = size // (fmt[4] if fmt[4] else 1)
                break
            else:
                f.seek(size + (size % 2), 1)
        if fmt is None or frames is None:
            raise ValueError("missing fmt or data chunk")
        return fmt[0], fmt[1], fmt[2], fmt[5], frames


def check_samples(root, max_seconds, strict):
    errors = warnings = files = 0
    for inst in sorted(os.listdir(root)):
        folder = os.path.join(root, inst)
        if not os.path.isdir(folder) or inst.startswith("."):
            continue
        if len(inst) >= NAME_LIMIT:
            print(f"ERROR   {inst}: instrument folder name is {len(inst)} characters, must be under {NAME_LIMIT}")
            errors += 1
        for name in sorted(os.listdir(folder)):
            path = os.path.join(folder, name)
            if name.startswith(".") or os.path.isdir(path):
                if os.path.isdir(path):
                    print(f"WARN    {inst}/{name}: subfolders are not indexed")
                    warnings += 1
                continue
            if not name.lower().endswith(".wav"):
                print(f"WARN    {inst}/{name}: not a .wav file, ignored by the firmware")
                warnings += 1
                continue
            files += 1
            if len(name) >= NAME_LIMIT:
                print(f"ERROR   {inst}/{name}: file name is {len(name)} characters with .wav, must be under {NAME_LIMIT} (stem at most {NAME_LIMIT - 5})")
                errors += 1
            try:
                audio, ch, rate, bits, frames = read_wav(path)
            except ValueError as e:
                print(f"ERROR   {inst}/{name}: {e}")
                errors += 1
                continue
            if audio not in (1, 3) or ch not in (1, 2) or bits not in (8, 16, 24, 32) or rate not in RATES:
                print(f"ERROR   {inst}/{name}: unsupported format (code {audio}, {ch} ch, {rate} Hz, {bits} bit)")
                errors += 1
                continue
            # The engine plays frames at 44.1 kHz whatever the file's rate, so the length that counts is frames / 44100.
            limit = (max_seconds * 44100.0) if max_seconds else (STEREO_FRAMES if ch == 2 else MONO_FRAMES)
            if frames > limit:
                text = f"{frames / 44100.0:.2f} s of playback is over the limit ({limit / 44100.0:.2f} s)"
                if strict:
                    print(f"ERROR   {inst}/{name}: {text}")
                    errors += 1
                else:
                    print(f"WARN    {inst}/{name}: {text}; the firmware cuts it there")
                    warnings += 1
            if rate != 44100:
                print(f"WARN    {inst}/{name}: {rate} Hz plays {'about 1.5 semitones low' if rate == 48000 else 'with the firmware resampling'}; convert to 44100 Hz")
                warnings += 1
    print(f"{files} wav files checked: {errors} errors, {warnings} warnings")
    return errors


def kit_references(path):
    d = open(path, "rb").read()
    if d[:len(KIT_HEADER)] != KIT_HEADER:
        raise ValueError("not a Drumboy Pro drumkit (.rwd)")
    refs = []
    for i in range(LAYERS):
        o = LAYER_REF_BASE + i * 100
        inst = d[o + 1:o + 33].split(b"\0")[0].decode("latin1") if d[o:o + 1] == b"x" else None
        samp = d[o + 51:o + 83].split(b"\0")[0].decode("latin1") if d[o + 50:o + 51] == b"x" else None
        refs.append((i + 1, inst, samp))
    return refs


def check_kit(root, path):
    errors = 0
    try:
        refs = kit_references(path)
    except ValueError as e:
        print(f"ERROR   {path}: {e}")
        return 1
    for layer, inst, samp in refs:
        if inst is None:
            continue
        if not os.path.isdir(os.path.join(root, inst)):
            print(f"ERROR   {os.path.basename(path)} layer {layer}: instrument folder {inst!r} not found")
            errors += 1
        elif samp is not None and not os.path.isfile(os.path.join(root, inst, samp)):
            print(f"ERROR   {os.path.basename(path)} layer {layer}: sample {inst}/{samp} not found (names are exact and case-sensitive)")
            errors += 1
    print(f"{os.path.basename(path)}: {sum(1 for r in refs if r[1])} layers with an instrument, {errors} errors")
    return errors


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sample_root")
    ap.add_argument("--kit", nargs="*", default=[])
    ap.add_argument("--max-seconds", type=float, default=None)
    ap.add_argument("--strict", action="store_true", help="a sample over the limit is an error (for packs; the planned loader ignores such samples)")
    a = ap.parse_args()
    errors = check_samples(a.sample_root, a.max_seconds, a.strict)
    for k in a.kit:
        errors += check_kit(a.sample_root, k)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
