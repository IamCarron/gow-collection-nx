// Launcher do gow-nx: capa em tela cheia, menu para escolher God of War / God of War II, música de fundo em loop,
// detecção automática do idioma do Switch (com seleção manual) e encadeamento do NRO escolhido via envSetNextLoad.
//   romfs:/bg.bgra    capa 1280x720, BGRA 8 bits (gerada de res/launcher_bg.jpg)
//   romfs:/music.mp3  música de fundo (decodificada com libmpg123)
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <mpg123.h>

#define W 1280
#define H 720

// ---------------------------------------------------------------- idiomas
enum { LANG_EN = 0, LANG_ES, LANG_PT, LANG_FR, LANG_IT, LANG_DE, NLANG };

typedef struct {
    const char *nome_idioma;
    const char *escolher;
    const char *sub_jogos[2];
    const char *opcoes_menu;
    const char *opcoes_titulo;
    const char *op_nomes[8];
    const char *auto_tela;
    const char *auto_prop;
    const char *max_fps;
    const char *desligado;
    const char *ligado;
    const char *nota_opcoes;
    const char *falta_arq;
    const char *msg_hbmenu;
    const char *msg_applet;
    const char *msg_sem_jogo;
    const char *msg_falha_prep;
    const char *msg_falha_ini;
    const char *btn_jogar;
    const char *btn_abrir;
    const char *btn_sair;
    const char *btn_mudar;
    const char *btn_salvar;
} Textos;

static const Textos kTextos[NLANG] = {
    [LANG_EN] = {
        .nome_idioma = "English",
        .escolher = "CHOOSE GAME",
        .sub_jogos = { "Kratos's journey against Ares", "Vengeance against Zeus" },
        .opcoes_menu = "Options",
        .opcoes_titulo = "OPTIONS",
        .op_nomes = { "Language", "Internal resolution", "Frame rate", "VSync",
                      "Aspect ratio", "FPS counter", "HD textures", "Enemy health bars" },
        .auto_tela = "Auto (screen)",
        .auto_prop = "Auto",
        .max_fps = "Max",
        .desligado = "Off",
        .ligado = "On",
        .nota_opcoes = "Applies to both games. Graphics also change in-game with R + Plus.",
        .falta_arq = "file not found in launcher folder",
        .msg_hbmenu = "Open from hbmenu to be able to launch the games.",
        .msg_applet = "Applet mode: open hbmenu holding R on a game (title takeover).",
        .msg_sem_jogo = "Game file not found in the launcher folder.",
        .msg_falha_prep = "Failed to prepare the selected game.",
        .msg_falha_ini = "Could not save data/vitart_options.ini.",
        .btn_jogar = " Play",
        .btn_abrir = " Open",
        .btn_sair = " Exit",
        .btn_mudar = " Change",
        .btn_salvar = " Save and back",
    },
    [LANG_ES] = {
        .nome_idioma = "Español",
        .escolher = "ELIGE EL JUEGO",
        .sub_jogos = { "El viaje de Kratos contra Ares", "La venganza contra Zeus" },
        .opcoes_menu = "Opciones",
        .opcoes_titulo = "OPCIONES",
        .op_nomes = { "Idioma", "Resolución interna", "Tasa de cuadros", "VSync",
                      "Relación de aspecto", "Contador de FPS", "Texturas HD", "Barras de vida enemigas" },
        .auto_tela = "Automática (pantalla)",
        .auto_prop = "Automática",
        .max_fps = "Máximo",
        .desligado = "Desactivado",
        .ligado = "Activado",
        .nota_opcoes = "Se aplica a ambos juegos. Los gráficos también cambian en juego con R + Plus.",
        .falta_arq = "archivo no encontrado en la carpeta del launcher",
        .msg_hbmenu = "Abre desde hbmenu para poder iniciar los juegos.",
        .msg_applet = "Modo applet: abre hbmenu manteniendo R en un juego (title takeover).",
        .msg_sem_jogo = "Archivo del juego no encontrado en la carpeta del launcher.",
        .msg_falha_prep = "Error al preparar el juego elegido.",
        .msg_falha_ini = "No se pudo guardar data/vitart_options.ini.",
        .btn_jogar = " Jugar",
        .btn_abrir = " Abrir",
        .btn_sair = " Salir",
        .btn_mudar = " Cambiar",
        .btn_salvar = " Guardar y volver",
    },
    [LANG_PT] = {
        .nome_idioma = "Português (BR)",
        .escolher = "ESCOLHA O JOGO",
        .sub_jogos = { "A jornada de Kratos contra Ares", "A vingança contra Zeus" },
        .opcoes_menu = "Opções",
        .opcoes_titulo = "OPÇÕES",
        .op_nomes = { "Idioma", "Resolução interna", "Taxa de quadros", "VSync",
                      "Proporção da tela", "Contador de FPS", "Texturas HD", "Barras de vida dos inimigos" },
        .auto_tela = "Automática (tela)",
        .auto_prop = "Automática",
        .max_fps = "Máximo",
        .desligado = "Desligado",
        .ligado = "Ligado",
        .nota_opcoes = "Valem para os dois jogos. Gráficos também mudam no jogo com R + Plus.",
        .falta_arq = "arquivo não encontrado na pasta do launcher",
        .msg_hbmenu = "Abra pelo hbmenu para poder iniciar os jogos.",
        .msg_applet = "Modo applet: abra o hbmenu segurando R num jogo (title takeover).",
        .msg_sem_jogo = "Arquivo do jogo não encontrado na pasta do launcher.",
        .msg_falha_prep = "Falha ao preparar o jogo escolhido.",
        .msg_falha_ini = "Não foi possível gravar data/vitart_options.ini.",
        .btn_jogar = " Jogar",
        .btn_abrir = " Abrir",
        .btn_sair = " Sair",
        .btn_mudar = " Mudar",
        .btn_salvar = " Salvar e voltar",
    },
    [LANG_FR] = {
        .nome_idioma = "Français",
        .escolher = "CHOISIR UN JEU",
        .sub_jogos = { "Le voyage de Kratos contre Arès", "La vengeance contre Zeus" },
        .opcoes_menu = "Options",
        .opcoes_titulo = "OPTIONS",
        .op_nomes = { "Langue", "Résolution interne", "Images par seconde", "VSync",
                      "Format d'image", "Compteur de FPS", "Textures HD", "Barres de vie ennemies" },
        .auto_tela = "Auto (écran)",
        .auto_prop = "Automatique",
        .max_fps = "Max",
        .desligado = "Désactivé",
        .ligado = "Activé",
        .nota_opcoes = "Valable pour les deux jeux. Les graphismes changent aussi en jeu avec R + Plus.",
        .falta_arq = "fichier introuvable dans le dossier du lanceur",
        .msg_hbmenu = "Ouvrez via hbmenu pour pouvoir lancer les jeux.",
        .msg_applet = "Mode applet : ouvrez hbmenu en maintenant R sur un jeu (title takeover).",
        .msg_sem_jogo = "Fichier du jeu introuvable dans le dossier du lanceur.",
        .msg_falha_prep = "Échec de la préparation du jeu choisi.",
        .msg_falha_ini = "Impossible d'enregistrer data/vitart_options.ini.",
        .btn_jogar = " Jouer",
        .btn_abrir = " Ouvrir",
        .btn_sair = " Quitter",
        .btn_mudar = " Changer",
        .btn_salvar = " Enregistrer et retour",
    },
    [LANG_IT] = {
        .nome_idioma = "Italiano",
        .escolher = "SCEGLI IL GIOCO",
        .sub_jogos = { "Il viaggio di Kratos contro Ares", "La vendetta contro Zeus" },
        .opcoes_menu = "Opzioni",
        .opcoes_titulo = "OPZIONI",
        .op_nomes = { "Lingua", "Risoluzione interna", "Frequenza fotogrammi", "VSync",
                      "Proporzioni schermo", "Contatore FPS", "Texture HD", "Barre vita nemici" },
        .auto_tela = "Automatica (schermo)",
        .auto_prop = "Automatico",
        .max_fps = "Massimo",
        .desligado = "Disattivato",
        .ligado = "Attivato",
        .nota_opcoes = "Valido per entrambi i giochi. La grafica cambia anche in gioco con R + Plus.",
        .falta_arq = "file non trovato nella cartella del launcher",
        .msg_hbmenu = "Apri da hbmenu per poter avviare i giochi.",
        .msg_applet = "Modalità applet: apri hbmenu tenendo premuto R su un gioco (title takeover).",
        .msg_sem_jogo = "File del gioco non trovato nella cartella del launcher.",
        .msg_falha_prep = "Impossibile preparare il gioco selezionato.",
        .msg_falha_ini = "Impossibile salvare data/vitart_options.ini.",
        .btn_jogar = " Gioca",
        .btn_abrir = " Apri",
        .btn_sair = " Esci",
        .btn_mudar = " Cambia",
        .btn_salvar = " Salva e torna",
    },
    [LANG_DE] = {
        .nome_idioma = "Deutsch",
        .escolher = "SPIEL WÄHLEN",
        .sub_jogos = { "Kratos' Reise gegen Ares", "Die Rache an Zeus" },
        .opcoes_menu = "Optionen",
        .opcoes_titulo = "OPTIONEN",
        .op_nomes = { "Sprache", "Interne Auflösung", "Bildrate", "VSync",
                      "Seitenverhältnis", "FPS-Zähler", "HD-Texturen", "Gegner-Lebensleisten" },
        .auto_tela = "Automatisch (Display)",
        .auto_prop = "Automatisch",
        .max_fps = "Maximum",
        .desligado = "Aus",
        .ligado = "Ein",
        .nota_opcoes = "Gilt für beide Spiele. Grafik ändert sich im Spiel auch mit R + Plus.",
        .falta_arq = "Datei im Launcher-Ordner nicht gefunden",
        .msg_hbmenu = "Über hbmenu öffnen, um die Spiele starten zu können.",
        .msg_applet = "Applet-Modus: hbmenu mit gedrücktem R über ein Spiel öffnen (Title Takeover).",
        .msg_sem_jogo = "Spieldatei im Launcher-Ordner nicht gefunden.",
        .msg_falha_prep = "Ausgewähltes Spiel konnte nicht vorbereitet werden.",
        .msg_falha_ini = "data/vitart_options.ini konnte nicht gespeichert werden.",
        .btn_jogar = " Spielen",
        .btn_abrir = " Öffnen",
        .btn_sair = " Beenden",
        .btn_mudar = " Ändern",
        .btn_salvar = " Speichern & zurück",
    },
};

enum { MSG_NONE = 0, MSG_HBMENU, MSG_APPLET, MSG_SEM_JOGO, MSG_FALHA_PREP, MSG_FALHA_INI };

static int idioma_sistema = LANG_EN;

static int detectar_idioma_switch(void) {
    u64 code = 0;
    if (R_SUCCEEDED(setInitialize())) {
        if (R_FAILED(setGetSystemLanguage(&code))) code = 0;
        setExit();
    }
    char tag[9];
    memcpy(tag, &code, 8);
    tag[8] = 0;
    if (!strncmp(tag, "es", 2)) return LANG_ES;
    if (!strncmp(tag, "pt", 2)) return LANG_PT;
    if (!strncmp(tag, "fr", 2)) return LANG_FR;
    if (!strncmp(tag, "it", 2)) return LANG_IT;
    if (!strncmp(tag, "de", 2)) return LANG_DE;
    return LANG_EN;
}

typedef struct { const char *titulo, *arquivo; } Jogo;
static const Jogo jogos[] = {
    { "God of War", "gow1.nro" },
    { "God of War II", "gow2.nro" },
};
#define N (int)(sizeof jogos / sizeof jogos[0])
static int jogo_ok[N];

static char dir[FS_MAX_PATH] = "sdmc:/switch/gow-nx/";

static int existe(const char *p) { struct stat st; return stat(p, &st) == 0; }
static void caminho(char *out, size_t n, int i) { snprintf(out, n, "%.700s%s", dir, jogos[i].arquivo); }
static void atualizar_jogos(void) {
    for (int i = 0; i < N; i++) {
        char p[FS_MAX_PATH]; caminho(p, sizeof p, i);
        jogo_ok[i] = existe(p);
    }
}

static int ler_ultimo(void) {
    char p[FS_MAX_PATH]; snprintf(p, sizeof p, "%.700slauncher.cfg", dir);
    FILE *f = fopen(p, "r"); int i = 0;
    if (f) { if (fscanf(f, "%d", &i) != 1) i = 0; fclose(f); }
    return (i >= 0 && i < N) ? i : 0;
}
static void gravar_ultimo(int i) {
    char p[FS_MAX_PATH]; snprintf(p, sizeof p, "%.700slauncher.cfg", dir);
    FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%d\n", i); fclose(f); }
}

#ifndef VERSAO
#define VERSAO "dev"
#endif

// ---------------------------------------------------------------- opções
// As opções gráficas (as mesmas do menu R + Plus) e o idioma ficam em <pasta>/data/vitart_options.ini.
// As linhas que o launcher não conhece (mods, cheats, ISO...) são preservadas como estão.
enum { OP_IDIOMA = 0, OP_RES, OP_FPS, OP_VSYNC, OP_ASPECTO, OP_FPS_CNT, OP_TEX_HD, OP_BARRAS, NOP };

typedef struct { const char *chave; int n; const char *const *val; int atual; } Opcao;
static const char *const l_val[] = {"auto", "en", "es", "pt-BR", "fr", "it", "de"};
static const char *const r_val[] = {"0", "1", "6", "5", "2", "3", "4"};
static const char *const r_fix[] = {"", "1x (720x408)", "Vita (960x544)", "720p (1270x720)", "2x (1440x816)", "3x (2160x1224)", "4x (2880x1632)"};
static const char *const f_val[] = {"30", "60", "max"};
static const char *const a_val[] = {"auto", "16:9", "16:10", "4:3"};
static const char *const b_val[] = {"0", "1"};
static Opcao opcoes[NOP] = {
    [OP_IDIOMA]  = { "language=",          7, l_val, 0 },
    [OP_RES]     = { "resolution_switch=", 7, r_val, 0 },
    [OP_FPS]     = { "fps=",               3, f_val, 0 },
    [OP_VSYNC]   = { "vsync=",             2, b_val, 1 },
    [OP_ASPECTO] = { "aspect=",            4, a_val, 0 },
    [OP_FPS_CNT] = { "fps_counter=",       2, b_val, 0 },
    [OP_TEX_HD]  = { "hd_textures=",       2, b_val, 1 },
    [OP_BARRAS]  = { "enemy_health_bars=", 2, b_val, 1 },
};

static int idioma_atual(void) {
    int v = opcoes[OP_IDIOMA].atual;
    return (v >= 1 && v <= NLANG) ? (v - 1) : idioma_sistema;
}
static const Textos *txt(void) { return &kTextos[idioma_atual()]; }

static const char *msg_texto(int id) {
    const Textos *t = txt();
    switch (id) {
    case MSG_HBMENU:     return t->msg_hbmenu;
    case MSG_APPLET:     return t->msg_applet;
    case MSG_SEM_JOGO:   return t->msg_sem_jogo;
    case MSG_FALHA_PREP: return t->msg_falha_prep;
    case MSG_FALHA_INI:  return t->msg_falha_ini;
    default:             return NULL;
    }
}

static int msg_inicial(void) {
    if (!envHasNextLoad()) return MSG_HBMENU;
    AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication) return MSG_APPLET;
    return MSG_NONE;
}

static void opcao_valor_texto(int i, char *out, size_t n) {
    const Textos *t = txt();
    int a = opcoes[i].atual;
    switch (i) {
    case OP_IDIOMA:
        if (a == 0) snprintf(out, n, "Auto (%s)", kTextos[idioma_sistema].nome_idioma);
        else snprintf(out, n, "%s", kTextos[a - 1].nome_idioma);
        break;
    case OP_RES:
        snprintf(out, n, "%s", a == 0 ? t->auto_tela : r_fix[a]);
        break;
    case OP_FPS:
        snprintf(out, n, "%s", a == 2 ? t->max_fps : f_val[a]);
        break;
    case OP_ASPECTO:
        snprintf(out, n, "%s", a == 0 ? t->auto_prop : a_val[a]);
        break;
    default:
        snprintf(out, n, "%s", a ? t->ligado : t->desligado);
        break;
    }
}

static void ini_caminho(char *p, size_t n) { snprintf(p, n, "%.700sdata/vitart_options.ini", dir); }
static void apara(char *s) { size_t n = strlen(s); while (n && (unsigned char)s[n - 1] <= 32) s[--n] = 0; }

static const char *idioma_codigo_vitart(int lang) {
    // O vitart (hle_io.cpp) aplica data/traducoes/<codigo>/ quando language=<codigo>; vazio/ausente = inglês original.
    switch (lang) {
    case LANG_ES: return "es";
    case LANG_PT: return "pt-BR";
    case LANG_FR: return "fr";
    case LANG_IT: return "it";
    case LANG_DE: return "de";
    default:      return "";
    }
}

static int parse_idioma(const char *v) {
    if (!strcmp(v, "pt") || !strcmp(v, "pt_BR") || !strcmp(v, "pt-br")) return 3;
    for (int j = 0; j < opcoes[OP_IDIOMA].n; j++)
        if (!strcmp(v, opcoes[OP_IDIOMA].val[j])) return j;
    return -1;
}

static void opcoes_ler(void) {
    char p[FS_MAX_PATH], l[1024]; ini_caminho(p, sizeof p);
    FILE *f = fopen(p, "r"); if (!f) return;
    int lang_cfg = -1, lang_ini = -1;
    while (fgets(l, sizeof l, f)) {
        apara(l);
        if (!strncmp(l, "launcher_language=", 18)) {
            int idx = parse_idioma(l + 18);
            if (idx >= 0) lang_cfg = idx;
            continue;
        }
        for (int i = 0; i < NOP; i++) {
            size_t k = strlen(opcoes[i].chave);
            if (strncmp(l, opcoes[i].chave, k)) continue;
            const char *v = l + k;
            if (i == OP_FPS) { opcoes[i].atual = !strcmp(v, "max") ? 2 : atoi(v) == 60 ? 1 : atoi(v) > 60 ? 2 : 0; continue; }
            if (i == OP_IDIOMA) {
                int idx = parse_idioma(v);
                if (idx >= 0) lang_ini = idx;
                continue;
            }
            for (int j = 0; j < opcoes[i].n; j++) if (!strcmp(v, opcoes[i].val[j])) opcoes[i].atual = j;
        }
    }
    if (lang_cfg >= 0) opcoes[OP_IDIOMA].atual = lang_cfg;
    else if (lang_ini >= 0) opcoes[OP_IDIOMA].atual = lang_ini;
    fclose(f);
}

static int opcoes_gravar(void) {
    char p[FS_MAX_PATH], np[FS_MAX_PATH + 8], l[1024]; ini_caminho(p, sizeof p);
    { char d[FS_MAX_PATH]; snprintf(d, sizeof d, "%.700sdata", dir); mkdir(d, 0777); }
    snprintf(np, sizeof np, "%s.novo", p);
    FILE *o = fopen(np, "w"); if (!o) return 0;
    FILE *f = fopen(p, "r");
    if (f) {
        while (fgets(l, sizeof l, f)) {
            apara(l);
            int nossa = !strncmp(l, "launcher_language=", 18);
            for (int i = 0; i < NOP; i++) nossa |= !strncmp(l, opcoes[i].chave, strlen(opcoes[i].chave));
            if (!nossa) fprintf(o, "%s\n", l);
        }
        fclose(f);
    }
    fprintf(o, "launcher_language=%s\n", opcoes[OP_IDIOMA].val[opcoes[OP_IDIOMA].atual]);
    const char *vc = idioma_codigo_vitart(idioma_atual());
    if (vc[0]) fprintf(o, "language=%s\n", vc);
    for (int i = 1; i < NOP; i++) fprintf(o, "%s%s\n", opcoes[i].chave, opcoes[i].val[opcoes[i].atual]);
    fclose(o);
    remove(p);
    return rename(np, p) == 0;
}

// ---------------------------------------------------------------- música
#define AFRAMES 2048                       // quadros por buffer (~43 ms a 48 kHz)
#define ABYTES  ((AFRAMES * 4 + 0xfff) & ~0xfff)
#define NBUF 4
static Thread a_thr;
static volatile int a_parar = 0, a_ok = 0;
static void *a_mem[NBUF];
static AudioOutBuffer a_buf[NBUF];

static void musica(void *arg) {
    (void)arg;
    mpg123_handle *mh = mpg123_new(NULL, NULL);
    if (!mh) return;
    mpg123_param(mh, MPG123_FLAGS, MPG123_FORCE_STEREO | MPG123_QUIET, 0);
    if (mpg123_open(mh, "romfs:/music.mp3") != MPG123_OK) { mpg123_delete(mh); return; }
    long rate = 44100; int ch, enc;
    mpg123_getformat(mh, &rate, &ch, &enc);
    mpg123_format_none(mh);
    mpg123_format(mh, rate, MPG123_STEREO, MPG123_ENC_SIGNED_16);

    // Reamostragem linear rate -> 48000 (o audout só toca a 48 kHz). in[0] guarda a última amostra do bloco anterior.
    const double passo = (double)rate / 48000.0;
    static int16_t in[8192 * 2];
    size_t n_in = 0; double pos = 0;
    int16_t prev[2] = { 0, 0 };
    float ganho = 0.f;                    // sobe de 0 (fade-in) e desce ao sair (fade-out)
    int fila = 0, vazio = 0, mudo = 0;

    while (1) {
        AudioOutBuffer *b = NULL;
        if (fila < NBUF) b = &a_buf[fila++];
        else { u32 cnt = 0; audoutWaitPlayFinish(&b, &cnt, UINT64_MAX); if (!b) continue; }
        int16_t *out = (int16_t *)b->buffer;
        for (int f = 0; f < AFRAMES; f++) {
            while (!mudo && pos + 1 >= (double)n_in) {
                if (n_in) { prev[0] = in[(n_in - 1) * 2]; prev[1] = in[(n_in - 1) * 2 + 1]; pos -= n_in; }
                size_t got = 0;
                int r = mpg123_read(mh, in + 2, sizeof in - 4, &got);
                in[0] = prev[0]; in[1] = prev[1];
                n_in = got / 4 + 1; pos += 1;
                if (got) vazio = 0; else if (++vazio > 64) mudo = 1;
                if (r == MPG123_DONE) mpg123_seek(mh, 0, SEEK_SET);
            }
            if (a_parar) ganho -= 1.f / (48000 * 0.35f); else if (ganho < 1.f) ganho += 1.f / (48000 * 1.5f);
            if (ganho < 0) ganho = 0;
            if (mudo) { out[f * 2] = out[f * 2 + 1] = 0; continue; }
            size_t i = (size_t)pos; float t = (float)(pos - i), g = ganho * ganho * 0.85f;
            for (int c = 0; c < 2; c++) {
                float v = in[i * 2 + c] + (in[(i + 1) * 2 + c] - in[i * 2 + c]) * t;
                out[f * 2 + c] = (int16_t)(v * g);
            }
            pos += passo;
        }
        b->data_size = AFRAMES * 4;
        audoutAppendAudioOutBuffer(b);
        if (a_parar && ganho <= 0) break;
    }
    for (int k = 0; k < NBUF; k++) { AudioOutBuffer *r = NULL; u32 c = 0; audoutWaitPlayFinish(&r, &c, 200000000ULL); }
    mpg123_close(mh); mpg123_delete(mh);
}

static void musica_iniciar(void) {
    if (!existe("romfs:/music.mp3")) return;
    if (R_FAILED(audoutInitialize())) return;
    if (R_FAILED(audoutStartAudioOut())) { audoutExit(); return; }
    mpg123_init();
    for (int i = 0; i < NBUF; i++) {
        a_mem[i] = aligned_alloc(0x1000, ABYTES); memset(a_mem[i], 0, ABYTES);
        a_buf[i].next = NULL; a_buf[i].buffer = a_mem[i]; a_buf[i].buffer_size = ABYTES;
        a_buf[i].data_size = AFRAMES * 4; a_buf[i].data_offset = 0;
    }
    if (R_FAILED(threadCreate(&a_thr, musica, NULL, NULL, 0x40000, 0x2C, -2))) {
        audoutStopAudioOut(); audoutExit(); mpg123_exit();
        for (int i = 0; i < NBUF; i++) { free(a_mem[i]); a_mem[i] = NULL; }
        return;
    }
    threadStart(&a_thr); a_ok = 1;
}

static void musica_parar(void) {                 // fade-out curto e libera o áudio para o jogo
    if (!a_ok) return;
    a_parar = 1;
    threadWaitForExit(&a_thr); threadClose(&a_thr);
    audoutStopAudioOut(); audoutExit(); mpg123_exit();
    for (int i = 0; i < NBUF; i++) free(a_mem[i]);
    a_ok = 0;
}

// ---------------------------------------------------------------- desenho
static u32 *bg;                                   // capa já em RGBA (formato do framebuffer), com o lado esquerdo escurecido
static FT_Library ft;
static FT_Face f_txt, f_ext;                      // fonte do sistema + fonte com os botões da Nintendo
static u32 *px; static u32 pitch;                 // destino atual (pitch em pixels)

static inline void mistura(int x, int y, u32 cor, int a) {
    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0) return;
    if (a > 255) a = 255;
    u32 *d = &px[y * pitch + x], o = *d;
    int r = ((o & 0xff) * (255 - a) + (cor & 0xff) * a) / 255;
    int g = (((o >> 8) & 0xff) * (255 - a) + ((cor >> 8) & 0xff) * a) / 255;
    int b = (((o >> 16) & 0xff) * (255 - a) + ((cor >> 16) & 0xff) * a) / 255;
    *d = r | g << 8 | b << 16 | 0xff000000u;
}

static u32 utf8(const char **s) {
    const unsigned char *p = (const unsigned char *)*s; u32 c = *p++;
    if (c >= 0xf0) { c = (c & 7) << 18 | (p[0] & 63) << 12 | (p[1] & 63) << 6 | (p[2] & 63); p += 3; }
    else if (c >= 0xe0) { c = (c & 15) << 12 | (p[0] & 63) << 6 | (p[1] & 63); p += 2; }
    else if (c >= 0xc0) { c = (c & 31) << 6 | (p[0] & 63); p += 1; }
    *s = (const char *)p; return c;
}

// Desenha texto UTF-8 com a linha de base em y. Devolve a largura.
static int texto(int x, int y, int tam, u32 cor, int alfa, const char *s) {
    int x0 = x;
    while (*s) {
        u32 c = utf8(&s);
        FT_Face f = f_txt;
        if (c >= 0xE000 && c <= 0xF8FF && f_ext && FT_Get_Char_Index(f_ext, c)) f = f_ext;
        if (!f) break;
        FT_Set_Pixel_Sizes(f, 0, tam);
        if (FT_Load_Char(f, c, FT_LOAD_RENDER)) continue;
        FT_GlyphSlot g = f->glyph;
        for (unsigned yy = 0; yy < g->bitmap.rows; yy++)
            for (unsigned xx = 0; xx < g->bitmap.width; xx++) {
                int a = g->bitmap.buffer[yy * g->bitmap.pitch + xx] * alfa / 255;
                mistura(x + g->bitmap_left + xx, y - g->bitmap_top + yy, cor, a);
            }
        x += g->advance.x >> 6;
    }
    return x - x0;
}

static int texto_sombra(int x, int y, int tam, u32 cor, int alfa, const char *s) {
    texto(x + 2, y + 3, tam, 0x000000, alfa * 3 / 4, s);
    return texto(x, y, tam, cor, alfa, s);
}

static void retangulo(int x, int y, int w, int h, u32 cor, int a) {
    for (int yy = y; yy < y + h; yy++) for (int xx = x; xx < x + w; xx++) mistura(xx, yy, cor, a);
}

#define RGB(r, g, b) ((u32)(r) | (u32)(g) << 8 | (u32)(b) << 16)
static const u32 OURO = RGB(236, 178, 74), OURO_ESC = RGB(150, 92, 30), CINZA = RGB(190, 180, 165),
                 BRANCO = RGB(240, 232, 220), VERMELHO = RGB(230, 80, 60);

static void assinatura(int yb) {
    if (!f_txt) return;
    const char *ass = "Guto Recomp", *ver = "v" VERSAO;
    FT_Set_Pixel_Sizes(f_txt, 0, 22);
    int wa = 0, wv = 0;
    { int sx = 0; const char *s = ass; while (*s) { u32 c = utf8(&s); if (!FT_Load_Char(f_txt, c, FT_LOAD_DEFAULT)) sx += f_txt->glyph->advance.x >> 6; } wa = sx; }
    FT_Set_Pixel_Sizes(f_txt, 0, 18);
    { int sx = 0; const char *s = ver; while (*s) { u32 c = utf8(&s); if (!FT_Load_Char(f_txt, c, FT_LOAD_DEFAULT)) sx += f_txt->glyph->advance.x >> 6; } wv = sx; }
    texto_sombra(W - 60 - wa - 12 - wv, yb, 22, OURO, 235, ass);
    texto_sombra(W - 60 - wv, yb, 18, CINZA, 220, ver);
}

static void desenhar_opcoes(int sel, int msg, u64 tick) {
    const Textos *t = txt();
    if (bg) { for (int y = 0; y < H; y++) memcpy(&px[y * pitch], &bg[y * W], W * 4); }
    else { for (int y = 0; y < H; y++) memset(&px[y * pitch], 0, W * 4); }
    float pulso = 0.5f + 0.5f * sinf(tick * 0.06f);
    texto_sombra(110, 174, 26, CINZA, 255, t->opcoes_titulo);
    retangulo(110, 188, 300, 2, OURO_ESC, 200);
    for (int i = 0; i < NOP; i++) {
        int y = 228 + i * 44, s = i == sel;
        if (s) {
            for (int x = 96; x < 860; x++) retangulo(x, y - 30, 1, 40, OURO_ESC, 80 * (860 - x) / 764);
            retangulo(90, y - 30, 5, 40, OURO, 150 + (int)(105 * pulso));
        }
        texto_sombra(116, y, 24, s ? OURO : CINZA, s ? 255 : 210, t->op_nomes[i]);
        char raw[80], v[96];
        opcao_valor_texto(i, raw, sizeof raw);
        snprintf(v, sizeof v, s ? "<  %s  >" : "%s", raw);
        texto_sombra(490, y, 24, s ? BRANCO : CINZA, s ? 255 : 200, v);
    }
    texto_sombra(116, 228 + NOP * 44 + 8, 18, CINZA, 200, t->nota_opcoes);
    const char *ms = msg_texto(msg);
    if (ms) texto_sombra(110, 640, 22, VERMELHO, 255, ms);
    int x = 110, yb = 676;
    x += texto_sombra(x, yb, 28, BRANCO, 255, "\xEE\x83\xA0");   // U+E0E0 botão A
    x += texto_sombra(x + 4, yb, 22, BRANCO, 230, t->btn_mudar) + 44;
    x += texto_sombra(x, yb, 28, BRANCO, 255, "\xEE\x83\xA1");   // U+E0E1 botão B
    texto_sombra(x + 4, yb, 22, BRANCO, 230, t->btn_salvar);
    assinatura(yb);
}

static void desenhar(int sel, int msg, u64 tick) {
    const Textos *t = txt();
    if (bg) { for (int y = 0; y < H; y++) memcpy(&px[y * pitch], &bg[y * W], W * 4); }
    else { for (int y = 0; y < H; y++) memset(&px[y * pitch], 0, W * 4); }
    float pulso = 0.5f + 0.5f * sinf(tick * 0.06f);

    texto_sombra(110, 250, 26, CINZA, 255, t->escolher);
    retangulo(110, 264, 260, 2, OURO_ESC, 200);

    {   // item das opções, abaixo dos jogos
        int y = 340 + N * 110 - 20, s = sel == N;
        if (s) {
            for (int x = 96; x < 560; x++) retangulo(x, y - 34, 1, 48, OURO_ESC, 80 * (560 - x) / 464);
            retangulo(90, y - 34, 5, 48, OURO, 150 + (int)(105 * pulso));
        }
        texto_sombra(120, y, s ? 30 : 26, s ? OURO : CINZA, s ? 255 : 200, t->opcoes_menu);
    }

    for (int i = 0; i < N; i++) {
        int y = 340 + i * 110;
        int ok = jogo_ok[i], s = i == sel;
        if (s) {
            for (int x = 96; x < 560; x++) retangulo(x, y - 50, 1, 82, OURO_ESC, 80 * (560 - x) / 464);
            retangulo(90, y - 50, 6, 82, OURO, 150 + (int)(105 * pulso));
        }
        texto_sombra(120, y, s ? 48 : 42, s ? OURO : CINZA, s ? 255 : 200, jogos[i].titulo);
        texto_sombra(122, y + 26, 20, ok ? (s ? BRANCO : CINZA) : VERMELHO, s ? 230 : 160,
                     ok ? t->sub_jogos[i] : t->falta_arq);
    }

    const char *ms = msg_texto(msg);
    if (ms) texto_sombra(110, 640, 22, VERMELHO, 255, ms);

    int x = 110, yb = 676;
    x += texto_sombra(x, yb, 28, BRANCO, 255, "\xEE\x83\xA0");   // U+E0E0 botão A
    x += texto_sombra(x + 4, yb, 22, BRANCO, 230, sel == N ? t->btn_abrir : t->btn_jogar) + 44;
    x += texto_sombra(x, yb, 28, BRANCO, 255, "\xEE\x83\xAF");   // U+E0EF botão +
    texto_sombra(x + 4, yb, 22, BRANCO, 230, t->btn_sair);

    assinatura(yb); // canto inferior direito
}

static void carregar_capa(void) {
    bg = malloc(W * H * 4);
    if (!bg) return;
    FILE *f = fopen("romfs:/bg.bgra", "rb");
    size_t n = 0;
    if (f) { n = fread(bg, 4, W * H, f); fclose(f); }
    if (n != W * H) memset(bg, 0, W * H * 4);
    for (int i = 0; i < W * H; i++) { u32 v = bg[i]; bg[i] = (v >> 16 & 0xff) | (v & 0xff00) | (v & 0xff) << 16 | 0xff000000u; }
    // escurece o lado esquerdo (já é escuro na arte) uma vez só, para o menu ler bem
    px = bg; pitch = W;
    for (int x = 0; x < 640; x++) {
        int a = x < 420 ? 110 : 110 * (640 - x) / 220;
        for (int y = 0; y < H; y++) mistura(x, y, 0, a);
    }
}

int main(int argc, char **argv) {
    // Pasta do launcher (onde também ficam gow1.nro / gow2.nro), tirada do argv[0].
    if (argc > 0 && argv[0] && strchr(argv[0], '/')) {
        const char *a = argv[0];
        if (strncmp(a, "sdmc:", 5) != 0 && a[0] == '/') snprintf(dir, sizeof dir, "sdmc:%.700s", a);
        else snprintf(dir, sizeof dir, "%.700s", a);
        char *b = strrchr(dir, '/'); if (b) b[1] = 0;
    }
    romfsInit();
    carregar_capa();

    plInitialize(PlServiceType_User);
    FT_Init_FreeType(&ft);
    PlFontData fd;
    if (R_SUCCEEDED(plGetSharedFontByType(&fd, PlSharedFontType_Standard)))
        FT_New_Memory_Face(ft, fd.address, fd.size, 0, &f_txt);
    if (R_SUCCEEDED(plGetSharedFontByType(&fd, PlSharedFontType_NintendoExt)))
        FT_New_Memory_Face(ft, fd.address, fd.size, 0, &f_ext);

    Framebuffer fb;
    framebufferCreate(&fb, nwindowGetDefault(), W, H, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);

    musica_iniciar();

    idioma_sistema = detectar_idioma_switch();
    opcoes_ler();
    atualizar_jogos();
    int sel = ler_ultimo(), lancar = 0, tela_op = 0, sel_op = 0;
    int msg = msg_inicial();
    u64 tick = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 k = padGetButtonsDown(&pad);
        if (tela_op) {
            Opcao *o = &opcoes[sel_op];
            if (k & HidNpadButton_AnyUp) sel_op = (sel_op + NOP - 1) % NOP;
            if (k & HidNpadButton_AnyDown) sel_op = (sel_op + 1) % NOP;
            if (k & (HidNpadButton_AnyRight | HidNpadButton_A)) o->atual = (o->atual + 1) % o->n;
            if (k & HidNpadButton_AnyLeft) o->atual = (o->atual + o->n - 1) % o->n;
            if (k & (HidNpadButton_B | HidNpadButton_Plus)) {
                msg = !opcoes_gravar() ? MSG_FALHA_INI : msg_inicial();
                tela_op = 0;
            }
            px = framebufferBegin(&fb, &pitch); pitch /= 4;
            desenhar_opcoes(sel_op, msg, tick++);
            framebufferEnd(&fb);
            continue;
        }
        if (k & HidNpadButton_Plus) break;
        if (k & HidNpadButton_AnyUp) sel = (sel + N) % (N + 1);
        if (k & HidNpadButton_AnyDown) sel = (sel + 1) % (N + 1);
        if ((k & HidNpadButton_A) && sel == N) { tela_op = 1; msg = MSG_NONE; }
        else if (k & HidNpadButton_A) {
            char p[FS_MAX_PATH]; caminho(p, sizeof p, sel);
            jogo_ok[sel] = existe(p);
            if (!jogo_ok[sel]) msg = MSG_SEM_JOGO;
            else if (envHasNextLoad()) {
                if (opcoes[OP_IDIOMA].atual == 0) opcoes_gravar();
                char args[FS_MAX_PATH + 4]; snprintf(args, sizeof args, "\"%.760s\"", p);
                if (R_SUCCEEDED(envSetNextLoad(p, args))) { gravar_ultimo(sel); lancar = 1; }
                else msg = MSG_FALHA_PREP;
            }
        }
        px = framebufferBegin(&fb, &pitch); pitch /= 4;
        desenhar(sel, msg, tick++);
        if (lancar) retangulo(0, 0, W, H, 0, 140);
        framebufferEnd(&fb);
        if (lancar) break;
    }

    musica_parar();
    framebufferClose(&fb);
    if (f_txt) FT_Done_Face(f_txt);
    if (f_ext) FT_Done_Face(f_ext);
    FT_Done_FreeType(ft);
    plExit();
    romfsExit();
    free(bg);
    return 0;
}
