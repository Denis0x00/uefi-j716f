# Building the retained-log TB-J716F firmware

Use a Linux build host with the UniXNU toolchain and Python dependencies already
prepared. First reconstruct the workspace using the companion
[device repository](https://github.com/Denis0x00/UniXNU-j716f)'s
`scripts/export_workspace.py --destination /absolute/new/workspace --fetch-core`.
That export preserves the official core revision and its local source changes.
Place this firmware checkout at `work/uefi-j716f-early-log` inside that workspace.

From the exported workspace root:

```sh
git -C work/uefi-j716f-early-log submodule update --init Common/Mu Common/Mu_OEM_Sample
python3 work/UniXNU-j716f-early-log-firmware/scripts/firmware.py defconfig
python3 work/UniXNU-j716f-early-log-firmware/scripts/firmware.py tree
python3 work/UniXNU-j716f-early-log-firmware/scripts/firmware.py status
python3 work/UniXNU-j716f-early-log-firmware/scripts/firmware.py firmware
```

The wrapper calls the official UniXNU `build firmware` operation. Its board
target selects `Platforms/Lenovo/j716fPkg/j716f.dsc`, `DEVICE_MODEL=j716f`,
`ENABLE_SECUREBOOT=0`, and `J716F_MENU_DIAGNOSTIC=1`. It uses the core EDK2 feed
`cfb93724a06ff86ff5ba971413368a7809ee42a5` and DarwinBoot
`5243c68ec332a227fe3680cd5da6d5a0e127035c`. The inactive root `Mu_Basecore`
gitlink is retained as historical source metadata and is not substituted for
that feed. The root `build_uefi.py` legacy Mu patches are not applied by this
validated path.

The companion export applies the core menu patch already. For a separately
prepared clean core at `7b730b231746667d8b7f54746c2c1ce45852611c`, the same patch
is available at `Resources/UniXNUPatches/j716f-menu-hold.patch`; check whether
it is already applied before using `git apply`. Do not apply it twice or switch
the core revision silently. Use the read-only dependency checker to compare
the actual repositories and package bytes:

```sh
python3 work/uefi-j716f-early-log/scripts/verify_dependencies.py --core UniXNU --edk2 UniXNU/out-early-log-firmware-20261114/feeds/mu_basecore-8f10084c
```

The firmware output is
`work/uefi-j716f-early-log/Build/j716fPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`.
The historical final r22 FD was 7,340,032 bytes with SHA-256
`f45f81be07b83062b6e8a24c8e531eaf7d8c00bfadfddfbae66c5048cfe7a2b9`.
That identity records the tested build, not a guarantee of byte-reproducibility
on a new host. Recheck new outputs and provenance before packaging.

This source export has not itself received a fresh clean-room firmware rebuild.
The frozen original source and build evidence were checked during migration.
Boot image assembly additionally needs the companion device definition,
BootShim, the reviewed normalized stock DTB, kernel and user-provided system
inputs. These firmware instructions neither assemble nor flash an image.
