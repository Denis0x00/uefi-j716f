# uefi-j716f

Mu-Silicium UEFI port for the Lenovo Xiaoxin Pad Pro 2021 (TB-J716F, `j716f`,
Qualcomm SM8250).

This is the companion firmware repository for `UniXNU-j716f`. It is a research
and bring-up port; it does not claim to boot iPadOS, iOS or a production XNU
image.

## Included

- J716F platform DSC/FDF files, memory map and platform libraries.
- UniXNU menu, RAM FAT, loader and DarwinBoot diagnostic paths.
- Android boot image header-v2 build patch.
- Source locks and configuration for the Mu-Silicium base.

## Excluded from the public tree

- `Binaries/j716f/` and vendor PE files extracted from ZUI XBL.
- Generated FAT resources, firmware volumes, boot images and build output.
- Device backups, photos, ADB/fastboot logs, VM details and serial numbers.
- IPSW, kernelcache, ADT, trust cache, userdata and Data seed.

The public tree therefore needs a local vendor-input checkout before it can be
built. The exact ZUI source and hashes are checked by the private preflight used
in the local bring-up environment.

## Current validation

- UEFI menu, volume keys, power key and RAM FAT / BlockIO paths were validated
  on the real device.
- Official DarwinBoot v2 protocol and invalid-payload refusal were validated.
- Earlier V16/V17 diagnostics recorded ExitBootServices and early XNU entry
  checkpoints. V18 still reset after replacing its post-MMU reset with WFE;
  the reset source remains unresolved.
- V21 was built, audited, flashed to `boot_b` and rebooted. A device photograph
  confirms the V21 menu, enlarged menu text and `PMIC RESET: 0x02000001`.
- V21 video confirms its entry bands and a subsequent Lenovo-logo restart; reset attribution remains unresolved.
- V22A/1s and V22B/3s passed host builds and artifact audits; device testing is pending. Native platform drivers,
  physical UFS/root, trust cache and userspace remain unverified.
- TzDxe is intentionally out of scope for the current phase.

Current details: [V22 checkpoints](docs/V22_DIAGNOSTICS.md),
[V22 validation](docs/v22-validation.json), [V21 diagnostics](docs/V21_DIAGNOSTICS.md),
[public validation metadata](docs/v21-validation.json), and the companion
[device progress record](https://github.com/Denis0x00/UniXNU-j716f/blob/main/docs/V21_RESET_DIAGNOSTICS.md).

## Build boundary

### V13 memory survey

`Platforms/Lenovo/j716fPkg/j716f-memory-v13.dsc` is a separate diagnostic profile
derived from V12. It builds an external `J716fMemoryProbe.efi` for the local
embedded FAT. The application captures `GetMemoryMap`, reports free memory and
the largest 2 MiB-aligned conventional descriptor, then holds paged details for
photographs. Its capacity estimate does not include the kernel base residue.

The local Ubuntu build and Android header-v2/FV/FAT artifact audits passed.
The real device survey and EFI_SUCCESS return were validated. The profile retains the original memory map;
it does not load a kernel, allocate a kernel window, or exit boot services.
The generated `J716fMemoryV13.fat` and boot image remain local inputs/outputs.
This survey is needed because the current static table exposes a 224 MiB DXE
region, while the pinned DarwinBoot loader needs a minimum 256 MiB window.

### V14 memory-window qualification

`Platforms/Lenovo/j716fPkg/j716f-window-v14.dsc` selects independent memory-map
and menu-report libraries. V13 is retained unchanged. The new candidate region
is `[0xD0000000,0xE8000000)`; the application allocates one 256 MiB window with
the pinned DarwinBoot alignment and the current kernel's `0x7C000` residue.

Real-device evidence on 2026-10-24 confirms `[0xD007C000,0xE007C000)` allocation,
two complete address-dependent 64-bit write/read rounds (second inverted), data
cache clean/invalidate, and FreePages. Result: 7 PASS, 0 FAIL, 512 MiB verified
across two rounds of the same 256 MiB window. The V14 return page also confirms
EFI_SUCCESS, Complete=1 and matching counts. This does not certify all RAM.

The application and menu use identical version-2 result headers. Firmware
artifact audits checked 32 source PE modules, no vendor PE, APRIORI, embedded
FAT identity and Android header-v2. Kernel entry and ExitBootServices remain
unverified. Generated FAT, firmware and photos stay outside this repository.

`Tools/make_j716f_window_fat.py --efi <J716fMemoryWindowProbe.efi> --out
<Resources/J716fWindowV14.fat>` creates the local FAT after the first build;
rebuild the firmware to embed it. The FAT helper retains the earlier tested
volume layout. `Tests/WindowPolicyTest.c` compiles the actual WindowPolicy.h.

### V15 real-kernel entry diagnostic (device test pending)

`j716f-entry-v15.dsc` keeps the V14 memory map and 7 MiB FD, but selects a
separate read-only 64 MiB FAT32 driver and entry menu. The local volume carries
the experimental prelinked kernel, J716F ADT and manual XNU menu entry. Firmware
and payload identity audits passed. The new volume, runtime window allocation,
ExitBootServices and real XNU entry remain unverified. Large-volume heap use can
fragment free runs. No UFS, root volume or Data seed is configured. The source
qualification kernel lacks board CPU/platform drivers; the candidate diagnoses
entry/MMU/first-panic behavior. Generated payloads remain local.

### V21 entry reset diagnostics (entry bands and restart observed)

The entry trace library now probes the optional Qualcomm `EFI_RESETREASON`
protocol after serial initialization and again after `ConnectAll()`. When a
provider is present, the reset code and text are shown in the UniXNU menu and
on the held return page; the serial trace records the numeric code. When the
protocol is absent, the page records the EFI status instead of guessing that a
post-MMU reboot was caused by the UEFI watchdog. The probe is read-only and
does not clear or write reset state. V21 puts a short PMIC result in the
prominent status row, renames the stale V19 labels, and increases the
active framebuffer diagnostic font scale from 3 to 5 on the J716F panel.
The diagnostic menu uses scale 4 and hides the logo to make room for results.

Use `Platforms/Lenovo/j716fPkg/j716f-entry-v21.dsc` for the recorded diagnostic
profile. Its contents match the local V21 build, retaining the historical V15
FDF and output path. The V15 DSC also contains additional standard initrd
integration declarations; those declarations were absent from the built
V21 profile. V21 uses the existing embedded read-only FAT and an empty Android
ramdisk. The public profile needs matching local binary inputs before building.

The photographed PMIC value is a boot-history baseline. Its bit meanings and
the cause of the earlier resets have not been established.

### GitHub Actions

`J716F UEFI source checks` runs on push/PR. It checks the pinned submodule Git
entries, Linux executable modes, script execution and J716F configuration
syntax. A green check means source validation passed, not that UEFI compiled.

The inherited `Build upstream active devices (manual)` workflow is retained for
explicit manual runs. It builds the devices marked Active in upstream
`Status.md`; it is not a J716F-specific builder. J716F is still a bring-up port.

The previous export lost executable modes and submodule Git entries. They have
been restored from Mu-Silicium `42fe563791e51bf5500c2e470ed294ca0c94b97c`.
The batch script also invokes Python explicitly to avoid relying on its mode.

The J716F platform uses the pinned Mu-Silicium and Mu Basecore revisions in
`.gitmodules` and the local build lock. Vendor inputs are supplied through the
local checkout only. Do not copy another device's memory map, DTB values or
vendor binaries into this platform.

## License

The base project and its subcomponents retain their original licenses. J716F
source files follow the Mu-Silicium BSD-2-Clause-Patent terms unless a file
states otherwise.
