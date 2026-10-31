#!/usr/bin/env python3
"""Extracts the VU1 microcode from the ELF (DMA chain at D_00257C10, MPG packets) and disassembles it.
Usage: python3 tools/vu1dis.py [--elf disc/SCUS_971.97] [--addr 0x257C10] [--out build/vu1/ucode.txt]
The output is game code: it stays in build/ (ignored by git); never paste it into docs/issues.
Opcode tables checked against the runtime's VU1 interpreter (ps2_vu1_upper/lower.cpp)."""
import struct, sys, argparse, os

BC = "xyzw"
UP = {0x00: "ADD", 0x04: "SUB", 0x08: "MADD", 0x0C: "MSUB", 0x10: "MAX", 0x14: "MINI", 0x18: "MUL"}
UP2 = {0x1C: "MULq", 0x1D: "MAXi", 0x1E: "MULi", 0x1F: "MINIi", 0x20: "ADDq", 0x21: "MADDq", 0x22: "ADDi", 0x23: "MADDi",
       0x24: "SUBq", 0x25: "MSUBq", 0x26: "SUBi", 0x27: "MSUBi", 0x28: "ADD", 0x29: "MADD", 0x2A: "MUL", 0x2B: "MAX",
       0x2C: "SUB", 0x2D: "MSUB", 0x2E: "OPMSUB", 0x2F: "MINI"}
USP = {0x10: "ITOF0", 0x11: "ITOF4", 0x12: "ITOF12", 0x13: "ITOF15", 0x14: "FTOI0", 0x15: "FTOI4", 0x16: "FTOI12",
       0x17: "FTOI15", 0x1C: "MULAq", 0x1D: "ABS", 0x1E: "MULAi", 0x1F: "CLIP", 0x20: "ADDAq", 0x21: "MADDAq",
       0x22: "ADDAi", 0x23: "MADDAi", 0x24: "SUBAq", 0x25: "MSUBAq", 0x26: "SUBAi", 0x27: "MSUBAi", 0x28: "ADDA",
       0x29: "MADDA", 0x2A: "MULA", 0x2C: "SUBA", 0x2D: "MSUBA", 0x2E: "OPMULA", 0x2F: "NOP", 0x30: "NOP"}
UBC = {0x00: "ADDA", 0x04: "SUBA", 0x08: "MADDA", 0x0C: "MSUBA", 0x18: "MULA"}


def sx(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


def dest(i):
    s = "".join(c for k, c in enumerate("xyzw") if i >> (24 - k) & 1)
    return "." + s if s != "xyzw" else ""


def upper(i):
    ft, fs, fd = i >> 16 & 31, i >> 11 & 31, i >> 6 & 31
    d = dest(i)
    op = i & 0x3F
    if op < 0x1C:
        base, bc = op & ~3, BC[op & 3]
        if base == 0x18 or base in UP:
            n = UP[base]
            if n.endswith("A") or n == "MULA" or n in ("ADDA", "SUBA", "MADDA", "MSUBA"):
                pass
            return f"{UP[base]}{bc}{d} vf{fd}, vf{fs}, vf{ft}{bc}"
    if op in UP2:
        n = UP2[op]
        if n.endswith("q") or n.endswith("i"):
            return f"{n}{d} vf{fd}, vf{fs}, {n[-1]}"
        if n == "OPMSUB":
            return f"OPMSUB{d} vf{fd}, vf{fs}, vf{ft}"
        return f"{n}{d} vf{fd}, vf{fs}, vf{ft}"
    if op >= 0x3C:
        sp = (i & 3) | (i >> 4 & 0x7C)
        if sp < 0x10:
            return f"{UBC[sp & 0xC]}{BC[sp & 3]}{d} ACC, vf{fs}, vf{ft}{BC[sp & 3]}"
        if 0x18 <= sp < 0x1C:
            return f"MULA{BC[sp & 3]}{d} ACC, vf{fs}, vf{ft}{BC[sp & 3]}"
        n = USP.get(sp, f"UP?{sp:02x}")
        if n.startswith(("ITOF", "FTOI", "ABS")):
            return f"{n}{d} vf{ft}, vf{fs}"
        if n == "NOP":
            return "nop"
        if n == "CLIP":
            return f"CLIP vf{fs}xyz, vf{ft}w"
        if n.endswith(("q", "i")):
            return f"{n}{d} ACC, vf{fs}, {n[-1]}"
        return f"{n}{d} ACC, vf{fs}, vf{ft}"
    return f"UP?{op:02x}"


LSP = {0x30: "MOVE", 0x31: "MR32", 0x34: "LQI", 0x35: "SQI", 0x36: "LQD", 0x37: "SQD", 0x38: "DIV", 0x39: "SQRT",
       0x3A: "RSQRT", 0x3B: "WAITQ", 0x3C: "MTIR", 0x3D: "MFIR", 0x3E: "ILWR", 0x3F: "ISWR", 0x40: "RNEXT", 0x41: "RGET",
       0x42: "RINIT", 0x43: "RXOR", 0x64: "MFP", 0x68: "XTOP", 0x69: "XITOP", 0x6C: "XGKICK", 0x70: "ESADD",
       0x71: "ERSADD", 0x72: "ELENG", 0x73: "ERLENG", 0x74: "EATANxy", 0x75: "EATANxz", 0x76: "ESUM", 0x79: "ERSQRT",
       0x78: "ESQRT", 0x7A: "ERCPR", 0x7B: "WAITP", 0x7C: "ESIN", 0x7D: "EATAN", 0x7E: "EEXP"}  # PCSX2 LowerOP_T3_xx


def lower(i, pc):
    if i == 0 or i == 0x8000033C:
        return "nop"
    hi = i >> 25 & 0x7F
    ft, fs = i >> 16 & 31, i >> 11 & 31
    it, is_, id_ = ft, fs, i >> 6 & 31
    imm11 = sx(i & 0x7FF, 11)
    d = dest(i)
    if hi == 0x00: return f"LQ{d} vf{ft}, {imm11}(vi{is_})"
    if hi == 0x01: return f"SQ{d} vf{fs}, {imm11}(vi{it})"
    if hi == 0x04: return f"ILW{d} vi{it}, {imm11}(vi{is_})"
    if hi == 0x05: return f"ISW{d} vi{it}, {imm11}(vi{is_})"
    if hi in (0x08, 0x09):
        imm = (i & 0x7FF) | (i >> 10 & 0x7800)
        return f"{'IADDIU' if hi == 8 else 'ISUBIU'} vi{it}, vi{is_}, {imm:#x}"
    names = {0x10: "FCEQ", 0x11: "FCSET", 0x12: "FCAND", 0x13: "FCOR", 0x14: "FSEQ", 0x15: "FSSET", 0x16: "FSAND",
             0x17: "FSOR", 0x18: "FMEQ", 0x1A: "FMAND", 0x1B: "FMOR", 0x1C: "FCGET"}
    if hi in names: return f"{names[hi]} vi{it}, vi{is_}, {i & 0xFFFFFF:#x}"
    tgt = pc + 8 + imm11 * 8
    if hi == 0x20: return f"B {tgt:#06x}"
    if hi == 0x21: return f"BAL vi{it}, {tgt:#06x}"
    if hi == 0x24: return f"JR vi{is_}"
    if hi == 0x25: return f"JALR vi{it}, vi{is_}"
    br = {0x28: "IBEQ", 0x29: "IBNE", 0x2C: "IBLTZ", 0x2D: "IBGTZ", 0x2E: "IBLEZ", 0x2F: "IBGEZ"}
    if hi in (0x28, 0x29): return f"{br[hi]} vi{it}, vi{is_}, {tgt:#06x}"
    if hi in br: return f"{br[hi]} vi{is_}, {tgt:#06x}"
    if hi == 0x40:
        fn = i & 0x3F
        if fn == 0x30: return f"IADD vi{id_}, vi{is_}, vi{it}"
        if fn == 0x31: return f"ISUB vi{id_}, vi{is_}, vi{it}"
        if fn == 0x32: return f"IADDI vi{it}, vi{is_}, {sx(i >> 6 & 31, 5)}"
        if fn == 0x34: return f"IAND vi{id_}, vi{is_}, vi{it}"
        if fn == 0x35: return f"IOR vi{id_}, vi{is_}, vi{it}"
        if fn == 0x38: return f"CALLMS {i >> 6 & 0x7FFF:#x}"
        if fn == 0x39: return "CALLMSR"
        if fn >= 0x3C:
            sp = (i & 3) | (i >> 4 & 0x7C)
            n = LSP.get(sp, f"LO?{sp:02x}")
            fsf, ftf = i >> 21 & 3, i >> 23 & 3
            if n in ("MOVE", "MR32"): return f"{n}{d} vf{ft}, vf{fs}"
            if n in ("LQI", "LQD"): return f"{n}{d} vf{ft}, (vi{is_}{'++' if n == 'LQI' else '--'})"
            if n in ("SQI", "SQD"): return f"{n}{d} vf{fs}, (vi{it}{'++' if n == 'SQI' else '--'})"
            if n in ("DIV", "RSQRT"): return f"{n} Q, vf{fs}{BC[fsf]}, vf{ft}{BC[ftf]}"
            if n == "SQRT": return f"SQRT Q, vf{ft}{BC[ftf]}"
            if n == "MTIR": return f"MTIR vi{it}, vf{fs}{BC[fsf]}"
            if n == "MFIR": return f"MFIR{d} vf{ft}, vi{is_}"
            if n == "ILWR": return f"ILWR{d} vi{it}, (vi{is_})"
            if n == "ISWR": return f"ISWR{d} vi{it}, (vi{is_})"
            if n in ("XGKICK",): return f"XGKICK vi{is_}"
            if n in ("XTOP", "XITOP"): return f"{n} vi{it}"
            if n in ("RNEXT", "RGET"): return f"{n}{d} vf{ft}"
            if n in ("RINIT", "RXOR"): return f"{n} R, vf{fs}{BC[fsf]}"
            if n == "MFP": return f"MFP{d} vf{ft}, P"
            return f"{n} vf{fs}"
    return f"LO?{hi:02x} {i:08x}"


def flags(u):
    f = ""
    if u >> 31 & 1: f += " [I]"
    if u >> 30 & 1: f += " [E]"
    if u >> 29 & 1: f += " [M]"
    if u >> 28 & 1: f += " [D]"
    if u >> 27 & 1: f += " [T]"
    return f


def extract(elf, addr):
    d = open(elf, "rb").read()
    phoff = struct.unpack_from("<I", d, 0x1c)[0]
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from("<8I", d, phoff)
    rd = lambda a, l: d[off + a - va:off + a - va + l]
    img = {}
    a = addr
    while True:
        w0, w1 = struct.unpack("<2I", rd(a, 8))
        qwc, id_ = w0 & 0xFFFF, w0 >> 28 & 7
        p, end = a + 8, a + 16 + qwc * 16   # the tag's 2 upper words are VIF codes too
        while p < end:
            v = struct.unpack("<I", rd(p, 4))[0]
            p += 4
            if v >> 24 == 0x4A:
                num = (v >> 16 & 0xFF) or 256
                img[(v & 0xFFFF) * 8] = rd(p, num * 8)
                p += num * 8
        if id_ != 1:   # 1 = cnt (data follows); 0 = refe, 7 = end: stop
            break
        a = end
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", default="disc/SCUS_971.97")
    ap.add_argument("--addr", default="0x257C10")
    ap.add_argument("--out", default="build/vu1/ucode.txt")
    a = ap.parse_args()
    img = extract(a.elf, int(a.addr, 0))
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w") as f:
        for base in sorted(img):
            blob = img[base]
            f.write(f"; --- MPG @ {base:#06x} ({len(blob) // 8} instr)\n")
            for k in range(0, len(blob), 8):
                lo, up = struct.unpack_from("<2I", blob, k)
                pc = base + k
                lw = f"{lower(lo, pc)}" if not up >> 31 & 1 else f"IMM {lo:#010x}"
                f.write(f"{pc:04x}: {up:08x} {lo:08x}  {upper(up):<44} | {lw}{flags(up)}\n")
    print({hex(k): len(v) // 8 for k, v in sorted(img.items())}, "->", a.out)


if __name__ == "__main__":
    main()
