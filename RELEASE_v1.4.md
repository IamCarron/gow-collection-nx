# God of War Collection NX v1.4

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

---

**English**

The biggest update since launch: a **native deko3d renderer**, a **redesigned launcher and mod menu**, new image
options, a trophy system, a built-in updater and **English and Portuguese** support.

### Performance
- **deko3d renderer** (the Switch GPU's native API) as the main path, with less CPU overhead than Vulkan. Vulkan is
  still available under *Options → Image → Renderer*.
- **GPU at 460.8 MHz in handheld mode**: the game uses the official handheld performance profile (the same one
  retail games use; not an overclock) and keeps it for the whole session. Docked, the system stays at 768 MHz.
- **Frame generation (LSFG)** in *God of War I* with deko3d: creates in-between frames for 60 fps on screen with the
  game at 30. The adjustable optical flow (full, 1/2, 1/3, 1/4) scales the cost with resolution, falls back
  automatically when a flow size doesn't fit the chosen resolution, and pauses automatically when the GPU can't keep up.
- At 720p handheld, FSR skips the upscale pass (it is already close to 1x) and only sharpens, saving GPU time.

### Image
- **AMD FSR 1.0** (EASU + RCAS) upscaler, with sharpness from 0 to 10.
- **FXAA**, **anisotropic filtering** (up to 16x), **brightness**, **saturation**, **vibrance** and **debanding**.
- **Separate TV resolution**: one resolution for handheld and another for docked, switched automatically.
- **Quick presets** (Performance, Balanced, Quality) in the launcher and the mod menu.

### Redesigned launcher
- New look: cards with each game's cover, a panel with save, trophies and installed extras (HD textures, HD videos),
  background music and the last game played.
- **Tabbed options** (Image, Color, Performance, Game), with a description and default for every item; everything
  applies to both games.
- **Trophies**: the full list for each game, with progress, points, filters (all, earned, missing) and spoiler-safe
  hidden trophies.
- **Built-in updater**: the launcher checks GitHub for new versions and installs the launcher, *God of War* and
  *God of War II* in one go, keeping saves, options, videos and textures.

### Redesigned mod menu (R + Plus)
- New tabbed panel (**Graphics, Mods, Cheats, System**) with toggles, level bars and a description for every option.
- All new image and performance options change while the game runs, no restart needed.
- **Cheats**: infinite health, magic and rage. **Mods**: HD textures, enemy health bars and auto QTE.
- GPU diagnostics for performance logs, with a warning before it turns on.
- **Exit game** under *System*, with a confirmation.

### Languages
- **English and Portuguese (Brazil)** in the launcher and the mod menu. Pick it under *Options → Game → Language* or
  *System → Language* in R + Plus; it applies to both.

### Fixes
- Diagonal analog movement no longer slows Kratos down.
- HD videos open correctly on Switch.
- HD textures use a fixed decode queue and a smaller cache (less memory, fewer stutters).
- Threads are reused and memory is checked before scanning (#8).
- Black screen with FSR 1.0 fixed.
- Frame generation not starting at some resolutions fixed; VSync wait time no longer counts as GPU load.
- Unexpected exits (abort, unhandled C++ error) now close the game cleanly and log the reason, instead of hitting the
  Atmosphère error screen (2168-0001).
- The updater installs the `gow-nx.zip` package (launcher + both games) and no longer keeps offering the same version.

