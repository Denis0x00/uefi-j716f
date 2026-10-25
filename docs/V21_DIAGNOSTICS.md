# J716F V21 reset diagnostics

Updated: 2026-10-25. V21 has passed the local firmware build and host image
audit, and a device photograph confirms its menu and PMIC summary.
The V21 XNU entry attempt is still awaiting observation.

## Source changes

- The entry trace library uses V21 menu, return-page and stage labels.
- The optional reset-reason protocol supplies a short, prominent PMIC summary.
  `Config.Notice` is CHAR8 and is populated with `AsciiSPrint`.
- The diagnostic menu uses scale 4 with no logo.
- The active `SiliciumPkg/Library/FrameBufferSerialPortLib` selects scale 5
  on the 2560x1600 panel, up from 3. The older board-specific trace library
  is not selected by this profile and was left unchanged.
- The existing V20 PMIC provider remains read-only: its clear operation
  returns `EFI_UNSUPPORTED`.

## Recorded build profile

`Platforms/Lenovo/j716fPkg/j716f-entry-v21.dsc` preserves the exact DSC contents
used by the local build. The builder invoked those contents under the
historical `j716f-entry-v15.dsc` filename. The V21 profile deliberately keeps
the historical `j716f-entry-v15.fdf` and `Build/j716fEntryV15Pkg` output path.

The public V15 DSC has additional HobLib, UniXNURamDiskDxe, DiskIoDxe,
UnicodeCollation and FAT build declarations. These additions were not in
the recorded V21 DSC. V21 retains the existing embedded read-only FAT and
uses an empty Android ramdisk; standard initrd integration is a separate gate.

With the documented local binary resources, matching dependencies and the
prepared UniXNU/DarwinBoot overlay available, run from this UEFI checkout:

```sh
export WORKSPACE="$PWD"
export PACKAGES_PATH="$PWD/Platforms/Lenovo:$PWD/Common/Mu:$PWD/Common/Mu_OEM_Sample:$PWD/Mu_Basecore:$PWD/Silicon/Qualcomm:$PWD/Silicon/Silicium:${J716F_UNIXNU_OVERLAY:?set the prepared overlay path}"
source Mu_Basecore/BaseTools/BuildEnv
build -a AARCH64 -t CLANGPDB -b RELEASE \
  -p Platforms/Lenovo/j716fPkg/j716f-entry-v21.dsc \
  -D FD_BASE=0xce000000 -D FD_SIZE=0x700000 -D FD_BLOCKS=0x700 \
  -D ENABLE_SECUREBOOT=0
```

The build output is `Build/j716fEntryV15Pkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd`.
This source-only public checkout does not include the binary resources needed
to reproduce a complete FD. The compiled artifact used the pinned upstream Mu
base plus local board sources; source hashes and profile identity are recorded
in [v21-validation.json](v21-validation.json).

## Host artifact audit

- FD is 7 MiB; the downloaded FD hash matches the builder's hash.
- FV and FFS header checksums, AARCH64 PE headers and linked PE byte identity pass.
- Compiled V21 and PMIC markers are present; stale V19 stage labels are absent.
- The module GUID set and embedded FAT payload match V20.
- BootShim ARM64 header, gzip round trip, both stock DTB copies and Android
  header v2 parameters, image SHA-1 ID, padding and length pass.
- Packaging retains page size 4096, kernel address `0x8000`, tags `0x100`,
  DTB address `0x1f00000` and zero ramdisk bytes.

| Local artifact | Bytes | SHA256 |
| --- | ---: | --- |
| V21 boot image | 8,331,264 | `88e78f74cbe402fbd9bda0c287e16d3422824d5b51a3ea42a1a441316ebd1efc` |
| V21 UEFI FD | 7,340,032 | `05b5486fc9b75d361b17e93a39bca83871b2b1b926630a6863879eebb8cf5d32` |

## Device observations and limits

Only `boot_b` was flashed. Its slot was `b` before and after the operation;
the flash and reboot commands returned `OKAY`.
The photograph shows the V21 title, readable enlarged menu text, hidden logo,
`PMIC RESET: 0x02000001` and `drv 0/0 kits 0 acpi 0`.

The provider packs `Pon | (Warm1 << 8) | (Warm2 << 16) | (Poff << 24)`.
This sample therefore decodes to `pon=0x01`, `warm1=0x00`, `warm2=0x00`,
`poff=0x02`. Register bit meanings have not been classified. This is a
boot-history baseline after flashing/rebooting, not a diagnosis of an XNU fault.

The photo validates menu rendering and a PMIC read. It does not validate the
framebuffer log font, a V21 DarwinBoot attempt, sustained MMU-on execution,
native platform drivers, physical UFS/root, userspace or iPadOS.
Next, record the first XNU attempt, final text/color bars, any Lenovo logo
reappearance, and the eventual held page or menu. Keep testing limited to `boot_b`.

Earlier V19/V20 observations and the current device-side record are summarized
in the companion [V21 progress document](https://github.com/Denis0x00/UniXNU-j716f/blob/main/docs/V21_RESET_DIAGNOSTICS.md).
Generated firmware, boot images, raw DTBs, photos and raw logs remain local.
