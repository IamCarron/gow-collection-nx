# Changelog

[Português](CHANGELOG.md) · **English**

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
