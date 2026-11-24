# Changelog

**Português** · [English](CHANGELOG.en.md)

## v1.4-nx

A maior atualização desde o lançamento: **renderizador nativo deko3d**, **launcher e mod menu reformulados**, novas
opções de imagem, sistema de troféus, atualizador integrado e suporte a **português e inglês**.

### Desempenho
- **Renderizador deko3d** (API nativa da GPU do Switch) como caminho principal, com menos sobrecarga de CPU que o
  Vulkan. O Vulkan continua disponível em *Opções → Imagem → Renderizador*.
- **GPU a 460,8 MHz no modo portátil**: o jogo usa o perfil oficial de desempenho do portátil (o mesmo que jogos
  comerciais usam; não é overclock) e o mantém durante toda a partida. Na base o sistema segue em 768 MHz.
- **Geração de quadros (LSFG)** no *God of War I* com deko3d: cria quadros intermediários para 60 quadros na tela com
  o jogo a 30. O fluxo óptico ajustável (inteiro, 1/2, 1/3, 1/4) adapta o custo à resolução, com recuo automático
  quando um fluxo não cabe na resolução escolhida e pausa automática quando a GPU não dá conta.
- No 720p portátil o FSR pula a etapa de ampliação (já está perto de 1x) e aplica só a nitidez, economizando GPU.

### Imagem
- **AMD FSR 1.0** (EASU + RCAS) como upscaler, com nitidez ajustável de 0 a 10.
- **FXAA**, **filtro anisotrópico** (até 16x), **brilho**, **saturação**, **vibrância** e **debanding**.
- **Resolução separada para a TV**: uma resolução para o portátil e outra para a base, trocadas sozinhas ao encaixar
  o console.
- **Presets rápidos** (Desempenho, Equilibrado, Qualidade) no launcher e no mod menu.

### Launcher reformulado
- Visual novo: cards com a capa de cada jogo, painel com save, troféus e extras instalados (texturas HD, vídeos HD),
  música de fundo e indicação do último jogo aberto.
- **Opções em abas** (Imagem, Cor, Desempenho, Jogo), com descrição de cada item e valor padrão; tudo vale para os
  dois jogos.
- **Troféus**: lista completa de cada jogo, com progresso, pontos, filtros (todos, conquistados, faltando) e troféus
  ocultos protegidos contra spoiler.
- **Atualizador integrado**: o launcher procura versões novas no GitHub e instala launcher, *God of War* e
  *God of War II* de uma vez, sem apagar saves, opções, vídeos ou texturas.

### Mod menu reformulado (R + Plus)
- Painel novo com abas (**Gráficos, Mods, Cheats, Sistema**), chaves liga/desliga, barras de nível e descrição de
  cada opção.
- Todas as opções novas de imagem e desempenho mudam com o jogo rodando, sem reiniciar.
- **Cheats**: vida, magia e fúria infinitas. **Mods**: texturas HD, barras de vida dos inimigos e QTE automático.
- Diagnóstico da GPU para gerar logs de desempenho, com aviso antes de ligar.
- **Sair do jogo** em *Sistema*, com confirmação.

### Idiomas
- **Português (Brasil) e inglês** no launcher e no mod menu. A escolha fica em *Opções → Jogo → Idioma* ou em
  *Sistema → Idioma* no R + Plus, e vale para os dois.

### Correções
- Analógico na diagonal não deixa mais o Kratos lento.
- Vídeos HD abrem corretamente no Switch.
- Texturas HD com fila de decodificação fixa e cache menor (menos memória e menos travadas).
- Threads reaproveitadas e checagem de memória antes de varrer (#8).
- Tela preta com FSR 1.0 corrigida.
- Geração de quadros que não ligava em algumas resoluções corrigida; o tempo de espera do VSync não conta mais como
  carga da GPU.
- Encerramentos inesperados (abort, erro C++ não tratado) fecham o jogo direito e gravam o motivo no log, em vez de
  cair na tela de erro do Atmosphère (2168-0001).
- O atualizador instala o pacote `gow-nx.zip` (launcher + os dois jogos) e não fica mais pedindo a mesma versão.

## v1.3-nx

Correções e launcher novo. Obrigado a **[mvhoang92](https://github.com/mvhoang92)** pelos testes no console e pelo
diagnóstico de cada problema (#5, #6, #7).

- **Linha na borda esquerda/superior corrigida** (#6): com escala acima de 1x (Vita 960x544, 720p, 2x...) o meio pixel
  do viewport deixava a primeira coluna e a primeira linha de todo alvo sem desenho, com lixo da memória. A
  apresentação agora começa a ler depois delas (calculado a cada quadro pela escala).
- **Contador de FPS sem o mod de menu** (#7): sem o `.vtpatch` do menu de pausa, o item *FPS counter* do R + Plus usa
  o contador do vitart; com o mod, continua o do HUD.
- **Shaders do God of War II** (#5): o [BUILDING.md](docs/BUILDING.md) explica como completar o `shaders.pak` jogando
  (`shaders/miss` → `usse_build_cache.py` → `pack_shaders.py`). Um pack completo para os dois jogos tem ~9.700
  programas.
- **Launcher**: menu **Opções gráficas** (resolução interna, taxa de quadros, VSync, proporção, contador de FPS,
  texturas HD, barras de vida), gravadas em `data/vitart_options.ini` e válidas para os dois jogos; a versão
  mostrada agora vem do build (antes ficava sempre "v1.0").

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
