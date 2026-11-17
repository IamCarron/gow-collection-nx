# Changelog

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
