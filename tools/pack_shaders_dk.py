#!/usr/bin/env python3
"""Gera shaders_dk.pak (renderizador deko3d) a partir do shaders.pak (SPIR-V).

Uso: python tools/pack_shaders_dk.py <shaders.pak> <shaders_dk.pak> [processos]

Cada SPIR-V passa por spirv-cross (GLSL 460) e uam (DKSH do deko3d). A chave é o FNV-1a 64 dos bytes do SPIR-V,
a mesma conta que o gxm_dk.cpp faz em tempo de execução: o hle_gxm continua lendo o shaders.pak e o renderizador
acha o binário pelo conteúdo, sem saber o nome.

Formato: "DKSPAK1\\0", u32 n, u32 0, n x {u64 chave, u32 offset, u32 tamanho} (ordenado pela chave), dados (DKSH).
Os .dksh ficam em cache numa pasta ao lado da saída: rodar de novo só converte o que mudou.
"""
import os, re, struct, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

SC = os.environ.get("SPIRV_CROSS", "C:/VulkanSDK/1.4.363.0/Bin/spirv-cross.exe")
UAM = os.environ.get("UAM", "C:/devkitPro/tools/bin/uam.exe")
UBO = os.environ.get("DK_UBO", "1") != "0"  # uniforms como UBO (o gxm_dk.cpp liga com uniform buffers)
HALF = os.environ.get("DK_HALF", "1") != "0"  # registradores de meia precisão como vec2 nos fragmentos (dk_half.py)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dk_half


def fnv(b):
    h = 0xCBF29CE484222325
    for x in b:
        h = ((h ^ x) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def stage(spv):
    w = struct.unpack("<%dI" % (len(spv) // 4), spv[: len(spv) // 4 * 4])
    i = 5
    while i < len(w):
        op, cnt = w[i] & 0xFFFF, w[i] >> 16
        if op == 15:
            return {0: "vert", 4: "frag", 5: "comp"}.get(w[i + 1])
        i += max(cnt, 1)


UB_SSBO = re.compile(r"layout\(binding = 0, std430\) readonly buffer uniforms\s*\{\s*float _m0\[\];\s*\} uniforms_1;")
UB_UBO = "layout(binding = 0, std140) uniform uniforms\n{\n    vec4 _m0[4096];\n} uniforms_1;"


def to_ubo(g):
    """Uniforms do GXM como uniform buffer (cache de constantes) em vez de storage buffer (memória global).

    O spirv-cross gera "float _m0[]" num SSBO; no Maxwell cada leitura vira um LDG que vai à L2 (lento em todo
    vértice e pixel). Como UBO std140 o array é de vec4: o índice i vira [i >> 2][i & 3], que com índice constante
    (o caso comum) o compilador resolve como leitura direta c[...].
    """
    g2, n = UB_SSBO.subn(UB_UBO, g)
    if not n:
        return g
    out, i, tok = [], 0, "uniforms_1._m0["
    while True:
        j = g2.find(tok, i)
        if j < 0:
            out.append(g2[i:])
            break
        out.append(g2[i:j])
        k, depth = j + len(tok), 1
        while depth:
            c = g2[k]
            depth += c == "[" or -(c == "]")
            k += 1
        idx = g2[j + len(tok):k - 1]
        if idx.isdigit():
            v = int(idx)
            out.append("uniforms_1._m0[%d][%d]" % (v >> 2, v & 3))
        else:
            out.append("uniforms_1._m0[uint(%s) >> 2u][uint(%s) & 3u]" % (idx, idx))
        i = k
    return "".join(out)


def convert(job):
    key, spv, cache = job
    out = os.path.join(cache, "%016x.dksh" % key)
    if os.path.exists(out) and os.path.getsize(out) > 0:
        return key, out, None
    st = stage(spv)
    if st not in ("vert", "frag"):
        return key, None, "estágio %s" % st
    with tempfile.TemporaryDirectory() as td:
        s, g = os.path.join(td, "s.spv"), os.path.join(td, "s.glsl")
        open(s, "wb").write(spv)
        r = subprocess.run([SC, s, "--version", "460", "--no-es", "--output", g], capture_output=True, text=True)
        if r.returncode:
            return key, None, "spirv-cross: " + (r.stderr.strip().splitlines() or ["?"])[-1][:160]
        if UBO or HALF:
            src = open(g).read()
            if UBO:
                src = to_ubo(src)
            if HALF and st == "frag":
                src = dk_half.transform(src)
            open(g, "w").write(src)
        r = subprocess.run([UAM, "-s", st, "-o", out + ".tmp", g], capture_output=True, text=True)
        if r.returncode or not os.path.exists(out + ".tmp"):
            return key, None, "uam: " + ((r.stderr + r.stdout).strip().splitlines() or ["?"])[0][:160]
    os.replace(out + ".tmp", out)
    return key, out, None


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    nproc = int(sys.argv[3]) if len(sys.argv) > 3 else os.cpu_count() or 4
    cache = os.path.splitext(dst)[0] + ("_cache_ubo" if UBO else "_cache") + ("_h" if HALF else "")
    os.makedirs(cache, exist_ok=True)
    f = open(src, "rb")
    if f.read(8) != b"SPVPAK1\0":
        sys.exit("não é um shaders.pak")
    n, = struct.unpack("<I", f.read(4))
    ents = [struct.unpack("<28sIQ", f.read(40)) for _ in range(n)]
    jobs = {}
    for _, size, off in ents:
        f.seek(off)
        spv = f.read(size)
        jobs.setdefault(fnv(spv), spv)  # conteúdos repetidos viram uma conversão só
    jobs = [(k, v, cache) for k, v in jobs.items()]
    n = len(jobs)
    print("%d shaders, %d processos" % (n, nproc), flush=True)
    res, errs, done = {}, [], 0
    with ThreadPoolExecutor(nproc) as ex:
        for key, path, err in ex.map(convert, jobs, chunksize=4):
            done += 1
            if path:
                res[key] = path
            else:
                errs.append((key, err))
            if done % 500 == 0:
                print("  %d/%d (%d falhas)" % (done, n, len(errs)), flush=True)
    keys = sorted(res)
    hdr = 16 + 16 * len(keys)
    blobs, table, off = [], [], hdr
    for k in keys:
        b = open(res[k], "rb").read()
        pad = (-off) & 15
        if pad:
            blobs.append(b"\0" * pad)
            off += pad
        table.append(struct.pack("<QII", k, off, len(b)))
        blobs.append(b)
        off += len(b)
    with open(dst, "wb") as o:
        o.write(b"DKSPAK1\0" + struct.pack("<II", len(keys), 0))
        o.write(b"".join(table))
        o.write(b"".join(blobs))
    print("%s: %d shaders, %.1f MB, %d falhas" % (dst, len(keys), off / 1048576, len(errs)))
    for k, e in errs[:20]:
        print("  %016x %s" % (k, e))


if __name__ == "__main__":
    main()
