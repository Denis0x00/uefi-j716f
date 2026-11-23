# UEFI for Lenovo TB-J716F

Board UEFI firmware sources for the Lenovo Xiaoxin Pad Pro 2021 (`TB-J716F`,
Snapdragon 870 / SM8250-AC), built on Mu-Silicium for the
[UniXNU](https://github.com/UniXNU/UniXNU) bring-up flow.

## Current Capabilities

- **Board memory map & early protection (`J716fEarlyHandoffLib`, `MemoryMapLib`)**:
  reserves ABL FDT, initrd, inherited framebuffer, boot log, and DXE/XNU windows
  with non-overlapping descriptor checks before HOB publication and MMU setup.
- **Display ownership & console (`J716fInheritedFbDxe`, `J716fKeysDxe`, `J716fIoConsoleBdsDxe`)**:
  verified menu entry, volume/power key navigation, clean rendering without
  overlay corruption, restart, and power-off (with external USB power removed).
- **Retained early XNU logs (`J716fRamLogLib`, `J716fEarlyLogSecLib`, `J716fHandoffDiagnosticsDxe`)**:
  survives diagnostic reset and long-power-button reset, with Advanced Logger QR
  export and ReadyToBoot payload CRC32 reporting.
- **r35–r38 UFS/SMMU RAM handoff slot (`J716fR35RecordLib`, `J716fR35RecordDxe`, `PlatformSecLib/R35Handoff.c`)**:
  captures the SM8250 UFS HCI + APPS SMMU handoff state in SEC and publishes a
  versioned, checksummed RAM record for the companion XNU read-only storage
  bring-up driver (`UniXNU-j716f`).

## Building

See [Building.md](Building.md) for the official UniXNU Format 2 firmware build
path (`./unixnu build firmware`) as well as standalone EDK II `CLANGPDB`
compilation instructions.

The optional core menu-hold patch for local diagnostic builds is kept at
`Resources/UniXNUPatches/j716f-menu-hold.patch`. Without `-D J716F_MENU_DIAGNOSTIC=1`,
`Platforms/Lenovo/j716fPkg/j716f.dsc` compiles directly against upstream
`UniXNU/UniXNU` and produces a 7,340,032-byte (`0x700000`) `SILICIUM_UEFI.fd`.

## Scope & Provenance

- The six upstream submodule gitlink pins are preserved; only `Common/Mu` and
  `Common/Mu_OEM_Sample` are required alongside UniXNU's pinned `mu_basecore`
  feed (`edk2`).
- Generated firmware volumes, boot images, kernels, IPSW/ROM binaries, private
  identities, and device logs are excluded from this repository.
- Original notices, per-file SPDX headers, and [LICENSE](LICENSE) are retained.

References:
- [UniXNU](https://github.com/UniXNU/UniXNU)
- [TB-J716F Device Repository](https://github.com/Denis0x00/UniXNU-j716f)
