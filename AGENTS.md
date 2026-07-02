## Short project description
MinUI is a custom lightweight game launcher and user interface frontend written in C, adapted for the Minime handheld firmware.

## Project folder structure
- `assets/` — Graphical assets, system resources, and configuration files.
- `docs/` — Quality guidelines (`QUALITY.md`) and C style conventions (`STYLE.md`).
- `scripts/` — Ancillary shell scripts, including development hooks.
- `src/` — C source codebase:
  - `common/` — Common data structures, algorithms, and utilities.
  - `keymon/` — Key input monitoring daemon.
  - `libmsettings/` — Hardware control library for brightness, volume, and power.
  - `main/` — Primary UI launcher wrapper.
  - `minarch/` — Core RetroArch libretro frontend wrapper.
  - `platform/` — Hardware-specific display and controller drivers.
  - `settings/` — In-game menu overlay and system settings launcher.
  - `show/` — Helper utility to write/display images on the framebuffer.
  - `ui/` — Base UI widget components and rendering loops.
- `tests/` — Test suites and testing runner (`test_dummy.c`).
- `Makefile` — Build system compiling the binaries and test runners.

## Agent directives
- Emphasize correctness, compilation safety, and clean formatting for all modified or new code.
- Always check that your changes compile successfully by running `just verify`.
- Avoid modifying legacy MinUI code unless directly requested or required for Minime compatibility.
- Ensure any new C source file starts with a header explaining its purpose and is added to the format and lint checks.
