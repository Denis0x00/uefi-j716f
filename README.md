# uefi-j716f

UEFI platform integration for the Lenovo Xiaoxin Pad Pro 2021 (j716f / TB-J716F, SM8250-AC), using Project Silicium and Project Mu.

The platform supplies memory reservations, board peripherals, display handoff (GOP framebuffer), hardware buttons, and the firmware volume. UniXNU supplies the boot application and DarwinBoot loader.

- [Firmware integration](https://unixnu.com/zh/porting/uefi-firmware)
- [j716f device support](https://unixnu.com/zh/devices/j716f)
- [Build instructions](Building.md)
- [Platform details](Platforms/Lenovo/j716fPkg/README.md)

## Payload firmware

Build through `./unixnu build firmware --uefi ../uefi-j716f` from UniXNU.
The DSC and FDF include `UniXNURamDiskDxe`, DiskIoDxe and FAT support so the boot-image payload appears as a filesystem. `UniXNURamDiskDxe` is a shared module in UniXNU's `package/`, and uses the Mu-Silicium FDT handoff.

## Hardware Status

| Feature | Status | Implementation |
| --- | --- | --- |
| UEFI Menu & GOP | Verified | 2560x1600 BGRX8888 Simple FrameBuffer at `0x9c000000` (`J716fInheritedFbDxe`) |
| Buttons & Keys | Verified | PM8150/PM8150B SPMI PON Power, Vol+, Vol- key support (`J716fKeysDxe`) |
| Memory Map | Verified | 6 GiB / 8 GiB physical DRAM map (`0x80000000..0x1f8000000`), `0xb0000000` boot log, `0xce000000` UEFI FD, `0xd0000000` XNU window |
| BootShim & A/B slot | Verified | BootShim header, `Resources/DTBs/j716f.dtb` (728,300 bytes), safe Boot Slot B targeting |
| DarwinBoot Loader | Verified | Mu-Silicium FDT handoff, `UniXNURamDiskDxe` 96 MiB FAT payload ramdisk (`J716fIoConsoleBdsDxe`) |

## License

All code except drivers in `GPLDrivers` & `GPLApplications` & `GPLLibrary` directories are licensed under BSD-2-Clause.
GPL Drivers are licensed under `GPLv2` or later license.
