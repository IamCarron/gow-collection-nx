# Changelog

**Português** · [English](CHANGELOG.en.md)

## v1.2-nx

Foco: **mais FPS em 720p no relógio padrão** (CPU 1020 MHz, GPU 307 MHz, sem overclock).

- **GoW1 em 720p, relógio padrão: ~45–60 FPS** com *FPS máximo* (antes ~22 FPS nas cenas pesadas).
- **Compressão de hardware e ZCULL do Maxwell ligadas no NVK**: alvos de render com alocação dedicada e
  profundidade limpa por render pass (sem `TRANSFER_DST`), o que libera o descarte de pixels escondidos antes do
  shader.
- **Leitura direta de alvos**: um desenho que lê outro alvo (cor ou profundidade) usa a imagem dele sem cópia.
- **Cópias de alvo por compute shader** (`fsr/snap_*.comp`) em vez de `vkCmdCopyImage`: a cópia da profundidade
  caiu de ~2,1 ms para ~0,3 ms por quadro e a cópia da própria cena de ~1,2 ms para ~0,65 ms.
  `VITART_VK_SNAPCS=0` volta ao caminho antigo.
- **Filtro anisotrópico 4x** no Switch (antes 16x): mais leve na banda de memória (`VITART_VK_ANISO` muda).
- **CPU**: cache de bindings por thread no `host_import`, cache de SPIR-V/programa por desenho no HLE do GXM,
  uma trava por desenho em vez de várias e `sceGxmSetUniformDataF` sem divisões.
- **Tela cheia em 16:9**: a imagem de 1271x720 preenche os 1280x720 da tela (sem faixas pretas de 4 px).
- **Saída pelo HOME** limpa (sem travar ao fechar) e log mais leve (fsync no máximo 1x/s).
- **Diagnóstico**: tempo de GPU por segmento (cópias, limpezas, leituras diretas) no log e a thread
  `gravacao vk` incluída no `perfil_<jogo>.txt`.

Conhecido: *God of War II* ainda tem quedas pontuais em cenas pesadas (limitado pela GPU); é o próximo alvo.

## v1.1-nx

- **Menu de mods** no jogo (**R + Plus**): resolução interna, taxa de quadros, VSync, contador de FPS, texturas HD e
  barras de vida dos inimigos, com fonte do sistema; as escolhas vão para `data/vitart_options.ini`.
- **Resoluções novas**: *Vita (960x544)* (a nativa do Vita, ~43% menos pixels que 720p) e *720p (1270x720)*, além de
  1x a 4x.
- **Vídeos HD**: cenas remasterizadas em `.mp4` (HEVC/H.264) decodificadas no NVDEC do Tegra via FFmpeg e convertidas
  de NV12 para RGBA num compute shader; o Bink do jogo continua cuidando do som e do pular. Sem a pasta `videos`,
  nada muda.
- **Relógios de fábrica**: o port não faz mais overclock (CPU 1020 MHz, GPU 307 MHz no portátil).
- **Diagnóstico de GPU** no log: tempo por alvo de render, tempo fora dos passes, GPU parada entre envios e espera
  da apresentação, para achar o que pesa em cada cena.
- Swapchain com uma imagem a mais de folga.

## v1.0-nx

Primeira versão pública.

- **Launcher** `gow-nx.nro`: menu para escolher *God of War* ou *God of War II*, com capa em 1280x720, música de
  fundo (MP3 com fade), fontes do sistema e a última escolha lembrada; carrega o jogo pelo Homebrew Menu.
- **God of War** e **God of War II** recompilados para ARM64 e rodando no Switch com Vulkan (NVK).
- **30 FPS estáveis** em *God of War II* com a CPU em 1020 MHz (antes 23–25 FPS nas cenas pesadas):
  - threads fixadas por núcleo e gravação Vulkan numa thread dedicada;
  - `present()` sem segurar a trava global durante vsync;
  - `arc_mem` constante no código gerado e cache direto no `arc_dispatch`;
  - HLE do GXM e primitivas do kernel do Vita sem alocações nem notificações desnecessárias;
  - shaders num pacote único (`shaders.pak`).
- **Áudio** ATRAC9 sem engasgos (4 buffers de 10 ms, prioridade acima da gravação Vulkan).
- **Opções no menu de pausa**: resolução interna (1x–4x), taxa de quadros (30/60/máx), VSync, texturas HD, áudio do
  PS2, QTE automático, contador de FPS e cheats; salvas em `data/vitart_options.ini`.
- Resolução **1x** sem overclock; **2x** com a GPU a 614 MHz.
- Perfilador embutido (`logs/perfil_<jogo>.txt`) e logs em `logs/`.
