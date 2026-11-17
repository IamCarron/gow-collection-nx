// windows.h mínimo para o build do Switch: só o que o vitart e games/gow usam fora dos caminhos que já
// têm versão própria do Switch. A memória do guest é um bloco contínuo sempre confirmado
// (nx_platform.cpp), então VirtualQuery responde "confirmada, leitura e escrita" dentro dele.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sched.h>
#include <switch.h>

namespace vt { uint8_t *mem_base(); }

typedef uint32_t DWORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HWND;
typedef void *HMODULE;
typedef size_t SIZE_T;
typedef intptr_t LONG_PTR;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef WINAPI
#define WINAPI
#endif

#define MEM_COMMIT 0x1000
#define MEM_FREE 0x10000
#define MEM_RESERVE 0x2000
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_GUARD 0x100

struct MEMORY_BASIC_INFORMATION {
    void *BaseAddress;
    void *AllocationBase;
    DWORD AllocationProtect;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
};

inline SIZE_T VirtualQuery(const void *p, MEMORY_BASIC_INFORMATION *mi, SIZE_T) {
    const uint8_t *base = vt::mem_base();
    const uint8_t *lo = base + 0x80000000ull, *hi = base + 0xC0000000ull;
    const uint8_t *a = static_cast<const uint8_t *>(p);
    std::memset(mi, 0, sizeof *mi);
    if (a >= hi) return 0;
    const uintptr_t page = reinterpret_cast<uintptr_t>(a) & ~(uintptr_t)0xFFF;
    if (a < lo) { // abaixo do guest: livre até o começo do bloco
        mi->BaseAddress = reinterpret_cast<void *>(page);
        mi->RegionSize = static_cast<SIZE_T>(lo - reinterpret_cast<const uint8_t *>(page));
        mi->State = MEM_FREE;
        mi->Protect = PAGE_NOACCESS;
        return sizeof *mi;
    }
    mi->BaseAddress = reinterpret_cast<void *>(page);
    mi->AllocationBase = const_cast<uint8_t *>(lo);
    mi->RegionSize = static_cast<SIZE_T>(hi - reinterpret_cast<const uint8_t *>(page));
    mi->State = MEM_COMMIT;
    mi->Protect = mi->AllocationProtect = PAGE_READWRITE;
    return sizeof *mi;
}

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
inline int GetSystemMetrics(int i) { return i == SM_CXSCREEN ? 1280 : 720; }
inline short GetAsyncKeyState(int) { return 0; }
inline void Sleep(DWORD ms) { svcSleepThread((int64_t)ms * 1000000ll); }
inline DWORD GetTickCount() { return (DWORD)(armTicksToNs(armGetSystemTick()) / 1000000ull); }
inline DWORD GetLastError() { return 0; }
#define MB_ICONASTERISK 0x40
inline BOOL MessageBeep(unsigned) { return TRUE; }
#define SND_ASYNC 0x1
#define SND_FILENAME 0x20000
#define SND_NODEFAULT 0x2
inline BOOL PlaySoundA(const char *, void *, DWORD) { return TRUE; }
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_TIME_CRITICAL 15
inline HANDLE GetCurrentThread() { return nullptr; }
inline BOOL SetThreadPriority(HANDLE, int) { return TRUE; }
inline HMODULE LoadLibraryA(const char *) { return nullptr; }
// Códigos de tecla do Windows (só para as tabelas compilarem: no Switch não há teclado, GetAsyncKeyState dá 0).
enum : int {
    VK_LBUTTON = 0x01, VK_RBUTTON = 0x02, VK_MBUTTON = 0x04, VK_XBUTTON1 = 0x05, VK_XBUTTON2 = 0x06, VK_BACK = 0x08,
    VK_TAB = 0x09, VK_RETURN = 0x0D, VK_SHIFT = 0x10, VK_CONTROL = 0x11, VK_MENU = 0x12, VK_CAPITAL = 0x14,
    VK_ESCAPE = 0x1B, VK_SPACE = 0x20, VK_PRIOR = 0x21, VK_NEXT = 0x22, VK_END = 0x23, VK_HOME = 0x24, VK_LEFT = 0x25,
    VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28, VK_INSERT = 0x2D, VK_DELETE = 0x2E, VK_NUMPAD0 = 0x60,
    VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9,
    VK_F1 = 0x70, VK_LSHIFT = 0xA0, VK_RSHIFT = 0xA1, VK_LCONTROL = 0xA2, VK_RCONTROL = 0xA3, VK_LMENU = 0xA4,
    VK_RMENU = 0xA5, VK_OEM_1 = 0xBA, VK_OEM_PLUS = 0xBB, VK_OEM_COMMA = 0xBC, VK_OEM_MINUS = 0xBD,
    VK_OEM_PERIOD = 0xBE, VK_OEM_2 = 0xBF, VK_OEM_3 = 0xC0, VK_OEM_4 = 0xDB, VK_OEM_5 = 0xDC, VK_OEM_6 = 0xDD,
    VK_OEM_7 = 0xDE,
};
inline uint64_t GetTickCount64() { return armTicksToNs(armGetSystemTick()) / 1000000ull; }
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
struct DEVMODEW { uint16_t dmSize; DWORD dmDisplayFrequency; };
inline BOOL EnumDisplaySettingsW(const wchar_t *, DWORD, DEVMODEW *) { return FALSE; }
