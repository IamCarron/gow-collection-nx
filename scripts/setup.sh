#!/bin/bash
# Monta a árvore de fontes do port em build/src a partir de um clone do vitaRT + os patches deste repositório.
#   VITART=/caminho/do/vitaRT bash scripts/setup.sh
# Resultado: build/src/{vitart,arm,gow,nx}, pronto para scripts/build.sh e scripts/compile_mods.sh.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
VITART=${VITART:?defina VITART com o caminho de um clone do vitaRT}
BASE=63d899fc72b5c4d48e8c4d1f14324a7d6d184b53   # commit do vitaRT sobre o qual os patches foram feitos
SRC=$ROOT/build/src
UP=$ROOT/build/upstream

git -C "$VITART" cat-file -e "$BASE^{commit}" 2>/dev/null || {
  echo "O clone do vitaRT em $VITART não tem o commit $BASE (rode git fetch)."; exit 1; }

rm -rf "${UP:?}" "${SRC:?}"
mkdir -p "$UP" "$SRC"/{vitart,arm,gow,nx}
git -C "$VITART" archive "$BASE" runtime/vitart runtime/arm games/gow | tar -x -C "$UP"
( cd "$UP" && git init -q . && for p in "$ROOT"/patches/*.patch; do
    echo "aplicando $(basename "$p")"; git apply --whitespace=nowarn "$p"; done )

# runtime/vitart sem os arquivos que só existem no PC (Direct3D 11, janela/áudio/entrada do Windows, vídeo HD)
for f in "$UP"/runtime/vitart/*; do
  case $(basename "$f") in
    gxm_d3d11.cpp|gxm_window.cpp|host_audio.cpp|host_input.cpp|trophy_banner.cpp|video_hd.cpp) ;;
    *) cp -r "$f" "$SRC/vitart/" ;;
  esac
done
cp "$UP"/runtime/arm/* "$SRC/arm/"
cp "$UP"/games/gow/*.cpp "$SRC/gow/"
cp -r "$ROOT"/platform/nx/* "$SRC/nx/"
echo "fontes prontas em $SRC"
