# UEFI for Lenovo TB-J716F

Current board firmware sources for the Lenovo Xiaoxin Pad Pro 2021, Snapdragon
870 / SM8250-AC. This snapshot contains the r9 display-ownership repair and the
r22 retained early-XNU-log path used by the subsequent boot diagnostics.

The tablet has verified firmware menu entry, volume-key navigation, power-key
confirmation, stable QR log pages, restart, and menu rendering without the
previous black overlay. Power off stays off with USB/charging disconnected.
Both diagnostic reset and long-power-button reset preserved the early-log test
record. These firmware results do not establish UFS access, XNU touch/HID,
APFS root mounting, launchd, SpringBoard, or a usable iPadOS installation.

This repository supplies board memory reservations, firmware display ownership,
hardware keys, the loader handoff and log import. The UniXNU boot application,
QR viewer and DarwinBoot remain separately pinned dependencies. The menu entries
and kernel checkpoint are selected by the companion device target; the two RAM
log test entries are not automatically added by this firmware source.

Use the [official firmware build path](Building.md). The source comes from base
revision `d1d183b79d28329f46fc18a67eea9c2a51a9b939` plus the reviewed local board
changes. The six original gitlink pins are preserved. Only `Common/Mu` and
`Common/Mu_OEM_Sample` are active UEFI submodules in the validated official target;
both were clean. In particular, there is no hidden AdvancedLogger submodule
patch. The package-level menu change is retained in
`Resources/UniXNUPatches/j716f-menu-hold.patch`.

See [source provenance](docs/source-provenance.json) for dependency commits and
source identities. The six historical DTB/EFI/policy/bitmap resources remain
byte-identical to their original tracked Git blobs. Generated boot images, FD
images, kernel binaries, ROM/IPSW inputs, identities and private device logs are
not part of this source snapshot.

Original notices, per-file SPDX identifiers and [LICENSE](LICENSE) are retained.
Dependency licenses remain in their pinned repositories; this snapshot does not
relicense them. The original broader Mu-Silicium build helpers are retained for
provenance, while the instructions below describe the firmware path actually used.

References: [UniXNU](https://github.com/UniXNU/UniXNU),
[TB-J716F device sources](https://github.com/Denis0x00/UniXNU-j716f).
