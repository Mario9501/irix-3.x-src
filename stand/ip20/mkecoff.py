#!/usr/bin/env python3
#
# mkecoff.py
#	Convert ip20mon's ELF to the ECOFF layout the IP20 PROM loads:
#	OMAGIC, big-endian MIPS magic 0x160, and exactly three sections
#	(.text, .data, .bss) with data following text in the file.  This
#	is the same layout the elf2ecoff tools in NetBSD and Linux write
#	(NetBSD uses theirs to netboot IP20s).  binutils' "objcopy -O
#	ecoff-bigmips" differs: it marks MIPS III code with magic 0x140
#	and keeps .rodata as a separate section, which the PROM may refuse.
#
#	usage: mkecoff.py in.elf out.ecoff
#

import struct
import sys

PT_LOAD = 1
PF_W = 2
EM_MIPS = 8

MIPSEBMAGIC = 0x160
OMAGIC = 0o407
VSTAMP = 200			# >= 23 means sections are 16-byte aligned
F_FLAGS = 0x100f		# executable, stripped
STYP_TEXT, STYP_DATA, STYP_BSS = 0x20, 0x40, 0x82
FILHSZ, AOUTHSZ, SCNHSZ = 20, 56, 40
MAXGAP = 65536


def die(msg):
    sys.exit("mkecoff: " + msg)


def round16(n):
    return (n + 15) & ~15


def main():
    if len(sys.argv) != 3:
        die("usage: mkecoff.py in.elf out.ecoff")
    elf = open(sys.argv[1], "rb").read()

    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 2:
        die("not a 32-bit big-endian ELF file")
    (e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags,
     e_ehsize, e_phentsize, e_phnum) = struct.unpack(">HHIIIIIHHH",
                                                      elf[16:46])
    if e_machine != EM_MIPS:
        die("not a MIPS ELF file")

    text = data = None
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        (p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz,
         p_flags, p_align) = struct.unpack(">IIIIIIII", elf[off:off + 32])
        if p_type != PT_LOAD:
            continue
        seg = (p_vaddr, elf[p_offset:p_offset + p_filesz], p_memsz)
        if p_flags & PF_W:
            if data:
                die("more than one writable segment")
            data = seg
        else:
            if text:
                die("more than one read-only segment")
            text = seg
    if text is None or data is None:
        die("need one text and one data segment (see ip20.ld)")

    text_start, text_bytes, _ = text
    data_start, data_bytes, data_memsz = data
    bss_start = data_start + len(data_bytes)
    bss_size = data_memsz - len(data_bytes)

    # Pad text up to the start of data so the file is one contiguous image.
    gap = data_start - (text_start + len(text_bytes))
    if gap < 0:
        die("data segment overlaps text")
    if gap > MAXGAP:
        die("gap of %d bytes between text and data" % gap)
    text_bytes += b"\0" * gap

    nscns = 3
    txtoff = round16(FILHSZ + AOUTHSZ + nscns * SCNHSZ)
    datoff = txtoff + len(text_bytes)

    filehdr = struct.pack(">HHIIIHH", MIPSEBMAGIC, nscns, 0, 0, 0,
                          AOUTHSZ, F_FLAGS)
    aouthdr = struct.pack(">HHIIIIIIII4II", OMAGIC, VSTAMP,
                          len(text_bytes), len(data_bytes), bss_size,
                          e_entry, text_start, data_start, bss_start,
                          0xf3fffffe, 0, 0, 0, 0, 0)

    def scnhdr(name, vaddr, size, scnptr, flags):
        return struct.pack(">8sIIIIIIHHI", name, vaddr, vaddr, size, scnptr,
                           0, 0, 0, 0, flags)

    out = filehdr + aouthdr
    out += scnhdr(b".text", text_start, len(text_bytes), txtoff, STYP_TEXT)
    out += scnhdr(b".data", data_start, len(data_bytes), datoff, STYP_DATA)
    out += scnhdr(b".bss", bss_start, bss_size,
                  datoff + round16(len(data_bytes)), STYP_BSS)
    out += b"\0" * (txtoff - len(out))
    out += text_bytes + data_bytes

    with open(sys.argv[2], "wb") as f:
        f.write(out)
    print("%s: text %#x+%#x data %#x+%#x bss %#x+%#x entry %#x" % (
        sys.argv[2], text_start, len(text_bytes), data_start,
        len(data_bytes), bss_start, bss_size, e_entry))


main()
