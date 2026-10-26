# V23P9 KERNEL TRACE r1

Independent profile: `j716f-kernel-trace-v23p9.dsc` / `.fdf`.
Reader: `Application/J716fKernelTrace`, built for the FAT payload rather than
executed as a firmware-volume module. P8 source/profile/artifacts are preserved.

Two untimed payloads identify P9E (kernel entry record then reset) and P9M
(kernel entry/pre-MMU/post-MMU records then reset). Both default to the read-only
reader and require manual Query/ARM plus a separate manual kernel launch.
There is no automatic launch or reader reset action. The kernel diagnostic
requests direct SYSTEM_RESET only after its terminal checkpoint; returned x0
is logged. No counter, graphics or normal XNU continuation is claimed.

The reader accepts old P8 records plus phases 9..14 and freezes six per-phase
logical/raw summaries for the highest existing session ID. ARM cookie and
expected prior phase bind each kernel append to the current manual attempt.
An invalid ring is never repaired by P9. Its only persistent RAM write range
remains the audited `0xb00be000+0x40000` console. Temporary stack use and
bootstrap page-table edits are confined to the kernel-owned allocations.

See `UniXNU-j716f/docs/V23P9_KERNEL_TRACE.md` for exact sites, register/stack
preservation, console-only NC mapping, guard codes and the E-before-M hardware
sequence. `docs/v23p9-validation.json` describes host artifact validation;
`UniXNU-j716f/research/v23p9-kernel-trace.json` tracks device observations.

Do not equate a host emulator or descriptor walk with actual MMU/cache success.
The original BCM access remains in M, so ENTRY without PRE can still precede
that legacy access or another original-prefix failure.

## P9E device checkpoint

P9E controlled entry trial is device-observed. Video starts with armed ID=2/CRC=47CCACB5, shows the E entry-only kernel selected, then screen loss, Lenovo/Android splash and return to the P9E menu. The separate photo, identified by the user as first reader reentry after that reboot, shows frozen ENTRY and NOW VALID: DBGC=0x43474244, next=size=0x2b264, whole-console CRC=0x8b79118f, logical/raw counts=6/12, latest ID=2/phase=10/CRC=0xf17e59fc. ARM has one logical and one raw hit with matching prior CRC=0x47ccacb5; KERNEL_ENTRY has one of each with CRC=0xf17e59fc and SCTLR_EL1=0xc00808 (M=0). Reconstructing phase-10 record bytes from ID/value/prior header reproduces the photographed CRC. New reader invocation ID=3/query=0/armed=0 and read-only status confirm capture before new Query/ARM. PRE_MMU, POST_MMU, MAP_FAILURE and RESET_RETURN have zero hits, expected for E. The valid header and same-session entry record survived one observed reboot; actual reset-source attribution, M execution and sustained MMU-on remain unverified.

Next: P9E entry-record retention prerequisite passed. Prepare fastboot and recording for the already-built V23P9M; flash only boot_b, verify B active/bootable, then open reader FIRST and photograph frozen ENTRY. If NOW VALID, Query then ARM once and photograph the new ID/CRC before launching M. After reset photograph reader ENTRY before any Query/ARM; match ARM/ENTRY/PRE_MMU/POST_MMU or MAP_FAILURE by that new ID. If no reset, preserve last display/video before recovery.

## P9M menu launch

V23P9M was flashed only to boot_b after exact image SHA256 and fastbootd product/unlock/slot-count/partition-size gates. B activation, current-slot b and slot-unbootable:b=no were verified; reboot command succeeded. Launch targets the untimed menu only; recording readiness for the manual kernel trial was not newly confirmed. Initial M menu identity, reader ENTRY, Query/ARM and kernel/MMU checkpoints await device observations. The observed P9E entry-record retention prerequisite remains preserved.

Next: At the P9M menu open P9 reader FIRST and photograph the complete frozen ENTRY before Query/ARM. If NOW is VALID, prepare recording, then Query once and ARM once; photograph THIS RUN ID/CRC and ARM readback status before launching the M kernel. Stop on INVALID and preserve the page.

## M initial reader observation

Initial reader photo after the P9M fastboot launch confirms common J716F V23P9 KERNEL TRACE r1 reader, frozen ENTRY/NOW DBGC=0x43474244, next=size=0x2bf58, ENTRY whole-console CRC=0xbdb07c93 and NOW VALID. Logical count=0; raw count=6 with old P8 ID=1/phase=3/CRC=0xe13f0c63; all six frozen P9 checkpoint rows have zero logical/raw hits. THIS RUN ID=2/query=0/armed=0 at EL1, read-only status. The earlier E ARM/ENTRY chain is not present in this snapshot after the intervening fastboot path; the cause or timing of those changes is not isolated, and this does not alter the separately observed E controlled-reboot result. No M kernel launch or MMU progress is observed.

Next: In this reader invocation prepare recording, select Query PSCI + record (NO RESET), then ARM next kernel attempt once after successful Query. Photograph the full ARM page including NOW VALID, THIS RUN ID/CRC and ARM readback OK before manual M kernel launch. Stop and preserve the page if Query/ARM fails or the header becomes INVALID.

## M recovery reader

M recovery reader photo shows frozen ENTRY and NOW VALID: DBGC=0x43474244, next=size=0x2c0d8, whole-console CRC=0xc0749332, logical/raw counts=6/12, latest ID=2/phase=10/CRC=0xdc7aa441. Frozen ARM has one logical and one raw hit, CRC=0xc2c22d94 and cookie=0x50394b45524e454c, exactly matching the M prelaunch baseline. KERNEL_ENTRY has one logical and one raw hit, CRC=0xdc7aa441, SCTLR_EL1=0xc00808 (M=0); reconstructing the record with prior header 0x2c098 reproduces that CRC. PRE_MMU, POST_MMU, MAP_FAILURE and RESET_RETURN all have zero hits. New reader invocation ID=3/query=0/armed=0 with read-only status confirms no new Query/ARM in the snapshot. This recovery capture follows the user-reported first automatic reboot, an unarmed second launch that stalled, and the requested hardware recovery; it is not the missing immediate first-reboot capture, and recovery method/timing is not independently captured. It establishes a surviving M-session entry record and consumed ARM, with no retained later checkpoints. Source requires ARM immediately before entry: the now-visible consumed chain supports second-launch gate rejection, but does not prove the exact PC of that launch. The missing PRE record spans original firstcpu code, pre-hook stack handling, mapping checks/setup/barriers and PRE append; it does not isolate an SCTLR/MMU failure or a reset source.

Next: Preserve this recovery snapshot. Repeat the existing P9M once with a clean observed sequence: current reader invocation ID=3, Query then ARM once, photograph actual ID/CRC and valid header, launch M exactly once, record the complete cycle, then open reader FIRST and photograph frozen ENTRY before any Query/ARM or second kernel selection. If it stalls, preserve last screen/video before further recovery. If ENTRY-only repeats in the immediate reader capture, add finer prefix and pre-hook-before-mapping checkpoints; do not change SCTLR or disable the legacy BCM store based on the present snapshot.
