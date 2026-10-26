# V23 independent PSCI, counter and MMU records

V23 uses a separate DSC/FDF and UniXNULib identity. It retains the V22 memory
map, loader, module GUID selection, APRIORI ordering, framebuffer and key drivers.
The four payloads provide terminal single-CPU diagnostics:

| Variant | Probe | Terminal observation |
| --- | --- | --- |
| P | Pre-MMU PSCI SYSTEM_RESET, no counter | A0 before SMC; A1 and RETURN if SMC returns |
| C | Pre-MMU CNTPCT, fixed iteration sampling | B2 with FREQ/FIRST/LAST/DELTA/BACK/MOVES |
| V | Pre-MMU CNTVCT, fixed iteration sampling | Same records as C |
| M | Explicit framebuffer mapping around SCTLR/ISB | D1 before enable, D2/D3 after enable; DF refuses incompatible tables |

The screen displays hexadecimal values with an eight-times-scaled 5x7 font.
E0 attempts to show the previous stage, vector, ESR/FAR/ELR/SPSR. FAR validity
depends on the exception class. An unusable vector or display mapping can still
prevent this output. This handler never returns to normal XNU.

The device repository owns the assembly, pinned-kernel patch guards and crypto
resealing. See its [complete test and mapping description](https://github.com/Denis0x00/UniXNU-j716f/blob/main/docs/V23_INDEPENDENT_PROBES.md).
The linked document is being prepared locally and may not be published yet.

M maps only the existing board display reservation through a checked spare page
in the kernel's own bootstrap tables; it adds no firmware memory allocation or
retained RAM log. Screen records require continuous video. Counter tests do not
request reset; M uses neither counters nor PSCI. Normal platform initialization,
root mounting, persistent logging and native display support remain unverified.

All four FD/FAT/Android images passed host audits and source crypto HMAC checks.
The local instruction emulator passed 16 logic scenarios with mocked system
registers and firmware behavior; this is not real MMU or PSCI qualification.
V23P was flashed only to boot_b, B activated and reboot requested. Its video
confirms the menu and first V23/mode-1 line, followed about 17.1 seconds later
by screen loss and a Lenovo-logo restart back to the menu. EL/SCTLR, A0/A1,
RETURN and E0 are not observed; PMIC remains 0x02000041. PSCI execution and
reset attribution remain unknown. C/V/M have not been hardware tested.
Investigate first-line completion/DSB/return and the common register-read
prefix before interpreting later probes. Metadata is in
`docs/v23-validation.json`; firmware configuration is
`Platforms/Lenovo/j716fPkg/j716f-entry-v23.dsc`.

Hardware test scope remains boot_b only. Record B-slot metadata before any slot
change. Never write boot_a, vbmeta or userdata; TzDxe remains outside scope.
