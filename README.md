# uefi-j716f

Mu-Silicium UEFI port for Lenovo Xiaoxin Pad Pro 2021 (TB-J716F, SM8250/Kona).
Companion device repository: [UniXNU-j716f](https://github.com/Denis0x00/UniXNU-j716f).
This is an early kernel bring-up port; a complete iPadOS boot is not established.

## Current status — 2026-10-26

- Device-tested UEFI menu, hardware keys, RAM FAT and DarwinBoot handoff.
- One 256 MiB kernel allocation window passed two full read/write rounds.
- Version-bound CRC trace, independent witnesses and post-reboot read-only reader.
- P13 verified bounded MMU-on execution; P14 reached original cpu_data_init return.
- P15 reached first original PE_init_platform(0, bootArgs) entry and return.
- P16 reached original WFE configure/init and misc returns, recording actual
  CNTKCTL_EL1=0x4f; all witnesses survived one video-observed reboot.

P17 deliberately terminates before boot CPU publication/max-CPU validation. Native timer IRQ,
GIC/ArmCpu driver operation, sustained initialization, scheduling, physical storage
and userspace remain unverified. These are frozen-kernel diagnostics, not an
integrated native timer platform. Reset source is independently unproven.

## Sources and results

- [Current progress and upstream/sheng comparison](docs/PROGRESS_AFTER_V23P16_20261026.md)
- [P16 procedure and hardware observations](docs/V23P16_TIMER_WFE.md)
- [P16 build validation](docs/v23p16-validation.json)
- [P13 MMU](docs/V23P13_MMU_CONTINUATION.md), [P14 early init](docs/V23P14_EARLY_INIT.md),
  [P15 platform init](docs/V23P15_PLATFORM_INIT.md)
- [Power/reboot menu requirement](docs/NEXT_MENU_BUILD.md)
- [Historical progress archive](docs/HISTORY_THROUGH_V23P16.md)

The platform contains separate DSC/FDF profiles and reader applications for each
diagnostic version. The P16 profile is
`Platforms/Lenovo/j716fPkg/j716f-wfe-trace-v23p16.dsc`.
The menu uses the previously qualified smaller scale and includes power actions.
Each kernel attempt requires fresh version-specific Query → ARM; consumed ARM
must not be reused. Hardware diagnostic writes remain boot_b only.

## Build and publication boundaries

See [Actions status and historical failures](docs/CI_STATUS_20261026.md) for the
current source-check scope and the earlier all-device build failure.

Included: platform source, source locks, diagnostic applications, configuration,
and sanitized validation summaries. Full reproduction requires private vendor
inputs and generated resources referenced by the selected profile.

Excluded: Binaries/j716f, vendor PE, generated FAT/FD/boot images, Apple payloads,
raw DTBs, photos, raw device logs, device identifiers, VM details and credentials.
Native display/storage/peripheral support is not implied by the working UEFI menu.
TzDxe remains outside this phase.

## P17M current result

[Grouped chosen/stall/CPU-topology diagnostic](docs/V23P17_CHOSEN_TOPOLOGY.md)
reached all six witnesses and terminal SCTLR0x3454593d/M1 on boot_b. Nine CRCs
match fresh ID1/ARM6303C7DA; full logical header0x380 and records survived one
video-observed reboot. The terminal intentionally requests reset before boot CPU
publication/max-CPU checks. P17 is the latest bounded kernel hardware milestone;
native provider/timer IRQ/scheduler qualification is separate. Next review covers
boot CPU validation, bootstrap/current-thread and rtclock_early_init boundaries.

## P18M prepared

[Boot CPU/current-thread/early-clock grouped diagnostic](docs/V23P18_BOOTSTRAP_CLOCK.md)
built and host audited: 50 actual-kernel and 31 linked-reader mocked cases.
Unflashed; P17 remains the latest bounded hardware milestone. P18 deliberately
requests reset after rtclock_early_init return. No native timer IRQ claim.
