#!/usr/bin/env python3
"""Batch oracle: for each equivalent function with special VU0/FPU instructions and NO calls (a leaf), generates a harness
with arguments derived from the mangled name, runs the recompiled retail vs our recompiled version and classifies the result.
Runs inside WSL.  Output: ~/wotm-recomp/work/oracle_all.csv"""
import csv, os, re, subprocess, sys, glob, concurrent.futures as cf

HOME = os.path.expanduser("~")
W = f"{HOME}/wotm-recomp/work"; P = f"{HOME}/wotm-recomp/PS2Recomp"
REPO = os.environ.get("WOTM_REPO", os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
OUT = f"{W}/oracle_batch"; os.makedirs(OUT, exist_ok=True)

VU = re.compile(r"\b(lqc2|sqc2|qmtc2|qmfc2|cfc2|ctc2|vadd\w*|vsub\w*|vmul\w*|vmadd\w*|vmsub\w*|vopmula|vopmsub|vdiv|vsqrt|vrsqrt|vmax\w*|vmini\w*|vabs|vftoi\w*|vitof\w*|vmove|vmr32|vclip\w*)\b")
SP = re.compile(r"\b(min\.s|max\.s|madd\.s|msub\.s|mula\.s|madda\.s|msuba\.s|rsqrt\.s|adda\.s|suba\.s)\b")

def parse_sig(mangled):
    if "__" not in mangled: return None
    # the last "__" separates name/rest; names with an inner "__" (ctor "__10AiDodgeRam") are rare: skip them
    name, rest = mangled.rsplit("__", 1)[0], mangled.rsplit("__", 1)[1]
    if not name: return None
    args = []; has_this = False
    if rest[:1].isdigit():
        mm = re.match(r"(\d+)", rest); n = int(mm.group(1)); rest = rest[mm.end() + n:]; has_this = True
    elif rest[:1] == "F":
        rest = rest[1:]
    else:
        return None
    if has_this: args.append("p")
    kinds = []
    i = 0
    while i < len(rest):
        c = rest[i]
        if c == "R" or c == "P":
            i += 1
            if rest[i:i+1] == "C": i += 1
            if rest[i:i+1].isdigit():
                mm = re.match(r"(\d+)", rest[i:]); n = int(mm.group(1)); i += mm.end() + n
            elif rest[i:i+1] == "A":      # PA3_f
                mm = re.match(r"A\d+_[a-zA-Z]", rest[i:]);
                if not mm: return None
                i += mm.end()
            elif rest[i:i+1] in "cfiUsb":
                i += 2 if rest[i] == "U" else 1
            else: return None
            kinds.append("p")
        elif c == "U":
            i += 2; kinds.append("i")
        elif c in "icsblb": i += 1; kinds.append("i")
        elif c == "f": i += 1; kinds.append("f")
        elif c == "T":
            mm = re.match(r"T(\d)", rest[i:]);
            if not mm: return None
            k = int(mm.group(1)) - 1
            if k >= len(kinds): return None
            kinds.append(kinds[k]); i += mm.end()
        elif c == "v": i += 1
        else: return None
    return "".join(args) + "".join(kinds)

TEMPLATE = r'''
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <random>
#include <vector>
extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x0u;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x01000000u;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = (g_ps2RecompiledFunctionTableEnd - g_ps2RecompiledFunctionTableBase) >> 2;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[g_ps2RecompiledFunctionTableSlotCount] = {};
void RET_SYM(uint8_t *, R5900Context *, PS2Runtime *);
void NM_SYM(uint8_t *, R5900Context *, PS2Runtime *);
static const uint32_t kImg = 0x00100000u, kImgFile = 0x5f8b44u, kImgMem = 0x780f10u, kGp = 0x006FF8F0u;
static std::vector<uint8_t> g_img;
static const uint32_t kArena = 0x01000000u, kSize = 0x4000u, kSp = kArena + 0x3F00u, kCmp = 0x3A00u, kPtrRange = 0x3000u;
struct Res { uint32_t f0, v0; std::vector<uint8_t> mem, img; };
static const char *kSpec = ARGSPEC;
static Res run(PS2Runtime &rt, uint8_t *rdram, void (*fn)(uint8_t *, R5900Context *, PS2Runtime *), const std::vector<uint8_t> &init,
               const std::vector<uint32_t> &iv, const std::vector<float> &fv) {
    std::memcpy(rdram + kImg, g_img.data(), g_img.size());
    std::memcpy(rdram + kArena, init.data(), kSize);
    R5900Context ctx{};
    SET_GPR_U32((&ctx), 28, kGp);
    ctx.vu0_vf[0] = _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f); // vf0 = (0,0,0,1)
    int ni = 0, nf = 0;
    for (const char *s = kSpec; *s; ++s) {
        if (*s == 'f') { ctx.f[12 + nf] = fv[nf]; ++nf; }
        else { SET_GPR_U32((&ctx), 4 + ni, iv[ni]); ++ni; }
    }
    SET_GPR_U32((&ctx), 29, kSp); SET_GPR_U32((&ctx), 31, 0x1000u);
    fn(rdram, &ctx, &rt);
    Res r; std::memcpy(&r.f0, &ctx.f[0], 4); r.v0 = GPR_U32((&ctx), 2);
    r.mem.assign(rdram + kArena, rdram + kArena + kSize);
    r.img.assign(rdram + kImg, rdram + kImg + g_img.size());
    return r;
}
static bool near(uint32_t a, uint32_t b) {
    float x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4);
    if (std::isnan(x) && std::isnan(y)) return true;
    if (!std::isfinite(x) || !std::isfinite(y)) return a == b;
    return std::fabs(x - y) <= 1e-3f * std::fmax(std::fabs(x), std::fabs(y)) + 1e-3f;
}
int main() {
    PS2Runtime rt; if (!rt.memory().initialize()) return 2;
    uint8_t *rdram = rt.memory().getRDRAM();
    { FILE *f = std::fopen(IMGPATH, "rb"); if (!f) return 3; g_img.assign(kImgMem, 0); std::fseek(f, 0x1000, SEEK_SET);
      if (std::fread(g_img.data(), 1, kImgFile, f) != kImgFile) return 4; std::fclose(f); }
    std::mt19937 rng(777);
    std::uniform_real_distribution<float> fl(-100.f, 100.f);
    int exact = 0, approx = 0, retonly = 0, diff = 0, N = 200;
    for (int n = 0; n < N; ++n) {
        std::vector<uint8_t> init(kSize);
        for (uint32_t o = 0; o < kSize; o += 4) {
            uint32_t w; int k = rng() % 10;
            if (k < 4) { float f = fl(rng); std::memcpy(&w, &f, 4); }
            else if (k < 7) w = kArena + ((rng() % (kPtrRange / 16)) * 16);
            else if (k < 9) w = rng() % 17;
            else w = rng();
            std::memcpy(&init[o], &w, 4);
        }
        std::vector<uint32_t> iv; std::vector<float> fv;
        for (const char *s = kSpec; *s; ++s) {
            if (*s == 'p') iv.push_back(kArena + ((rng() % (kPtrRange / 16)) * 16));
            else if (*s == 'i') iv.push_back(rng() % 17);
            else fv.push_back(fl(rng));
        }
        Res a = run(rt, rdram, RET_SYM, init, iv, fv);
        Res b = run(rt, rdram, NM_SYM, init, iv, fv);
        bool memEx = std::memcmp(a.mem.data(), b.mem.data(), kCmp) == 0 && a.img == b.img;
        bool retEx = a.f0 == b.f0 && a.v0 == b.v0;
        if (memEx && retEx) { ++exact; continue; }
        bool memAp = true;
        for (uint32_t o = 0; memAp && o < kCmp; o += 4) {
            uint32_t x, y; std::memcpy(&x, &a.mem[o], 4); std::memcpy(&y, &b.mem[o], 4);
            if (x != y && !near(x, y)) memAp = false;
        }
        for (size_t o = 0; memAp && o + 4 <= a.img.size(); o += 4) {
            uint32_t x, y; std::memcpy(&x, &a.img[o], 4); std::memcpy(&y, &b.img[o], 4);
            if (x != y && !near(x, y)) memAp = false;
        }
        bool retAp = near(a.f0, b.f0) && (a.v0 == b.v0 || near(a.v0, b.v0));
        if (memAp && retAp) { ++approx; continue; }
        if (memAp) { ++retonly; continue; }   // memory matches, only f0/v0 differ (void or a return value that cannot be compared)
        {
            if (diff == 0) {
                std::printf("1st diff: f0 %08x/%08x v0 %08x/%08x;", a.f0, b.f0, a.v0, b.v0);
                for (uint32_t o = 0; o < kCmp; o += 4) if (std::memcmp(&a.mem[o], &b.mem[o], 4)) { uint32_t x, y; std::memcpy(&x,&a.mem[o],4); std::memcpy(&y,&b.mem[o],4); std::printf(" arena+0x%x: %08x/%08x;", o, x, y); break; }
                for (size_t o = 0; o < a.img.size(); o += 4) if (std::memcmp(&a.img[o], &b.img[o], 4)) { uint32_t x, y; std::memcpy(&x,&a.img[o],4); std::memcpy(&y,&b.img[o],4); std::printf(" img 0x%zx: %08x/%08x;", o + kImg, x, y); break; }
                std::printf("EOL");
            }
            ++diff;
        }
    }
    std::printf("RESULT exact=%d approx=%d retonly=%d diff=%d\n", exact, approx, retonly, diff);
    return 0;
}
'''

def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kw)

FL = f"-std=c++20 -O1 -msse4.1 -mavx -w -I{P}/ps2xRuntime/include -I{P}/ps2xIOP/include -I{P}/ps2xRuntime/src/lib/Kernel"
LINK = (f"{P}/out/rt/ps2xRuntime/libps2_runtime.a {P}/out/rt/_deps/raylib-build/raylib/libraylib.a {P}/out/rt/ps2xIOP/libps2_iop.a "
        "$(pkg-config --libs libavcodec libavformat libavutil libswresample libswscale) -lGL -lX11 -lXrandr -lXinerama -lXcursor -lXi -lm -lpthread -ldl")

def work(item):
    tu, fn, addr, spec = item
    d = f"{OUT}/{re.sub(r'[^A-Za-z0-9]', '_', fn)}"; os.makedirs(d, exist_ok=True)
    rf = glob.glob(f"{W}/output_fix/{fn}_0x{addr:x}.cpp")
    nf = [x for x in glob.glob(f"{W}/output_nm_all/{fn}_0x*.cpp")]
    if not rf or len(nf) != 1: return (tu, fn, "sem_arquivo", "")
    rt_, nm_ = open(rf[0]).read(), open(nf[0]).read()
    if re.search(r"GuestBranchKind::(DirectCall|IndirectCall)", rt_) or re.search(r"GuestBranchKind::(DirectCall|IndirectCall)", nm_):
        return (tu, fn, "nao_folha", "")
    ret_sym = os.path.basename(rf[0])[:-4]; nm_sym = os.path.basename(nf[0])[:-4]
    # the NM version that fit at the retail address has the same symbol: rename it on the NM side only
    nm_def = f"-D{nm_sym}={nm_sym}_nm" if nm_sym == ret_sym else ""
    if nm_def: nm_sym += "_nm"
    open(f"{d}/h.cpp", "w").write(TEMPLATE)
    cmds = [
        f"g++ {FL} -I{W}/output_fix -c {rf[0]} -o {d}/r.o",
        f"g++ {FL} -I{W}/output_nm_all {nm_def} -c {nf[0]} -o {d}/n.o",
        f"g++ {FL} -I{W}/output_fix -DRET_SYM={ret_sym} -DNM_SYM={nm_sym} '-DARGSPEC=\"{spec}\"' '-DIMGPATH=\"{W}/game.elf\"' -c {d}/h.cpp -o {d}/h.o",
        f"g++ -o {d}/o {d}/h.o {d}/r.o {d}/n.o {LINK} 2>&1 | grep -v 'warning: relocation' | head -3",
    ]
    for c in cmds:
        r = sh(c)
        if r.returncode != 0 or (r.stdout.strip() and "undefined" in r.stdout):
            return (tu, fn, "erro_build", (r.stderr or r.stdout)[:200].replace("\n", " "))
    r = sh(f"cd {d} && timeout 60 ./o")
    m = re.search(r"RESULT (.*)", r.stdout)
    extra = " | ".join(l.strip() for l in r.stdout.splitlines() if not l.startswith("RESULT"))
    return (tu, fn, "ok" if m else "erro_run", (m.group(1) if m else (r.stderr[:100] or str(r.returncode))) + ((" || " + extra) if extra else ""))

def main():
    items = []; skipped = {"sig": 0}
    for r in csv.DictReader(open(f"{REPO}/config/status.csv")):
        if r["state"] != "equivalent": continue
        p = f"{REPO}/asm/nonmatchings/{r['tu']}/{r['function']}.s"
        if not os.path.exists(p): continue
        t = open(p, errors="ignore").read()
        if not (VU.search(t) or SP.search(t)): continue
        spec = parse_sig(r["function"])
        if spec is None: skipped["sig"] += 1; continue
        items.append((r["tu"], r["function"], int(r["address"], 16), spec))
    print(f"candidates: {len(items)} (unsupported signature: {skipped['sig']})", flush=True)
    only = sys.argv[1:]
    if only: items = [i for i in items if any(o in i[1] for o in only)]
    with cf.ThreadPoolExecutor(6) as ex:
        res = list(ex.map(work, items))
    with open(f"{W}/oracle_all.csv", "w") as f:
        f.write("tu,function,status,detalhe\n")
        for r in res: f.write(",".join(x.replace(",", ";") for x in r) + "\n")
    from collections import Counter
    print(Counter(r[2] for r in res))
    for r in res:
        if r[2] == "ok": print(f"{r[0]:22s} {r[1][:60]:60s} {r[3]}")
        elif r[2] in ("erro_build", "erro_run"): print(f"{r[0]:22s} {r[1][:60]:60s} {r[2]} {r[3][:90]}")

main()
