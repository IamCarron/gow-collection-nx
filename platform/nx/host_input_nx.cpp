// Controles no Switch (libnx pad): Joy-Con, Pro Controller e o portátil, botões pela posição como no Vita
// (B embaixo = cruz, A = círculo, Y = quadrado, X = triângulo). Mesmo mapeamento do XInput no PC:
// L/R = L1/R1 (0x400/0x800), ZL/ZR = L2/R2 (0x100/0x200).
#include "host_input.h"
#include "vitart.h"
#include "nx_menu.h"

#include <switch.h>

#include <cstdlib>
#include <mutex>

namespace hostin {
namespace {
constexpr uint32_t SELECT = 0x1, L3 = 0x2, R3 = 0x4, START = 0x8, UP = 0x10, RIGHT = 0x20, DOWN = 0x40, LEFT = 0x80,
                   L2 = 0x100, R2 = 0x200, L1 = 0x400, R1 = 0x800, TRIANGLE = 0x1000, CIRCLE = 0x2000, CROSS = 0x4000,
                   SQUARE = 0x8000;
std::mutex g_m;
PadState g_pad;
bool g_init = false;

uint8_t ax(s32 v) { return (uint8_t)((v + 32768) >> 8); }
uint8_t ay(s32 v) { return (uint8_t)(255 - ((v + 32768) >> 8)); } // libnx: cima positivo
void deadzone(uint8_t &x, uint8_t &y, int dz) {
    const int dx = x - 128, dy = y - 128;
    if (dx * dx + dy * dy < dz * dz) x = y = 128;
}
} // namespace

State poll() {
    std::lock_guard<std::mutex> g(g_m);
    if (!g_init) {
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&g_pad);
        g_init = true;
        vt::logf("vitart: controles pelo HID do Switch");
    }
    padUpdate(&g_pad);
    const u64 b = padGetButtons(&g_pad);
    State s;
    if (b & HidNpadButton_Up) s.buttons |= UP;
    if (b & HidNpadButton_Down) s.buttons |= DOWN;
    if (b & HidNpadButton_Left) s.buttons |= LEFT;
    if (b & HidNpadButton_Right) s.buttons |= RIGHT;
    if (b & HidNpadButton_Plus) s.buttons |= START;
    if (b & HidNpadButton_Minus) s.buttons |= SELECT;
    if (b & HidNpadButton_StickL) s.buttons |= L3;
    if (b & HidNpadButton_StickR) s.buttons |= R3;
    if (b & HidNpadButton_L) s.buttons |= L1;
    if (b & HidNpadButton_R) s.buttons |= R1;
    if (b & HidNpadButton_ZL) s.buttons |= L2;
    if (b & HidNpadButton_ZR) s.buttons |= R2;
    if (b & HidNpadButton_B) s.buttons |= CROSS;
    if (b & HidNpadButton_A) s.buttons |= CIRCLE;
    if (b & HidNpadButton_Y) s.buttons |= SQUARE;
    if (b & HidNpadButton_X) s.buttons |= TRIANGLE;
    const HidAnalogStickState l = padGetStickPos(&g_pad, 0), r = padGetStickPos(&g_pad, 1);
    s.lx = ax(l.x), s.ly = ay(l.y), s.rx = ax(r.x), s.ry = ay(r.y);
    deadzone(s.lx, s.ly, 20);
    deadzone(s.rx, s.ry, 20);
    s.active = 4; // ícones de botão do Nintendo
    // R + Plus é do mod menu (nx_menu.cpp): com R apertado o Plus não chega ao jogo; com o menu aberto (e até soltar
    // os botões depois de fechar) o jogo recebe o controle parado
    if (b & HidNpadButton_R) s.buttons &= ~START;
    if (nxmenu::input_blocked()) {
        State n;
        n.active = 4;
        return n;
    }
    return s;
}
} // namespace hostin
