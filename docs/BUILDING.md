# Compilação

O processo tem quatro etapas: extrair o jogo da sua cópia, gerar C com o recompilador do vitaRT, montar as fontes do
port e compilar os NROs.

## 1. Ferramentas

- **devkitPro** com `devkitA64`, `libnx` e os portlibs `switch-freetype`, `switch-harfbuzz`, `switch-mpg123`, `switch-ffmpeg`, `switch-dav1d`, `switch-zlib`,
  `switch-bzip2`, `switch-libpng`, `switch-expat`:

  ```bash
  pacman -S switch-dev switch-freetype switch-harfbuzz switch-mpg123 switch-ffmpeg switch-dav1d switch-zlib switch-bzip2 switch-libpng switch-expat
  ```

- **NVK para Switch** (`libnvk.a` e `libnvk_support.a`, Mesa) instalado em `$DEVKITPRO/portlibs/switch/lib`.
- **Headers do Vulkan** com o **Volk** (por exemplo a pasta `Include` do Vulkan SDK) → variável `VULKAN_INCLUDE`.
- Um clone do **[vitaRT](https://github.com/luisxl15/vitaRT)** contendo o commit `63d899f` → variável `VITART`.
- Python 3 (para o recompilador e o `tools/pack_shaders.py`).

## 2. Jogo e código gerado

Siga a documentação do vitaRT (`docs/BUILD.md` dele) para, a partir do **seu** PKG `PCSA00126` e do seu `work.bin`:

1. extrair e decriptar o jogo (`game/PCSA00126`) e os ELFs dos módulos (`gow1`, `gow2`, `libc`, `libfios2`);
2. gerar o C com o `vitarecomp`, usando os planos de link do vitaRT (`gow1-plan.json`, `gow2-plan.json`):

```bash
python -P -m vitarecomp link gow1-plan.json -o out/g1/link.json
for m in gow1 libc libfios2; do python -P -m vitarecomp recomp --link out/g1/link.json --module $m -o out/gen/$m; done
python -P -m vitarecomp link gow2-plan.json -o out/g2/link.json
python -P -m vitarecomp recomp --link out/g2/link.json --module gow2 -o out/gen/gow2
```

> O banco de NIDs do `vita-headers` precisa estar disponível para o recompilador; sem ele os imports ficam sem
> nome e nenhuma função HLE é ligada.

3. traduzir os shaders para SPIR-V (ferramentas do vitaRT) e empacotar:

```bash
python tools/pack_shaders.py <pasta com os .spv> shaders.pak
```

## 3. Fontes do port

```bash
VITART=/caminho/vitaRT bash scripts/setup.sh
```

Copia o runtime do vitaRT no commit base para `build/src`, aplica os `patches/` e junta a camada `platform/nx`.

## 4. Módulos e NROs

```bash
GEN=/caminho/out/gen bash scripts/compile_mods.sh gow1 gow2 libc libfios2
VITART=/caminho/vitaRT VULKAN_INCLUDE=/caminho/Include GAME=gow1 bash scripts/build.sh
VITART=/caminho/vitaRT VULKAN_INCLUDE=/caminho/Include GAME=gow2 bash scripts/build.sh
```

`libc` e `libfios2` são os mesmos nos dois jogos. Os ícones opcionais ficam em `res/icon_gow1.jpg` e
`res/icon_gow2.jpg` (use o `icon0` da sua cópia; não são versionados).

## 5. Launcher

```powershell
powershell -File tools/make_launcher_assets.ps1 -Image capa.jpg -Music musica.mp3
```

```bash
bash launcher/build.sh
```

Saída: `build/gow-nx.nro`, `build/gow1.nro` e `build/gow2.nro`. Veja o [README](../README.md) para a estrutura no SD.
