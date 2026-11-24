// SPDX-License-Identifier: GPL-3.0-or-later
// lsfg_prep: prepara no PC o cache de shaders do LSFG (frame gen) para o gxm_dk.cpp.
// Uso: lsfg_prep <Lossless.dll> <pasta de saída>   (grava <saída>/Performance e <saída>/Quality, com o fluxo na
// resolução cheia, e <saída>/Performance_f2 e <saída>/Quality_f2, com o fluxo na metade)
// Meia precisão (FP16) não sai: o GLSL do SPIRV-Cross pede uma extensão de FP16 que o uam não tem.
#include <lsfg/common/cache_store.hpp>
#include <lsfg/common/error.hpp>
#include <lsfg/common/prepare.hpp>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "uso: %s <Lossless.dll> <pasta de saída>\n", argv[0]);
        return 2;
    }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<std::uint8_t> dll((std::istreambuf_iterator<char>(f)), {});
    if (dll.empty()) {
        std::fprintf(stderr, "não li %s\n", argv[1]);
        return 1;
    }
    const std::string root = argv[2];
    mkdir(root.c_str(), 0777);
    struct Var {
        unsigned den;
        bool half;
    };
    for (const Var v : {Var{1, false}, Var{2, false}, Var{3, false}, Var{4, false}})
    for (const bool perf : {true, false}) {
        const unsigned den = v.den;
        const std::string dir = root + (perf ? "/Performance" : "/Quality") + (den > 1 ? "_f" + std::to_string(den) : "") + (v.half ? "h" : "");
        mkdir(dir.c_str(), 0777);
        lsfg::prepare::Options opt{};
        opt.precision = v.half ? lsfg::shaders::Precision::low : lsfg::shaders::Precision::high;
        opt.graph.performance = perf;
        opt.graph.flow_numerator = 1;
        opt.graph.flow_denominator = den;
        lsfg::prepare::Result res;
        std::printf("%s: preparando...\n", dir.c_str());
        const auto rc = lsfg::prepare::run(dll, opt, res, [](const lsfg::prepare::ModuleReport &m) {
            std::printf("  %-28s spirv %6zu glsl %6zu dksh %6zu regs %3u scratch %u shared %u\n", m.name.c_str(), m.spirv_bytes,
                        m.glsl_bytes, m.dksh_bytes, m.registers, m.scratch_bytes, m.shared_memory_bytes);
        });
        if (!lsfg::succeeded(rc)) {
            std::fprintf(stderr, "prepare falhou: %s\n", std::string(lsfg::error_name(rc)).c_str());
            return 1;
        }
        std::remove((dir + "/manifest.bin").c_str());
        if (const auto w = lsfg::cache::write(dir, res.contents); !lsfg::succeeded(w)) {
            std::fprintf(stderr, "gravação falhou: %s\n", std::string(lsfg::error_name(w)).c_str());
            return 1;
        }
        lsfg::cache::Loaded back;
        if (!lsfg::succeeded(lsfg::cache::read(dir, back)) || !lsfg::cache::same_modules(back, res.contents)) {
            std::fprintf(stderr, "conferência do cache falhou\n");
            return 1;
        }
        std::printf("%s: %zu passadas gravadas\n", dir.c_str(), res.contents.passes.size());
    }
    return 0;
}
