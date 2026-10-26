# V23P8 RETENTION r1

Independent UEFI application in
`Platforms/Lenovo/j716fPkg/Application/J716fRetentionProbe`, selected by
`j716f-retention-v23p8.dsc` / `.fdf`. The menu has no timed start. It contains
no XNU and defaults to read-only entry/refresh.

See `UniXNU-j716f/docs/V23P8_RETENTION.md` for the controlled device sequence,
record format, previous invalid-header evidence and interpretation limits.
`docs/v23p8-validation.json` records the audited candidate identity.

The original P7 memory map reserves 0xb0000000+0x100000 uncached. P8 writes
only the previously audited 0xb00be000+0x40000 console. Manual invalid-header
initialization needs two consecutive confirmations and an unchanged snapshot;
valid rings cannot be reinitialized. The 12-byte header is replaced and DBGC
published last, then an INIT record saves the old header/whole-region CRC.
Payload is preserved except for ordinary record appends.

CRC-checked 64-byte records are scanned logically and separately as raw bytes.
The ENTRY snapshot remains frozen throughout the invocation. BASELINE and
successful PSCI version/FEATURES queries are prerequisites for direct reset.
The first reset selection only writes PRE_RESET for photography; the second
checks its ID/CRC before one SYSTEM_RESET call. Navigation cancels confirmation.
There is no RESET2 or EFI Warm path. The SMC wrapper is byte-identical to P7.

Host checks pass: two firmware passes, sanitizer ring/record tests, three mocked
wrapper cases and nine linked-app UI cases. The final image changes only the
embedded FAT config/app versus P7; non-payload FV sections and compact SEC
executable are byte-identical. P8 was flashed only to boot_b; B was activated,
current-slot b and slot-unbootable:b=no were verified, and reboot succeeded.
One controlled hardware SYSTEM_RESET cycle is now observed: immediate read-only
P8 reentry retains a valid header, next=size=0x3fcbf, six logical/raw records
and matching PRE_RESET ID=1/phase=3/CRC=E13F0C63. Invocation advances to 2
with baseline/query still zero. This proves that record survived this cycle;
other reboot paths and XNU/MMU progress remain unverified. See the device
repository research/v23p8-retention.json for media hashes and observations.
