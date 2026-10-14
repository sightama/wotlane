"""Gera C++ nativo para os microprogramas do VU0 do WotM (wotm_vu0_native.gen.inc, incluido por
ps2xRuntime/src/lib/vu/wotm_vu0_native.cpp no runtime do PS2Recomp).

O microcodigo sai do ELF (pacote VIF com MPG que o jogo manda ao VU0) ou de um vu0_code.bin (PS2X_DUMP_VU0). O C++ gerado
reproduz o VU1Interpreter do runtime bit a bit: operandos normalizados, conta em float com arredondamento para zero e o
resultado classificado pelo valor em double, flags MAC/status publicadas 4 ciclos depois da emissao (o fmand/fsand ve so as
que ja chegaram), stores e escritas de VI/VF na ordem do programa (o VU para em dependencia de VF/VI/ACC, entao a semantica
sequencial vale) e o par dividido como no hardware (o lower le os registradores antes da escrita do upper do mesmo par).
Os ciclos de emissao vem de um modelo das paradas do interpretador (latencias de decodeUpperUsage/decodeLowerUsage).

So programas em linha reta com as instrucoes suportadas viram C++; os demais (desvios, DIV, EFU...) seguem no
interpretador, e o gerador diz por que. O .inc gerado e traducao do codigo do jogo: fica fora do repo.

Uso: python tools/vu0native.py <SCUS_971.97 | vu0_code.bin> <saida .inc> [--trace trace.txt]
     --trace confere o modelo de ciclos contra a saida do vu0trace (bancada do runtime).
"""
import struct
import sys

# Pacote do microcodigo do VU0 no ELF: tag DMA (cnt) seguida de codigos VIF (NOP, MPG); offset no arquivo do NTSC-U.
ELF_VU0_PACKET = 0x15C890
# Entradas usadas pelo jogo (vcallms)
ENTRIES = [0x0, 0xC8, 0x1A8, 0x398, 0x3F8, 0x430, 0x4D0, 0x748, 0x8D8, 0x970, 0xA00, 0xA80, 0xB00, 0xB48]
CODE_SIZE = 4096
LANES = (8, 4, 2, 1)   # x y z w


class Unsupported(Exception):
    pass


def load_code(path):
    data = open(path, 'rb').read()
    if len(data) == CODE_SIZE:
        return data
    code = bytearray(CODE_SIZE)
    tag = struct.unpack_from('<Q', data, ELF_VU0_PACKET)[0]
    qwc, tid = tag & 0xFFFF, (tag >> 28) & 7
    if tid != 1:
        raise SystemExit(f'tag DMA inesperado em {ELF_VU0_PACKET:#x}: {tag:#x}')
    pos, end = ELF_VU0_PACKET + 8, ELF_VU0_PACKET + 16 + qwc * 16
    while pos < end:
        vif = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        cmd = (vif >> 24) & 0x7F
        if cmd == 0x00:
            continue
        if cmd != 0x4A:
            raise SystemExit(f'codigo VIF inesperado {vif:#x} em {pos - 4:#x}')
        num = (vif >> 16) & 0xFF or 256
        addr = (vif & 0xFFFF) * 8
        pos = (pos + 7) & ~7
        code[addr:addr + num * 8] = data[pos:pos + num * 8]
        pos += num * 8
    return bytes(code)


def lanes_of(i):
    return (i >> 21) & 0xF


# ---------------------------------------------------------------------------------------------------------------------
# Decodificacao (so o subconjunto usado pelos programas em linha reta)

FORMS = {0: 'add', 1: 'sub', 2: 'madd', 3: 'msub', 4: 'max', 5: 'mini', 6: 'mul'}
QI = {0x1C: ('mul', 'q'), 0x1D: ('max', 'i'), 0x1E: ('mul', 'i'), 0x1F: ('mini', 'i'), 0x20: ('add', 'q'), 0x21: ('madd', 'q'),
      0x22: ('add', 'i'), 0x23: ('madd', 'i'), 0x24: ('sub', 'q'), 0x25: ('msub', 'q'), 0x26: ('sub', 'i'), 0x27: ('msub', 'i')}
VEC = {0x28: 'add', 0x29: 'madd', 0x2A: 'mul', 0x2B: 'max', 0x2C: 'sub', 0x2D: 'msub', 0x2F: 'mini'}


def decode_upper(i):
    """dict: form, dst ('vf', r) | ('acc',), mask, fs, b (('bc', ft, comp) | ('q',) | ('i',) | ('vec', ft)); ou None (nop)."""
    op = i & 0x3F
    mask, ft, fs, fd = lanes_of(i), (i >> 16) & 31, (i >> 11) & 31, (i >> 6) & 31
    if op < 0x1C:
        return dict(form=FORMS[op >> 2], dst=('vf', fd), mask=mask, fs=fs, b=('bc', ft, op & 3))
    if op in QI:
        form, src = QI[op]
        return dict(form=form, dst=('vf', fd), mask=mask, fs=fs, b=(src,))
    if op in VEC:
        return dict(form=VEC[op], dst=('vf', fd), mask=mask, fs=fs, b=('vec', ft))
    if op >= 0x3C:
        sp = (i & 3) | ((i >> 4) & 0x7C)
        if sp in (0x2F, 0x30):
            return None
        if sp < 0x10 or 0x18 <= sp < 0x1C:
            return dict(form=FORMS[sp >> 2], dst=('acc',), mask=mask, fs=fs, b=('bc', ft, sp & 3))
        if sp in QI and QI[sp][0] not in ('max', 'mini'):
            form, src = QI[sp]
            return dict(form=form, dst=('acc',), mask=mask, fs=fs, b=(src,))
        if sp in (0x28, 0x29, 0x2A, 0x2C, 0x2D):
            return dict(form=VEC[sp], dst=('acc',), mask=mask, fs=fs, b=('vec', ft))
        raise Unsupported(f'upper especial {sp:#x}')
    raise Unsupported(f'upper {op:#x}')


def imm11(i):
    v = i & 0x7FF
    return v - 0x800 if v & 0x400 else v


def decode_lower(i):
    if i in (0, 0x8000033C):
        return None
    op = (i >> 25) & 0x7F
    t, s = (i >> 16) & 31, (i >> 11) & 31
    vt, vs, vd = (i >> 16) & 15, (i >> 11) & 15, (i >> 6) & 15
    mask = lanes_of(i)
    if op == 0x00:
        return dict(op='lq', mask=mask, ft=t, vis=vs, imm=imm11(i))
    if op == 0x01:
        return dict(op='sq', mask=mask, fs=s, vit=vt, imm=imm11(i))
    if op == 0x08:
        return dict(op='iaddiu', vit=vt, vis=vs, imm=(i & 0x7FF) | ((i >> 10) & 0x7800))
    if op == 0x16:
        return dict(op='fsand', vit=vt, imm=(((i >> 21) & 1) << 11) | (i & 0x7FF))
    if op == 0x1A:
        return dict(op='fmand', vit=vt, vis=vs)
    if op == 0x40:
        lo = i & 0x3F
        if lo == 0x35:
            return dict(op='ior', vid=vd, vis=vs, vit=vt)
        if lo >= 0x3C and ((i & 3) | ((i >> 4) & 0x7C)) == 0x30:
            return dict(op='move', mask=mask, ft=t, fs=s)
    raise Unsupported(f'lower {i:#010x}')


def decode_program(code, start):
    pairs, pc, ebit_at = [], start, None
    while True:
        if pc + 8 > CODE_SIZE:
            raise Unsupported('passou do fim da memoria de codigo')
        lo, up = struct.unpack_from('<II', code, pc)
        if up & 0x38000000:
            raise Unsupported(f'bit M/D/T em {pc:#x}')
        ibit, ebit = bool(up & 0x80000000), bool(up & 0x40000000)
        pairs.append(dict(pc=pc, upper=decode_upper(up), lower=None if ibit else decode_lower(lo), loi=lo if ibit else None))
        if ebit_at is not None:
            return pairs, pc + 8
        if ebit:
            ebit_at = pc
        pc += 8


# ---------------------------------------------------------------------------------------------------------------------
# Modelo de ciclos (VU1Interpreter::calculatePairReadyCycle / markPairWrites, laco do run e flushPipelines)

def upper_usage(u):
    reads, writes, acc_read, acc_write = [], None, 0, 0
    if u is None:
        return reads, writes, acc_read, acc_write
    reads.append((u['fs'], u['mask']))
    b = u['b']
    if b[0] == 'bc':
        reads.append((b[1], LANES[b[2]]))
    elif b[0] == 'vec':
        reads.append((b[1], u['mask']))
    if u['form'] in ('madd', 'msub'):
        acc_read = u['mask']
    if u['dst'][0] == 'acc':
        acc_write = u['mask']
    elif u['dst'][1] != 0:
        writes = (u['dst'][1], u['mask'], 4)
    return reads, writes, acc_read, acc_write


def lower_usage(l):
    """(vf lidos, vf escrito (reg, mask, lat), vi lidos, vi escrito (reg, lat), store)"""
    if l is None:
        return [], None, [], None, False
    op = l['op']
    if op == 'lq':
        return [], (l['ft'], l['mask'], 4) if l['ft'] else None, [l['vis']], None, False
    if op == 'sq':
        return [(l['fs'], l['mask'])], None, [l['vit']], None, True
    if op == 'iaddiu':
        return [], None, [l['vis']], (l['vit'], 1), False
    if op == 'fsand':
        return [], None, [], (l['vit'], 1), False
    if op == 'fmand':
        return [], None, [l['vis']], (l['vit'], 1), False
    if op == 'ior':
        return [], None, [l['vis'], l['vit']], (l['vid'], 1), False
    if op == 'move':
        return [(l['fs'], l['mask'])], (l['ft'], l['mask'], 4) if l['ft'] else None, [], None, False
    raise Unsupported(op)


def schedule(pairs):
    """Ciclo de emissao de cada par; ciclo final (o 'cycles' do estado ao fim do programa)."""
    vf_ready = {}
    vi_ready = {}
    acc_ready = {}
    cycle, pending = 0, 0
    issues = []
    for p in pairs:
        ur, uw, uar, uaw = upper_usage(p['upper'])
        lr, lw, lvr, lvw, store = lower_usage(p['lower'])
        ready = cycle
        for reg, mask in ur[:2] + lr[:2]:
            for c in range(4):
                if mask & LANES[c]:
                    ready = max(ready, vf_ready.get((reg, c), 0))
        for reg in lvr:
            if reg:
                ready = max(ready, vi_ready.get(reg, 0))
        for c in range(4):
            if uar & LANES[c]:
                ready = max(ready, acc_ready.get(c, 0))
        cycle = ready
        issues.append(cycle)
        for w in (lw, uw):
            if w:
                for c in range(4):
                    if w[1] & LANES[c]:
                        vf_ready[(w[0], c)] = cycle + w[2]
                pending = max(pending, cycle + w[2])
        if lvw and lvw[0]:
            vi_ready[lvw[0]] = cycle + lvw[1]
            pending = max(pending, cycle + lvw[1])
        for c in range(4):
            if uaw & LANES[c]:
                acc_ready[c] = cycle + 1
        if uaw:
            pending = max(pending, cycle + 1)
        if p['upper'] is not None and p['upper']['form'] not in ('max', 'mini') and p['upper']['mask']:
            pending = max(pending, cycle + 4)   # entrada do pipeline de flags
        if store:
            pending = max(pending, cycle + 1)
        cycle += 1
    return issues, max(cycle, pending)


# ---------------------------------------------------------------------------------------------------------------------
# Emissao

def flt(bits):
    return f'B({bits:#010x}u)'


def operand_b(u, c):
    b = u['b']
    if b[0] == 'bc':
        return f's.vf[{b[1]}][{b[2]}]'
    if b[0] == 'vec':
        return f's.vf[{b[1]}][{c}]'
    return 's.q' if b[0] == 'q' else 's.i'


def emit_upper(u, ent, out):
    """Calcula o resultado do upper em u0..u3 (sem escrever); devolve as linhas de escrita."""
    form = u['form']
    for c in range(4):
        if not (u['mask'] & LANES[c]):
            continue
        a, b = f'N(s.vf[{u["fs"]}][{c}])', f'N({operand_b(u, c)})'
        if form in ('max', 'mini'):
            cmp = '>' if form == 'max' else '<'
            out.append(f'        const float a{c} = {a}, b{c} = {b};')
            out.append(f'        const float u{c} = a{c} {cmp} b{c} ? a{c} : b{c};')
            continue
        out.append(f'        const float a{c} = {a}, b{c} = {b};')
        if form in ('madd', 'msub'):
            sign = '+' if form == 'madd' else '-'
            out.append(f'        const float c{c} = N(s.acc[{c}]);')
            out.append(f'        const float u{c} = L(c{c} {sign} a{c} * b{c}, (double)c{c} {sign} (double)a{c} * (double)b{c}, {LANES[c]}u, f{ent});')
            out.append(f'        P(a{c}, b{c}, f{ent});')
        else:
            sym = {'add': '+', 'sub': '-', 'mul': '*'}[form]
            out.append(f'        const float u{c} = L(a{c} {sym} b{c}, (double)a{c} {sym} (double)b{c}, {LANES[c]}u, f{ent});')
    dst = 's.acc' if u['dst'][0] == 'acc' else (None if u['dst'][1] == 0 else f's.vf[{u["dst"][1]}]')
    writes = []
    if dst:
        for c in range(4):
            if u['mask'] & LANES[c]:
                writes.append(f'        {dst}[{c}] = u{c};')
    return writes


def emit_lower(l, upper_dst, out):
    """Executa o lower (le o estado antes da escrita do upper); escritas de VF ficam para depois do upper."""
    op = l['op']
    if op == 'lq':
        out.append(f'        const uint32_t la = addr16(s.vi[{l["vis"]}], {l["imm"]});')
        if l['ft'] == 0 or l['ft'] == upper_dst:
            return []
        return [f'        std::memcpy(&s.vf[{l["ft"]}][{c}], d + la + {4 * c}, 4);' for c in range(4) if l['mask'] & LANES[c]]
    if op == 'sq':
        out.append(f'        const uint32_t sa = addr16(s.vi[{l["vit"]}], {l["imm"]});')
        for c in range(4):
            if l['mask'] & LANES[c]:
                out.append(f'        std::memcpy(d + sa + {4 * c}, &s.vf[{l["fs"]}][{c}], 4);')
        return []
    if op == 'iaddiu':
        if l['vit']:
            out.append(f'        s.vi[{l["vit"]}] = (int16_t)(int16_t)(s.vi[{l["vis"]}] + {l["imm"]});')
        return []
    if op == 'fsand':
        if l['vit']:
            out.append(f'        s.vi[{l["vit"]}] = (int16_t)((s.status & 0xFFFu) & {l["imm"]:#x}u);')
        return []
    if op == 'fmand':
        if l['vit']:
            out.append(f'        s.vi[{l["vit"]}] = (int16_t)(s.mac & (uint32_t)(uint16_t)s.vi[{l["vis"]}]);')
        return []
    if op == 'ior':
        if l['vid']:
            out.append(f'        s.vi[{l["vid"]}] = (int16_t)(s.vi[{l["vis"]}] | s.vi[{l["vit"]}]);')
        return []
    if op == 'move':
        if l['ft'] == 0 or l['ft'] == upper_dst:
            return []
        for c in range(4):
            if l['mask'] & LANES[c]:
                out.append(f'        const float m{c} = s.vf[{l["fs"]}][{c}];')
        return [f'        s.vf[{l["ft"]}][{c}] = m{c};' for c in range(4) if l['mask'] & LANES[c]]
    raise Unsupported(op)


def disasm_upper(u):
    if u is None:
        return 'nop'
    m = ''.join(ch for ch, b in zip('xyzw', LANES) if u['mask'] & b)
    dst = 'ACC' if u['dst'][0] == 'acc' else f'vf{u["dst"][1]}'
    b = u['b']
    bs = {'bc': lambda: f'vf{b[1]}{"xyzw"[b[2]]}', 'vec': lambda: f'vf{b[1]}', 'q': lambda: 'Q', 'i': lambda: 'I'}[b[0]]()
    return f'{u["form"]}{"a" if u["dst"][0] == "acc" else ""}.{m} {dst}, vf{u["fs"]}, {bs}'


def emit_program(code, start):
    pairs, end = decode_program(code, start)
    issues, total = schedule(pairs)
    body, entries = [], []   # entries: (ciclo em que fica visivel, indice)
    committed = 0
    for p, issue in zip(pairs, issues):
        while committed < len(entries) and entries[committed][0] <= issue:
            body.append(f'    commit(s, f{entries[committed][1]});')
            committed += 1
        u, l = p['upper'], p['lower']
        lo_txt = f'loi {p["loi"]:#010x}' if p['loi'] is not None else (l['op'] if l else 'nop')
        body.append(f'    {{ // {p["pc"]:04x} @{issue}: {disasm_upper(u)} | {lo_txt}')
        writes = []
        upper_dst = None
        if u is not None:
            ent = len(entries)
            flags = u['form'] not in ('max', 'mini') and u['mask'] != 0
            if flags:
                entries.append((issue + 4, ent))
            writes = emit_upper(u, ent, body)
            if u['dst'][0] == 'vf':
                upper_dst = u['dst'][1]
        lower_writes = emit_lower(l, upper_dst, body) if l else []
        if p['loi'] is not None:
            body.append(f'        s.i = N({flt(p["loi"])});')
        body.extend(writes)
        body.extend(lower_writes)
        body.append('    }')
    while committed < len(entries):
        body.append(f'    commit(s, f{entries[committed][1]});')
        committed += 1
    body.append(f'    finish(s, {end & (CODE_SIZE - 1):#x}u, {total}u);')
    decl = '    Fm ' + ', '.join(f'f{k}' for _, k in entries) + ';' if entries else ''
    name = f'vu0_{start:04x}'
    fn = [f'static void {name}(VU1State &s, uint8_t *d)', '{']
    if decl:
        fn.append(decl)
    fn += body + ['}', '']
    return name, end - start, fnv1a(code[start:end]), fn, issues, total


def fnv1a(b):
    h = 0xcbf29ce484222325
    for x in b:
        h = ((h ^ x) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def check_trace(path, results):
    issues_ref, total_ref, prog = {}, {}, None
    for ln in open(path):
        f = ln.split()
        if f[0] == 'prog':
            prog = int(f[1], 16)
            total_ref[prog] = int(f[3])
            issues_ref[prog] = []
        elif f[0] == 'pair':
            issues_ref[prog].append(int(f[2]))
    bad = 0
    for start, (issues, total) in results.items():
        if start not in issues_ref:
            continue
        if issues != issues_ref[start] or total != total_ref[start]:
            bad += 1
            print(f'  ciclos divergem em {start:#x}: modelo {issues} total {total}; interpretador {issues_ref[start]} total {total_ref[start]}')
    print(f'modelo de ciclos x interpretador: {len([s for s in results if s in issues_ref])} programas, {bad} divergentes')
    return bad == 0


def main():
    args = sys.argv[1:]
    trace = None
    if '--trace' in args:
        k = args.index('--trace')
        trace = args[k + 1]
        del args[k:k + 2]
    code = load_code(args[0])
    out = ['// Gerado por tools/vu0native.py a partir do microcodigo do VU0 do jogo. Nao editar; nao versionar.', '']
    table, results = [], {}
    for start in ENTRIES:
        try:
            name, length, h, fn, issues, total = emit_program(code, start)
        except Unsupported as e:
            print(f'{start:#05x}: fica no interpretador ({e})')
            continue
        out += fn
        table.append(f'    {{{start:#x}u, {length}u, {h:#018x}ull, {name}}},')
        results[start] = (issues, total)
        print(f'{start:#05x}: {length // 8} pares, {total} ciclos')
    out.append('static const WotmVu0Program kPrograms[] = {')
    out += table
    out.append('};')
    out.append('static constexpr size_t kProgramCount = sizeof(kPrograms) / sizeof(kPrograms[0]);')
    if trace and not check_trace(trace, results):
        raise SystemExit(1)
    with open(args[1], 'w', newline='\n') as f:
        f.write('\n'.join(out) + '\n')


main()
