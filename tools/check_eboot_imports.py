#!/usr/bin/env python3
"""Check an EBOOT.PBP / .elf for imports a user-mode homebrew cannot use on a real PSP.

Usage:  python3 check_eboot_imports.py EBOOT.PBP
Exits with 1 when kernel libraries (*ForKernel, *_driver, ...) are imported.
PPSSPP resolves such imports anyway, so the EBOOT works there but not on hardware.
"""
import struct, sys

BAD_SUFFIXES = ("ForKernel", "_driver")
BAD_NAMES = {"SysclibForKernel", "LoadCoreForKernel", "SysMemForKernel", "KDebugForKernel"}


def get_elf(data):
    if data[:4] == b"\0PBP":
        offs = list(struct.unpack_from("<8I", data, 8)) + [len(data)]
        data = data[offs[6]:offs[7]]  # DATA.PSP
    if data[:4] == b"~PSP":
        sys.exit("DATA.PSP is encrypted/compressed (~PSP); check the unpacked .elf/.prx")
    if data[:4] != b"\x7fELF":
        sys.exit("No ELF found in the file")
    return data


def main(path):
    d = get_elf(open(path, "rb").read())
    shoff = struct.unpack_from("<I", d, 32)[0]
    shnum, shstr = struct.unpack_from("<HH", d, 48)
    secs = [struct.unpack_from("<10I", d, shoff + i * 40) for i in range(shnum)]
    so = secs[shstr][4]
    name = lambda o: d[so + o:d.index(b"\0", so + o)].decode()
    by = {name(s[0]): s for s in secs}

    def v2o(a):
        for s in secs:
            if s[3] and s[3] <= a < s[3] + s[5] and s[1] != 8:
                return s[4] + a - s[3]
        return a + 0 if a < len(d) else None

    def cstr(a):
        o = v2o(a)
        return d[o:d.index(b"\0", o)].decode()

    mi = by.get(".rodata.sceModuleInfo")
    if mi:
        attr = struct.unpack_from("<H", d, mi[4])[0]
        print(f"module attr: 0x{attr:04x} ({'KERNEL' if attr & 0x1000 else 'user'})")

    st = by[".lib.stub"]
    o, bad = st[4], []
    while o < st[4] + st[5]:
        lib, ver, attr, esz, nvar, nfunc, nids, funcs = struct.unpack_from("<IHHBBHII", d, o)
        lname = cstr(lib)
        flag = lname in BAD_NAMES or lname.endswith(BAD_SUFFIXES)
        print(f"{'!!' if flag else '  '} {lname:28s} funcs={nfunc}")
        if flag:
            bad.append(lname)
        o += esz * 4
    if bad:
        print("\nERROR: user-mode module imports kernel libraries:", ", ".join(bad))
        print("This EBOOT will not start on a real PSP (PPSSPP does not check this).")
        return 1
    print("\nOK: no kernel imports.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
