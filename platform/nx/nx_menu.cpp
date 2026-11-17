// Mod menu do Switch: R + Plus abre/fecha. Desenho na CPU (fonte do sistema pelo FreeType), copiado pela apresentação
// para a imagem da swapchain (gxm_vk.cpp). As mudanças vão para game_options.cpp pela ponte nxopt.
#include "nx_menu.h"
#include "vitart.h"

#include <switch.h>
#include <ft2build.h>
#include FT_FREETYPE_H

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

struct Item {
    int id; // nxopt::Id, -1 = fechar
    const char *nome;
};
const Item kItems[] = {
    {nxopt::RES, "Internal resolution"},
    {nxopt::FPS, "Frame rate"},          {nxopt::VSYNC, "VSync"},          {nxopt::COUNTER, "FPS counter"},
    {nxopt::TEX, "HD textures"},         {nxopt::HPBARS, "Enemy health bars"},
    {-1, "Close menu"},
};
constexpr int kN = sizeof kItems / sizeof kItems[0];

// resolução: ordem do menu -> código da opção (5 = 720p)
const uint32_t kRes[] = {1, 6, 5, 2, 3, 4};
const char *kResNome[] = {"1x (720x408)", "Vita (960x544)", "720p (1270x720)", "2x (1440x816)", "3x (2160x1224)", "4x (2880x1632)"};

int n_vals(int id) {
    switch (id) {
    case nxopt::RES: return 6;
    case nxopt::SHARP: return 11;
    case nxopt::FPS: return 3;
    default: return 2;
    }
}
// valor da opção -> posição na lista
int pos_of(int id, uint32_t v) {
    if (id == nxopt::RES) {
        for (int i = 0; i < 6; i++)
            if (kRes[i] == v) return i;
        return 0;
    }
    if (id == nxopt::FPS) return v >= 1 && v <= 3 ? (int)v - 1 : 0;
    return (int)v;
}
uint32_t val_of(int id, int p) {
    if (id == nxopt::RES) return kRes[p];
    if (id == nxopt::FPS) return (uint32_t)p + 1;
    return (uint32_t)p;
}
std::string val_text(int id, uint32_t v) {
    char b[48];
    switch (id) {
    case nxopt::RES: return kResNome[pos_of(id, v)];
    case nxopt::SHARP: std::snprintf(b, sizeof b, "%u", v); return b;
    case nxopt::FPS: return v == 3 ? "Max" : v == 2 ? "60" : "30";
    default: return v ? "On" : "Off";
    }
}
bool item_ok(int i) {
    const int id = kItems[i].id;
    if (id < 0) return true;
    if (id == nxopt::SHARP) return false;
    return nxopt::available(id);
}

void open() {
    for (int i = 0; i < nxopt::COUNT; i++) g_val[i] = nxopt::get(i);
    if (!item_ok(g_sel)) g_sel = 0;
    g_active = true, g_block = true, g_dirty = true;
    vt::logf("nxmenu: aberto");
}
void close() {
    g_active = false, g_dirty = true;
    vt::logf("nxmenu: fechado");
}
void change(int dir) {
    const int id = kItems[g_sel].id;
    if (id < 0) return;
    const int n = n_vals(id);
    int p = pos_of(id, g_val[id]) + dir;
    if (id == nxopt::SHARP) p = p < 0 ? 0 : p >= n ? n - 1 : p;
    else p = (p % n + n) % n;
    const uint32_t v = val_of(id, p);
    if (v == g_val[id]) return;
    g_val[id] = v;
    nxopt::set(id, v);
    g_dirty = true;
}
void move(int dir) {
    for (int k = 0; k < kN; k++) {
        g_sel = (g_sel + dir + kN) % kN;
        if (item_ok(g_sel)) break;
    }
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
    if (down & (HidNpadButton_B | HidNpadButton_Plus)) { close(); return; }
    if (down & HidNpadButton_A) {
        if (kItems[g_sel].id < 0) { close(); return; }
        change(+1);
    }
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

void draw(uint32_t screen_h) {
    const float k = screen_h / 720.f;
    auto S = [k](float v) { return (int)(v * k + 0.5f); };
    const int row = S(44), head = S(70), foot = S(44);
    g_w = (uint32_t)S(640), g_h = (uint32_t)(head + row * kN + foot + S(12));
    g_px.assign((size_t)g_w * g_h, 0xff14121au);
    const uint32_t gold = 0xffecb24au, white = 0xfff2f0f5u, grey = 0xff8a8794u, red = 0xff8e1b1bu, off = 0xff4a4752u;
    // moldura
    rect(0, 0, (int)g_w, S(3), gold);
    rect(0, (int)g_h - S(3), (int)g_w, S(3), gold);
    rect(0, 0, S(3), (int)g_h, gold);
    rect((int)g_w - S(3), 0, S(3), (int)g_h, gold);
    // título
    text(S(24), S(44), "NX MENU", S(30), gold);
#ifdef GOW_GAME_GOW2
    const char *jogo = "God of War II";
#else
    const char *jogo = "God of War";
#endif
    text((int)g_w - S(24) - text_w(jogo, S(20)), S(42), jogo, S(20), grey);
    rect(S(20), head - S(10), (int)g_w - S(40), S(2), off);
    for (int i = 0; i < kN; i++) {
        const int y = head + row * i;
        const bool ok = item_ok(i), sel = i == g_sel;
        if (sel) {
            rect(S(12), y, (int)g_w - S(24), row - S(4), red);
            rect(S(12), y, S(5), row - S(4), gold);
        }
        const uint32_t tc = !ok ? off : sel ? white : 0xffcfccd6u;
        const int base = y + row / 2 + S(8);
        text(S(30), base, kItems[i].nome, S(22), tc);
        const int id = kItems[i].id;
        if (id >= 0) {
            std::string v = ok ? val_text(id, g_val[id]) : "Unavailable";
            if (sel && ok) v = "<  " + v + "  >";
            const uint32_t vc = !ok ? off : sel ? gold : white;
            text((int)g_w - S(30) - text_w(v.c_str(), S(22)), base, v.c_str(), S(22), vc);
        }
    }
    const int fy = (int)g_h - foot;
    rect(S(20), fy, (int)g_w - S(40), S(2), off);
    const char *dica = "Up/Down: select     Left/Right: change     B: close";
    text(((int)g_w - text_w(dica, S(18))) / 2, fy + S(30), dica, S(18), grey);
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
