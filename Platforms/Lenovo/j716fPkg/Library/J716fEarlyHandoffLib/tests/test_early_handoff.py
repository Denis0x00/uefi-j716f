#!/usr/bin/env python3
"""Compile the actual board map, early library, AddHob and pinned libfdt on Linux."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def without_includes(text):
    return re.sub(r'^\s*#include[^\n]*$', '', text, flags=re.MULTILINE)


def c_function(text, name):
    match = re.search(r'\b' + re.escape(name) + r'\s*\([^;]*?\)\s*\{', text)
    if not match:
        raise AssertionError(f'Missing function {name}')
    start = text.index('{', match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


def assert_order(text, names):
    positions = [text.index(name) for name in names]
    if positions != sorted(positions) or len(set(positions)) != len(positions):
        raise AssertionError(f'Wrong call order: {names}')


def source_checks(repo, production):
    sec = (repo / 'Silicon/Silicium/SiliciumPkg/Sec/Sec.c').read_text()
    pei = (repo / 'Silicon/Silicium/SiliciumPkg/Library/MemoryInitPeiLib/MemoryInitPei.c').read_text()
    entry = c_function(sec, 'SecEntry')
    main = c_function(sec, 'SecMain')
    init = c_function(sec, 'InitializeMemory')
    memory = c_function(pei, 'MemoryPeim')
    assert_order(entry, ['PrepareEarlyHandoff (gSecEntryArg0)', 'RamLogReset ();', 'SecMain (HandoffStatus)'])
    assert_order(main, ['ReportEarlyHandoff ();', 'if (EFI_ERROR (HandoffStatus))', 'InitializeMemory ();', 'BuildGuidDataHob ('])
    assert_order(init, ['HobConstructor (', 'MemoryPeim ('])
    assert_order(memory, ['GetMemoryMap (', 'ApplyEarlyHandoffMemoryMap (', 'ValidateMemoryMap (', 'AddHob (', 'ArmConfigureMmu ('])
    assert_order(c_function(production, 'PrepareEarlyHandoff'), ['ValidateFdtLayout (', 'fdt_check_full ('])
    before_validate = memory[memory.index('ApplyEarlyHandoffMemoryMap ('):memory.index('ValidateMemoryMap (')]
    assert 'if (EFI_ERROR (Status))' in before_validate and 'return Status;' in before_validate
    # The whole library is linked in SEC. Reject calls that can allocate or use
    # a HOB, and require logging to occur only in the separate reporting API.
    prepare_part = production[:production.index('ReportEarlyHandoff (')]
    prepare_part = re.sub(r'/\*.*?\*/|//[^\n]*', '', prepare_part, flags=re.DOTALL)
    assert not re.search(r'\b(?:Allocate\w*|Free\w*|Build\w*Hob|GetHobList|DEBUG)\s*\(', prepare_part)
    print('PASS source hooks: prepare before log/heap; reject before HOB; protected map before HOB/MMU; no early allocation/logging')
    return pei


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--libfdt', required=True, type=Path, help='Pinned Mu_Basecore BaseFdtLib/libfdt/libfdt source directory')
    parser.add_argument('--dtb', required=True, type=Path, help='Stock normalized board DTB; read only')
    parser.add_argument('--cc', default='gcc')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[5]
    production_path = here.parent / 'J716fEarlyHandoff.c'
    production = production_path.read_text()
    map_path = repo / 'Platforms/Lenovo/j716fPkg/Library/MemoryMapLib/MemoryMapLib.c'
    header_path = repo / 'Silicon/Silicium/SiliciumPkg/Include/Library/MemoryMapLib.h'
    pei = source_checks(repo, production)
    files = ['fdt.c', 'fdt_ro.c', 'fdt_wip.c', 'fdt_sw.c', 'fdt_rw.c', 'fdt_strerror.c',
             'fdt_empty_tree.c', 'fdt_addresses.c', 'fdt_overlay.c', 'fdt_check.c']
    for filename in files:
        if not (args.libfdt / filename).is_file():
            raise FileNotFoundError(args.libfdt / filename)
    inputs = [production_path, map_path, header_path, args.dtb, here / 'host_shim.h',
              here / 'harness.c', here / 'harness_asan.c', here / 'hob_capture.h']
    identity = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    print(json.dumps({'input_sha256': identity, 'libfdt': str(args.libfdt.resolve())}, indent=2), flush=True)
    unit = '\n'.join([
        (here / 'host_shim.h').read_text(),
        without_includes(header_path.read_text()),
        '#define GetMemoryMap GetBoardMemoryMap',
        without_includes(map_path.read_text()),
        '#undef GetMemoryMap',
        'static EFI_MEMORY_REGION_DESCRIPTOR *test_map; static UINT8 test_count;',
        'void GetMemoryMap(EFI_MEMORY_REGION_DESCRIPTOR **map, UINT8 *count) {'
        ' if (test_map) { *map=test_map; *count=test_count; } else GetBoardMemoryMap(map,count); }',
        without_includes(production),
        '#include "hob_capture.h"',
        'static void ' + c_function(pei, 'AddHob'),
    ])
    with tempfile.TemporaryDirectory(prefix='j716f-early-handoff-') as tmp:
        # Actual board-like addresses conflict with x86-64 ASan shadow mappings.
        # Keep two explicit test builds; never replace sanitizer mappings.
        variants = [('fixed-address-ubsan', 'harness.c', 'undefined'),
                    ('ranges-map-fdt-asan-ubsan', 'harness_asan.c', 'address,undefined')]
        for name, harness, sanitizer in variants:
            print(f'RUN {name}: -fsanitize={sanitizer}', flush=True)
            source = Path(tmp) / f'{name}.c'
            binary = Path(tmp) / name
            source.write_text(unit + '\n' + (here / harness).read_text())
            command = [args.cc, '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                       f'-fsanitize={sanitizer}', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
                       '-I', str(args.libfdt), '-I', str(here), str(source),
                       *[str(args.libfdt / filename) for filename in files], '-o', str(binary)]
            subprocess.run(command, check=True)
            env = dict(os.environ)
            env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
            env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
            subprocess.run([str(binary), str(args.dtb.resolve())], env=env, check=True)
    print('PASS: fixed-address production path under UBSan; range/map/rejection and libfdt under ASan/UBSan')


if __name__ == '__main__':
    main()
