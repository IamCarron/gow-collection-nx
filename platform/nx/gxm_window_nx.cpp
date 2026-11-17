// Janela no Switch: a NWindow padrão (1280x720 portátil, 1920x1080 na base). Sem mouse, sem bordas.
#include "gxm_window.h"
#include "nx_platform.h"

#include <switch.h>

#include <atomic>
#include <cstdio>

namespace gvk::win {
namespace {
bool g_created = false;
std::atomic<bool> g_resized{false};
AppletOperationMode g_mode = AppletOperationMode_Handheld;
} // namespace

bool create(const wchar_t *) {
    NWindow *w = nwindowGetDefault();
    g_mode = appletGetOperationMode();
    const u32 width = g_mode == AppletOperationMode_Console ? 1920 : 1280;
    const u32 height = g_mode == AppletOperationMode_Console ? 1080 : 720;
    nwindowSetDimensions(w, width, height);
    g_created = true;
    return true;
}

void *hwnd() { return nwindowGetDefault(); }

void pump() {
    if (!nxp::applet_loop()) return;
    const AppletOperationMode m = appletGetOperationMode();
    if (m != g_mode) { // encaixou/desencaixou: refaz a swapchain no tamanho novo
        g_mode = m;
        create(nullptr);
        g_resized = true;
    }
}

bool take_resized() { return g_resized.exchange(false); }
bool take_capture() { return false; }
void request_close() { nxp::request_exit(); }
void mark_resized() { g_resized = true; }
void presented() {}
bool closed() { return nxp::exit_requested(); }

bool rect(int &x, int &y, int &w, int &h) {
    if (!g_created) return false;
    x = y = 0;
    w = g_mode == AppletOperationMode_Console ? 1920 : 1280;
    h = g_mode == AppletOperationMode_Console ? 1080 : 720;
    return true;
}

bool pointer(float &, float &) { return false; }
bool focused() { return true; }
void notify(const std::string &) {}
void request_borderless(bool) {}
bool borderless() { return true; }

} // namespace gvk::win
