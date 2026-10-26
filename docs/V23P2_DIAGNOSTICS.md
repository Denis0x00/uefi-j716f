# V23P2 prefix checkpoints

P2 reuses the V23 firmware profile and replaces only the separately built payload.
It preserves the previous P/C/V/M artifacts. The menu identifies V23P2 and mode 5.
Twelve inline squares isolate title rendering, its original DSB/RET, CurrentEL,
SCTLR, subsequent lines, pre-SMC progress, SMC return and exception entry.

Host audits, source crypto resealing and five mocked instruction scenarios passed.
On hardware only squares 1 and 2 are visible: title stores completed, but no
post-title-DSB square is observed. Prioritize the DSB boundary and earlier
accesses. This is not a PC trace or proof of a particular reset cause. The legacy
BCM2837 timer store remains a candidate to isolate, not a confirmed cause.
PMIC changed from 0x02000001 to 0x02000041 after the Lenovo-logo restart.
No PSCI, counter or MMU-on qualification is claimed.

Only boot_b was targeted. The bootloader flash command failed with unknown
command; fastbootd subsequently reported successful flash, B activation and
reboot. B retry-count was 2 in bootloader mode and unsupported in fastbootd.
See `docs/v23p2-validation.json` for sanitized metadata and the companion device
repository's `docs/V23P2_PREFIX_CHECKPOINTS.md` for square meanings.
No generated payload or private input is published. Sources remain local additions.
