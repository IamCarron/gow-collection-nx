// Mod menu do Switch: R + Plus abre/fecha; painel encostado à esquerda, abas com L/R. Desenho na CPU (fonte do sistema pelo FreeType), copiado pela apresentação
// para a imagem da swapchain (gxm_vk.cpp). As mudanças vão para game_options.cpp pela ponte nxopt.
#include "nx_menu.h"
#include "vitart.h"
#include "nx_platform.h"

#include <switch.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace nxmenu {
namespace {
std::atomic<bool> g_active{false}, g_block{false};
PadState g_pad;
bool g_pad_ok = false;
int g_sel = 0;
uint32_t g_val[nxopt::COUNT] = {};
bool g_dirty = true;
int g_hold = 0, g_hold_dir = 0;
int g_confirm = 0; // pop-up de confirmação: 1 = ligar o diagnóstico da GPU, 2 = sair do jogo
// idioma (opção LANG, a mesma do launcher: language= no vitart_options.ini): 0 português (Brasil), 1 inglês
bool en() { return g_val[nxopt::LANG] != 0; }
const char *L(const char *pt, const char *en_) { return en() ? en_ : pt; }

// Abas do menu (o mesmo visual do launcher: Gráficos / Mods / Cheats / Sistema)
enum { T_GFX, T_MODS, T_CHEATS, T_SYS, kTabs };
const char *const kTabNome[kTabs] = {"Gráficos", "Mods", "Cheats", "Sistema"};
const char *const kTabNomeEn[kTabs] = {"Graphics", "Mods", "Cheats", "System"};
const char *tab_nome(int t) { return en() ? kTabNomeEn[t] : kTabNome[t]; }
// ações que não são opções do jogo
enum { A_CLOSE = -1, A_PRESET = -2, A_RESET = -3, A_EXIT = -4 };

struct Item {
    int tab, id; // id: nxopt::Id, ou uma ação A_*
    const char *nome, *desc, *nome_en, *desc_en;
    const char *n() const { return en() ? nome_en : nome; }
    const char *d() const { return en() ? desc_en : desc; }
};
const Item kItems[] = {
    {T_GFX, A_PRESET, "Preset rápido", "Troca resolução interna e taxa de quadros de uma vez, sem reiniciar o jogo.",
     "Quick preset", "Changes internal resolution and frame rate at once, without restarting the game."},
    {T_GFX, nxopt::RES, "Resolução (portátil)", "Resolução de renderização 3D antes da ampliação até a tela.",
     "Resolution (handheld)", "3D render resolution before it is upscaled to the screen."},
    {T_GFX, nxopt::RES_DOCK, "Resolução (na TV)", "Resolução usada com o console na base. \"Igual ao portátil\" usa a de cima.",
     "Resolution (TV)", "Resolution used while docked. \"Same as handheld\" uses the one above."},
    {T_GFX, nxopt::UPSCALE, "Upscaler", "Como a imagem é ampliada até a tela. FSR 1.0 reconstrói bordas e aplica nitidez.",
     "Upscaler", "How the image is scaled to the screen. FSR 1.0 rebuilds edges and sharpens."},
    {T_GFX, nxopt::SHARP, "Nitidez (RCAS)", "Realce de nitidez depois da ampliação. Com FSR e nitidez Off, usa o padrão da AMD.",
     "Sharpness (RCAS)", "Sharpening after upscaling. With FSR and sharpness Off, AMD's default is used."},
    {T_GFX, nxopt::AA, "Anti-aliasing", "FXAA suaviza os serrilhados das bordas com custo baixo.",
     "Anti-aliasing", "FXAA smooths jagged edges at a low cost."},
    {T_GFX, nxopt::ANISO, "Filtro anisotrópico", "Deixa texturas de chão e paredes nítidas quando vistas de lado.",
     "Anisotropic filtering", "Keeps floor and wall textures sharp at steep angles."},
    {T_GFX, nxopt::GAMMA, "Brilho", "Ajuste de gama da imagem final.", "Brightness", "Gamma of the final image."},
    {T_GFX, nxopt::SAT, "Saturação", "Intensidade das cores.", "Saturation", "Color intensity."},
    {T_GFX, nxopt::VIB, "Vibrância", "Realça as cores mais apagadas sem estourar as já fortes.",
     "Vibrance", "Boosts dull colors without blowing out strong ones."},
    {T_GFX, nxopt::DITHER, "Debanding", "Disfarça faixas em degradês (céu, névoa, sombras).",
     "Debanding", "Hides banding in gradients (sky, fog, shadows)."},
    {T_GFX, nxopt::FGEN, "Geração de quadros", "Cria quadros intermediários (LSFG) compilados no deko3d.",
     "Frame generation", "Creates in-between frames (LSFG) compiled for deko3d."},
    {T_GFX, nxopt::FGFLOW, "Fluxo da geração", "Resolução do fluxo óptico da geração de quadros. Menor é mais leve.",
     "Generation flow", "Optical flow resolution for frame generation. Lower is lighter."},
    {T_MODS, nxopt::TEX, "Texturas HD", "Carrega o pacote de texturas remasterizadas em data/texpack.",
     "HD textures", "Loads the remastered texture pack from data/texpack."},
    {T_MODS, nxopt::HPBARS, "Barras de vida dos inimigos", "Mostra a vida que resta em cada inimigo.",
     "Enemy health bars", "Shows the health left on each enemy."},
    {T_MODS, nxopt::QTE, "QTE automático", "Os comandos rápidos (botões na tela) são feitos sozinhos.",
     "Auto QTE", "Quick-time events (on-screen buttons) are done for you."},
    {T_CHEATS, nxopt::CH_HP, "Vida infinita", "Kratos não perde vida com ataques de inimigos ou armadilhas.",
     "Infinite health", "Kratos takes no damage from enemies or traps."},
    {T_CHEATS, nxopt::CH_MP, "Magia infinita", "Mantém a barra de magia sempre cheia ao usar os poderes dos deuses.",
     "Infinite magic", "Keeps the magic bar full when using the gods' powers."},
    {T_CHEATS, nxopt::CH_RAGE, "Fúria infinita", "A Fúria dos Deuses / dos Titãs fica ligada sem gastar a barra.",
     "Infinite rage", "Rage of the Gods / Titans stays on without draining the bar."},
    {T_SYS, nxopt::LANG, "Idioma", "Idioma do mod menu e do launcher.", "Language", "Language of the mod menu and launcher."},
    {T_SYS, nxopt::FPS, "Taxa de quadros", "Limite de quadros por segundo. Máximo usa a taxa da tela sem acelerar o jogo.",
     "Frame rate", "Frames per second cap. Max uses the screen rate without speeding up the game."},
    {T_SYS, nxopt::VSYNC, "VSync", "Sincroniza com a tela para evitar cortes na imagem.",
     "VSync", "Syncs with the screen to avoid tearing."},
    {T_SYS, nxopt::COUNTER, "Contador de FPS", "Mostra os quadros por segundo no canto da tela.",
     "FPS counter", "Shows frames per second in the corner of the screen."},
    {T_SYS, nxopt::GPUDIAG, "Diagnóstico da GPU (log)", "Mede cada desenho da GPU e grava no log. Deixa o jogo bem mais lento.",
     "GPU diagnostics (log)", "Times every GPU draw and writes it to the log. Makes the game much slower."},
    {T_SYS, A_RESET, "Restaurar padrões", "Volta imagem e cor para o padrão (resolução e quadros ficam como estão).",
     "Restore defaults", "Resets image and color settings (resolution and frame rate stay as they are)."},
    {T_SYS, A_CLOSE, "Fechar menu", "Volta para o jogo. Também fecha com B ou R + Plus.",
     "Close menu", "Back to the game. B or R + Plus also closes it."},
    {T_SYS, A_EXIT, "Sair do jogo", "Fecha o jogo. O progresso que não foi salvo no jogo se perde.",
     "Exit game", "Closes the game. Progress not saved in the game is lost."},
};
constexpr int kN = sizeof kItems / sizeof kItems[0];
int g_tab = 0;

// presets (os mesmos do launcher): resolução + taxa de quadros
struct Preset {
    const char *nome, *nome_en;
    uint32_t res, fps;
};
const Preset kPresets[] = {{"Desempenho", "Performance", 6, 2}, {"Equilibrado", "Balanced", 5, 2}, {"Qualidade", "Quality", 2, 1}};
constexpr int kNPre = 3;
int preset_atual() {
    for (int i = 0; i < kNPre; i++)
        if (g_val[nxopt::RES] == kPresets[i].res && g_val[nxopt::FPS] == kPresets[i].fps) return i;
    return -1;
}

// resolução: ordem do menu -> código da opção (5 = 720p)
const uint32_t kRes[] = {1, 6, 5, 2, 3, 4};
const char *kResNome[] = {"1x (720x408)", "Vita (960x544)", "720p (1270x720)", "2x (1440x816)", "3x (2160x1224)", "4x (2880x1632)"};

int n_vals(int id) {
    switch (id) {
    case nxopt::RES: return 6;
    case nxopt::RES_DOCK: return 7;
    case nxopt::ANISO: return 5;
    case nxopt::GAMMA: return 9;
    case nxopt::SAT: return 9;
    case nxopt::VIB: return 5;
    case nxopt::SHARP: return 11;
    case nxopt::FPS: return 3;
    case nxopt::FGEN: return 3;
    case nxopt::FGFLOW: return 4;
    default: return 2;
    }
}
// liga/desliga (desenhado como chave)
bool is_toggle(int id) { return id >= 0 && id != nxopt::LANG && id != nxopt::AA && id != nxopt::UPSCALE && n_vals(id) == 2; }
// opção com barrinha de nível
bool is_level(int id) { return id == nxopt::SHARP || id == nxopt::GAMMA || id == nxopt::SAT || id == nxopt::VIB || id == nxopt::ANISO; }
// valor da opção -> posição na lista
int pos_of(int id, uint32_t v) {
    if (id == nxopt::RES) {
        for (int i = 0; i < 6; i++)
            if (kRes[i] == v) return i;
        return 0;
    }
    if (id == nxopt::RES_DOCK) {
        for (int i = 0; i < 6; i++)
            if (kRes[i] == v) return i + 1;
        return 0;
    }
    if (id == nxopt::ANISO) return v >= 16 ? 4 : v >= 8 ? 3 : v >= 4 ? 2 : v >= 2 ? 1 : 0;
    if (id == nxopt::FPS) return v >= 1 && v <= 3 ? (int)v - 1 : 0;
    if (id == nxopt::FGFLOW) return v >= 1 && v <= 4 ? (int)v - 1 : 1;
    return (int)v;
}
uint32_t val_of(int id, int p) {
    if (id == nxopt::RES) return kRes[p];
    if (id == nxopt::RES_DOCK) return p ? kRes[p - 1] : 0;
    if (id == nxopt::ANISO) return 1u << p;
    if (id == nxopt::FPS) return (uint32_t)p + 1;
    if (id == nxopt::FGFLOW) return (uint32_t)p + 1;
    return (uint32_t)p;
}
std::string val_text(int id, uint32_t v) {
    char b[48];
    switch (id) {
    case A_PRESET: {
        const int p = preset_atual();
        return p >= 0 ? L(kPresets[p].nome, kPresets[p].nome_en) : L("Personalizado", "Custom");
    }
    case nxopt::RES: return kResNome[pos_of(id, v)];
    case nxopt::RES_DOCK: return v ? kResNome[pos_of(id, v) - 1] : L("Igual ao portátil", "Same as handheld");
    case nxopt::SHARP:
        if (!v) return "Off";
        std::snprintf(b, sizeof b, "%u", v);
        return b;
    case nxopt::UPSCALE: return v ? "FSR 1.0" : "Bilinear";
    case nxopt::AA: return v ? "FXAA" : "Off";
    case nxopt::ANISO:
        if (v <= 1) return "Off";
        std::snprintf(b, sizeof b, "%ux", v);
        return b;
    case nxopt::GAMMA:
    case nxopt::SAT:
        if (v == 4) return L("Padrão", "Default");
        std::snprintf(b, sizeof b, "%+d", (int)v - 4);
        return b;
    case nxopt::VIB:
        if (!v) return "Off";
        std::snprintf(b, sizeof b, "%u", v);
        return b;
    case nxopt::LANG: return v ? "English" : "Português (BR)";
    case nxopt::FPS: return v == 3 ? L("Máximo", "Max") : v == 2 ? "60" : "30";
    case nxopt::FGEN: return v == 2 ? L("Qualidade", "Quality") : v == 1 ? L("Desempenho", "Performance") : "Off";
    case nxopt::FGFLOW: return v == 1 ? L("Inteiro (pesado)", "Full (heavy)") : v == 2 ? "1/2" : v == 3 ? "1/3" : L("1/4 (leve)", "1/4 (light)");
    default: return v ? "ON" : "OFF";
    }
}
bool item_ok(int i) {
    const int id = kItems[i].id;
    if (id < 0) return true;
    return nxopt::available(id);
}
// itens da aba atual (índices de kItems)
int tab_items(int *out) {
    int n = 0;
    for (int i = 0; i < kN; i++)
        if (kItems[i].tab == g_tab) out[n++] = i;
    return n;
}

void open() {
    for (int i = 0; i < nxopt::COUNT; i++) g_val[i] = nxopt::get(i);
    int l[kN];
    const int n = tab_items(l);
    if (g_sel >= n || !item_ok(l[g_sel])) g_sel = 0;
    g_hold = g_hold_dir = 0;
    g_active = true, g_block = true, g_dirty = true;
    vt::logf("nxmenu: aberto");
}
void close() {
    g_active = false, g_dirty = true;
    vt::logf("nxmenu: fechado");
}
void set_val(int id, uint32_t v) {
    if (v == g_val[id] || !nxopt::available(id)) return;
    g_val[id] = v;
    nxopt::set(id, v);
}
void change(int dir) {
    int l[kN];
    const int n = tab_items(l);
    if (g_sel >= n) return;
    const int id = kItems[l[g_sel]].id;
    if (id == A_CLOSE) {
        if (dir > 0) close();
        return;
    }
    if (id == A_EXIT) {
        if (dir > 0) g_confirm = 2, g_dirty = true;
        return;
    }
    if (id == A_RESET) {
        if (dir <= 0) return;
        set_val(nxopt::UPSCALE, 0), set_val(nxopt::SHARP, 0), set_val(nxopt::AA, 0), set_val(nxopt::ANISO, 1);
        set_val(nxopt::GAMMA, 4), set_val(nxopt::SAT, 4), set_val(nxopt::VIB, 0), set_val(nxopt::DITHER, 0);
        g_dirty = true;
        return;
    }
    if (id == A_PRESET) {
        int p = preset_atual();
        p = p < 0 ? (dir > 0 ? 0 : kNPre - 1) : (p + dir + kNPre) % kNPre;
        set_val(nxopt::RES, kPresets[p].res), set_val(nxopt::FPS, kPresets[p].fps);
        g_dirty = true;
        return;
    }
    const int nv = n_vals(id);
    int p = pos_of(id, g_val[id]) + dir;
    if (is_level(id)) p = p < 0 ? 0 : p >= nv ? nv - 1 : p;
    else p = (p % nv + nv) % nv;
    const uint32_t v = val_of(id, p);
    if (v == g_val[id]) return;
    if (id == nxopt::GPUDIAG && v) { g_confirm = 1, g_dirty = true; return; } // pede confirmação primeiro
    g_val[id] = v;
    nxopt::set(id, v);
    g_dirty = true;
}
void move(int dir) {
    int l[kN];
    const int n = tab_items(l);
    for (int k = 0; k < n; k++) {
        g_sel = (g_sel + dir + n) % n;
        if (item_ok(l[g_sel])) break;
    }
    g_dirty = true;
}
void tab(int dir) {
    g_tab = (g_tab + dir + kTabs) % kTabs, g_sel = 0;
    int l[kN];
    const int n = tab_items(l);
    if (n && !item_ok(l[0])) move(1);
    g_dirty = true;
}
} // namespace

bool active() { return g_active; }
bool input_blocked() { return g_block; }

void update() {
    if (!g_pad_ok) {
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&g_pad);
        g_pad_ok = true;
    }
    padUpdate(&g_pad);
    const u64 held = padGetButtons(&g_pad), down = padGetButtonsDown(&g_pad);
    if (!g_active) {
        if ((held & HidNpadButton_R) && (down & HidNpadButton_Plus)) open();
        else if (g_block && !(held & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Plus | HidNpadButton_R | HidNpadButton_AnyUp |
                                      HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight)))
            g_block = false; // só devolve o controle ao jogo com os botões do menu soltos
        return;
    }
    if (g_confirm) {
        if (down & HidNpadButton_A) {
            if (g_confirm == 2) {
                g_confirm = 0;
                close();
                vt::logf("nxmenu: sair do jogo");
                nxp::request_exit();
                return;
            }
            g_confirm = 0, g_val[nxopt::GPUDIAG] = 1, g_dirty = true;
            nxopt::set(nxopt::GPUDIAG, 1);
        } else if (down & (HidNpadButton_B | HidNpadButton_Plus)) g_confirm = 0, g_dirty = true;
        return;
    }
    if (down & (HidNpadButton_B | HidNpadButton_Plus)) { close(); return; }
    if (down & HidNpadButton_A) change(+1);
    if (!g_active) return;
    if (down & (HidNpadButton_R | HidNpadButton_ZR)) tab(+1);
    if (down & (HidNpadButton_L | HidNpadButton_ZL)) tab(-1);
    if (down & HidNpadButton_AnyLeft) change(-1);
    if (down & HidNpadButton_AnyRight) change(+1);
    const int dir = (held & HidNpadButton_AnyUp) ? -1 : (held & HidNpadButton_AnyDown) ? 1 : 0;
    if (dir != g_hold_dir) g_hold_dir = dir, g_hold = 0;
    if (dir) {
        if (g_hold == 0 || (g_hold >= 18 && g_hold % 4 == 0)) move(dir);
        g_hold++;
    }
}

// ---------------------------------------------------------------- desenho
namespace {
FT_Library g_ft = nullptr;
FT_Face g_face = nullptr;
bool g_ft_tried = false;
std::vector<uint32_t> g_px;
uint32_t g_w = 0, g_h = 0, g_for_h = 0;

bool font() {
    if (g_face) return true;
    if (g_ft_tried) return false;
    g_ft_tried = true;
    if (R_FAILED(plInitialize(PlServiceType_User))) return false;
    PlFontData fd;
    if (R_FAILED(plGetSharedFontByType(&fd, PlSharedFontType_Standard))) return false;
    if (FT_Init_FreeType(&g_ft)) return false;
    if (FT_New_Memory_Face(g_ft, (const FT_Byte *)fd.address, fd.size, 0, &g_face)) { g_face = nullptr; return false; }
    return true;
}

uint32_t utf8(const char *&s) {
    const uint8_t c = (uint8_t)*s++;
    if (c < 0x80) return c;
    int n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
    uint32_t cp = c & (0x3f >> n);
    while (n-- && (*s & 0xc0) == 0x80) cp = cp << 6 | ((uint8_t)*s++ & 0x3f);
    return cp;
}

void rect(int x, int y, int w, int h, uint32_t c) {
    for (int j = y < 0 ? 0 : y; j < y + h && j < (int)g_h; j++)
        for (int i = x < 0 ? 0 : x; i < x + w && i < (int)g_w; i++) g_px[(size_t)j * g_w + i] = c;
}

uint32_t mix(uint32_t d, uint32_t c, uint32_t a) {
    uint32_t r = 0xff000000u;
    for (int sh = 0; sh < 24; sh += 8) {
        const uint32_t dc = d >> sh & 0xff, cc = c >> sh & 0xff;
        r |= ((dc * (255 - a) + cc * a) / 255) << sh;
    }
    return r;
}

// largura do texto em pixels
int text_w(const char *s, int size) {
    if (!font()) return (int)std::strlen(s) * size / 2;
    FT_Set_Pixel_Sizes(g_face, 0, size);
    int w = 0;
    while (*s) {
        const uint32_t cp = utf8(s);
        if (!FT_Load_Char(g_face, cp, FT_LOAD_DEFAULT)) w += g_face->glyph->advance.x >> 6;
    }
    return w;
}

// y = linha de base
void text(int x, int y, const char *s, int size, uint32_t c) {
    if (!font()) return;
    FT_Set_Pixel_Sizes(g_face, 0, size);
    while (*s) {
        const uint32_t cp = utf8(s);
        if (FT_Load_Char(g_face, cp, FT_LOAD_RENDER)) continue;
        const FT_GlyphSlot g = g_face->glyph;
        const FT_Bitmap &bm = g->bitmap;
        for (unsigned j = 0; j < bm.rows; j++) {
            const int py = y - g->bitmap_top + (int)j;
            if (py < 0 || py >= (int)g_h) continue;
            for (unsigned i = 0; i < bm.width; i++) {
                const int px = x + g->bitmap_left + (int)i;
                if (px < 0 || px >= (int)g_w) continue;
                const uint32_t a = bm.buffer[j * bm.pitch + i];
                if (a) {
                    uint32_t &d = g_px[(size_t)py * g_w + px];
                    d = mix(d, c, a);
                }
            }
        }
        x += g->advance.x >> 6;
    }
}

// retângulo com cantos arredondados (raio r), misturado com alfa a
void rrect(int x, int y, int w, int h, int r, uint32_t c, uint32_t a) {
    for (int j = 0; j < h; j++) {
        const int py = y + j;
        if (py < 0 || py >= (int)g_h) continue;
        for (int i = 0; i < w; i++) {
            const int px = x + i;
            if (px < 0 || px >= (int)g_w) continue;
            const int dx = i < r ? r - i : i >= w - r ? i - (w - r - 1) : 0, dy = j < r ? r - j : j >= h - r ? j - (h - r - 1) : 0;
            if (dx * dx + dy * dy > r * r) continue;
            uint32_t &d = g_px[(size_t)py * g_w + px];
            d = mix(d, c, a);
        }
    }
}
void blend(int x, int y, int w, int h, uint32_t c, uint32_t a) { rrect(x, y, w, h, 0, c, a); }
void disc(int cx, int cy, int r, uint32_t c) {
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            const float d = std::sqrt((float)(i * i + j * j));
            const float k = r + 0.5f - d;
            if (k <= 0) continue;
            const int px = cx + i, py = cy + j;
            if (px < 0 || py < 0 || px >= (int)g_w || py >= (int)g_h) continue;
            uint32_t &p = g_px[(size_t)py * g_w + px];
            p = mix(p, c, (uint32_t)(255 * (k > 1 ? 1 : k)));
        }
}
void ring(int cx, int cy, int r, uint32_t c) {
    for (int j = -r - 2; j <= r + 2; j++)
        for (int i = -r - 2; i <= r + 2; i++) {
            const float d = std::sqrt((float)(i * i + j * j)), k = 1.2f - std::fabs(d - r);
            if (k <= 0) continue;
            const int px = cx + i, py = cy + j;
            if (px < 0 || py < 0 || px >= (int)g_w || py >= (int)g_h) continue;
            uint32_t &p = g_px[(size_t)py * g_w + px];
            p = mix(p, c, (uint32_t)(255 * (k > 1 ? 1 : k)));
        }
}

// cores (0xAARRGGBB)
constexpr uint32_t kGold = 0xffe5c158u, kGoldMid = 0xffc9a24du, kGoldDark = 0xff8a6a2cu, kWhite = 0xfff3ede2u,
                   kText = 0xffcfc6b8u, kGrey = 0xff9e9484u, kDim = 0xff5c554bu, kRed = 0xff991b1bu, kInk = 0xff140f07u;

// texto cortado em maxw pixels
void text_fit(int x, int y, const char *s, int size, uint32_t c, int maxw) {
    if (text_w(s, size) <= maxw) return text(x, y, s, size, c);
    std::string t = s;
    while (!t.empty()) {
        do t.pop_back();
        while (!t.empty() && ((uint8_t)t.back() & 0xc0) == 0x80);
        if (!t.empty()) t.pop_back();
        if (text_w((t + "…").c_str(), size) <= maxw) break;
    }
    text(x, y, (t + "…").c_str(), size, c);
}

// quebra a descrição em até 2 linhas
void text_wrap(int x, int y, const char *s, int size, uint32_t c, int maxw, int lh) {
    std::string linha, resto = s;
    for (int l = 0; l < 2 && !resto.empty(); l++) {
        size_t corte = resto.size();
        if (text_w(resto.c_str(), size) > maxw) {
            corte = 0;
            for (size_t i = 0; i < resto.size(); i++)
                if (resto[i] == ' ' && text_w(resto.substr(0, i).c_str(), size) <= maxw) corte = i;
            if (!corte || l == 1) {
                text_fit(x, y + l * lh, resto.c_str(), size, c, maxw);
                return;
            }
        }
        text(x, y + l * lh, resto.substr(0, corte).c_str(), size, c);
        resto = corte < resto.size() ? resto.substr(corte + 1) : "";
    }
}

// botão redondo do controle ("A", "B", "L"...) com o texto ao lado; devolve o x seguinte
int hint(int x, int base, int S8, const char *bt, bool cheio, const char *txt, int size) {
    const int r = S8;
    if (cheio) disc(x + r, base - size / 3, r, bt[0] == 'B' ? kWhite : kGold);
    else {
        disc(x + r, base - size / 3, r, kGoldDark);
        disc(x + r, base - size / 3, r - 1, 0xff2a241eu);
    }
    const int bw = text_w(bt, size - 2);
    text(x + r - bw / 2, base - size / 3 + (size - 2) * 7 / 20, bt, size - 2, cheio ? kInk : kGold);
    return x + 2 * r + S8 / 2;
}

void draw(uint32_t screen_h) {
    const float k = screen_h / 720.f;
    auto S = [k](float v) { return (int)(v * k + 0.5f); };
    const int margem = S(16);
    g_w = (uint32_t)S(470), g_h = (uint32_t)(screen_h - 2 * margem);
    // fundo: obsidiana com degradê vertical
    g_px.resize((size_t)g_w * g_h);
    for (uint32_t y = 0; y < g_h; y++) {
        const uint32_t c = mix(0xff120e0cu, 0xff0b0908u, y * 255 / g_h);
        for (uint32_t x = 0; x < g_w; x++) g_px[(size_t)y * g_w + x] = c;
    }
    // moldura dourada fina + filete vermelho/dourado no topo
    blend(0, 0, (int)g_w, 1, kGoldMid, 90), blend(0, (int)g_h - 1, (int)g_w, 1, kGoldMid, 90);
    blend(0, 0, 1, (int)g_h, kGoldMid, 90), blend((int)g_w - 1, 0, 1, (int)g_h, kGoldMid, 90);
    for (int x = 0; x < (int)g_w; x++) {
        const float t = (float)x / g_w;
        const uint32_t c = t < .5f ? mix(kRed, kGold, (uint32_t)(t * 2 * 255)) : kGold;
        const uint32_t a = t < .5f ? 255 : (uint32_t)(255 * (1 - (t - .5f) * 2));
        blend(x, 0, 1, S(2), c, a);
    }

    // cabeçalho: brasão Ω, título e combinação para abrir
    const int hy = S(16), hh = S(52);
    disc(S(30), hy + hh / 2, S(13), 0xff4c1414u);
    ring(S(30), hy + hh / 2, S(13), kGoldMid);
    const int ow = text_w("Ω", S(16));
    text(S(30) - ow / 2, hy + hh / 2 + S(6), "Ω", S(16), kGold);
    text(S(54), hy + S(24), "NX MOD MENU", S(17), kGold);
#ifdef GOW_GAME_GOW2
    const char *jogo = "God of War II · deko3d";
#else
    const char *jogo = "God of War · deko3d";
#endif
    text(S(54), hy + S(44), jogo, S(13), kGrey);
    const char *combo = "R + Plus";
    const int cw = text_w(combo, S(13));
    rrect((int)g_w - S(18) - cw - S(16), hy + S(14), cw + S(16), S(24), S(5), 0xffffffffu, 14);
    text((int)g_w - S(18) - cw - S(8), hy + S(31), combo, S(13), 0xffc7bfb2u);
    const int ty = hy + hh + S(8);
    blend(0, ty - 1, (int)g_w, 1, kGoldMid, 50);

    // abas
    const int th = S(38), tw = (int)g_w / kTabs;
    blend(0, ty, (int)g_w, th, 0xff000000u, 77);
    for (int t = 0; t < kTabs; t++) {
        const bool on = t == g_tab;
        const int x = t * tw, w = text_w(tab_nome(t), S(15));
        if (on) blend(x, ty, tw, th, kGoldMid, 26), blend(x, ty + th - S(2), tw, S(2), kGold, 255);
        text(x + (tw - w) / 2, ty + th / 2 + S(6), tab_nome(t), S(15), on ? kGold : 0xff8e8577u);
    }
    blend(0, ty + th, (int)g_w, 1, kGoldMid, 38);

    // lista
    int l[kN];
    const int n = tab_items(l);
    const int foot = S(116), ly = ty + th + S(8), row = S(42);
    const int vis = std::max(1, ((int)g_h - foot - ly) / row);
    const int first = std::max(0, std::min(g_sel - vis / 2, n - vis));
    for (int r = first; r < n && r < first + vis; r++) {
        const int i = l[r], id = kItems[i].id, y = ly + (r - first) * row;
        const bool ok = item_ok(i), sel = r == g_sel;
        if (sel) {
            for (int x = 0; x < (int)g_w; x++) {
                const float t = (float)x / g_w;
                blend(x, y, 1, row, mix(kGoldMid, kRed, (uint32_t)(t * 255)), (uint32_t)(41 - 5 * t));
            }
            blend(0, y, S(3), row, kGold, 255);
        } else if (r != first) blend(S(16), y, (int)g_w - S(32), 1, 0xffffffffu, 10);
        const int base = y + row / 2 + S(6);
        const uint32_t tc = !ok ? kDim : sel ? kWhite : kText;
        const int xr = (int)g_w - S(18);
        if (id == A_CLOSE || id == A_RESET || id == A_EXIT) {   // ação: selo "Executar"
            const char *e = id == A_CLOSE ? L("Fechar", "Close") : id == A_EXIT ? L("Sair", "Exit") : L("Executar", "Run");
            const int ew = text_w(e, S(13));
            rrect(xr - ew - S(16), y + row / 2 - S(11), ew + S(16), S(22), S(4), kGoldMid, 51);
            text(xr - ew - S(8), base - S(1), e, S(13), kGold);
            text_fit(S(18), base, kItems[i].n(), S(16), tc, xr - ew - S(40) - S(18));
            continue;
        }
        if (id >= 0 && !ok) {
            const char *u = L("Indisponível", "Unavailable");
            text(xr - text_w(u, S(13)), base, u, S(13), kDim);
            text_fit(S(18), base, kItems[i].n(), S(16), tc, (int)g_w - S(150));
            continue;
        }
        const uint32_t v = id >= 0 ? g_val[id] : 0;
        int wv = 0;
        if (is_toggle(id)) {   // chave ON/OFF
            const bool on = v != 0;
            const char *t = on ? "ON" : "OFF";
            const int lw = text_w(t, S(12)), sw = S(28), sh = S(16);
            const int sx = xr - lw - S(6) - sw, sy = y + (row - sh) / 2;
            rrect(sx, sy, sw, sh, sh / 2, on ? 0xffb88c34u : 0xffffffffu, on ? 255 : 26);
            disc(on ? sx + sw - sh / 2 : sx + sh / 2, sy + sh / 2, sh / 2 - S(2), on ? kInk : 0xff8e8577u);
            text(xr - lw, base - S(1), t, S(12), on ? kGold : 0xff7c7365u);
            wv = lw + S(6) + sw;
        } else {               // ‹ valor ›
            const std::string t = val_text(id, v);
            const int vw = text_w(t.c_str(), S(14)), aw = text_w("›", S(14));
            text(xr - aw, base, "›", S(14), 0xff8e8577u);
            text(xr - aw - S(6) - vw, base, t.c_str(), S(14), kGold);
            text(xr - aw - S(12) - vw - aw, base, "‹", S(14), 0xff8e8577u);
            wv = vw + 2 * aw + S(12);
            if (is_level(id)) {   // barrinha de nível
                const int bw = S(56), bx = xr - wv - S(10) - bw, by = y + row / 2 - S(1);
                const int nv = n_vals(id), p = pos_of(id, v);
                blend(bx, by, bw, S(3), 0xffffffffu, 26);
                blend(bx, by, bw * p / (nv - 1), S(3), sel ? kGold : kGoldDark, 255);
                wv += bw + S(10);
            }
        }
        text_fit(S(18), base, kItems[i].n(), S(16), tc, xr - wv - S(14) - S(18));
    }
    auto seta = [&](int cy, bool cima) { // triângulo pequeno: tem mais itens acima/abaixo
        for (int j = 0; j < S(5); j++) blend((int)g_w / 2 - j, cima ? cy + j : cy - j, 2 * j + 1, 1, kGrey, 255);
    };
    if (first > 0) seta(ly - S(6), true);
    if (first + vis < n) seta(ly + vis * row + S(5), false);

    // descrição + dicas do controle
    const int fy = (int)g_h - foot;
    blend(0, fy, (int)g_w, foot, 0xff000000u, 140);
    blend(0, fy, (int)g_w, 1, kGoldMid, 51);
    if (g_sel < n) text_wrap(S(18), fy + S(26), kItems[l[g_sel]].d(), S(14), 0xffb8ae9eu, (int)g_w - S(36), S(19));
    const int hy2 = (int)g_h - S(22);
    blend(S(18), hy2 - S(26), (int)g_w - S(36), 1, 0xffffffffu, 13);
    int x = S(18);
    x = hint(x, hy2, S(9), "A", true, nullptr, S(14));
    const char *alt = L("Alterar", "Change");
    text(x, hy2, alt, S(14), 0xffa89f91u);
    x += text_w(alt, S(14)) + S(16);
    x = hint(x, hy2, S(9), "L", false, nullptr, S(14));
    x = hint(x - S(2), hy2, S(9), "R", false, nullptr, S(14));
    text(x, hy2, L("Abas", "Tabs"), S(14), 0xffa89f91u);
    const char *fec = L("Fechar", "Close");
    const int fw = text_w(fec, S(14));
    const int bx = (int)g_w - S(18) - fw - S(24);
    hint(bx, hy2, S(9), "B", true, nullptr, S(14));
    text((int)g_w - S(18) - fw, hy2, fec, S(14), 0xffa89f91u);

    if (g_confirm) {
        for (auto &p : g_px) p = mix(p, 0xff000000u, 150); // escurece o menu atrás
        const int bw = (int)g_w - S(40), bh = S(230), bx2 = S(20), by = ((int)g_h - bh) / 2;
        rrect(bx2, by, bw, bh, S(10), 0xff1c1712u, 255);
        blend(bx2, by, bw, S(2), kGold, 255);
        const char *l2pt[] = {"AVISO", "O diagnóstico mede cada desenho da GPU", "e deixa o jogo bem mais lento.",
                              "Use só para gravar um log e depois desligue."};
        const char *l2en[] = {"WARNING", "Diagnostics time every GPU draw", "and make the game much slower.",
                              "Use it only to record a log, then turn it off."};
        const char *l3pt[] = {"SAIR DO JOGO", "O jogo vai fechar agora.", "O progresso que não foi salvo no jogo", "será perdido."};
        const char *l3en[] = {"EXIT GAME", "The game will close now.", "Progress not saved in the game", "will be lost."};
        const char *const *l2 = g_confirm == 2 ? (en() ? l3en : l3pt) : (en() ? l2en : l2pt);
        text(bx2 + (bw - text_w(l2[0], S(24))) / 2, by + S(44), l2[0], S(24), kGold);
        for (int i = 1; i < 4; i++) text(bx2 + (bw - text_w(l2[i], S(16))) / 2, by + S(64) + S(28) * i, l2[i], S(16), kWhite);
        const char *b = g_confirm == 2 ? L("A: sair     B: cancelar", "A: exit     B: cancel")
                                       : L("A: ligar mesmo assim     B: cancelar", "A: turn on anyway     B: cancel");
        text(bx2 + (bw - text_w(b, S(16))) / 2, by + bh - S(22), b, S(16), kGrey);
    }
}
} // namespace

bool panel(uint32_t screen_h, const uint32_t *&px, uint32_t &w, uint32_t &h, bool &dirty) {
    if (!g_active) return false;
    dirty = g_dirty || screen_h != g_for_h;
    if (dirty) {
        draw(screen_h);
        g_for_h = screen_h;
        g_dirty = false;
    }
    px = g_px.data(), w = g_w, h = g_h;
    return true;
}
} // namespace nxmenu

namespace nxmenu {
uint32_t panel_x(uint32_t screen_h) { return (uint32_t)(16.f * screen_h / 720.f + 0.5f); }
} // namespace nxmenu
