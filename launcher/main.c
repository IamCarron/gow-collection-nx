// Launcher do gow-nx: capa em tela cheia, menu para escolher God of War / God of War II, música de fundo em loop,
// e encadeia o NRO escolhido (mesma pasta) via hbloader (envSetNextLoad).
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

typedef struct { const char *titulo, *sub, *arquivo; } Jogo;
static const Jogo jogos[] = {
    { "God of War", "A jornada de Kratos contra Ares", "gow1.nro" },
    { "God of War II", "A vingança contra Zeus", "gow2.nro" },
};
#define N (int)(sizeof jogos / sizeof jogos[0])

static char dir[FS_MAX_PATH] = "sdmc:/switch/gow-nx/";

static int existe(const char *p) { struct stat st; return stat(p, &st) == 0; }
static void caminho(char *out, size_t n, int i) { snprintf(out, n, "%.700s%s", dir, jogos[i].arquivo); }

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
    if (R_FAILED(threadCreate(&a_thr, musica, NULL, NULL, 0x40000, 0x2C, -2))) { audoutStopAudioOut(); audoutExit(); return; }
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

static void desenhar(int sel, const char *msg, u64 tick) {
    for (int y = 0; y < H; y++) memcpy(&px[y * pitch], &bg[y * W], W * 4);
    float pulso = 0.5f + 0.5f * sinf(tick * 0.06f);

    texto_sombra(110, 250, 26, CINZA, 255, "ESCOLHA O JOGO");
    retangulo(110, 264, 260, 2, OURO_ESC, 200);

    for (int i = 0; i < N; i++) {
        int y = 340 + i * 110;
        char p[FS_MAX_PATH]; caminho(p, sizeof p, i);
        int ok = existe(p), s = i == sel;
        if (s) {
            for (int x = 96; x < 560; x++) retangulo(x, y - 50, 1, 82, OURO_ESC, 80 * (560 - x) / 464);
            retangulo(90, y - 50, 6, 82, OURO, 150 + (int)(105 * pulso));
        }
        texto_sombra(120, y, s ? 48 : 42, s ? OURO : CINZA, s ? 255 : 200, jogos[i].titulo);
        texto_sombra(122, y + 26, 20, ok ? (s ? BRANCO : CINZA) : VERMELHO, s ? 230 : 160,
                     ok ? jogos[i].sub : "arquivo não encontrado na pasta do launcher");
    }

    if (msg) texto_sombra(110, 600, 22, VERMELHO, 255, msg);

    int x = 110, yb = 676;
    x += texto_sombra(x, yb, 28, BRANCO, 255, "\xEE\x83\xA0");   // U+E0E0 botão A
    x += texto_sombra(x + 4, yb, 22, BRANCO, 230, " Jogar") + 44;
    x += texto_sombra(x, yb, 28, BRANCO, 255, "\xEE\x83\xAF");   // U+E0EF botão +
    texto_sombra(x + 4, yb, 22, BRANCO, 230, " Sair");

    // assinatura no canto inferior direito
    const char *ass = "Guto Recomp", *ver = "v1.0";
    FT_Set_Pixel_Sizes(f_txt, 0, 22);
    int wa = 0, wv = 0;
    { int sx = 0; const char *s = ass; while (*s) { u32 c = utf8(&s); if (!FT_Load_Char(f_txt, c, FT_LOAD_DEFAULT)) sx += f_txt->glyph->advance.x >> 6; } wa = sx; }
    FT_Set_Pixel_Sizes(f_txt, 0, 18);
    { int sx = 0; const char *s = ver; while (*s) { u32 c = utf8(&s); if (!FT_Load_Char(f_txt, c, FT_LOAD_DEFAULT)) sx += f_txt->glyph->advance.x >> 6; } wv = sx; }
    texto_sombra(W - 60 - wa - 12 - wv, yb, 22, OURO, 235, ass);
    texto_sombra(W - 60 - wv, yb, 18, CINZA, 220, ver);
}

static void carregar_capa(void) {
    bg = malloc(W * H * 4);
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

    int sel = ler_ultimo(), lancar = 0;
    const char *msg = envHasNextLoad() ? NULL : "Abra pelo hbmenu para poder iniciar os jogos.";
    u64 tick = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 k = padGetButtonsDown(&pad);
        if (k & HidNpadButton_Plus) break;
        if (k & HidNpadButton_AnyUp) sel = (sel + N - 1) % N;
        if (k & HidNpadButton_AnyDown) sel = (sel + 1) % N;
        if (k & HidNpadButton_A) {
            char p[FS_MAX_PATH]; caminho(p, sizeof p, sel);
            if (!existe(p)) msg = "Arquivo do jogo não encontrado na pasta do launcher.";
            else if (envHasNextLoad()) {
                char args[FS_MAX_PATH + 4]; snprintf(args, sizeof args, "\"%.760s\"", p);
                if (R_SUCCEEDED(envSetNextLoad(p, args))) { gravar_ultimo(sel); lancar = 1; }
                else msg = "Falha ao preparar o jogo escolhido.";
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
