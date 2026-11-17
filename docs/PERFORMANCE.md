# Desempenho

Meta: **30 fps no relógio padrão do Switch** (CPU 1020 MHz, sem overclock).

| Resolução interna | Requisito |
|---|---|
| 1x | relógios padrão, **sem overclock** |
| 2x | **GPU a 614 MHz** |

## Resultado (God of War II, Switch real, portátil, CPU 1020 MHz)

| Etapa | Cena pesada |
|---|---|
| inicial | 23–25 fps |
| threads por núcleo + present sem segurar a trava | 27–35 fps |
| `arc_mem` constante no código gerado | 31–46 fps |
| prioridade do áudio | **30 fps estáveis** (limite de quadro do próprio jogo) |

Com isso, as threads `main` e `Render` passam a maior parte do quadro esperando: a CPU deixou de ser o gargalo.

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
