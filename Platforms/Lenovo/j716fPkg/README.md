# J716F platform package

This package contains the board-specific Mu-Silicium platform sources for
TB-J716F. It is a diagnostic port for UEFI/UniXNU bring-up.

The memory map and DTB reservations are evidence from the stock ZUI 13.0.430
environment. V14 qualified one 256 MiB window on hardware; this does not
validate ownership of all RAM. The entry profiles carry a local experimental
kernel/ADT payload. Physical UFS, a root volume and userspace remain unverified.

Vendor binaries and generated FAT resources are local build inputs and are
ignored by the public repository. See the top-level README before building.

## UniXNU payload integration

The V15 DSC/FDF declares `UniXNURamDiskDxe` together with DiskIoDxe and FAT support, as required by the current UniXNU firmware guide. The existing embedded FAT volume remains enabled for the V15 entry diagnostic until the standard Android initrd handoff is rebuilt and verified.

The recorded `j716f-entry-v21.dsc` preserves the built diagnostic profile,
which omits those additional initrd integration declarations and uses the
existing embedded FAT with an empty Android ramdisk. See the top-level
[V21 diagnostics](../../../docs/V21_DIAGNOSTICS.md) for the profile,
artifact hashes, PMIC baseline and device verification scope.
