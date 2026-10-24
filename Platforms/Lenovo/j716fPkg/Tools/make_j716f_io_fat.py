"""Deterministic 4MiB FAT16 superfloppy for the J716F V9 read-only test."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

SECTOR = 512
SECTORS = 8192
FAT_SECTORS = 32
ROOT_SECTORS = 32
DATA_SECTOR = 1 + FAT_SECTORS * 2 + ROOT_SECTORS
PAYLOAD_GUID = 'a4934a8e-547b-4e44-b2b2-e044ebfa19eb'

CONFIG = b'''version = 1
[boot]
timeout = -1
fallback_timeout = -1
rotate = 0
connect_all = false
default = "probe"
[ui]
title = "J716F V9 - Keys / RAM FAT / EFI"
[[entry]]
id = "probe"
title = "Run read-only FAT / external EFI test"
type = "efi"
path = "\\\\EFI\\\\BOOT\\\\BOOTAA64.EFI"
[[entry]]
id = "probe-again"
title = "Run the same test again"
type = "efi"
path = "\\\\EFI\\\\BOOT\\\\BOOTAA64.EFI"
'''


def build(efi, config=CONFIG):
    image = bytearray(SECTORS * SECTOR)
    image[:3] = b'\xeb\x3c\x90'
    image[3:11] = b'J716FV9 '
    struct.pack_into('<HBHBHHBHHHII', image, 11, SECTOR, 1, 1, 2, 512, SECTORS,
                     0xf8, FAT_SECTORS, 32, 64, 0, 0)
    image[36:39] = b'\x80\x00\x29'
    struct.pack_into('<I', image, 39, 0x716f0009)
    image[43:54] = b'J716F_V9   '
    image[54:62] = b'FAT16   '
    image[510:512] = b'\x55\xaa'
    fat = bytearray(FAT_SECTORS * SECTOR)
    struct.pack_into('<HH', fat, 0, 0xfff8, 0xffff)
    next_cluster = 2

    def chain(data):
        nonlocal next_cluster
        count = max(1, (len(data) + SECTOR - 1) // SECTOR)
        first = next_cluster
        for i in range(count):
            cluster = next_cluster
            next_cluster += 1
            assert DATA_SECTOR + next_cluster - 2 <= SECTORS, 'FAT capacity'
            struct.pack_into('<H', fat, cluster * 2, 0xffff if i == count - 1 else cluster + 1)
            offset = (DATA_SECTOR + cluster - 2) * SECTOR
            piece = data[i * SECTOR:(i + 1) * SECTOR]
            image[offset:offset + len(piece)] = piece
        return first

    def entry(name, attr, cluster, size=0):
        assert len(name) == 11
        e = bytearray(32)
        e[:11] = name
        e[11] = attr
        # Fixed valid DOS timestamp, 2026-10-24.
        struct.pack_into('<H', e, 24, ((2026 - 1980) << 9) | (10 << 5) | 24)
        struct.pack_into('<HI', e, 26, cluster, size)
        return bytes(e)

    # Reserve single-cluster directories before filling their contents.
    efi_dir = chain(b'')
    boot_dir = chain(b'')
    unixnu_dir = chain(b'')
    app = chain(efi) if efi else 0
    cfg = chain(config)
    pattern = bytes((i * 37 + 11) & 255 for i in range(8193))
    pat = chain(pattern)
    def directory(cluster, parent, entries):
        data = entry(b'.          ', 0x10, cluster) + entry(b'..         ', 0x10, parent) + b''.join(entries)
        assert len(data) <= SECTOR
        offset = (DATA_SECTOR + cluster - 2) * SECTOR
        image[offset:offset + len(data)] = data
    directory(efi_dir, 0, [entry(b'BOOT       ', 0x10, boot_dir), entry(b'UNIXNU     ', 0x10, unixnu_dir)])
    directory(boot_dir, efi_dir, [entry(b'BOOTAA64EFI', 0x21, app, len(efi))] if efi else [])
    directory(unixnu_dir, efi_dir, [entry(b'UNIXNU  TOM', 0x21, cfg, len(config))])
    # Config's extension is .toml and therefore requires an LFN entry.
    name = 'unixnu.toml'
    short = b'UNIXNU  TOM'
    checksum = 0
    for byte in short:
        checksum = (((checksum & 1) << 7) + (checksum >> 1) + byte) & 255
    lfn = bytearray(b'\xff' * 32)
    lfn[0] = 0x41; lfn[11] = 0x0f; lfn[12] = 0; lfn[13] = checksum; lfn[26:28] = b'\x00\x00'
    chars = (name + '\0').ljust(13, '\uffff').encode('utf-16-le')
    lfn[1:11] = chars[:10]; lfn[14:26] = chars[10:22]; lfn[28:32] = chars[22:26]
    directory(unixnu_dir, efi_dir, [bytes(lfn), entry(short, 0x21, cfg, len(config))])
    root = entry(b'J716F_V9   ', 0x08, 0) + entry(b'EFI        ', 0x10, efi_dir) + entry(b'PATTERN BIN', 0x21, pat, len(pattern))
    offset = (1 + FAT_SECTORS * 2) * SECTOR
    image[offset:offset + len(root)] = root
    image[SECTOR:SECTOR + len(fat)] = fat
    image[(1 + FAT_SECTORS) * SECTOR:(1 + FAT_SECTORS) * SECTOR + len(fat)] = fat
    return bytes(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--efi', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    efi = args.efi.read_bytes() if args.efi else b''
    if efi:
        assert efi[:2] == b'MZ'
        pe = struct.unpack_from('<I', efi, 0x3c)[0]
        assert efi[pe:pe + 4] == b'PE\0\0' and struct.unpack_from('<H', efi, pe + 4)[0] == 0xaa64
    data = build(efi)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    report = {'bytes': len(data), 'fat_type': 'FAT16', 'sector_size': 512,
              'data_clusters': SECTORS - DATA_SECTOR, 'payload_guid': PAYLOAD_GUID,
              'efi_bytes': len(efi), 'efi_sha256': hashlib.sha256(efi).hexdigest() if efi else None,
              'sha256': hashlib.sha256(data).hexdigest(), 'hardware_verified': False}
    args.out.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
