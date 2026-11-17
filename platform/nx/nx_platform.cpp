// Camada do Switch: ver nx_platform.h.
#include "nx_platform.h"
#include "vitart.h"

#include <switch.h>
#include <pthread.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef GOW_GAME
#define GOW_GAME "gow1"
#endif

// Mais heap para o processo: o NRO roda como aplicativo (Eden, ou title override no console).
extern "C" {
u32 __nx_applet_type = AppletType_Default;
// Sem console de texto: o vitart registra em arquivo.
}

extern "C" int main(int, char **);
namespace nxp {
namespace {
std::atomic<bool> g_exit{false};
std::string g_root = "sdmc:/switch/gow-nx";
uint8_t *g_window = nullptr;

constexpr uint64_t kGuestLo = 0x80000000ull;
constexpr uint64_t kGuestHi = 0xC0000000ull; // faixas do vitart: módulos em 0x81..., alocadores até 0xC0000000
} // namespace

void init() {
    // NVK (Mesa): sem cache de shaders em disco (pediria usuário/HOME) e sem cache de pipelines do Mesa.
    setenv("MESA_SHADER_CACHE_DISABLE", "true", 1);
    setenv("MESA_GLSL_CACHE_DISABLE", "true", 1);
    // O NVK só expõe GPUs Maxwell (o Tegra X1 é GM20B) com esta variável; sem ela vkEnumeratePhysicalDevices dá 0.
    setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);
    // As mensagens do Mesa (mesa_loge) vão para o stderr: guardar em logs/mesa.txt.
    {
        const std::string p = g_root + "/logs/mesa.txt";
        // Com buffer: sem ele cada linha do log vira uma escrita no cartão SD (o vitart já grava o seu log à parte).
        if (std::FILE *f = std::freopen(p.c_str(), "w", stderr)) setvbuf(f, nullptr, _IOFBF, 1 << 16);
    }
    appletLockExit(); // o HOME não mata o processo sem o jogo fechar os arquivos
    appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
    // Sem ApmCpuBoostMode_FastLoad: ele sobe a CPU para 1785 MHz mas derruba a GPU para 76,8 MHz.
    // set_clocks() vem depois de abrir o log (core.cpp), senão a linha dos relógios se perde.
}

// CPU 1785 MHz e GPU no máximo do modo (460,8 MHz no portátil, 768 MHz na base) pelo clkrst. Se o processo não
// tiver acesso ao serviço, fica nos relógios do sistema (o sys-clk pode cuidar disso).
void set_clocks() {
    if (R_FAILED(clkrstInitialize())) {
        vt::logf("nx: clkrst indisponível, relógios do sistema");
        return;
    }
    const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
    const u32 gpu = docked ? 768000000u : 460800000u;
    // Nunca baixa um relógio já mais alto (overclock do sys-clk etc.): só sobe até o alvo do modo.
    auto raise = [](PcvModuleId mod, u32 want, u32 &before, u32 &after) -> Result {
        ClkrstSession s;
        Result rc = clkrstOpenSession(&s, mod, 3);
        if (R_FAILED(rc)) return rc;
        before = 0;
        clkrstGetClockRate(&s, &before);
        rc = 0;
        if (before < want) rc = clkrstSetClockRate(&s, want);
        after = 0;
        clkrstGetClockRate(&s, &after);
        clkrstCloseSession(&s);
        return rc;
    };
    u32 cpu0 = 0, cpu1 = 0, gpu0 = 0, gpu1 = 0;
    const Result rc_cpu = raise(PcvModuleId_CpuBus, 1785000000u, cpu0, cpu1);
    const Result rc_gpu = raise(PcvModuleId_GPU, gpu, gpu0, gpu1);
    clkrstExit();
    vt::logf("nx: relógios (%s): CPU %u -> %u MHz (0x%x), GPU %u -> %u MHz (0x%x)", docked ? "base" : "portátil", cpu0 / 1000000,
             cpu1 / 1000000, rc_cpu, gpu0 / 1000000, gpu1 / 1000000, rc_gpu);
}

// Relógios atuais (CPU, GPU, memória) no log: mostra se o overclock está valendo durante o jogo.
void log_clocks() {
    if (R_FAILED(clkrstInitialize())) return;
    auto get = [](PcvModuleId mod) -> u32 {
        ClkrstSession s;
        u32 hz = 0;
        if (R_SUCCEEDED(clkrstOpenSession(&s, mod, 3))) clkrstGetClockRate(&s, &hz), clkrstCloseSession(&s);
        return hz / 1000000;
    };
    const u32 cpu = get(PcvModuleId_CpuBus), gpu = get(PcvModuleId_GPU), emc = get(PcvModuleId_EMC);
    clkrstExit();
    vt::logf("nx: relógios agora: CPU %u MHz, GPU %u MHz, memória %u MHz", cpu, gpu, emc);
}

uint8_t *guest_window() {
    if (g_window) return g_window;
    const size_t size = (size_t)(kGuestHi - kGuestLo);
    u64 total = 0, used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    vt::logf("nx: memória do processo: %llu MiB total, %llu MiB usados; pedindo %zu MiB", (unsigned long long)(total >> 20),
             (unsigned long long)(used >> 20), size >> 20);
    void *p = aligned_alloc(0x10000, size);
    vt::logf("nx: bloco do guest em %p", p);
    if (!p) {
        vt::logf("nx: sem %zu MiB para a memória do guest", size >> 20);
        return nullptr;
    }
    std::memset(p, 0, size);
    g_window = static_cast<uint8_t *>(p) - kGuestLo;
    vt::logf("nx: memória do guest: %zu MiB em %p (0x%08llx..0x%08llx)", size >> 20, p, kGuestLo, kGuestHi);
    return g_window;
}

const std::string &root_dir() { return g_root; }
uint8_t *g_window_base() { return g_window; }
const char *game_name() { return GOW_GAME; }

void request_exit() { g_exit = true; }
bool exit_requested() { return g_exit; }

bool applet_loop() {
    if (!appletMainLoop()) g_exit = true;
    return !g_exit;
}

} // namespace nxp

// ------------------------------------------------------------------------- threads
namespace {
struct ThreadStart {
    std::function<void()> fn;
    std::string name;
};
std::atomic<unsigned> g_next_core{0};
// Threads conhecidas, para o vigia poder pausar cada uma e ver onde está (nxp::dump_threads).
struct KnownThread { Handle h; std::string name; };
std::mutex g_known_mutex;
std::vector<KnownThread> g_known;
void register_current(const std::string &name) {
    const std::lock_guard<std::mutex> lock(g_known_mutex);
    g_known.push_back({threadGetCurHandle(), name});
}
void *thread_entry(void *arg) {
    std::unique_ptr<ThreadStart> s(static_cast<ThreadStart *>(arg));
    // O libnx cria toda thread no núcleo padrão, e o Horizon não reveza threads de mesma prioridade: uma que não
    // dorme toma o núcleo. Cada thread nova ganha um núcleo ideal (0..2, em rodízio) e pode migrar entre os três.
    //
    // O caminho crítico do quadro são três threads: "main" (lógica do jogo), "Render" (thread do jogo que faz todos os
    // sceGxm*: monta os desenhos) e "gravacao vk" (grava os comandos Vulkan). Cada uma ganha um núcleo só seu
    // (0, 1 e 2) e prioridade acima das outras, que ficam livres nos três núcleos (ideal 2) e só rodam onde uma
    // delas estiver esperando: uma thread qualquer nunca atrasa as três. VITART_NX_PIN=0 volta ao rodízio antigo.
    static const bool pin = !(getenv("VITART_NX_PIN") && *getenv("VITART_NX_PIN") == '0');
    const Handle me = threadGetCurHandle();
    if (!pin) svcSetThreadCoreMask(me, (s32)(g_next_core++ % 3), 0x7);
    else if (s->name == "main") svcSetThreadCoreMask(me, 0, 0x1), svcSetThreadPriority(me, 0x2A);
    else if (s->name == "Render") svcSetThreadCoreMask(me, 1, 0x2), svcSetThreadPriority(me, 0x2A);
    else if (s->name == "gravacao vk") svcSetThreadCoreMask(me, 2, 0x4), svcSetThreadPriority(me, 0x2B);
    // Som: dorme quase sempre e acorda a cada poucos ms com prazo curto; acima de todas para não engasgar quando a
    // gravação Vulkan (núcleo 2, 0x2B) está ocupada.
    else if (s->name == "audio" || s->name == "audio_out_thread" || s->name == "SndStreamThread")
        svcSetThreadCoreMask(me, 2, 0x7), svcSetThreadPriority(me, 0x28);
    else svcSetThreadCoreMask(me, 2, 0x7);
    register_current(s->name);
    s->fn();
    return nullptr;
}
} // namespace

namespace vt {
void spawn_detached(std::function<void()> fn, const char *name) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 4u << 20); // o C recompilado empilha os quadros do guest no host
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    auto *s = new ThreadStart{std::move(fn), name ? name : "?"};
    pthread_t th;
    const int r = pthread_create(&th, &attr, thread_entry, s);
    pthread_attr_destroy(&attr);
    if (r != 0) {
        delete s;
        fatal("pthread_create falhou (%d) para %s", r, name ? name : "?");
    }
}
} // namespace vt

namespace nxp {
void register_main_thread() { register_current("principal"); }

// Perfil das threads "main" e "Render" do jogo: a cada 4 ms pausa cada uma, lê PC e LR, solta. Quando o PC está numa
// espera (svcWaitProcessWideKeyAtomic/svcArbitrateLock/svcWaitSynchronization), varre a pilha e guarda os 6 primeiros
// endereços de código achados: diz quem espera. A cada 10 s o log recebe os 12 PCs mais vistos de cada uma e o arquivo
// logs/perfil_<jogo>.txt é reescrito com tudo (acumulado), em offsets de main (somar ao main do .elf).
void prof_start() {
    static bool started = false;
    if (started || (getenv("VITART_NX_PROF") && *getenv("VITART_NX_PROF") == '0')) return;
    started = true;
    vt::spawn_detached([] {
        struct Target {
            const char *name;
            Handle h = 0;
            std::unordered_map<int64_t, uint32_t> pcs, lrs, win;
            std::unordered_map<std::string, uint32_t> waits;
            uint64_t total = 0, wtotal = 0;
        };
        Target tg[2];
        tg[0].name = "main", tg[1].name = "Render";
        const uint64_t m = (uint64_t)(uintptr_t)&main;
        MemoryInfo cm{};
        u32 pi = 0;
        svcQueryMemory(&cm, &pi, m);
        const uint64_t clo = cm.addr, chi = cm.addr + cm.size;
        const uint64_t w1 = (uint64_t)(uintptr_t)&svcWaitProcessWideKeyAtomic, w2 = (uint64_t)(uintptr_t)&svcArbitrateLock,
                       w3 = (uint64_t)(uintptr_t)&svcWaitSynchronization;
        uint64_t last = armGetSystemTick();
        for (;;) {
            svcSleepThread(4000000ll);
            for (Target &t : tg) {
                if (!t.h) {
                    const std::lock_guard<std::mutex> lock(g_known_mutex);
                    for (const KnownThread &k : g_known)
                        if (k.name == t.name) t.h = k.h;
                    if (!t.h) continue;
                }
                if (R_FAILED(svcSetThreadActivity(t.h, ThreadActivity_Paused))) continue;
                ThreadContext c{};
                const Result rc = svcGetThreadContext3(&c, t.h);
                char key[160];
                key[0] = 0;
                if (R_SUCCEEDED(rc) && ((c.pc.x >= w1 - 4 && c.pc.x < w1 + 0x40) || (c.pc.x >= w2 - 4 && c.pc.x < w2 + 0x40) ||
                    (c.pc.x >= w3 - 4 && c.pc.x < w3 + 0x40))) {
                    MemoryInfo si{};
                    svcQueryMemory(&si, &pi, c.sp);
                    const uint64_t send = std::min<uint64_t>(si.addr + si.size, c.sp + 0x2000);
                    int found = 0, k = 0;
                    for (uint64_t a = c.sp & ~7ull; a + 8 <= send && found < 6; a += 8) {
                        const uint64_t v = *reinterpret_cast<const uint64_t *>(a);
                        if (v >= clo && v < chi) k += std::snprintf(key + k, sizeof key - k, " %llx", (unsigned long long)(v - m)), found++;
                    }
                }
                svcSetThreadActivity(t.h, ThreadActivity_Runnable);
                if (R_FAILED(rc)) continue;
                const int64_t pc = (int64_t)(c.pc.x - m), lr = (int64_t)(c.lr - m);
                t.pcs[pc]++, t.lrs[lr]++, t.win[pc]++, t.total++, t.wtotal++;
                if (key[0]) t.waits[key]++;
            }
            const uint64_t now = armGetSystemTick();
            if (armTicksToNs(now - last) < 10000000000ull) continue;
            last = now;
            for (Target &t : tg) {
                if (!t.wtotal) continue;
                std::vector<std::pair<uint32_t, int64_t>> v;
                for (auto &kv : t.win) v.push_back({kv.second, kv.first});
                std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.first > b.first; });
                char line[900];
                int n = std::snprintf(line, sizeof line, "perfil %s: %llu amostras;", t.name, (unsigned long long)t.wtotal);
                for (size_t i = 0; i < v.size() && i < 12 && n < (int)sizeof line - 40; i++)
                    n += std::snprintf(line + n, sizeof line - n, " %llx %.1f%%", (unsigned long long)v[i].second, 100.0 * v[i].first / t.wtotal);
                vt::logf("%s", line);
                t.win.clear(), t.wtotal = 0;
            }
            char path[256];
            std::snprintf(path, sizeof path, "%s/logs/perfil_%s.txt", g_root.c_str(), GOW_GAME);
            if (FILE *f = std::fopen(path, "w")) {
                for (Target &t : tg) {
                    std::fprintf(f, "# thread %s: amostras %llu (offsets de main)\n", t.name, (unsigned long long)t.total);
                    for (int pass = 0; pass < 2; pass++) {
                        auto &src = pass ? t.lrs : t.pcs;
                        std::vector<std::pair<uint32_t, int64_t>> all;
                        for (auto &kv : src) all.push_back({kv.second, kv.first});
                        std::sort(all.begin(), all.end(), [](auto &a, auto &b) { return a.first > b.first; });
                        std::fprintf(f, "%s %s\n", pass ? "lr" : "pc", t.name);
                        for (auto &e : all) std::fprintf(f, "%llx %u\n", (unsigned long long)e.second, e.first);
                    }
                    std::vector<std::pair<uint32_t, std::string>> ws;
                    for (auto &kv : t.waits) ws.push_back({kv.second, kv.first});
                    std::sort(ws.begin(), ws.end(), [](auto &a, auto &b) { return a.first > b.first; });
                    std::fprintf(f, "espera %s\n", t.name);
                    for (size_t i = 0; i < ws.size() && i < 60; i++) std::fprintf(f, "%u:%s\n", ws[i].first, ws[i].second.c_str());
                }
                std::fclose(f);
            }
        }
    }, "perfil");
}
// Pausa cada thread conhecida e registra PC, LR e a cadeia de quadros (fp) como offsets de main (achar no gow1.map).
void dump_threads() {
    const std::lock_guard<std::mutex> lock(g_known_mutex);
    const Handle self = threadGetCurHandle();
    const uint64_t m = (uint64_t)(uintptr_t)&main;
    for (const KnownThread &t : g_known) {
        if (t.h == self) continue;
        if (R_FAILED(svcSetThreadActivity(t.h, ThreadActivity_Paused))) continue;
        ThreadContext c{};
        const Result rc = svcGetThreadContext3(&c, t.h);
        char line[640];
        int n = 0;
        if (R_SUCCEEDED(rc)) {
            n = std::snprintf(line, sizeof line, "pilha %s: pc main%+lld lr main%+lld", t.name.c_str(), (long long)(c.pc.x - m),
                              (long long)(c.lr - m));
            uint64_t fp = c.fp;
            const uint64_t sp = c.sp;
            for (int i = 0; i < 16 && fp > sp && fp < sp + (8u << 20) && !(fp & 7) && n < (int)sizeof line - 32; i++) {
                const uint64_t *f = reinterpret_cast<const uint64_t *>(fp);
                const uint64_t ret = f[1], next = f[0];
                n += std::snprintf(line + n, sizeof line - n, " <- main%+lld", (long long)(ret - m));
                if (next <= fp) break;
                fp = next;
            }
        } else {
            std::snprintf(line, sizeof line, "pilha %s: contexto indisponível (0x%x)", t.name.c_str(), rc);
        }
        // Varredura da pilha: palavras que caem no código do NRO (endereços de retorno prováveis), mesmo sem fp.
        char scan[900];
        int k = 0;
        if (R_SUCCEEDED(rc)) {
            MemoryInfo mi{};
            u32 pi = 0;
            svcQueryMemory(&mi, &pi, m);
            const uint64_t lo = mi.addr, hi = mi.addr + mi.size;
            MemoryInfo si{};
            svcQueryMemory(&si, &pi, c.sp);
            const uint64_t send = std::min<uint64_t>(si.addr + si.size, c.sp + 0x4000);
            int found = 0;
            for (uint64_t a = c.sp & ~7ull; a + 8 <= send && found < 40 && k < (int)sizeof scan - 24; a += 8) {
                const uint64_t v = *reinterpret_cast<const uint64_t *>(a);
                if (v >= lo && v < hi) k += std::snprintf(scan + k, sizeof scan - k, " %llx", (unsigned long long)(v - m)), found++;
            }
        }
        svcSetThreadActivity(t.h, ThreadActivity_Runnable);
        vt::logf("%s", line);
        if (k) vt::logf("  varredura %s (main+hex):%s", t.name.c_str(), scan);
    }
}
} // namespace nxp

// ------------------------------------------------------------------- operações no SD
// Cada abertura/criação/consulta no sistema de arquivos passa por aqui (-Wl,--wrap): o vigia mostra as que estão
// pendentes (caminho, thread, há quanto tempo) e as últimas concluídas; as que passam de 200 ms vão para o log.
namespace {
struct FsOp {
    std::atomic<uint64_t> t0{0}; // 0 = livre
    Handle h = 0;
    const char *op = "";
    char path[192] = {};
};
FsOp g_fsops[32];
std::mutex g_fs_recent_mutex;
std::string g_fs_recent[8];
unsigned g_fs_recent_n = 0;
thread_local bool g_in_fs_log = false;

std::string thread_name(Handle h) {
    const std::lock_guard<std::mutex> lock(g_known_mutex);
    for (const KnownThread &t : g_known)
        if (t.h == h) return t.name;
    char b[24];
    std::snprintf(b, sizeof b, "thread 0x%x", h);
    return b;
}

struct FsScope {
    FsOp *slot = nullptr;
    const char *op;
    const char *path;
    uint64_t t0 = armGetSystemTick();
    Result r = 0;
    FsScope(const char *o, const char *p) : op(o), path(p ? p : "") {
        for (FsOp &s : g_fsops) {
            uint64_t z = 0;
            if (s.t0.compare_exchange_strong(z, t0)) {
                s.h = threadGetCurHandle();
                s.op = op;
                std::snprintf(s.path, sizeof s.path, "%s", path);
                slot = &s;
                break;
            }
        }
    }
    ~FsScope() {
        if (slot) slot->t0.store(0);
        const double ms = armTicksToNs(armGetSystemTick() - t0) / 1e6;
        char b[260];
        std::snprintf(b, sizeof b, "%s %s -> 0x%x (%.1f ms)", op, path, r, ms);
        {
            const std::lock_guard<std::mutex> lock(g_fs_recent_mutex);
            g_fs_recent[g_fs_recent_n++ % 8] = b;
        }
        if (ms > 200 && !g_in_fs_log) {
            g_in_fs_log = true;
            vt::logf("sd lento: %s", b);
            g_in_fs_log = false;
        }
    }
};
} // namespace

extern "C" {
Result __real_fsFsOpenFile(FsFileSystem *, const char *, u32, FsFile *);
Result __real_fsFsOpenDirectory(FsFileSystem *, const char *, u32, FsDir *);
Result __real_fsFsCreateFile(FsFileSystem *, const char *, s64, u32);
Result __real_fsFsCreateDirectory(FsFileSystem *, const char *);
Result __real_fsFsGetEntryType(FsFileSystem *, const char *, FsDirEntryType *);
Result __wrap_fsFsOpenFile(FsFileSystem *fs, const char *p, u32 mode, FsFile *out) {
    FsScope s("abrir", p);
    return s.r = __real_fsFsOpenFile(fs, p, mode, out);
}
Result __wrap_fsFsOpenDirectory(FsFileSystem *fs, const char *p, u32 mode, FsDir *out) {
    FsScope s("abrir pasta", p);
    return s.r = __real_fsFsOpenDirectory(fs, p, mode, out);
}
Result __wrap_fsFsCreateFile(FsFileSystem *fs, const char *p, s64 size, u32 opt) {
    FsScope s("criar", p);
    return s.r = __real_fsFsCreateFile(fs, p, size, opt);
}
Result __wrap_fsFsCreateDirectory(FsFileSystem *fs, const char *p) {
    FsScope s("criar pasta", p);
    return s.r = __real_fsFsCreateDirectory(fs, p);
}
Result __wrap_fsFsGetEntryType(FsFileSystem *fs, const char *p, FsDirEntryType *out) {
    FsScope s("tipo", p);
    return s.r = __real_fsFsGetEntryType(fs, p, out);
}
}

namespace nxp {
void dump_fs_ops(bool recent) {
    const uint64_t now = armGetSystemTick();
    for (FsOp &s : g_fsops) {
        const uint64_t t0 = s.t0.load();
        if (!t0) continue;
        vt::logf("sd pendente: %s %s por %s há %.1f s", s.op, s.path, thread_name(s.h).c_str(),
                 armTicksToNs(now - t0) / 1e9);
    }
    if (!recent) return;
    const std::lock_guard<std::mutex> lock(g_fs_recent_mutex);
    for (unsigned i = 0; i < 8 && i < g_fs_recent_n; i++)
        vt::logf("sd recente: %s", g_fs_recent[(g_fs_recent_n - 1 - i) % 8].c_str());
}
} // namespace nxp

// ------------------------------------------------------------------------- falhas
// Tratador de exceções do libnx: grava logs/crash.txt com o PC (e o PC relativo a main, para achar no gow1.map), o
// endereço da falha e, se ele cair na janela do guest, o endereço do Vita correspondente.
extern "C" {
alignas(16) u8 __nx_exception_stack[0x8000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
extern "C" int main(int, char **);
void __libnx_exception_handler(ThreadExceptionDump *ctx) {
    char path[256];
    std::snprintf(path, sizeof path, "%s/logs/crash.txt", nxp::root_dir().c_str());
    if (FILE *f = std::fopen(path, "w")) {
        const uint64_t base = (uint64_t)(uintptr_t)nxp::g_window_base();
        std::fprintf(f, "exceção 0x%x\npc  0x%016llx (main%+lld)\nlr  0x%016llx (main%+lld)\nfar 0x%016llx\n", ctx->error_desc,
                     (unsigned long long)ctx->pc.x, (long long)(ctx->pc.x - (uintptr_t)&main), (unsigned long long)ctx->lr.x,
                     (long long)(ctx->lr.x - (uintptr_t)&main), (unsigned long long)ctx->far.x);
        if (base && ctx->far.x >= base && ctx->far.x < base + 0x100000000ull)
            std::fprintf(f, "endereço do guest 0x%08llx\n", (unsigned long long)(ctx->far.x - base));
        for (int i = 0; i < 29; i++) std::fprintf(f, "x%-2d 0x%016llx\n", i, (unsigned long long)ctx->cpu_gprs[i].x);
        std::fclose(f);
    }
    svcExitProcess();
}
}
