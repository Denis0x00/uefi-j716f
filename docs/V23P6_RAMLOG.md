# V23P6 RAMLOG r1

Independent app-only retention experiment; host verified; flashed only to boot_b, reboot command succeeded; live results pending.

- Platform: `j716fPkg/j716f-ramlog-v23p6.dsc` / `.fdf`.
- Application: `Application/J716fRamLogTest`, compiled for FAT packaging only.
- Resource: `Resources/J716fRamLogV23P6.fat` (private generated artifact).
- Memory map: `Library/J716fRamLogMemoryMapLib`; only Boot Log size changes
  from 0x80000 to the Recovery DT's 0x100000 reserved range.
- Same DXE module selection, APRIORI, compact menu scale 3 and inherited GOP as
  P5B. Actual SEC and four DXE images differ by one descriptor byte each; every
  other non-payload byte is identical.
- Console at 0xb00be000, size 0x40000, 12-byte DBGC header, no ECC. This layout
  is derived from matching-build Recovery machine code and live parameters;
  the first device run must still validate the actual header.
- Initial app action is read-only. Manual append is gated by bounds/signature,
  allowed once per invocation, and followed by DSB and readback. Never repairs
  or clears invalid headers. Full ring append replaces its oldest short span.
- Manual EFI warm restart, no automatic boot/restart, no XNU payload. A warm
  request does not certify physical reset type or retention.

Build with the normal J716F CLANGPDB RELEASE environment and FD_BASE=0xce000000,
FD_SIZE=0x700000, FD_BLOCKS=0x700, ENABLE_SECUREBOOT=0. First build the application
with a config-only FAT resource, regenerate FAT with its compiled EFI binary,
then build the final FD. Final FAT contains only unixnu.toml and the test EFI.
Never embed binaries in this public repository.

See `v23p6-validation.json` for host checks and image hashes, and the device
repository's `docs/V23P6_RAMLOG_RETENTION.md` for observations and test procedure.
No device result or persistent XNU trace is claimed. Hardware testing: boot_b only.

V23P6 RAMLOG r1 was flashed only to boot_b through fastbootd with the audited image; B activation, current-slot b and slot-unbootable:b=no were verified, and reboot command succeeded. First live header, marker append, warm-reset retention and Recovery readback await device observation. No other partition write or repository push occurred.

V23P6 first video/photo confirms the correct menu and read-only app: console DBGC 0x43474244, next=size=0x37bd9, capacity=0x3fff4, VALID, marker counts 0 and no append. The inferred console header is now device-readable and consistent, with 33819 bytes of unused capacity. No append, restart retention, Recovery readback or XNU progress is established. See research/v23p6-first-header.json.

V23P6 second video verifies manual RAM marker append and same-run readback: next=size 0x37bd9 -> 0x37c09 (+48 bytes), now=1, appended_this_run=1, APPEND READBACK OK. At about 15s the app reports Warm reset returned; no actual reboot is observed. At about 28s the app exits to parent EFI return diagnosis with Success/code 0, which is normal app return and not reset success. Cross-reset retention and Recovery readback remain unverified. Root cause of the returned firmware reset call is unknown. First-header transcription corrected to 0x37bd9 (33819 free bytes). See research/v23p6-append-reset-return.json.
