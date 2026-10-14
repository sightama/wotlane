"""ramdump.py <elf> <out.bin> [process]: dump the 32 MB of EE RAM of the running PCSX2 (found by matching the ELF's code at 0x100100).
process defaults to pcsx2-qt; pass ps2EntryRunner for the recompiled runtime."""
import ctypes, ctypes.wintypes as wt, subprocess, sys
from elftools.elf.elffile import ELFFile

elf = ELFFile(open(sys.argv[1], 'rb'))
seg0 = [s for s in elf.iter_segments() if s['p_type'] == 'PT_LOAD'][0]
anchor = seg0.data()[0x100:0x100 + 48]
k = ctypes.WinDLL('kernel32', use_last_error=True)
pid = int(subprocess.check_output(['powershell', '-NoProfile', '-Command', '(Get-Process %s).Id' % (sys.argv[3] if len(sys.argv) > 3 else 'pcsx2-qt')]).split()[0])
h = k.OpenProcess(0x0410, False, pid)


class MBI(ctypes.Structure):
    _fields_ = [('Base', ctypes.c_void_p), ('AllocBase', ctypes.c_void_p), ('AllocProt', wt.DWORD), ('Pad', wt.DWORD),
                ('Size', ctypes.c_size_t), ('State', wt.DWORD), ('Prot', wt.DWORD), ('Type', wt.DWORD)]


def regions():
    addr = 0
    mbi = MBI()
    while k.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
        if mbi.State == 0x1000 and mbi.Prot in (0x02, 0x04, 0x20, 0x40) and mbi.Size >= 0x1000:
            yield mbi.Base or 0, mbi.Size
        addr = (mbi.Base or 0) + mbi.Size
        if addr >= 1 << 47:
            break


def read(a, n):
    buf = ctypes.create_string_buffer(n)
    got = ctypes.c_size_t()
    if not k.ReadProcessMemory(h, ctypes.c_void_p(a), buf, n, ctypes.byref(got)):
        return None
    return buf.raw[:got.value]


found = False
for base, size in regions():
    for off in range(0, size, 0x800000):
        d = read(base + off, min(0x800000 + 64, size - off))
        if not d:
            continue
        j = d.find(anchor)
        while j >= 0:
            ram = base + off + j - 0x100100
            data = read(ram, 0x2000000)
            if data and len(data) == 0x2000000:
                import struct
                g = struct.unpack_from('<I', data, 0x6f81f8)[0]
                print('candidate', hex(ram), 'game ptr', hex(g))
                if 0x100000 < g < 0x2000000 and not found:
                    open(sys.argv[2], 'wb').write(data); found = True; print('  -> dumped')
            j = d.find(anchor, j + 1)
print('done', found)
sys.exit(0 if found else 1)
