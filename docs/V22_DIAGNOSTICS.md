# V22 paired post-MMU checkpoints

Updated 2026-10-25. V22A and V22B built successfully and passed host artifact
audits. Neither variant has been flashed or hardware-verified in this preparation.

## Profiles

- `Platforms/Lenovo/j716fPkg/j716f-entry-v22.dsc`
- `Platforms/Lenovo/j716fPkg/j716f-entry-v22.fdf`
- `Platforms/Lenovo/j716fPkg/Include/ENTRY_V22_DXE.inc` and `ENTRY_V22_APRIORI.inc`
- Independent `J716fV22TraceUniXNULib`, derived from the V21 library.

The module lists freeze the actual V21 embedded-FAT build selection. They do
not inherit the later public V15 standard-initrd additions. The V22 DSC uses
`Build/j716fEntryV22Pkg` and its own FDF, so its profile is explicit.
The new menu library preserves the payload title identifying V22A/1s or V22B/3s,
keeps scale 4, hides the logo and reports the startup PMIC snapshot.
The active Silicium framebuffer font remains scale 5.

## Build

Prepare one variant using the companion device repository's
`scripts/build-v22-payload.py`, then copy its `J716fEntryV22.fat` to
`Platforms/Lenovo/j716fPkg/Resources/J716fEntryV22.fat`.
With matching local dependencies and the prepared UniXNU/DarwinBoot overlay:

```sh
export WORKSPACE="$PWD"
export PACKAGES_PATH="$PWD/Platforms/Lenovo:$PWD/Common/Mu:$PWD/Common/Mu_OEM_Sample:$PWD/Mu_Basecore:$PWD/Silicon/Qualcomm:$PWD/Silicon/Silicium:${J716F_UNIXNU_OVERLAY:?set the prepared overlay path}"
source Mu_Basecore/BaseTools/BuildEnv
build -a AARCH64 -t CLANGPDB -b RELEASE \
  -p Platforms/Lenovo/j716fPkg/j716f-entry-v22.dsc \
  -D FD_BASE=0xce000000 -D FD_SIZE=0x700000 -D FD_BLOCKS=0x700 \
  -D ENABLE_SECUREBOOT=0
```

Save `Build/j716fEntryV22Pkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd` for that variant
before replacing the resource with the other variant and building again.
Use the same recorded BootShim and stock firmware DTB as V21, with Android
header v2, page 4096, kernel offset 0x8000, tags 0x100, DTB 0x1f00000 and empty
Android ramdisk. The public source checkout excludes private inputs and artifacts.

## Host results

| Variant | Boot image bytes | Boot image SHA256 |
| --- | ---: | --- |
| V22A, 1 second | 8,339,456 | `7c2cce9a188da50f3b060e0ce5b5efb966fa97d5e754e5d8ca88016589f5360a` |
| V22B, 3 seconds | 8,347,648 | `d3d8d1a7fed7a954b5506c71c02ca1f11b2605885084de3e1f08a699dbac59d1` |

Both FD sizes are 7 MiB. Host audits checked FD transfer hashes, FV/FFS
checksums, AARCH64 PE and linked-module identities, unchanged V21 module GUID
set and APRIORI ordering, embedded FAT files, entry/probe instructions, BootShim,
gzip, stock DTBs, Android header/SHA1, padding and length. The two kernels differ
only in their one timer literal. See [v22-validation.json](v22-validation.json).

## Interpretation and limits

The payloads request PSCI SYSTEM_RESET 1 or 3 seconds after the post-MMU hook.
Neither includes a panic renderer because the verified entry padding cannot
fit the current renderer plus the timer probe. These are terminating checkpoint
experiments; continued platform initialization is outside their execution path.
Preserve V21 as the original no-timer/panic baseline.

Only boot_b may be written for hardware testing. Record each variant's menu,
color bands, elapsed time to black screen, vendor logo and new PMIC snapshot.
Do not classify a reset from PMIC 0x02000041 alone. The unknown V21 reset cause,
counter behavior and PSCI reset behavior still require device evidence.
