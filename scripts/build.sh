#!/bin/bash
# Monta build/<jogo>.nro: runtime vitart portado (build/src), libatrac9, volk, NVK e os módulos do jogo em build/mod.
#   VITART=/caminho/do/vitaRT GAME=gow1 bash scripts/build.sh
#   VITART=/caminho/do/vitaRT GAME=gow2 bash scripts/build.sh
# Variáveis: DEVKITPRO (padrão /opt/devkitpro), VULKAN_INCLUDE (headers do Vulkan com Volk/), JOBS.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
S=$ROOT/build/src
DKP=${DEVKITPRO:-/opt/devkitpro}
B=$DKP/devkitA64/bin
VK=${VULKAN_INCLUDE:?defina VULKAN_INCLUDE com a pasta Include do Vulkan SDK (com Volk/)}
TP=${VITART:?defina VITART com o caminho do clone do vitaRT}/third_party
GAME=${GAME:-gow1}
O=$ROOT/build/obj_$GAME
GDEF=""; [ "$GAME" = gow2 ] && GDEF="-DGOW_GAME_GOW2"
TITLE="God of War"; [ "$GAME" = gow2 ] && TITLE="God of War II"
VERSION=${VERSION:-1.0}
JOBS=${JOBS:-8}
[ -d "$S/nx" ] || { echo "rode scripts/setup.sh antes"; exit 1; }
for m in $GAME libc libfios2; do [ -f "$ROOT/build/mod/$m.o" ] || { echo "falta build/mod/$m.o (scripts/compile_mods.sh)"; exit 1; }; done
mkdir -p "$O"

ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
COMMON="$ARCH -O3 -g0 -fno-omit-frame-pointer -w -ffunction-sections -fdata-sections -D__SWITCH__ -DVK_USE_PLATFORM_VI_NN -DGOW_GAME=\"$GAME\" $GDEF
 -I$DKP/libnx/include -I$DKP/portlibs/switch/include -I$S/vitart -I$S/arm -I$S/nx -I$S/nx/shim -I$S/gow
 -I$TP/libatrac9/src -I$TP/stb -I$VK -I$VK/Volk"
CXXFLAGS="$COMMON -std=gnu++20 -fexceptions -frtti -include $S/nx/nx_compat.h"
CFLAGS="$COMMON -std=gnu11"

compile() { # fonte objeto
    local src=$1 obj=$2
    [ "$obj" -nt "$src" ] && [ "$obj" -nt "$S/nx/shim/windows.h" ] && [ "$obj" -nt "$S/nx/nx_compat.h" ] && return 0
    case $src in
    *.c) "$B/aarch64-none-elf-gcc" $CFLAGS -c "$src" -o "$obj" ;;
    *) "$B/aarch64-none-elf-g++" $CXXFLAGS -c "$src" -o "$obj" ;;
    esac || { echo "ERRO: $src"; return 1; }
    echo "  $(basename "$src")"
}

SRCS="$(ls "$S"/vitart/*.cpp "$S"/gow/*.cpp "$S"/nx/*.cpp "$S"/nx/*.c) $(ls "$TP"/libatrac9/src/*.c) $VK/Volk/volk.c"
OBJS=""
fail=0
for f in $SRCS; do
    b=$(basename "$f"); o=$O/${b%.*}.o
    case $f in */libatrac9/*) o=$O/at9_${b%.*}.o ;; esac
    OBJS="$OBJS $o"
    compile "$f" "$o" &
    while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do sleep 0.1; done
done
for j in $(jobs -p); do wait "$j" || fail=1; done
[ $fail = 0 ] || { echo "falhou a compilação"; exit 1; }

echo "== ligando $GAME.elf"
"$B/aarch64-none-elf-g++" -specs="$DKP/libnx/switch.specs" $ARCH -Wl,-Map,"$ROOT/build/$GAME.map" -Wl,--gc-sections \
    -o "$ROOT/build/$GAME.elf" $OBJS "$ROOT/build/mod/$GAME.o" "$ROOT/build/mod/libc.o" "$ROOT/build/mod/libfios2.o" \
    -L"$DKP/portlibs/switch/lib" -L"$DKP/libnx/lib" \
    -Wl,--whole-archive -lnvk -Wl,--no-whole-archive -Wl,--start-group -lnvk_support -lz -lexpat -Wl,--end-group \
    -Wl,--wrap=fsFsOpenFile,--wrap=fsFsOpenDirectory,--wrap=fsFsCreateFile,--wrap=fsFsCreateDirectory,--wrap=fsFsGetEntryType -Wl,-u,vk_icdGetInstanceProcAddr -lnx -lm

echo "== $GAME.nro"
"$DKP/tools/bin/nacptool" --create "$TITLE" "Guto Recomp" "$VERSION" "$ROOT/build/$GAME.nacp"
ICON=""; [ -f "$ROOT/res/icon_$GAME.jpg" ] && ICON="--icon=$ROOT/res/icon_$GAME.jpg"
"$DKP/tools/bin/elf2nro" "$ROOT/build/$GAME.elf" "$ROOT/build/$GAME.nro" --nacp="$ROOT/build/$GAME.nacp" $ICON
ls -la "$ROOT/build/$GAME.nro"
