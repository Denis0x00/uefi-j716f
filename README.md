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
- Physical UFS, ExitBootServices, real Apple ADT, XNU entry, trust cache and
  userspace remain unverified.
- TzDxe is intentionally out of scope for the current phase.

## Build boundary

### V13 memory survey

`Platforms/Lenovo/j716fPkg/j716f-memory-v13.dsc` is a separate diagnostic profile
derived from V12. It builds an external `J716fMemoryProbe.efi` for the local
embedded FAT. The application captures `GetMemoryMap`, reports free memory and
the largest 2 MiB-aligned conventional descriptor, then holds paged details for
photographs. Its capacity estimate does not include the kernel base residue.

The local Ubuntu build and Android header-v2/FV/FAT artifact audits passed.
Hardware validation is pending. The profile retains the existing memory map;
it does not load a kernel, allocate a kernel window, or exit boot services.
The generated `J716fMemoryV13.fat` and boot image remain local inputs/outputs.
This survey is needed because the current static table exposes a 224 MiB DXE
region, while the pinned DarwinBoot loader needs a minimum 256 MiB window.

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
