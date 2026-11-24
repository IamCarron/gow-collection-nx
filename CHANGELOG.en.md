# Changelog

[Português](CHANGELOG.md) · **English**

## v1.4-nx

The biggest update since launch: a **native deko3d renderer**, a **redesigned launcher and mod menu**, new image
options, a trophy system, a built-in updater and **English and Portuguese** support.

### Performance
- **deko3d renderer** (the Switch GPU's native API) as the main path, with less CPU overhead than Vulkan. Vulkan is
  still available under *Options → Image → Renderer*.
- **GPU at 460.8 MHz in handheld mode**: the game uses the official handheld performance profile (the same one
  retail games use; not an overclock) and keeps it for the whole session. Docked, the system stays at 768 MHz.
- **Frame generation (LSFG)** in *God of War I* with deko3d: creates in-between frames for 60 fps on screen with the
  game at 30. The adjustable optical flow (full, 1/2, 1/3, 1/4) scales the cost with resolution, falls back
  automatically when a flow size doesn't fit the chosen resolution, and pauses automatically when the GPU can't keep up.
- At 720p handheld, FSR skips the upscale pass (it is already close to 1x) and only sharpens, saving GPU time.

### Image
- **AMD FSR 1.0** (EASU + RCAS) upscaler, with sharpness from 0 to 10.
- **FXAA**, **anisotropic filtering** (up to 16x), **brightness**, **saturation**, **vibrance** and **debanding**.
- **Separate TV resolution**: one resolution for handheld and another for docked, switched automatically.
- **Quick presets** (Performance, Balanced, Quality) in the launcher and the mod menu.

### Redesigned launcher
- New look: cards with each game's cover, a panel with save, trophies and installed extras (HD textures, HD videos),
  background music and the last game played.
- **Tabbed options** (Image, Color, Performance, Game), with a description and default for every item; everything
  applies to both games.
- **Trophies**: the full list for each game, with progress, points, filters (all, earned, missing) and spoiler-safe
  hidden trophies.
- **Built-in updater**: the launcher checks GitHub for new versions and installs the launcher, *God of War* and
  *God of War II* in one go, keeping saves, options, videos and textures.

### Redesigned mod menu (R + Plus)
- New tabbed panel (**Graphics, Mods, Cheats, System**) with toggles, level bars and a description for every option.
- All new image and performance options change while the game runs, no restart needed.
- **Cheats**: infinite health, magic and rage. **Mods**: HD textures, enemy health bars and auto QTE.
- GPU diagnostics for performance logs, with a warning before it turns on.
- **Exit game** under *System*, with a confirmation.

### Languages
- **English and Portuguese (Brazil)** in the launcher and the mod menu. Pick it under *Options → Game → Language* or
  *System → Language* in R + Plus; it applies to both.

### Fixes
- Diagonal analog movement no longer slows Kratos down.
- HD videos open correctly on Switch.
- HD textures use a fixed decode queue and a smaller cache (less memory, fewer stutters).
- Threads are reused and memory is checked before scanning (#8).
- Black screen with FSR 1.0 fixed.
- Frame generation not starting at some resolutions fixed; VSync wait time no longer counts as GPU load.
- Unexpected exits (abort, unhandled C++ error) now close the game cleanly and log the reason, instead of hitting the
  Atmosphère error screen (2168-0001).
- The updater installs the `gow-nx.zip` package (launcher + both games) and no longer keeps offering the same version.

## v1.3-nx

Fixes and a new launcher. Thanks to **[mvhoang92](https://github.com/mvhoang92)** for the console testing and the
diagnosis of each issue (#5, #6, #7).

- **Left/top edge line fixed** (#6): at scales above 1x (Vita 960x544, 720p, 2x...) the viewport half-pixel offset
  left the first column and row of every render target undrawn, showing stale memory. Presentation now starts reading
  after them (computed every frame from the scale).
- **FPS counter without the menu mod** (#7): without the pause-menu `.vtpatch`, the R + Plus *FPS counter* uses
  vitart's counter; with the mod, the HUD counter is still used.
- **God of War II shaders** (#5): [BUILDING.md](docs/BUILDING.md) explains how to complete `shaders.pak` by playing
  (`shaders/miss` → `usse_build_cache.py` → `pack_shaders.py`). A complete pack for both games has ~9,700 programs.
- **Launcher**: new **Graphics options** menu (internal resolution, frame rate, VSync, aspect ratio, FPS counter, HD
  textures, enemy health bars), saved to `data/vitart_options.ini` for both games; the version shown now comes from
  the build (it always said "v1.0").

## v1.2-nx

Focus: **more FPS at 720p on stock clocks** (CPU 1020 MHz, GPU 307 MHz, no overclock).

- **GoW1 at 720p, stock clocks: ~45–60 FPS** with *max FPS* (was ~22 FPS in heavy scenes).
- **Maxwell hardware compression and ZCULL enabled in NVK**: render targets use dedicated allocations and depth is
  cleared by the render pass (no `TRANSFER_DST`), which lets the GPU reject hidden pixels before shading.
- **Direct target reads**: a draw that samples another target (color or depth) uses its image directly, with no copy.
- **Render-target copies by compute shader** (`fsr/snap_*.comp`) instead of `vkCmdCopyImage`: the depth copy went from
  ~2.1 ms to ~0.3 ms per frame and the scene copy from ~1.2 ms to ~0.65 ms. `VITART_VK_SNAPCS=0` restores the old path.
- **4x anisotropic filtering** on Switch (was 16x): lighter on memory bandwidth (`VITART_VK_ANISO` changes it).
- **CPU**: per-thread binding cache in `host_import`, per-draw SPIR-V/program cache in the GXM HLE, one lock per draw
  instead of several, and `sceGxmSetUniformDataF` without divisions.
- **Full screen in 16:9**: the 1271x720 image now fills the whole 1280x720 screen (no 4 px black bars).
- **Clean exit through HOME** (no hang on close) and lighter logging (fsync at most once per second).
- **Diagnostics**: GPU time per segment (copies, clears, direct reads) in the log, and the `gravacao vk` (Vulkan
  recording) thread included in `perfil_<game>.txt`.

Known: *God of War II* still has occasional slowdowns in heavy scenes (GPU bound); it is the next target.

## v1.1-nx

- **In-game mod menu** (**R + Plus**): internal resolution, frame rate, VSync, FPS counter, HD textures and enemy
  health bars, drawn with the system font; choices are saved to `data/vitart_options.ini`.
- **New resolutions**: *Vita (960x544)* (the Vita's native resolution, ~43% fewer pixels than 720p) and
  *720p (1270x720)*, in addition to 1x to 4x.
- **HD videos**: remastered cutscenes as `.mp4` (HEVC/H.264) decoded on the Tegra's NVDEC through FFmpeg and converted
  from NV12 to RGBA in a compute shader; the game's Bink still handles sound and skipping. Without the `videos`
  folder nothing changes.
- **Stock clocks**: the port no longer overclocks (CPU 1020 MHz, GPU 307 MHz handheld).
- **GPU diagnostics** in the log: time per render target, time outside passes, GPU idle between submits and present
  wait, to find what is heavy in each scene.
- One extra swapchain image of headroom.

## v1.0-nx

First public release.

- **Launcher** `gow-nx.nro`: menu to pick *God of War* or *God of War II*, with 1280x720 cover art, background music
  (MP3 with fade), system fonts and the last choice remembered; boots the game through the Homebrew Menu.
- **God of War** and **God of War II** recompiled to ARM64 and running on Switch with Vulkan (NVK).
- **Stable 30 FPS** in *God of War II* with the CPU at 1020 MHz (was 23–25 FPS in heavy scenes):
  - threads pinned per core and Vulkan recording on a dedicated thread;
  - `present()` no longer holds the global lock during vsync;
  - constant `arc_mem` in the generated code and a direct cache in `arc_dispatch`;
  - GXM HLE and Vita kernel primitives without needless allocations or notifications;
  - shaders in a single package (`shaders.pak`).
- **Stutter-free ATRAC9 audio** (4 buffers of 10 ms, priority above Vulkan recording).
- **Pause-menu options**: internal resolution (1x–4x), frame rate (30/60/max), VSync, HD textures, PS2 audio, auto
  QTE, FPS counter and cheats; saved to `data/vitart_options.ini`.
- **1x** resolution without overclock; **2x** with the GPU at 614 MHz.
- Built-in profiler (`logs/perfil_<game>.txt`) and logs in `logs/`.
