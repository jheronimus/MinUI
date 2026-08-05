MinUI for Minime Firmware

This is a fork of [MinUI by Shaun Inman](https://github.com/shauninman/MinUI) for the Minime custom firmware. It's basically a port of MinUI, includes both the base and the extra PAKs.

----------------------------------------
Card Layout & Structure

  /
  ├── Bios/            # BIOS files for emulators
  ├── Roms/            # Game ROMs organized by console folder
  ├── Collections/     # Custom game lists (.txt)
  └── Saves/           # In-game saves and state files

----------------------------------------
Shortcuts

Dedicated MENU button devices (or key combinations):

  Brightness: MENU + VOLUME UP / DOWN
  Volume:     VOLUME UP / DOWN
  Sleep:      POWER (Press once)
  Wake:       POWER (Press once)
  Power Off:  Hold POWER or select Shutdown in menu

Quicksave & Auto-Resume:
MinUI automatically creates a quicksave when powering off in-game. On power-on, the device automatically resumes from where you left off.

----------------------------------------
Roms & Bios

Place ROMs in `/Roms/<System>` and BIOS files in `/Bios/<System>`.

Supported BIOS file names (case-sensitive):

   FC:   disksys.rom
   GB:   gb_bios.bin
   GBA:  gba_bios.bin
   GBC:  gbc_bios.bin
   MD:   bios_CD_E.bin, bios_CD_J.bin, bios_CD_U.bin
   MGBA: gba_bios.bin
   PCE:  syscard3.pce
   PKM:  bios.min
   PS:   psxonpsp660.bin
   SGB:  sgb.bios

----------------------------------------
Multi-disc & Cue Files

For multi-disc games, group `.bin`, `.cue`, and `.m3u` files inside a single folder with the same name as the `.cue` or `.m3u` file.

----------------------------------------
Credits & Acknowledgments

MinUI by Shaun Inman: https://github.com/shauninman/minui