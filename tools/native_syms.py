#!/usr/bin/env python3
"""Simbolos do ELF para a camada nativa: gera native/wotm_syms.h e troca enderecos crus por nomes.

  python tools/native_syms.py --gen            grava native/wotm_syms.h (wotm::sym::<dado>, wotm::fn::<funcao>) a partir de
                                               symbol_addrs.txt (+ symbol_addrs_manual.txt, se existir)
  python tools/native_syms.py --nm <nm.txt> <out.h>
                                               mesmo header, com os enderecos de outro ELF (saida de `nm` dele): o runtime cobaia
                                               (build_dev_nm) o recebe como wotm_syms_nm.h, que o wotm_syms.h usa no lugar dos
                                               enderecos retail quando esta no caminho de include
  python tools/native_syms.py --audit         lista os literais hex de native/ que batem com um simbolo (exato), caem dentro de um
                                               (offset) ou nao casam; nao altera nada
  python tools/native_syms.py --apply [arq..]  troca, nos arquivos dados (padrao: todos de native/), cada literal que e exatamente o
                                               endereco de um simbolo por wotm::sym::<nome> / wotm::fn::<nome>; os de offset ficam
                                               (sao campos de struct: o nome certo e o do header de tipos, nao do simbolo)

O nome vira identificador C++: '$' e '.' viram '_', nome que comeca com digito ganha '_' na frente; colisao ganha o endereco no fim.
Enderecos considerados: 0x100000..0x2000000 (RAM do jogo), com 5 a 8 digitos hex e sufixo u/U opcional, como token isolado.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")
HEADER = os.path.join(NATIVE, "wotm_syms.h")
SYM_FILES = [os.path.join(ROOT, "symbol_addrs.txt"), os.path.join(ROOT, "symbol_addrs_manual.txt")]
LINE = re.compile(r"^\s*([A-Za-z_$.][\w$.]*)\s*=\s*0x([0-9A-Fa-f]+)\s*;\s*(?://\s*(.*))?$")
LITERAL = re.compile(r"(?<![\w.])0x0*([0-9A-Fa-f]{5,8})[uU]?(?![\w.])")
LO, HI = 0x100000, 0x2000000


def load_symbols():
    """[(name, addr, is_func, size)], ordenado por endereco."""
    out = []
    for path in SYM_FILES:
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = LINE.match(line)
                if not m:
                    continue
                name, addr, note = m.group(1), int(m.group(2), 16), m.group(3) or ""
                is_func = "type:func" in note
                sm = re.search(r"size:0x([0-9A-Fa-f]+)", note)
                size = int(sm.group(1), 16) if sm else 0
                out.append((name, addr, is_func, size))
    out.sort(key=lambda t: t[1])
    return out


RESERVED = {"errno", "stdin", "stdout", "stderr", "assert", "EOF", "NULL", "min", "max", "near", "far", "pascal", "cdecl", "environ",
            "alloca", "unlink", "getpid", "isatty", "fileno", "strdup", "stricmp", "strnicmp", "putenv", "tzname", "timezone", "daylight"}


def ident(name):
    """Identificador C++ sem bater em macro/nome reservado do MSVC (_errno, ___mb_cur_max_func, errno...): esses ganham 'x' na frente."""
    s = re.sub(r"[$.]", "_", name)
    if s[0].isdigit():
        s = "x" + s
    if s.startswith("_") or s in RESERVED:
        s = "x" + s if s.startswith("_") else "x_" + s
    return s


def unique_idents(symbols):
    """{(name, addr): ident} sem colisao dentro de cada namespace (dados / funcoes)."""
    seen = {True: {}, False: {}}
    out = {}
    for name, addr, is_func, _ in symbols:
        base = ident(name)
        table = seen[is_func]
        key = base
        if key in table and table[key] != addr:
            key = "%s_%06X" % (base, addr)
        table[key] = addr
        out[(name, addr)] = key
    return out


def gen(symbols, header=HEADER, source="symbol_addrs.txt (tabela de simbolos do ELF retail)", remap=None):
    """remap = {(name, addr retail): addr no outro ELF}: mesmos identificadores do header retail, enderecos do outro ELF."""
    idents = unique_idents(symbols)
    if remap is not None:
        idents = {(n, remap.get((n, a), a)): idents[(n, a)] for n, a, _, _ in symbols}
        symbols = [(n, remap.get((n, a), a), f, s) for n, a, f, s in symbols]
    lines = [
        "// Gerado por tools/native_syms.py a partir de %s. Nao edite a mao." % source,
        "// wotm::sym::<nome> = endereco de um dado (global, tabela, objeto); wotm::fn::<nome> = endereco de uma funcao (hooks).",
        "// Os nomes sao os do ELF (mangling do gcc 2.95), com '$' e '.' trocados por '_'.",
        "#pragma once",
        "#include <cstdint>",
        "",
        "namespace wotm {",
        "namespace sym {",
    ]
    if remap is None:
        # o runtime cobaia (build_dev_nm) traz no caminho de include um wotm_syms_nm.h com os enderecos do ELF nmall
        lines[4:4] = ["#if __has_include(\"wotm_syms_nm.h\")", "#include \"wotm_syms_nm.h\"", "#else"]
    else:
        lines.insert(4, "#define WOTM_SYMS_NM 1   // runtime cobaia: enderecos do ELF nmall")
    for name, addr, is_func, size in symbols:
        if is_func:
            continue
        lines.append("constexpr uint32_t %s = 0x%08X;%s" % (idents[(name, addr)], addr, ("   // size 0x%X" % size) if size else ""))
    lines += ["} // namespace sym", "", "namespace fn {"]
    for name, addr, is_func, size in symbols:
        if not is_func:
            continue
        lines.append("constexpr uint32_t %s = 0x%08X;%s" % (idents[(name, addr)], addr, ("   // size 0x%X" % size) if size else ""))
    lines += ["} // namespace fn", ""]
    # Tabela dos dados por endereco, para dar nome a um endereco em tempo de execucao (inventario da checagem VU1 x nativo:
    # "src dentro de fontPacket+0x40"). Entradas com tamanho; a busca acha a ultima entrada que comeca antes do endereco.
    lines += [
        "// Dados por endereco (nome do ELF, sem troca de caracteres), para dar nome a um endereco: symtab::find(a).",
        "namespace symtab {",
        "struct Entry { uint32_t addr, size; const char *name; };",
        "inline const Entry kData[] = {",
    ]
    data = sorted((s for s in symbols if not s[2]), key=lambda t: t[1])
    for name, addr, _, size in data:
        lines.append('    {0x%08X, 0x%X, "%s"},' % (addr, size, name.replace("\\", "\\\\").replace('"', '\\"')))
    lines += [
        "};",
        "inline constexpr size_t kCount = sizeof(kData) / sizeof(kData[0]);",
        "// Entrada que contem `a` (ou a ultima que comeca antes dele, quando nao tem tamanho); nullptr antes da primeira.",
        "inline const Entry *find(uint32_t a) {",
        "    size_t lo = 0, hi = kCount;",
        "    while (lo < hi) { const size_t mid = (lo + hi) / 2; if (kData[mid].addr <= a) lo = mid + 1; else hi = mid; }",
        "    if (!lo) return nullptr;",
        "    const Entry *e = &kData[lo - 1];",
        "    return (!e->size || a < e->addr + e->size) ? e : nullptr;",
        "}",
        "} // namespace symtab",
        "} // namespace wotm",
    ]
    lines += ["#endif", ""] if remap is None else [""]
    with open(header, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    nd = sum(1 for s in symbols if not s[2])
    print("%s: %d dados, %d funcoes" % (header, nd, len(symbols) - nd))


def nm_remap(symbols, nm_txt):
    """Enderecos de outro ELF (saida do nm dele) por nome. Nome ausente ou ambiguo (estatico repetido) fica com o retail."""
    where = {}
    with open(nm_txt, encoding="utf-8", errors="replace") as f:
        for line in f:
            p = line.split()
            if len(p) == 3:
                where.setdefault(p[2], set()).add(int(p[0], 16))
    remap, moved, missing, ambiguous = {}, 0, 0, 0
    for name, addr, _, _ in symbols:
        got = where.get(name)
        if not got:
            missing += 1
        elif addr in got:
            continue
        elif len(got) > 1:
            ambiguous += 1
        else:
            remap[(name, addr)] = next(iter(got))
            moved += 1
    print("%d simbolos mudaram de endereco, %d ausentes e %d ambiguos ficam com o retail" % (moved, missing, ambiguous))
    return remap


def native_files():
    out = []
    for d in (NATIVE, os.path.join(NATIVE, "runtime")):
        for n in sorted(os.listdir(d)):
            p = os.path.join(d, n)
            if os.path.isfile(p) and n != "wotm_syms.h" and n.endswith((".hpp", ".h", ".cpp", ".inc")):
                out.append(p)
    return out


def classify(symbols):
    """addr -> (kind, name, is_func, offset): exato, dentro ou None."""
    import bisect
    addrs = [s[1] for s in symbols]
    exact = {}
    for name, addr, is_func, size in symbols:
        exact.setdefault(addr, (name, is_func))   # o primeiro nome no endereco (ha aliases)

    def look(a):
        if a in exact:
            n, f = exact[a]
            return ("exato", n, f, 0)
        i = bisect.bisect_right(addrs, a) - 1
        if i >= 0:
            name, start, is_func, size = symbols[i]
            if size and start < a < start + size:
                return ("dentro", name, is_func, a - start)
        return None
    return look


def audit(symbols, files):
    look = classify(symbols)
    tot = {"exato": 0, "dentro": 0, "nenhum": 0}
    for path in files:
        rel = os.path.relpath(path, ROOT)
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        rows = []
        for m in LITERAL.finditer(text):
            a = int(m.group(1), 16)
            if not (LO <= a < HI):
                continue
            r = look(a)
            if r is None:
                tot["nenhum"] += 1
                rows.append(("nenhum", a, "", ""))
            else:
                tot[r[0]] += 1
                rows.append((r[0], a, ("fn " if r[2] else "") + r[1], ("+0x%X" % r[3]) if r[3] else ""))
        if rows:
            ex = sum(1 for r in rows if r[0] == "exato")
            de = sum(1 for r in rows if r[0] == "dentro")
            ne = sum(1 for r in rows if r[0] == "nenhum")
            print("%s: %d literais (%d exatos, %d dentro de simbolo, %d sem simbolo)" % (rel, len(rows), ex, de, ne))
            seen = set()
            for kind, a, name, off in rows:
                if kind == "exato" and a not in seen:
                    seen.add(a)
                    print("   0x%06X = %s" % (a, name))
            seen = set()
            for kind, a, name, off in rows:
                if kind == "dentro" and a not in seen:
                    seen.add(a)
                    print("   0x%06X dentro de %s %s" % (a, name, off))
            seen = set()
            for kind, a, name, off in rows:
                if kind == "nenhum" and a not in seen:
                    seen.add(a)
                    print("   0x%06X sem simbolo" % a)
    print("total: %d exatos, %d dentro de simbolo, %d sem simbolo" % (tot["exato"], tot["dentro"], tot["nenhum"]))


def apply(symbols, files):
    look = classify(symbols)
    idents = unique_idents(symbols)
    by_addr = {}
    for name, addr, is_func, _ in symbols:
        by_addr.setdefault(addr, (name, is_func))
    total = 0
    for path in files:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        n = 0

        def sub(m):
            nonlocal n
            a = int(m.group(1), 16)
            if not (LO <= a < HI) or a not in by_addr:
                return m.group(0)
            name, is_func = by_addr[a]
            n += 1
            return "wotm::%s::%s" % ("fn" if is_func else "sym", idents[(name, a)])
        # so no codigo: o que vem depois de "//" numa linha fica como esta (comentarios citam enderecos de proposito)
        out_lines = []
        for line in text.split("\n"):
            cut = line.find("//")
            code, rest = (line, "") if cut < 0 else (line[:cut], line[cut:])
            out_lines.append(LITERAL.sub(sub, code) + rest)
        new = "\n".join(out_lines)
        if n and '#include "wotm_syms.h"' not in new:
            # cada arquivo que usa os nomes inclui o header por conta propria (os .hpp sao incluidos em ordens diferentes)
            inc = '#include "wotm_syms.h"   // wotm::sym / wotm::fn: enderecos do ELF por nome (tools/native_syms.py --gen)\n'
            m = re.search(r"^#pragma once[^\n]*\n|^#include[^\n]*\n", new, re.M)
            new = (new[:m.end()] + inc + new[m.end():]) if m else (inc + new)
        if n:
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(new)
            print("%s: %d literais trocados" % (os.path.relpath(path, ROOT), n))
            total += n
    print("total: %d" % total)


def main(argv):
    symbols = load_symbols()
    if not symbols:
        print("symbol_addrs.txt vazio ou ausente (gere com configure.py --split)")
        return 1
    if "--nm" in argv:
        i = argv.index("--nm")
        nm_txt, out = argv[i + 1], argv[i + 2]
        gen(symbols, out, "symbol_addrs.txt com os enderecos de %s (ELF nmall, runtime cobaia)" % os.path.basename(nm_txt),
            nm_remap(symbols, nm_txt))
        return 0
    if "--gen" in argv:
        gen(symbols)
    files = [a for a in argv if not a.startswith("--")] or native_files()
    if "--audit" in argv:
        audit(symbols, files)
    if "--apply" in argv:
        apply(symbols, files)
    if not any(a in argv for a in ("--gen", "--audit", "--apply")):
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
