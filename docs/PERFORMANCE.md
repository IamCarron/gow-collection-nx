# Desempenho

Meta: **30 fps sem overclock** (CPU 1020 MHz), e **60 na tela** com a geração de quadros.

| Versão | Relógios no portátil | Observação |
|---|---|---|
| até v1.3 | CPU 1020, GPU 307 MHz | 2x pedia a GPU a 614 MHz (sys-clk) |
| **v1.4** | CPU 1020, **GPU 460,8 MHz**, memória 1331 MHz | perfil oficial de desempenho do portátil, pedido pelo próprio port |

## Resultado (God of War II, Switch real, portátil, CPU 1020 MHz)

| Etapa | Cena pesada |
|---|---|
| inicial | 23–25 fps |
| threads por núcleo + present sem segurar a trava | 27–35 fps |
| `arc_mem` constante no código gerado | 31–46 fps |
| prioridade do áudio | **30 fps estáveis** (limite de quadro do próprio jogo) |

Com isso, as threads `main` e `Render` passam a maior parte do quadro esperando: a CPU deixou de ser o gargalo.

## v1.2 — GPU em 720p (God of War, relógio padrão, GPU 307 MHz)

| Etapa | Resultado |
|---|---|
| v1.0 | ~22 fps nas cenas pesadas |
| compressão + ZCULL do NVK | 25–28 fps |
| leitura direta de alvos, cópias por compute, anisotrópico 4x | **~45–60 fps** (FPS máximo) |

Tempo de GPU fora dos passes por quadro: cópia da profundidade 2,1 → 0,3 ms; cópia da própria cena 1,2 → 0,65 ms.
O log `vk: GPU por alvo` mostra cada segmento.

## v1.4 — deko3d, GPU a 460 MHz, FSR e geração de quadros (God of War, portátil)

| Mudança | Efeito |
|---|---|
| Renderizador **deko3d** (`renderer=deko3d`) | GXM gravado direto na API nativa da GPU, sem a camada Vulkan/NVK; shaders DKSH prontos no `shaders_dk.pak` (nada compila no console) |
| Perfil de desempenho do portátil (`apm`, 0x92220008) | GPU de 307,2 → **460,8 MHz**, mantida a partida toda; na base continua 768 MHz |
| FSR 1.0 em 720p | EASU pulado quando a escala é ~1x: só o RCAS roda |
| Geração de quadros (lsfg-deko3d) | **30 quadros reais → 60 na tela** |

Números do log do *God of War* em 1271x720, FSR, geração de quadros em Desempenho com fluxo 1/2:

| Medida | Valor |
|---|---|
| Quadros reais | 30 fps |
| Quadros na tela | 60 fps |
| GPU na cena | ~9–10 ms |
| GPU na geração | ~9,5–10 ms |
| GPU | 460,8 MHz a partida toda |

A geração tem um orçamento de 33,3 ms por quadro real: passando de 32 ms ela pausa por 5 s e só volta se o jogo mais o
último custo medido da geração couberem em 30 ms. Um fluxo que o plano recusa na resolução atual é trocado pelo fluxo
maior seguinte (`dk: fluxo 1/N não serve em WxH`). A opção **Diagnóstico da GPU** do mod menu grava no
log o tempo de cada passada.

## Threads

| Thread | Núcleo | Prioridade | Papel |
|---|---|---|---|
| `main` | 0 | 0x2A | lógica do jogo |
| `Render` | 1 | 0x2A | chamadas `sceGxm*` |
| `gravacao vk` | 2 | 0x2B | grava os comandos Vulkan (fila SPSC de `RecOp`) |
| `audio`, `audio_out_thread`, `SndStreamThread` | 0–2 (ideal 2) | 0x28 | som (sem engasgos) |
| demais | 0–2 (ideal 2) | padrão | partículas, streaming, fios |

`VITART_NX_PIN=0` desliga a fixação por núcleo.

## Otimizações

- **Código gerado**: `arc_mem` é `extern uint8_t *const` para o código recompilado; antes, cada store do guest fazia o
  compilador recarregar o ponteiro da memória (54 → 2 recargas numa função quente). `arc_dispatch` com cache direto
  de 16K entradas antes da busca binária.
- **Vulkan**: gravação de comandos numa thread própria; cópias do anel (vértices, índices, uniforms) feitas por ela;
  a gravadora só é acordada com ≥ 8 operações pendentes; `present()` solta a trava global durante a espera da cerca e
  o `vkAcquireNextImageKHR` (vsync).
- **HLE do GXM**: `context_of` com cache por thread; `DrawPrecomputed` sem copiar o contexto; layout de atributos em
  cache por programa; min/max de índices em NEON; sem `malloc` por desenho.
- **Kernel do Vita**: mutex, semáforos, event flags e lwcond só notificam quando há alguém esperando; cache por thread
  da busca de mutex.
- **E/S**: shaders num pacote único (`shaders.pak`) lido uma vez, em vez de milhares de arquivos soltos no SD.
- **Áudio**: 4 buffers de 10 ms no `audout` e threads de som acima da gravação Vulkan.

## Perfilador embutido

Ligado por padrão (`VITART_NX_PROF=0` desliga): amostra `main` e `Render` a cada 4 ms e grava `sdmc:/switch/gow-nx/logs/perfil_<jogo>.txt`
(seções `pc`, `lr` e `espera` por thread, com offsets relativos a `main`; símbolos com `aarch64-none-elf-nm -n`).
