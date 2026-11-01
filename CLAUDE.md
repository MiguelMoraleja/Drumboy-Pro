# Notes for AI assistants working on this repo

Fork: MiguelMoraleja/Drumboy-Pro. Firmware: `02 Software/Drumboy-Pro-App-H723`. Planned work: `BACKLOG.md`.

## Before pushing firmware changes

- `make -C "02 Software/Drumboy-Pro-App-H723" WERROR=1` (our code builds with zero warnings)
- `make -C "02 Software/tests"` (host tests: fills, routing, beat list, Euclid)
- Put hardware-free logic in `Core/Library/Global/*.h` with a host test.
- Test builds for the owner's device: `make package`; the file must reach the SD card at its exact size (the bootloader shows SIZE-CHECK otherwise).

## Every release

1. Bump `Version.h`, merge to `main`; the owner creates the release `vX.YY` (lowercase v) on github.com.
2. Check the Release workflow attached `Drumboy_Pro_Fw_XX_YY.bin` and that it matches the hardware-tested file.
3. Update the app `docs/app/drumboy-pro-inside.html`: new row at the top of `RELEASES`, version in the header, and every tab the release changes (new features get explained there). Republish it to https://claude.ai/artifact/VvFjAK4W1L5SNTW9hCqYEj (pass that URL so the link stays the same).
4. Update `BACKLOG.md` status and the README feature list.
