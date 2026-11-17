// Módulos do Vita recompilados e ligados no NRO. Cada um vem de um objeto relocável com os símbolos internos
// localizados e as entradas renomeadas para <módulo>_arc_* (ver Makefile, regra dos módulos).
#include "nx_platform.h"

#include <cstring>

#define ARC_DECL(m) ARC_DECL_(m)
#define ARC_ROW(m) ARC_ROW_(m)
#define ARC_DECL_(m)                                                                                 \
    extern "C" int m##_arc_bind(const void *, uint8_t *);                                            \
    extern "C" const arc_func_entry *m##_arc_get_table(unsigned *);                                 \
    extern "C" const arc_segment *m##_arc_get_segments(unsigned *);                                 \
    extern "C" const arc_module_info *m##_arc_get_module(void);
#define ARC_ROW_(m) {#m, m##_arc_bind, m##_arc_get_table, m##_arc_get_segments, m##_arc_get_module},

#if defined(GOW_GAME_GOW2)
#define GAME_MOD gow2
#else
#define GAME_MOD gow1
#endif

ARC_DECL(GAME_MOD)
ARC_DECL(libc)
ARC_DECL(libfios2)

namespace nxp {
static const StaticModule g_mods[] = {ARC_ROW(GAME_MOD) ARC_ROW(libc) ARC_ROW(libfios2)};

const StaticModule *find_module(const char *name) {
    for (const StaticModule &m : g_mods)
        if (!std::strcmp(m.name, name)) return &m;
    return nullptr;
}
} // namespace nxp
