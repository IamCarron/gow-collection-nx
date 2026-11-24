# Compilação

O processo tem quatro etapas: extrair o jogo da sua cópia, gerar C com o recompilador do vitaRT, montar as fontes do
port e compilar os NROs. Duas etapas extras deixam tudo pronto para a v1.4: os shaders do **deko3d** e, se quiser, o
cache da **geração de quadros (LSFG)**.

## 1. Ferramentas

- **devkitPro** com `devkitA64`, `libnx`, **`deko3d`** e os portlibs `switch-freetype`, `switch-libwebp` (capas do launcher), `switch-harfbuzz`, `switch-mpg123`, `switch-ffmpeg`, `switch-dav1d`, `switch-zlib`,
  `switch-bzip2`, `switch-libpng`, `switch-expat` (e `switch-curl`, `switch-mbedtls` para o atualizador do launcher):

  ```bash
  pacman -S switch-dev deko3d switch-libwebp switch-freetype switch-harfbuzz switch-mpg123 switch-ffmpeg switch-dav1d switch-zlib switch-bzip2 switch-libpng switch-expat switch-curl switch-mbedtls
  ```

- **NVK para Switch** (`libnvk.a` e `libnvk_support.a`, Mesa) instalado em `$DEVKITPRO/portlibs/switch/lib`.
- **Headers do Vulkan** com o **Volk** (por exemplo a pasta `Include` do Vulkan SDK) → variável `VULKAN_INCLUDE`.
- Um clone do **[vitaRT](https://github.com/luisxl15/vitaRT)** contendo o commit `63d899f` → variável `VITART`.
- Python 3 (para o recompilador, o `tools/pack_shaders.py` e o `tools/pack_shaders_dk.py`).
- Para os shaders do deko3d: **`spirv-cross`** (vem no Vulkan SDK) e **`uam`** (`$DEVKITPRO/tools/bin`, vem com o pacote
  `deko3d`/`switch-tools`).
- Opcional, para a geração de quadros: o código do **[lsfg-deko3d](https://github.com/NaGaa95/Cemu-nx)**
  (`third_party/lsfg-deko3d` do Cemu-nx), o código do **[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)**
  (commit `57e4efcc`), o g++ do MSYS2 do devkitPro e a sua cópia do *Lossless Scaling* (Steam).

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
python <vitaRT>/tools/usse_build_cache.py spv <vitaRT>/spv/miss
python tools/pack_shaders.py spv shaders.pak
```

### Completar os shaders jogando (necessário para o *God of War II*)

Os programas de shader (GXP) não ficam soltos nos `.psarc`: eles só existem na memória quando o jogo os carrega. O
conjunto que vem em `vitaRT/spv/miss` cobre quase só o *God of War* (cerca de 4.700 programas). Um pack completo
para os dois jogos tem cerca de **9.700**. Com o pack incompleto, o *God of War II* **fica parado na primeira tela de
carregamento** (GPU em 0%), e o log mostra centenas de linhas `vitart: programa 0x... sem spv no cache (<hash>)`.

Quando falta um programa, o port grava o binário dele em `sdmc:/switch/gow-nx/shaders/miss/<hash>.gxp`. Para
completar o pack:

1. Jogue até a parada (ou até onde quiser cobrir) e feche o jogo pelo HOME.
2. Copie `shaders/miss/*.gxp` do SD para o PC, por exemplo para `miss_sd/`.
3. Traduza só o que falta (os `.spv` que já existem não são refeitos) e reempacote:

   ```bash
   python <vitaRT>/tools/usse_build_cache.py spv miss_sd
   python tools/pack_shaders.py spv shaders.pak
   ```

4. Copie o `shaders.pak` novo para `sdmc:/switch/gow-nx/shaders/` e apague a pasta `shaders/miss` do SD.
5. Repita até o log não mostrar mais `sem spv no cache`. Em geral bastam duas ou três rodadas por jogo.

Os `.gxp` e `.spv` são derivados do jogo: não os compartilhe. Cada um gera os seus a partir da própria cópia.

### Shaders do deko3d (`shaders_dk.pak`)

O renderizador deko3d não compila shader no console: ele lê os binários DKSH prontos de `shaders/shaders_dk.pak`.
Gere esse pacote **sempre que o `shaders.pak` mudar** (depois de cada rodada de shaders acima):

```bash
python tools/pack_shaders_dk.py shaders.pak shaders_dk.pak        # 3º argumento opcional: nº de processos
```

Cada SPIR-V passa por `spirv-cross` (GLSL 460) e `uam` (DKSH). A chave de cada shader é o hash FNV-1a 64 dos bytes do
SPIR-V, a mesma conta que o jogo faz na hora de desenhar, então os dois `.pak` precisam sair da mesma rodada. Os
`.dksh` ficam em cache numa pasta ao lado da saída: rodar de novo só converte o que mudou.

| Variável | Padrão | O que faz |
|---|---|---|
| `SPIRV_CROSS` | `C:/VulkanSDK/1.4.363.0/Bin/spirv-cross.exe` | caminho do `spirv-cross` |
| `UAM` | `C:/devkitPro/tools/bin/uam.exe` | caminho do `uam` |
| `DK_UBO` | `1` | uniforms como uniform buffer (o renderizador espera assim) |
| `DK_HALF` | `1` | registradores de meia precisão dos fragmentos como `vec2` (`tools/dk_half.py`) |

Shader que não converter fica fora do pacote e aparece no log do jogo; o resto funciona. Copie os dois arquivos para
`sdmc:/switch/gow-nx/shaders/`.

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

Os dois renderizadores (Vulkan e deko3d) entram no mesmo NRO; a escolha é feita pela opção `renderer=` ao abrir o jogo.

| Variável do `build.sh` | O que faz |
|---|---|
| `DEKO3D_LIB` | biblioteca do deko3d no link (padrão `-ldeko3d`; troque para uma build própria, ex.: `-ldeko3dd` de debug) |
| `LSFG` | pasta do código do lsfg-deko3d. Com ela, a geração de quadros entra no NRO (`-DVITART_LSFG`); sem ela, a opção fica fora |
| `LSFG_GEN` | pasta com o cabeçalho de versão gerado: use `tools/lsfg_prep/gen` |

Com geração de quadros (só faz efeito no *God of War*, mas compila nos dois):

```bash
LSFG=/caminho/Cemu-nx/third_party/lsfg-deko3d LSFG_GEN=$PWD/tools/lsfg_prep/gen VITART=/caminho/vitaRT VULKAN_INCLUDE=/caminho/Include GAME=gow1 bash scripts/build.sh
```

O lsfg-deko3d é **GPL-3.0-or-later**: um NRO compilado com `LSFG` passa a ser distribuído sob a GPL (veja
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)).

`libc` e `libfios2` são os mesmos nos dois jogos. Os ícones opcionais ficam em `res/icon_gow1.jpg` e
`res/icon_gow2.jpg` (use o `icon0` da sua cópia; não são versionados).

## 5. Launcher

```powershell
powershell -File tools/make_launcher_assets.ps1 -Image capa.jpg -Music musica.mp3
```

```bash
bash launcher/build.sh
```

O launcher usa `VERSION` (padrão `1.4`) para comparar com o último release do GitHub no atualizador:
`VERSION=1.5 bash launcher/build.sh`. Sem `switch-curl`/`switch-mbedtls` o atualizador fica fora.

Saída: `build/gow-nx.nro`, `build/gow1.nro` e `build/gow2.nro`. Veja o [README](../README.md) para a estrutura no SD.

## Geração de quadros (LSFG)

A geração de quadros precisa de um cache de shaders feito no PC a partir do `Lossless.dll` da **sua** cópia do
*Lossless Scaling*. Esse arquivo e tudo o que sai dele são do autor do programa: **não versione nem compartilhe** o
DLL, a pasta `out/` nem a pasta `lsfg/` do SD (o `.gitignore` já bloqueia).

1. Compile o `lsfg_prep` no shell MSYS2 do devkitPro:

   ```bash
   LSFG=/caminho/Cemu-nx/third_party/lsfg-deko3d SPIRV_CROSS_SRC=/caminho/SPIRV-Cross bash tools/lsfg_prep/build.sh
   ```

2. Gere o cache (o `uam` precisa estar no `PATH` ou em `UAM=`):

   ```bash
   UAM=$DEVKITPRO/tools/bin/uam.exe tools/lsfg_prep/lsfg_prep.exe        "C:/Program Files (x86)/Steam/steamapps/common/Lossless Scaling/Lossless.dll" out
   ```

   Saem oito pastas: `Performance`, `Quality` e as variantes `_f2`, `_f3` e `_f4` de cada uma (fluxo óptico a 1/2,
   1/3 e 1/4 da resolução). Cada pasta é verificada depois de gravada.

3. Copie o conteúdo de `out/` para `sdmc:/switch/gow-nx/lsfg/`:

   ```
   sdmc:/switch/gow-nx/lsfg/
   ├── Performance/   Performance_f2/   Performance_f3/   Performance_f4/
   └── Quality/       Quality_f2/       Quality_f3/       Quality_f4/
   ```

4. No launcher: **Renderizador → deko3d** e **Geração de quadros → Desempenho** ou **Qualidade**. Se não ligar,
   o log (`logs/vitart_gow1.log`) mostra `dk: geração de quadros desligada:` seguido do motivo.

As revisões do SPIRV-Cross e do `uam` entram na chave do cache (`tools/lsfg_prep/gen/lsfg/common/version.hpp`). Gere o
cache de novo depois de trocar qualquer uma delas ou de atualizar o lsfg-deko3d.
