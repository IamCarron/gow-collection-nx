# Arquitetura

```
PKG PCSA00126 + work.bin  (sua cópia)
        │  extração / decriptação
        ▼
ELFs do Vita (gow1, gow2, libc, libfios2)
        │  vitarecomp link + recomp   (vitaRT)
        ▼
C gerado (uma função C por função ARMv7 do jogo)
        │  devkitA64 (scripts/compile_mods.sh)
        ▼
módulos AArch64 (.o)  +  runtime vitaRT com patches  +  platform/nx
        │  scripts/build.sh
        ▼
gow1.nro / gow2.nro
```

## Recompilação estática

O `vitarecomp` do vitaRT lê os ELFs do Vita, resolve imports/exports entre módulos (o plano de link) e traduz cada
função ARMv7/Thumb para C. O estado da CPU do Vita (registradores, flags, VFP/NEON) vira uma estrutura por thread e
a memória do Vita é um bloco único apontado por `arc_mem`. O código gerado acessa a memória como `arc_mem + endereço`.

- **`arc_mem`** é declarado `extern uint8_t *const` para o código gerado, então o compilador não recarrega o ponteiro
  depois de cada store.
- **`arc_dispatch`** resolve saltos indiretos (ponteiros de função, vtables) com um cache direto de 16K entradas
  antes da busca binária na tabela de funções.

## HLE (bordas do sistema)

As chamadas ao sistema do Vita são reimplementadas em C++ pelo runtime:

- **Kernel**: threads, mutex, semáforos, event flags, lwmutex/lwcond, mapeados para threads e primitivas do libnx.
- **E/S**: `sceIo*` e `fios2` mapeados para `sdmc:/switch/gow-nx/game/PCSA00126`.
- **Áudio**: ATRAC9 decodificado com LibAtrac9 e mixado para `audout`.
- **Entrada**: `sceCtrl` lido do `PadState` do libnx.
- **GPU**: `sceGxm*` traduzido para deko3d ou Vulkan (opção `renderer=`).

## GXM → Vulkan

A thread `Render` do jogo chama `sceGxm*`. O runtime converte cada chamada em uma operação (`RecOp`) numa fila SPSC.
A thread **`gravacao vk`** (núcleo 2) consome a fila, copia vértices/índices/uniforms do anel e grava os command
buffers Vulkan. Os shaders do GXM já chegam traduzidos para SPIR-V (`shaders.pak`) e os pipelines são criados sob
demanda e mantidos em cache.

No `present()` o runtime solta a trava global enquanto espera a cerca e o `vkAcquireNextImageKHR`, para que a lógica
do jogo continue rodando durante o vsync. O driver é o NVK (Mesa) ligado estaticamente.

## GXM → deko3d

Desde a v1.4 há um segundo renderizador (`gxm_dk.cpp`), que grava as mesmas operações direto na API nativa da GPU do
Switch. Os shaders não são compilados no console: o `tools/pack_shaders_dk.py` converte cada SPIR-V do `shaders.pak`
para DKSH no PC (SPIRV-Cross → GLSL → `uam`) e o `shaders_dk.pak` guarda os binários pela chave FNV-1a 64 do SPIR-V,
que o renderizador recalcula ao criar cada programa.

Depois da cena vêm as passadas de pós-processamento, na ordem: FXAA na resolução interna, ampliação (bilinear ou FSR
1.0 EASU, pulado quando a escala é ~1x), RCAS e ajustes de cor (brilho, saturação, vibrância, debanding). A geração de
quadros (lsfg-deko3d, só no *God of War*) roda como passadas de compute sobre os dois últimos quadros reais e
apresenta o quadro intermediário entre eles; o cache de shaders dela é preparado no PC pelo `tools/lsfg_prep`.

## Camada do Switch (`platform/nx`)

| Arquivo | Papel |
|---|---|
| `nx_platform.cpp/.h` | início, caminhos no SD, memória, threads (prioridade e núcleo), perfilador |
| `gxm_window_nx.cpp` | janela/swapchain sobre o `NWindow` do libnx |
| `host_audio_nx.cpp` | saída por `audout` (4 buffers de 10 ms) |
| `host_input_nx.cpp` | controles do Switch → layout do Vita |
| `nx_modules.cpp` | tabela dos módulos recompilados ligados no NRO |
| `posix_nx.c`, `stubs_nx.cpp`, `nx_compat.h`, `shim/` | o que o runtime espera do Windows/POSIX e o Switch não tem |

## Threads

| Thread | Núcleo | Papel |
|---|---|---|
| `main` | 0 | lógica do jogo |
| `Render` | 1 | chamadas GXM |
| `gravacao vk` | 2 | gravação Vulkan |
| áudio | 2 (ideal) | som, prioridade acima da gravação |

Detalhes e números em [PERFORMANCE.md](PERFORMANCE.md).

## Launcher

`launcher/main.c` desenha no framebuffer do libnx (1280x720), usa as fontes compartilhadas do sistema via FreeType,
toca o MP3 do romfs com mpg123 numa thread própria e, ao escolher, chama `envSetNextLoad` para o Homebrew Menu
carregar `gow1.nro` ou `gow2.nro`.
