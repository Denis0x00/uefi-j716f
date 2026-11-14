#!/usr/bin/env python3
"""Read-only validation of pinned firmware dependencies; never fetch or build."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--core', type=Path, required=True)
parser.add_argument('--edk2', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
record = json.loads((root / 'docs/source-provenance.json').read_text())


def git(path, *arguments):
    return subprocess.check_output(['git', '-C', str(path), *arguments], text=True).strip()


def require(condition, message):
    if not condition:
        raise SystemExit(message)


for relative, expected in record['active_firmware_submodules'].items():
    repository = root / relative
    require(Path(git(repository, 'rev-parse', '--show-toplevel')).resolve() == repository.resolve(),
            'Submodule is not initialized: ' + relative)
    require(git(repository, 'rev-parse', 'HEAD') == expected['commit'], 'Wrong submodule commit: ' + relative)
    require(not git(repository, 'status', '--porcelain'), 'Modified firmware submodule: ' + relative)
require(git(args.core, 'rev-parse', 'HEAD') == record['unixnu']['commit'], 'Wrong UniXNU core revision')
require(git(args.edk2, 'rev-parse', 'HEAD') == record['edk2_feed']['observed_commit'], 'Wrong EDK2 feed revision')
require(not git(args.edk2, 'status', '--porcelain'), 'EDK2 source has unrecorded changes')
darwin = args.core / 'drivers/DarwinBoot'
require(git(darwin, 'rev-parse', 'HEAD') == record['darwinboot']['commit'], 'Wrong DarwinBoot revision')
require(not git(darwin, 'status', '--porcelain'), 'DarwinBoot source has unrecorded changes')
for relative, expected in record['external_package_source_identities'].items():
    raw = (args.core / relative).read_bytes()
    actual = {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}
    normalized = raw.replace(b'\r\n', b'\n')
    require(actual == {key: expected[key] for key in ('bytes', 'sha256')} or
            {'bytes': len(normalized), 'sha256': hashlib.sha256(normalized).hexdigest()} == expected['normalized_lf'],
            'Wrong retained package source: ' + relative)
print('PASS: active firmware submodule pins, EDK2 feed, DarwinBoot and retained package sources')
