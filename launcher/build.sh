#!/bin/bash
# build/gow-nx.nro: launcher com capa, música e menu que abre gow1.nro / gow2.nro (mesma pasta) via envSetNextLoad.
# A capa (romfs/bg.bgra, 1280x720 BGRA) e a música (romfs/music.mp3) NÃO vêm no repositório: use as suas
# (tools/make_launcher_assets.ps1 gera o bg.bgra a partir de uma imagem). Sem elas o launcher abre com fundo preto e mudo.
# Precisa dos portlibs switch-freetype e switch-mpg123.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DKP=${DEVKITPRO:-/opt/devkitpro}; B=$DKP/devkitA64/bin; PL=$DKP/portlibs/switch; O=$ROOT/build/launcher
VERSION=${VERSION:-1.3}
mkdir -p "$O" "$ROOT/launcher/romfs"
ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
"$B/aarch64-none-elf-gcc" $ARCH -O2 -Wall -Wno-format-truncation -D__SWITCH__ -DVERSAO=\"$VERSION\" -I"$DKP/libnx/include" -I"$PL/include" -I"$PL/include/freetype2" \
    -c "$ROOT/launcher/main.c" -o "$O/main.o"
"$B/aarch64-none-elf-gcc" -specs="$DKP/libnx/switch.specs" $ARCH -o "$O/gow-nx.elf" "$O/main.o" -L"$PL/lib" -L"$DKP/libnx/lib" \
    -lfreetype -lharfbuzz -lfreetype -lbz2 -lpng16 -lz -lmpg123 -lnx -lm
"$DKP/tools/bin/nacptool" --create "God of War Collection" "Guto Recomp" "$VERSION" "$O/gow-nx.nacp"
ICON=""; [ -f "$ROOT/res/icon_launcher.jpg" ] && ICON="--icon=$ROOT/res/icon_launcher.jpg"
"$DKP/tools/bin/elf2nro" "$O/gow-nx.elf" "$ROOT/build/gow-nx.nro" --nacp="$O/gow-nx.nacp" $ICON --romfsdir="$ROOT/launcher/romfs"
ls -la "$ROOT/build/gow-nx.nro"
