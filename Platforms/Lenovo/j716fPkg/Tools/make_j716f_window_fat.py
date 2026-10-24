"""V14 embedded FAT with only the memory window verification and restart menu entries."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from make_j716f_io_fat import build, PAYLOAD_GUID

CONFIG = b'''version = 1
[boot]
timeout = -1
fallback_timeout = -1
rotate = 0
connect_all = false
default = "window"
[ui]
title = "J716F V14 - memory window verification before kernel hand-off"
[[entry]]
id = "window"
title = "Test 256 MiB RAM window (no kernel boot)"
type = "efi"
path = "\\\\EFI\\\\BOOT\\\\BOOTAA64.EFI"
[[entry]]
id = "reset"
title = "Restart"
type = "action"
action = "reset"
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--efi', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    raw = args.efi.read_bytes()
    pe = struct.unpack_from('<I',raw,0x3c)[0]
    assert raw[:2] == b'MZ' and raw[pe:pe+4] == b'PE\0\0' and struct.unpack_from('<H',raw,pe+4)[0] == 0xaa64
    data = build(raw,CONFIG)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_bytes(data)
    report = {'candidate':'j716f-window-v14','bytes':len(data),'fat_type':'FAT16','payload_guid':PAYLOAD_GUID,'efi_sha256':hashlib.sha256(raw).hexdigest(),'sha256':hashlib.sha256(data).hexdigest(),'hardware_verified':False,'scope':'allocate, two full 256 MiB patterns with cache maintenance, release; no XNU hand-off'}
    args.out.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__': main()
