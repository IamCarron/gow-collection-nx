// Mod menu do Switch (R + Plus): painel por cima do jogo com as opções de imagem e do jogo. Enquanto aberto, a
// apresentação repete o último quadro (o jogo fica parado esperando a thread Render) e o controle vai neutro para o
// jogo.
#pragma once
#include <cstdint>

namespace nxmenu {
bool active();        // painel na tela
bool input_blocked(); // aberto, ou fechado há pouco com botões ainda apertados
void update();        // a cada apresentação (thread Render): lê o controle, abre/fecha e navega
// Painel em BGRA para uma tela de altura screen_h; dirty = mudou desde a última chamada.
bool panel(uint32_t screen_h, const uint32_t *&px, uint32_t &w, uint32_t &h, bool &dirty);
} // namespace nxmenu

// Ponte com as opções (game_options.cpp)
namespace nxopt {
enum Id : int { RES, DRS, SHARP, FPS, VSYNC, COUNTER, TEX, QTE, HPBARS, CH_HP, CH_MP, CH_RAGE, COUNT };
uint32_t get(int id);
void set(int id, uint32_t v);
bool available(int id);
} // namespace nxopt
