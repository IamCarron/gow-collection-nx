# Changelog

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
