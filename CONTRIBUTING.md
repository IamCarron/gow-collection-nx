# Contributing

Thanks for wanting to help! Bug reports, testing on real hardware and pull requests are all welcome.

## Ground rules

- **Never commit game data.** No files from `PCSA00126`, no `work.bin`, no generated C from `vitarecomp`, no
  compiled NROs, shaders, icons, cover art or music from the game. The `.gitignore` blocks the common paths; double
  check `git status` before committing.
- **Only our own code lives here.** Changes to vitaRT go in `patches/` as diffs against the base commit `63d899f`,
  not as copied files.
- Keep the Switch-specific code in `platform/nx/`.

## Reporting a bug

Open an issue with:

- which game (*God of War* / *God of War II*) and where it happens;
- Atmosphère version, handheld or docked, CPU/GPU clocks and the internal resolution you use;
- `sdmc:/switch/gow-nx/logs/vitart_<game>.log` and, for performance problems, `perfil_<game>.txt`.

## Pull requests

1. Fork and create a branch.
2. Rebuild from scratch (`scripts/setup.sh` → `compile_mods.sh` → `build.sh`) to make sure the patches still apply.
3. If you change runtime code, regenerate the matching file in `patches/` with `git diff` against the base commit.
4. Describe what you tested on hardware. Measured FPS before/after is very helpful for performance changes.

Code is accepted under the [MIT license](LICENSE).
