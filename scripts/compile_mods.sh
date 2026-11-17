#!/bin/bash
# C gerado pelo vitarecomp ($GEN/<módulo>) -> objetos AArch64 -> build/mod/<módulo>.o, deixando globais só as
# entradas <módulo>_arc_* (bind, tabela, segmentos, módulo) para o runtime ligar vários módulos no mesmo NRO.
#   GEN=/caminho/out/gen bash scripts/compile_mods.sh gow1 libc libfios2
#   GEN=/caminho/out/gen bash scripts/compile_mods.sh gow2
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEN=${GEN:?defina GEN com a pasta do C gerado (uma subpasta por módulo)}
DKP=${DEVKITPRO:-/opt/devkitpro}
B=$DKP/devkitA64/bin
ARM=$ROOT/build/src/arm
OBJ=$ROOT/build/modobj
MOD=$ROOT/build/mod
FL="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -ffunction-sections -fdata-sections ${OPT:--O3} -w -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -I $ARM"
JOBS=${JOBS:-$(nproc)}
[ -f "$ARM/arc_runtime.c" ] || { echo "rode scripts/setup.sh antes"; exit 1; }
mkdir -p "$MOD"
for m in "$@"; do
  cp "$ARM/arc_runtime.c" "$ARM/arc_dll.c" "$GEN/$m/"
  rm -rf "${OBJ:?}/$m"; mkdir -p "$OBJ/$m"
  t0=$(date +%s)
  ls "$GEN/$m"/*.c | xargs -P "$JOBS" -I{} sh -c "$B/aarch64-none-elf-gcc $FL -c {} -o $OBJ/$m/\$(basename {} .c).o 2> $OBJ/$m/\$(basename {} .c).err || echo FALHOU {}"
  "$B/aarch64-none-elf-ld" -r -o "$MOD/$m.r.o" "$OBJ/$m"/*.o
  "$B/aarch64-none-elf-objcopy" -G ${m}_arc_bind -G ${m}_arc_get_table -G ${m}_arc_get_segments -G ${m}_arc_get_module \
    --redefine-sym arc_bind=${m}_arc_bind --redefine-sym arc_get_table=${m}_arc_get_table \
    --redefine-sym arc_get_segments=${m}_arc_get_segments --redefine-sym arc_get_module=${m}_arc_get_module \
    "$MOD/$m.r.o" "$MOD/$m.o" && rm "$MOD/$m.r.o"
  echo "== $m: $(( $(date +%s) - t0 )) s, $("$B/aarch64-none-elf-nm" -g "$MOD/$m.o" | grep -c ' T ') entradas"
done
