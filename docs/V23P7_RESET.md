# V23P7 RESET r1

Host verified; flashed only to boot_b; inventory and PSCI queries verified by photos; reset/retention pending. Independent app-only comparison of
the installed firmware reset route and zero-initialized direct PSCI calls.

- DSC/FDF: `Platforms/Lenovo/j716fPkg/j716f-reset-v23p7.dsc` / `.fdf`.
- Application: `Application/J716fResetProbe`, built for FAT packaging only.
- Private generated resource: `Resources/J716fResetV23P7.fat`.
- Identical P6 memory map, Ring.h, font and all non-payload FV module sections,
  APRIORI and compact SEC executable. Only FAT configuration/app change.
- The unchanged ResetSystemRuntimeDxe SHA-256 is
  `d78eceecd3a9d45a5a39d2b30b32735b4e376dbf51caf4a68c572577ea5908e6`.
  Its linked SMC Warm path queries RESET2 and falls back to SYSTEM_RESET.
  Its uninitialized unused argument fields are a host finding, not a proven
  explanation for P6's returned Warm request.
- Initial inventory is read-only. No automatic startup, SMC or reset.
  It shows ResetSystem owner GUID/base/RVA and a bounded 16-byte prefix match;
  this does not establish full integrity or execution of that function.
- Manual Query invokes PSCI_VERSION and FEATURES for RESET/RESET2. Manual EFI
  Warm preserves the old firmware path. Direct RESET2 requires completed
  queries and FEATURES==0; direct RESET requires version >=0.2.
- New assembly clears unused x0..x17 inputs, preserves x18 and ABI state used,
  captures x0..x7. No counter, HVC, PMIC or vendor reset path.
- Each action commits PRE and returned results to an already valid console
  ring with barriers/readback. Bad headers are never repaired or cleared;
  invalid header/failed record prevents subsequent calls. A full ring replaces
  its oldest record-length bytes. Sequence numbers are correlation aids.

Build in the normal J716F RELEASE CLANGPDB AARCH64 environment with
FD_BASE=0xce000000, FD_SIZE=0x700000, FD_BLOCKS=0x700, ENABLE_SECUREBOOT=0.
Use a config-only FAT for the first application build, then regenerate FAT
with J716fResetProbe.efi and rebuild the final FD. FAT contains only that EFI
and unixnu.toml; keep generated binaries outside the public repository.

Host verification covers five mocked SMC wrapper cases, old Warm branch
audits, two actual-linked Call/Record/PrintLib/ring cases, actual function
byte matches, sanitizer ring tests, source hashes, non-payload equality and
Android boot image unpacking. These do not establish hardware PSCI support,
actual reset type or RAM retention.

See `v23p7-validation.json` for hashes and the device repository's
`docs/V23P7_RESET_DIAGNOSTICS.md` for observations and procedure. First device
trial: boot_b only; film inventory and **Query PSCI (NO RESET)**, then stop on
the results page. No XNU payload or progress is claimed.

V23P7 RESET r1 (uncommitted source additions on the recorded repository base; exact source/image hashes in the P7 manifest) was flashed only to boot_b through fastbootd. B activation, current-slot b and slot-unbootable:b=no were verified; reboot command succeeded. Inventory/PSCI-query observations await video. No reset trial, retention result or new XNU checkpoint is established.

Latest device photos establish EL1, ResetArch/ResetNotify/ownerFound=1, expected
driver GUID/RVA/prefix match=1, PSCI_VERSION=0x00010001 (1.1), and FEATURES=0
for both SYSTEM_RESET and RESET2 A64. Queries complete=1; valid console
next=size grows 0x2923f -> 0x2958a (+843 bytes), P7 entry=0/now=6.
This establishes query returns and same-run records only. No P7 reset,
cross-reset retention, Recovery readback or XNU milestone is established.
Next manual trial is Direct RESET2 only; photograph either the returned
x0..x3 or the first read-only page after an observed reboot, before new queries.

Latest RESET2 video/photo: two direct A64 RESET2 calls (type=0, cookie=0,
unused inputs zero) return x0=0xfffffffffffffffe, INVALID_PARAMS (-2),
x1..x3=0. Console remains VALID; next=size progresses 0x2958a -> 0x296a3
-> 0x297bc and P7 now=6 -> 8 -> 10. No reboot is observed during the 10.50s
video. This does not test cross-reset retention or SYSTEM_RESET.
The old library falls back to SYSTEM_RESET only when FEATURES fails, not
after a supported RESET2 returns an error; P6's internal return remains
uncaptured. The zero-initialized direct failure does not establish that
fixing the old uninitialized fields alone will solve reset.
Next manual trial: Direct SYSTEM_RESET only in the same image; preserve
returned x0..x3 or the first read-only inventory after an actual reboot.

## SYSTEM_RESET user report and reentry gate

User reports the first P7 Direct SYSTEM_RESET rebooted, then selecting it after reentering the app did not reboot. No new SYSTEM_RESET photo/video or post-reset ring counts yet. Fresh app load clears mQueried; without rerunning Query, the action is blocked before SMC. Second-attempt gate status versus actual returned SMC is unresolved. Preserve full page/entry/now before new queries. One reboot is user-reported; repeatability and RAM retention remain unverified.

## SYSTEM_RESET clarification and controlled retention retry

User clarified the second no-reboot selection displayed blocked; rerunning Query then Direct SYSTEM_RESET rebooted again. Two successful SYSTEM_RESET invocations are user-reported; no SYSTEM_RESET error return was reported. User then forced power off and entered ZUI13 before post-reset inventory capture. Earlier reset retention remains unknown; later marker state cannot isolate that path. Repeat a controlled P7 boot_b cycle from a photographed baseline, without forced shutdown or ZUI detour. No new flash/source/device commands performed.

Controlled P7 retention cycle relaunched: fastboot initially reported current-slot a and slot-unbootable:b=yes. set_active b succeeded; current-slot b and slot-unbootable:b=no verified; reboot command succeeded. No partition image was flashed. Prior P7 flash/image hash supplies expected identity, not a new partition readback. Await menu identity, baseline and post-SYSTEM_RESET read-only photo; retention remains unverified.
