// SPDX-License-Identifier: GPL-3.0-or-later
// Substitui o compile_uam.cpp do lsfg-deko3d no PC: chama o uam.exe do devkitPro em vez da biblioteca.
#include <lsfg/common/dksh.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace lsfg::dksh {

bool compiler_available() noexcept { return true; }

ErrorCode compile(const std::string_view glsl, Blob &out) {
    out = Blob{};
    if (glsl.empty()) return ErrorCode::invalid_argument;
    static int n = 0;
    const char *tmp = std::getenv("LSFG_TMP");
    const std::string base = std::string(tmp ? tmp : ".") + "/lsfg_" + std::to_string(getpid()) + "_" + std::to_string(n++);
    const std::string in = base + ".comp", dk = base + ".dksh", log = base + ".log";
    {
        std::ofstream f(in, std::ios::binary);
        f.write(glsl.data(), (std::streamsize)glsl.size());
    }
    const char *uam = std::getenv("UAM");
    const std::string cmd = std::string("\"") + (uam ? uam : "uam") + "\" -s comp -o \"" + dk + "\" \"" + in + "\" > \"" + log + "\" 2>&1";
    const int rc = std::system(cmd.c_str());
    {
        std::ifstream l(log, std::ios::binary);
        out.log.assign(std::istreambuf_iterator<char>(l), {});
    }
    std::ifstream d(dk, std::ios::binary);
    if (rc == 0 && d) out.bytes.assign(std::istreambuf_iterator<char>(d), {});
    d.close();
    if (!std::getenv("LSFG_KEEP")) std::remove(in.c_str()), std::remove(dk.c_str()), std::remove(log.c_str());
    if (out.bytes.empty()) {
        std::fprintf(stderr, "uam falhou:\n%s\n", out.log.c_str());
        return ErrorCode::shader_compile_failed;
    }
    return validate(out.bytes, out.program);
}

} // namespace lsfg::dksh
