# Third-party notices

This repository only contains original code (MIT, see [LICENSE](LICENSE)) and patches. The components below are
**not** bundled in the source tree: they are fetched or installed by the person building the port, and some are
statically linked into the resulting NROs. Each keeps its own license.

## Runtime and recompiler

| Component | Source | License | How it is used |
|---|---|---|---|
| vitaRT (runtime, `vitarecomp`) | https://github.com/luisxl15/vitaRT | author's terms | cloned by the builder; modified through `patches/` |
| GOD-OF-WAR-HD-COLLECTION menu mod | https://github.com/luisxl15/GOD-OF-WAR-HD-COLLECTION | author's terms | optional, installed by the user in `data/mods` |
| vita-headers (NID database) | https://github.com/vitasdk/vita-headers | MIT | used by the recompiler |

## Linked libraries

| Component | Source | License |
|---|---|---|
| libnx | https://github.com/switchbrew/libnx | ISC |
| Mesa (NVK Vulkan driver) | https://gitlab.freedesktop.org/mesa/mesa | MIT |
| volk | https://github.com/zeux/volk | MIT |
| LibAtrac9 | https://github.com/Thealexbarney/LibAtrac9 | MIT |
| stb | https://github.com/nothings/stb | MIT / Public Domain |
| mpg123 | https://www.mpg123.de/ | LGPL-2.1 |
| FreeType | https://freetype.org/ | FreeType License (FTL) |
| HarfBuzz | https://github.com/harfbuzz/harfbuzz | MIT ("Old MIT") |
| libpng | http://www.libpng.org/pub/png/libpng.html | libpng License |
| zlib | https://zlib.net/ | zlib License |
| bzip2 | https://sourceware.org/bzip2/ | bzip2 License (BSD-style) |
| Expat | https://libexpat.github.io/ | MIT |

## Toolchain

| Component | Source | License |
|---|---|---|
| devkitA64 / devkitPro tools | https://devkitpro.org/ | GPL (tools only; not linked) |

**mpg123 (LGPL-2.1)** is linked statically into the launcher (`gow-nx.nro`). Its source is available at the link
above, and the launcher can be relinked against a modified mpg123 using `launcher/build.sh` from this repository.

## Game content

*God of War*, *God of War II* and all related assets belong to Sony Interactive Entertainment. None of them are
distributed here.
