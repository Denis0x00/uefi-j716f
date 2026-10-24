# J716F platform package

This package contains the board-specific Mu-Silicium platform sources for
TB-J716F. It is a diagnostic port for UEFI/UniXNU bring-up.

The memory map and DTB reservations are evidence from the stock ZUI 13.0.430
environment. They still require live verification before they can be used as an
XNU owned-memory map. The diagnostic build intentionally omits RamManager and
does not configure an IPSW payload, userdata window or kernel handoff.

Vendor binaries and generated FAT resources are local build inputs and are
ignored by the public repository. See the top-level README before building.
