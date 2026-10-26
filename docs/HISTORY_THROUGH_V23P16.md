# Historical progress narrative through V23P16

Archived on 2026-10-26. Entries describe their own point in time; later
evidence supersedes earlier pending/next-action statements. Current status
is on the repository home page and in the P16 progress assessment.

# uefi-j716f

Mu-Silicium UEFI port for the Lenovo Xiaoxin Pad Pro 2021 (TB-J716F, `j716f`,
Qualcomm SM8250).

This is the companion firmware repository for `UniXNU-j716f`. It is a research
and bring-up port; it does not claim to boot iPadOS, iOS or a production XNU
image.

## Included

- J716F platform DSC/FDF files, memory map and platform libraries.
- UniXNU menu, RAM FAT, loader and DarwinBoot diagnostic paths.
- Android boot image header-v2 build patch.
- Source locks and configuration for the Mu-Silicium base.

## Excluded from the public tree

- `Binaries/j716f/` and vendor PE files extracted from ZUI XBL.
- Generated FAT resources, firmware volumes, boot images and build output.
- Device backups, photos, ADB/fastboot logs, VM details and serial numbers.
- IPSW, kernelcache, ADT, trust cache, userdata and Data seed.

The public tree therefore needs a local vendor-input checkout before it can be
built. The exact ZUI source and hashes are checked by the private preflight used
in the local bring-up environment.

## Current validation

- UEFI menu, volume keys, power key and RAM FAT / BlockIO paths were validated
  on the real device.
- Official DarwinBoot v2 protocol and invalid-payload refusal were validated.
- Earlier V16/V17 diagnostics recorded ExitBootServices and early XNU entry
  checkpoints. V18 still reset after replacing its post-MMU reset with WFE;
  the reset source remains unresolved.
- V21 was built, audited, flashed to `boot_b` and rebooted. A device photograph
  confirms the V21 menu, enlarged menu text and `PMIC RESET: 0x02000001`.
- V21 video confirms its entry bands and a subsequent Lenovo-logo restart; reset attribution remains unresolved.
- V22A/B videos show 20.245/19.730 seconds from magenta to screen loss, one trial each; the intended 1s/3s response was not observed. B ended in A-slot ZUI.
- V23 P/C/V/M passed host image and crypto audits. P was flashed to boot_b and tested: only its first diagnostic line appeared before a later restart back to its menu. PSCI execution remains unverified; C/V/M are untested.
- Native platform drivers, physical UFS/root, trust cache and userspace remain unverified.
- TzDxe is intentionally out of scope for the current phase.

Current details: [V23 independent probes](docs/V23_DIAGNOSTICS.md),
[V23 validation](docs/v23-validation.json), [V22 checkpoints](docs/V22_DIAGNOSTICS.md),
[V22 validation](docs/v22-validation.json), [V21 diagnostics](docs/V21_DIAGNOSTICS.md),
[public validation metadata](docs/v21-validation.json), and the companion
[device progress record](https://github.com/Denis0x00/UniXNU-j716f/blob/main/docs/V21_RESET_DIAGNOSTICS.md).

## Build boundary

### V13 memory survey

`Platforms/Lenovo/j716fPkg/j716f-memory-v13.dsc` is a separate diagnostic profile
derived from V12. It builds an external `J716fMemoryProbe.efi` for the local
embedded FAT. The application captures `GetMemoryMap`, reports free memory and
the largest 2 MiB-aligned conventional descriptor, then holds paged details for
photographs. Its capacity estimate does not include the kernel base residue.

The local Ubuntu build and Android header-v2/FV/FAT artifact audits passed.
The real device survey and EFI_SUCCESS return were validated. The profile retains the original memory map;
it does not load a kernel, allocate a kernel window, or exit boot services.
The generated `J716fMemoryV13.fat` and boot image remain local inputs/outputs.
This survey is needed because the current static table exposes a 224 MiB DXE
region, while the pinned DarwinBoot loader needs a minimum 256 MiB window.

### V14 memory-window qualification

`Platforms/Lenovo/j716fPkg/j716f-window-v14.dsc` selects independent memory-map
and menu-report libraries. V13 is retained unchanged. The new candidate region
is `[0xD0000000,0xE8000000)`; the application allocates one 256 MiB window with
the pinned DarwinBoot alignment and the current kernel's `0x7C000` residue.

Real-device evidence on 2026-10-24 confirms `[0xD007C000,0xE007C000)` allocation,
two complete address-dependent 64-bit write/read rounds (second inverted), data
cache clean/invalidate, and FreePages. Result: 7 PASS, 0 FAIL, 512 MiB verified
across two rounds of the same 256 MiB window. The V14 return page also confirms
EFI_SUCCESS, Complete=1 and matching counts. This does not certify all RAM.

The application and menu use identical version-2 result headers. Firmware
artifact audits checked 32 source PE modules, no vendor PE, APRIORI, embedded
FAT identity and Android header-v2. Kernel entry and ExitBootServices remain
unverified. Generated FAT, firmware and photos stay outside this repository.

`Tools/make_j716f_window_fat.py --efi <J716fMemoryWindowProbe.efi> --out
<Resources/J716fWindowV14.fat>` creates the local FAT after the first build;
rebuild the firmware to embed it. The FAT helper retains the earlier tested
volume layout. `Tests/WindowPolicyTest.c` compiles the actual WindowPolicy.h.

### V15 real-kernel entry diagnostic (device test pending)

`j716f-entry-v15.dsc` keeps the V14 memory map and 7 MiB FD, but selects a
separate read-only 64 MiB FAT32 driver and entry menu. The local volume carries
the experimental prelinked kernel, J716F ADT and manual XNU menu entry. Firmware
and payload identity audits passed. The new volume, runtime window allocation,
ExitBootServices and real XNU entry remain unverified. Large-volume heap use can
fragment free runs. No UFS, root volume or Data seed is configured. The source
qualification kernel lacks board CPU/platform drivers; the candidate diagnoses
entry/MMU/first-panic behavior. Generated payloads remain local.

### V21 entry reset diagnostics (entry bands and restart observed)

The entry trace library now probes the optional Qualcomm `EFI_RESETREASON`
protocol after serial initialization and again after `ConnectAll()`. When a
provider is present, the reset code and text are shown in the UniXNU menu and
on the held return page; the serial trace records the numeric code. When the
protocol is absent, the page records the EFI status instead of guessing that a
post-MMU reboot was caused by the UEFI watchdog. The probe is read-only and
does not clear or write reset state. V21 puts a short PMIC result in the
prominent status row, renames the stale V19 labels, and increases the
active framebuffer diagnostic font scale from 3 to 5 on the J716F panel.
The diagnostic menu uses scale 4 and hides the logo to make room for results.

Use `Platforms/Lenovo/j716fPkg/j716f-entry-v21.dsc` for the recorded diagnostic
profile. Its contents match the local V21 build, retaining the historical V15
FDF and output path. The V15 DSC also contains additional standard initrd
integration declarations; those declarations were absent from the built
V21 profile. V21 uses the existing embedded read-only FAT and an empty Android
ramdisk. The public profile needs matching local binary inputs before building.

The photographed PMIC value is a boot-history baseline. Its bit meanings and
the cause of the earlier resets have not been established.

### GitHub Actions

`J716F UEFI source checks` runs on push/PR. It checks the pinned submodule Git
entries, Linux executable modes, script execution and J716F configuration
syntax. A green check means source validation passed, not that UEFI compiled.

The inherited `Build upstream active devices (manual)` workflow is retained for
explicit manual runs. It builds the devices marked Active in upstream
`Status.md`; it is not a J716F-specific builder. J716F is still a bring-up port.

The previous export lost executable modes and submodule Git entries. They have
been restored from Mu-Silicium `42fe563791e51bf5500c2e470ed294ca0c94b97c`.
The batch script also invokes Python explicitly to avoid relying on its mode.

The J716F platform uses the pinned Mu-Silicium and Mu Basecore revisions in
`.gitmodules` and the local build lock. Vendor inputs are supplied through the
local checkout only. Do not copy another device's memory map, DTB values or
vendor binaries into this platform.

## License

The base project and its subcomponents retain their original licenses. J716F
source files follow the Mu-Silicium BSD-2-Clause-Patent terms unless a file
states otherwise.

Latest prefix test: [V23P2](docs/V23P2_DIAGNOSTICS.md). Only squares 1/2 appeared; the title DSB boundary needs isolation. PSCI remains unverified.

Latest built controls: [V23P3A/B](docs/V23P3_DIAGNOSTICS.md). Every firmware module outside the payload matches P2. Host audits passed; no P3 device test or push has been performed.

V23P3A was flashed only to boot_b via fastbootd, B activated and reboot command succeeded. Video, menu identity, prefix progress and PSCI results are pending; P3B remains unflashed.

V23P3B was flashed only to boot_b via fastbootd, B activated and reboot command succeeded. User report for P3A was brief screen then reboot; no checkpoint or PSCI result established. P3B video is pending.

V23P3A 视频已确认菜单身份；约1.7秒后画面变黑，未见诊断方块或EL/SCTLR，PSCI与复位源仍未知。

V23P3B 视频确认标题 mode 5 和方块 1/2；方块 3、EL/SCTLR 未见。约20.9秒后画面消失，随后 Lenovo/Android 启动并回到 UniXNU 菜单。标题 DMB 对照未显示出越过第2格的硬件效果，PSCI/复位源仍未知。

Latest diagnostic: V23P4 early MMIO envelope (docs/V23P4_DIAGNOSTICS.md). Host audits passed; hardware test is pending and boot_b is the only proposed target.

Latest diagnostic: V23P4 r1 early MMIO envelope (docs/V23P4_DIAGNOSTICS.md). Host audits passed; hardware test is pending and boot_b is the only proposed target.

V23P4 r1 was flashed only to boot_b through fastbootd; B activation, current-slot b and slot-unbootable:b=no were verified; reboot command succeeded. Menu identity, marker visibility, PSCI and reset observations await video. Retry-count variable is unsupported.

V23P4 r1 video confirms both early MMIO markers and title mode 5 with prefix squares 1/2; square 3 and EL/SCTLR are not observed. Original STR was passed, but external transaction completion is unproven. Diagnostic display disappears about 15.9s after entry, then Lenovo/Android splash and r1 menu return; PMIC 0x02000001 -> 0x02000041, reset cause unknown.

Next-build source preference: UniXNU menu scale changed from 4 to 3 after r1 video; not compiled or flashed. Kernel diagnostic font unchanged. Original r1 library sources were privately snapshotted and match its remote-build hashes; the next build must explicitly audit the changed menu module.

V23P5A/B matched controls passed host image, source/HMAC, pair-delta and emulator checks. A kernel equals P4 r1; B differs only in relocated STR-to-NOP and HMAC. Both use compiled menu scale 3, identical non-payload modules, markers and title DSB. No P5 hardware trial or device write. Test A then B on boot_b. See docs/V23P5_DIAGNOSTICS.md.

V23P5A was flashed only to boot_b in fastbootd; B activation, current-slot b and slot-unbootable:b=no were verified; reboot command succeeded. A menu identity, marker progression and reset observations await video. P5B remains unflashed. No PSCI or MMU-on result is claimed.

V23P5A video confirms the compact A menu, title mode 5, prefix squares 1/2 and both early MMIO markers, reproducing P4 r1 visible progress. No square 3, EL/SCTLR or PSCI evidence. Display disappears about 17.47s after entry, followed by Lenovo/Android splash and return to A menu; PMIC 0x02000001 -> 0x02000041. Reset cause and external MMIO completion remain unknown. P5B is the next boot_b control and remains unflashed.

V23P5B was flashed only to boot_b in fastbootd; B activation, current-slot b and slot-unbootable:b=no were verified; reboot command succeeded. Its menu identity and diagnostic progress await video. Reviewed P5A remains the visible control; no P5B functional, PSCI or MMU-on result is claimed.

V23P5B video confirms the compact B menu, both lower envelope markers and partial title in frame 56; the diagnostic is captured in only one camera frame. Top prefix 1 completion, prefix 2/3, EL/SCTLR and A0/pre-SMC are not established. Entry band to display loss is about 0.067s (camera observation), followed by Lenovo/Android splash and return to B menu; PMIC 0x02000001 -> 0x02000041. Rapid intentional PSCI reset is a hypothesis, not a verified result. Next proposed control retains P5B and replaces only its SMC with an in-place branch, preserving the preceding DSB; not yet built. This checkpoint supersedes earlier pending/unflashed notes; no new device operations or push occurred during video review.

## V23P6 RAMLOG

App-only retention test prepared and host audited; no hardware result. See [V23P6](docs/V23P6_RAMLOG.md).

V23P6 RAMLOG r1 was flashed only to boot_b through fastbootd with the audited image; B activation, current-slot b and slot-unbootable:b=no were verified, and reboot command succeeded. First live header, marker append, warm-reset retention and Recovery readback await device observation. No other partition write or repository push occurred.

Latest prepared control: [V23P7 RESET r1](docs/V23P7_RESET.md), host verified and unflashed; only P6 FAT config/app changes. First trial is inventory and manual PSCI queries on boot_b. No reset/retention/XNU result.

V23P7 RESET r1 (uncommitted source additions on the recorded repository base; exact source/image hashes in the P7 manifest) was flashed only to boot_b through fastbootd. B activation, current-slot b and slot-unbootable:b=no were verified; reboot command succeeded. Inventory/PSCI-query observations await video. No reset trial, retention result or new XNU checkpoint is established.

V23P7 photos verify EL1, installed reset driver GUID/RVA/prefix match, PSCI_VERSION=0x00010001 (1.1) and FEATURES=0 for SYSTEM_RESET and RESET2 A64. Valid console next=size grows 0x2923f -> 0x2958a (+843 bytes); P7 entry=0/now=6 and Queries complete=1. Query returns and app-displayed same-run records are established. No actual P7 reset, retention, Recovery readback or new XNU checkpoint. Next manual trial uses Direct RESET2 only; preserve initial post-reboot inventory or returned x0..x3. Uninitialized old-driver inputs remain an unproven cause.

V23P7 RESET2 video/photo confirms two direct 0xc4000012 type=0/cookie=0 calls return x0=0xfffffffffffffffe (INVALID_PARAMS -2), x1..x3=0. FEATURES remains 0; valid console next=size 0x2958a -> 0x296a3 -> 0x297bc and P7 now 6 -> 8 -> 10. No reboot is observed in the 10.50s video; retention and Recovery readback remain untested. Old Warm falls back only on FEATURES failure, not on returned RESET2 error; historical P6 SMC returns remain uncaptured. Next manual test is existing Direct SYSTEM_RESET only. No new device commands, flash or source changes during this media review.

## V23P9 kernel trace

See [P9 kernel trace](docs/V23P9_KERNEL_TRACE.md) and [host validation](docs/v23p9-validation.json). The P8 retention baseline is preserved. Test E before M; actual device state is tracked in the device repository research/v23p9-kernel-trace.json.

V23P10A/B/C terminal prefix controls are host verified and unflashed. See [procedure and scope](docs/V23P10_PREFIX_CONTROLS.md).

Next firmware compilation: add Reboot and Power off to the XUI main menu; see [request and validation scope](docs/NEXT_MENU_BUILD.md).

## V23P11 current diagnostic candidates

A/B/D host verified and unflashed. [Publication stages and menu power actions](docs/V23P11_COMMIT_PUBLICATION.md); [image validation](docs/v23p11-validation.json). Test A first on boot_b only.

## V23P12 independent-slot candidates

V23P12A/B built and host verified, unflashed. Six independent pre-staged CRC-bound records use one-byte commits; ENTRY high32 binds ARM CRC and low32 is SCTLR. A skips BCM and B preserves captured original address/value, with exactly one STR/NOP probe delta and identical reader/ADT/nonpayload firmware. 58 actual-assembly and22 linked-reader mocked cases passed; final image/HMAC/source audits passed. Byte-identical P11 power app reuses22 host cases. No P12 hardware/retention/MMU-on result; A first on boot_b only. See [P12 procedure](docs/V23P12_INDEPENDENT_SLOTS.md).

## P12 r2 full-console session handling

P12 r2 A/B built and host verified, unflashed. New reader requires explicit double confirmation before discarding a VALID saturated/exhausted console logical history; frozen entry remains, payload not bulk-cleared, INIT records old next/size, failure locks Query/ARM. 45 actual-linked reader cases passed including full-session-to-both-unchanged-kernels-to-frozen-reader flow, changed-console cancellation, failure and stale-ID gates. A/B kernel/ADT/probes equal r1 byte for byte; 58 prior assembled cases reused after identity check. Power app equals P11, 22 cases reused. Final FD/FAT/EFI/source/HMAC/nonpayload/boot-image checks passed. No r2 hardware, retention or MMU-on result; A first on boot_b only. See [procedure](docs/V23P12R2_SESSION_ROLLOVER.md).

## V23P13 MMU continuation

P13P/M terminal bridge built and host verified: complete BCM routing block excluded, original continuation restored to guarded console mapping and pre/post SCTLR hooks. Six independent P13 ARM-bound commits plus actual terminal SCTLR record; 63 actual-kernel and 34 linked-reader cases passed with privileged/MMU/UEFI/SMC mocks. Final image/HMAC/source/nonpayload/SEC audits passed. P13M boot_b trial now retains a matching ID1 ARM, all six logical/raw witnesses and POST_MMU SCTLR0x3454593d(M1); all nine record CRCs reconstruct. Short hardware MMU-on execution is established. Reset source, sustained kernel continuation and timer/GIC remain unverified. Native timer source profile resolves but has not been compiled; ArmCpu integration remains pending. BCM B is optional; test P13M mainline on boot_b, P13P only if needed.

See [P13 procedure](docs/V23P13_MMU_CONTINUATION.md).

## V23P14 original early initialization

P14M postlaunch reader photo confirms ENTRY/NOW VALID next=size0x2e5a6, consoleCRC5D1C75BD, logical13/raw26, latest ID2/phase12/76EE0045. Fresh ARM6304806B matches prelaunch; ENTRY FCBEA27A value6304806B00C00808(M0). All six phases16..21 are L1/R1 with bound value503134536304806B and CRCs66FE13FA,A1D55ACD,BAF528F7,91424ACD,8CB5CC83,C0FB7ACD. Terminal76EE0045 records actual SCTLR3454593D(M1). All nine CRCs reconstruct exactly; header growth512 matches ENTRY+six slots+terminal. New reader ID3/query0/armed0, no MAP_FAILURE or RESET_RETURN. One original early-init path reaches cpu_data_init return before PE_init_platform, with records present after user-reported automatic reboot. No continuous reset video reviewed; reset source, sustained initialization, timer/GIC and userspace remain unverified.

Next: instrument PE_init_platform entry/return and early dependencies with fresh version-specific ARM binding.

See [P14 procedure](docs/V23P14_EARLY_INIT.md).

## V23P15 platform initialization

P15M continuous13.932s video confirms selected P15M kernel, DARWIN BOOT/green handoff, screen loss, Lenovo/Android splash, P15M menu return and read-only reader. Final video reader matches photo: ENTRY/NOW VALID next=size0x328a4, consoleCRCF498612C, logical13/raw13, latest ID1/phase12/F286B6BF. Fresh ARM B9A219EB matches prelaunch; ENTRY F2FD8F05 valueB9A219EB00C00808(M0). All six phases16..21 are L1/R1 with bound value50313553B9A219EB and CRCs99CE21F7,D58097B9,CEA0E583,E51787B9,F8E001F7,B6E7E7E2. Terminal F286B6BF records actual SCTLR3454593D(M1). All nine CRCs reconstruct exactly; growth512 matches ENTRY+six slots+terminal. New reader ID2/query0/armed0, no MAP_FAILURE/RESET_RETURN. Original PE_init_platform(0, bootArgs) entered and returned before next arm_init call, with records retained over one observed reboot. PMIC02000001->02000041. Reset source, sustained continuation, interrupt/GIC/native timer and userspace remain unverified.

Next: audit subsequent timer/WFE/miscellaneous setup before new original-continuation checkpoints.

See [P15 procedure](docs/V23P15_PLATFORM_INIT.md).

## V23P16 timer/WFE initialization

P16M timer/WFE/misc terminal diagnostic built and host verified: 48 actual-kernel and 31 linked-reader mocked cases passed. Original PE/timer/WFE/misc BL/LR and WFE bodies preserved; independent bound phases18..21 cover timer stub return, WFE configure return, WFE init return and misc completion. Terminal phase12 records actual CNTKCTL, not SCTLR, after M1 guard. Actual parser/calculation executes with pinned ADT in synthetic boot arguments (19.2MHz/default threshold4). Interrupted configure/init, original arithmetic trap/panic, late corruption, consumed ARM and mapping/session gates checked. Final source/FD/FAT/EFI/HMAC/nonpayload/SEC/boot-image audits passed. No P16 hardware result or device write; timer interrupt/WFE wait and ArmCpu/GIC remain separate. boot_b only; reader FIRST, fresh Query/ARM each launch.

See [P16 procedure](docs/V23P16_TIMER_WFE.md).

### P16M device operation

P16M flashed only to boot_b after final image/source hash and fastbootd product/unlock/slot/partition gates. B active and bootable verified; reboot command succeeded. Recording readiness confirmed. Menu appearance, initial reader, fresh Query/ARM and kernel observations remain pending. Phase12 is CNTKCTL, not SCTLR. Open P16 reader FIRST and photograph the full initial page before Query/ARM/session action or kernel launch.

### P16M initial reader

P16 initial reader photo confirms WFE TRACE r1 at EL1: ENTRY/NOW VALID next=size0x28761, consoleCRCECF994E2, logical0/raw13. Frozen ID1 ARM B9A219EB cookie5031354B45524E4C and ENTRY F2FD8F05 valueB9A219EB00C00808 match old P15; raw latest phase12 F286B6BF is not a P16 CNTKCTL terminal. All six P16 slot rows and four terminal rows are zero. THIS RUN ID2/query0/armed0. Unused tail96403 bytes exceeds Query896-byte budget; no new session needed. No P16 launch or controlled reset retention result; parent menu not pictured.

### P16M fresh ARM

P16 fresh Query/ARM photo confirms NOW VALID next=size0x288a1, logical5/raw18, THIS RUN ID2/query1/armed1/CRC93A96F6F, EL1, PSCI1.1 and SYSTEM_RESET FEATURES0. ARM + six invalid templates OK is displayed. Growth320 bytes matches four Query records plus ARM; phase9 CRC reconstructs exactly with P16 cookie5031364B45524E4C and prior0x28861. Frozen initial0x28761/ECF994E2 and old P15 rows remain unchanged. No new-session action or P16 launch observed. One fresh ID2 attempt is ready; predicted complete header0x28aa1 is derived, not observed.

### P16M hardware WFE/misc result

P16M continuous11.998s video confirms selected P16M, DARWIN BOOT/green handoff, screen loss, Lenovo/Android splash, P16M menu return and read-only reader matching final photo. ENTRY/NOW VALID next=size0x28aa1, consoleCRCB9A3B4BA, logical13/raw26, latest ID2/phase12/A684F65B. Fresh ARM93A96F6F matches prelaunch; ENTRY5AF6E0B5 value93A96F6F00C00808(M0). All six phases16..21 are L1/R1 with bound value5031365393A96F6F and CRCsAA287B4B,E666CD05,FD46BF3F,D6F1DD05,CB065B4B,48BB45ED. TerminalA684F65B records actual CNTKCTL_EL1=0x4f after M1 guard. All nine CRCs reconstruct exactly; growth512 matches ENTRY+six slots+terminal. New reader ID3/query0/armed0, no MAP_FAILURE/RESET_RETURN. Original WFE configuration/init and misc returns completed and records retained over one observed reboot. PMIC02000001->02000041. Timer IRQs, real WFE wait/wakeup, sustained initialization, ArmCpu/GIC, reset source and userspace remain unverified.

Next: Preserve this read-only result; next instrument original /chosen SecureDTLookupEntry, xnu_platform_stall property lookup and following control-flow/initialization boundary. Each new attempt requires fresh version-specific Query/ARM; do not directly relaunch consumed P16 ARM.
