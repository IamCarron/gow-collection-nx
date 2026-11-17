# God of War Collection NX

**Port para Nintendo Switch (homebrew) de God of War e God of War II da *God of War Collection* do PS Vita**,
recompilados estaticamente para AArch64 e rodando sobre o runtime [vitaRT](https://github.com/luisxl15/vitaRT)
com uma camada própria para o libnx e Vulkan (NVK).

**Guto Recomp · v1.0**

> ⚠️ **Nenhum arquivo do jogo é distribuído aqui.** Para jogar você precisa da **sua própria cópia** da
> *God of War Collection* de PS Vita (PKG americano `PCSA00126` + a licença `work.bin`). Este repositório traz apenas
> o código do port para o Switch: nenhum executável, código gerado, shader, textura, áudio ou arte do jogo.

---

## Destaques

- **Dois jogos, um launcher**: `gow-nx.nro` abre um menu com capa e música e carrega o `gow1.nro` ou o `gow2.nro`.
- **30 fps estáveis no relógio padrão** (CPU 1020 MHz) nas cenas pesadas de God of War II, com a resolução interna 1x.
- **Vulkan no Switch** via NVK (Mesa), com gravação de comandos numa thread dedicada.
- Threads do jogo fixadas por núcleo (lógica, render, gravação Vulkan e áudio), sincronização do kernel do Vita
  emulada sem acordar ninguém à toa, e várias otimizações no runtime e no código gerado.
  Detalhes em [docs/PERFORMANCE.md](docs/PERFORMANCE.md).

## Requisitos de desempenho

| Resolução interna | CPU | GPU | Overclock |
|---|---|---|---|
| **1x** (720p) | 1020 MHz (padrão) | padrão | **não precisa** |
| **2x** | 1020 MHz (padrão) | **614 MHz** | **precisa da GPU a 614 MHz** (ex.: sys-clk) |

Testado no Switch real (modo portátil).

## Instalação no Switch

1. Gere os NROs seguindo [docs/BUILD.md](docs/BUILD.md) (com a sua cópia do jogo).
2. Copie para o cartão SD:

```
sdmc:/switch/gow-nx/
├── gow-nx.nro          launcher (abra este pelo hbmenu)
├── gow1.nro            God of War
├── gow2.nro            God of War II
├── game/PCSA00126/     arquivos do jogo extraídos da SUA cópia
├── shaders/shaders.pak shaders SPIR-V traduzidos (gerados por você)
└── data/               opções, saves e mods (criada pelo jogo)
```

3. Abra o **hbmenu em modo de título** (segurando R ao abrir um jogo) para ter toda a memória, e escolha
   **God of War Collection**. No menu: **Cima/Baixo** escolhe, **A** joga, **+** sai.

> O launcher precisa ser aberto pelo hbmenu (hbloader) para conseguir iniciar os jogos.

## Estrutura do repositório

| Pasta | Conteúdo |
|---|---|
| `platform/nx/` | camada do Switch: plataforma libnx, janela/apresentação, áudio (audout), entrada (HID), shims POSIX/Windows |
| `patches/` | mudanças do port sobre o vitaRT (runtime, ARM e GoW), aplicadas pelo `scripts/setup.sh` |
| `launcher/` | launcher com capa, música (libmpg123) e fonte do sistema (FreeType) |
| `scripts/` | `setup.sh` (monta as fontes), `compile_mods.sh` (C gerado → objetos), `build.sh` (NRO) |
| `tools/` | `pack_shaders.py` (empacota os SPIR-V), `make_launcher_assets.ps1` (capa/ícone/música do launcher) |
| `docs/` | compilação e notas de desempenho |

## Créditos

- **[vitaRT](https://github.com/luisxl15/vitaRT)** e **[GOD-OF-WAR-HD-COLLECTION](https://github.com/luisxl15/GOD-OF-WAR-HD-COLLECTION)**
  por luisxl15: recompilador e runtime do PS Vita em que este port se baseia. O código deles **não** está incluído
  aqui; o `scripts/setup.sh` usa um clone seu e aplica os patches deste repositório.
- [libnx](https://github.com/switchbrew/libnx) e devkitPro, [Mesa/NVK](https://docs.mesa3d.org/drivers/nvk.html),
  [mpg123](https://www.mpg123.de/), [FreeType](https://freetype.org/), [libatrac9](https://github.com/Thealexbarney/LibAtrac9),
  [volk](https://github.com/zeux/volk).
- God of War é marca da Sony Interactive Entertainment. Projeto de fãs, sem relação com a Sony ou a Santa Monica Studio.

## Licença

O código **deste repositório** (camada `platform/nx`, launcher, scripts, ferramentas e documentação) está sob a
licença [MIT](LICENSE). Os patches modificam código do vitaRT, que continua sob os termos do autor original.

---

## English (summary)

Nintendo Switch homebrew port of **God of War** and **God of War II** from the PS Vita *God of War Collection*,
statically recompiled to AArch64 on top of the [vitaRT](https://github.com/luisxl15/vitaRT) runtime with a custom
libnx + Vulkan (NVK) platform layer. **No game files are included**: you need your own copy (US PKG `PCSA00126` +
`work.bin`). Holds a steady 30 fps at stock CPU clock (1020 MHz) at 1x internal resolution; 2x resolution needs the
GPU at 614 MHz. A launcher (`gow-nx.nro`) lets you pick either game. See [docs/BUILD.md](docs/BUILD.md).
