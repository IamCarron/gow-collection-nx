"""Tira as conversões de meia precisão do GLSL que o spirv-cross gera (só no pacote do deko3d).

Os registradores do Vita (pa, sa, temp, fpi, _out) viram arrays de float. Um registrador de meia precisão guarda duas
metades de 16 bits dentro de um float: cada escrita faz packHalf2x16 e cada leitura faz unpackHalf2x16. O uam
(compilador do deko3d) não tira essas conversões e cada uma vira F2F/PRMT no Maxwell: um shader de fragmento típico
saía com ~3x as instruções da conta de verdade.

Aqui cada posição que só é lida como par de metades passa a ser um vec2 (variável própria). As escritas viram a
conta direta (sem o arredondamento para 16 bits entre as operações: o resultado fica igual ou mais preciso) e as
leituras viram o próprio vec2. Posições lidas como float de 32 bits (ou com índice variável) ficam como estavam.
"""
import re, struct

ARRAYS = ("pa", "sa", "temp", "fpi", "_out")
UNPACK = "unpackHalf2x16(floatBitsToUint("
PACK = "uintBitsToFloat(packHalf2x16("


def _close(s, i):
    """Índice logo depois do ')' que fecha o '(' em s[i-1]."""
    d = 1
    while d:
        c = s[i]
        d += (c == "(") - (c == ")")
        i += 1
    return i


def _const(rhs):
    """Constante de 32 bits (literal float ou uintBitsToFloat(0x...u)) como par de metades com os bits exatos."""
    m = re.fullmatch(r"uintBitsToFloat\((0x[0-9a-fA-F]+)u(?: /\*.*?\*/)?\)", rhs)
    if m:
        return "unpackHalf2x16(%su)" % m.group(1)
    m = re.fullmatch(r"-?(\d+\.?\d*(?:[eE][-+]?\d+)?)", rhs)
    if m:
        b = struct.unpack("<I", struct.pack("<f", float(rhs)))[0]
        return "unpackHalf2x16(0x%08xu)" % b
    return None


def transform(g):
    if "float pa[" not in g:
        return g
    names = [n for n in ARRAYS if re.search(r"^float %s\[\d+\] = " % re.escape(n), g, re.M)]
    if not names:
        return g
    alt = "|".join(re.escape(n) for n in names)
    # índice que não é literal: desiste do shader
    for m in re.finditer(r"(%s)\[" % alt, g):
        if not re.match(r"\d+\]", g[m.end():]):
            return g
    slot = re.compile(r"\b(%s)\[(\d+)\]" % alt)
    lines = g.split("\n")
    body = [i for i, l in enumerate(lines) if not re.match(r"^float (%s)\[\d+\] = float\[\]" % alt, l)]

    # temporárias que copiam uma posição: float _N = pa[i];
    tmp_of = {}
    tdecl = re.compile(r"^\s*float (_\d+) = (%s)\[(\d+)\];$" % alt)
    for i in body:
        m = tdecl.match(lines[i])
        if m:
            tmp_of[m.group(1)] = (m.group(2), int(m.group(3)))

    # posições lidas como float de 32 bits (fora de unpackHalf2x16) não viram vec2
    raw = set()
    wdecl = re.compile(r"^\s*(%s)\[(\d+)\] = " % alt)
    for i in body:
        l = lines[i]
        mt = tdecl.match(l)
        if mt:
            continue  # cópia: decide-se pelos usos da temporária
        mw = wdecl.match(l)
        start = mw.end() if mw else 0
        s = l[start:].replace(UNPACK, "\x01")  # leituras como metades somem da busca
        s = re.sub(r"\x01(%s)\[\d+\]\)\)" % alt, "", s)
        s = re.sub(r"\x01(_\d+)\)\)", "", s)
        for m in slot.finditer(s):
            raw.add((m.group(1), int(m.group(2))))
        for m in re.finditer(r"\b(_\d+)\b", s):
            t = m.group(1)
            if t in tmp_of:
                raw.add(tmp_of[t])
    half_tmp = {t for t, sl in tmp_of.items() if sl not in raw}

    def var(n, k):
        return "h_%s_%d" % (n.strip("_"), k)

    used = set()
    out = []
    for i, l in enumerate(lines):
        if i not in body:
            out.append(l)
            continue
        mt = tdecl.match(l)
        if mt and mt.group(1) in half_tmp:
            n, k = mt.group(2), int(mt.group(3))
            used.add((n, k))
            out.append(l[: l.index("float")] + "vec2 %s = %s;" % (mt.group(1), var(n, k)))
            continue
        mw = wdecl.match(l)
        lhs = None
        if mw and (mw.group(1), int(mw.group(2))) not in raw:
            lhs = (mw.group(1), int(mw.group(2)))
            rhs = l[mw.end():].rstrip()
            assert rhs.endswith(";")
            rhs = rhs[:-1]
            if rhs.startswith(PACK) and _close(rhs, len(PACK)) == len(rhs) - 1:
                rhs = rhs[len(PACK):-2]
            else:
                rhs = _const(rhs) or "unpackHalf2x16(floatBitsToUint(%s))" % rhs
            l = l[: l.index(mw.group(1))] + "%s = %s;" % (var(*lhs), rhs)
            used.add(lhs)
        # leituras como metades
        def rd(m):
            n, k = m.group(1), int(m.group(2))
            if (n, k) in raw:
                return m.group(0)
            used.add((n, k))
            return var(n, k)
        l = re.sub(re.escape(UNPACK) + r"(%s)\[(\d+)\]\)\)" % alt, rd, l)
        l = re.sub(re.escape(UNPACK) + r"(_\d+)\)\)", lambda m: m.group(1) if m.group(1) in half_tmp else m.group(0), l)
        out.append(l)
    res = "\n".join(out)
    # sobrou acesso a uma posição convertida: não mexe no shader
    for n, k in used:
        if re.search(r"\b%s\[%d\]" % (re.escape(n), k), res.split("void main()", 1)[1]):
            return g
    decl = "".join("vec2 %s = vec2(0.0);\n" % var(n, k) for n, k in sorted(used))
    p = res.index("void main()")
    return res[:p] + decl + "\n" + res[p:]


if __name__ == "__main__":
    import sys
    print(transform(open(sys.argv[1]).read()))
