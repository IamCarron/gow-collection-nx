#!/bin/bash
# Compila o lsfg_prep (PC) com o g++ do MSYS2 do devkitPro.
set -e
cd "$(dirname "$0")"
# LSFG e SPIRV_CROSS_SRC: caminhos absolutos, ou relativos a esta pasta.
L=${LSFG:-../../../third_party/cemu-nx/third_party/lsfg-deko3d}   # fonte do lsfg-deko3d (Cemu-nx)
SC=${SPIRV_CROSS_SRC:-../../../third_party/spirv-cross}   # fonte do SPIRV-Cross
mkdir -p obj
SRCS="binding cache_format cache_load cache_store dksh error image_graph layout pe_resources prepare schedule sha256 shader_set spirv_module translate"
OBJS=""
for s in $SRCS; do
  o=obj/$s.o; OBJS="$OBJS $o"
  [ $o -nt $L/src/$s.cpp ] || g++ -std=gnu++20 -O2 -w -fexceptions -I$L/include -Igen -I$SC -c $L/src/$s.cpp -o $o &
done
for s in spirv_cfg spirv_cross spirv_cross_parsed_ir spirv_glsl spirv_parser; do
  o=obj/sc_$s.o; OBJS="$OBJS $o"
  [ $o -nt $SC/$s.cpp ] || g++ -std=gnu++17 -O2 -w -fexceptions -I$SC -c $SC/$s.cpp -o $o &
done
wait
g++ -std=gnu++20 -O2 -w -I$L/include -Igen compile_uam_cli.cpp main.cpp $OBJS -o lsfg_prep.exe
echo ok
