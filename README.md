# uefi-j716f

Mu-Silicium UEFI port for Lenovo Xiaoxin Pad Pro 2021 (TB-J716F, SM8250/Kona).
Companion device repository: [UniXNU-j716f](https://github.com/Denis0x00/UniXNU-j716f).
This is an early kernel bring-up port; a complete iPadOS boot is not established.

## Current status — 2026-10-27

- Device-tested UEFI menu, hardware keys, RAM FAT and DarwinBoot handoff.
- One 256 MiB kernel allocation window passed two full read/write rounds.
- Version-bound CRC trace, independent witnesses and post-reboot read-only reader.
- P13 verified bounded MMU-on execution; P14 reached original cpu_data_init return.
- P15 reached first original PE_init_platform(0, bootArgs) entry and return.
- P16 reached original WFE configure/init and misc returns, recording actual
  CNTKCTL_EL1=0x4f; all witnesses survived one video-observed reboot.
- P18 reached original boot CPU publication/validation, current-thread setter and
  rtclock_early_init return; all nine CRCs matched and survived one video-observed reboot.
- P19 reached original debug/startup, timer_call_init, cpu_init and
  processor_bootstrap returns; nine CRCs matched and survived one video-observed reboot.

P18 deliberately terminates after early-clock return; cpu_bootstrap is a RET stub.
Native timer IRQ,
GIC/ArmCpu driver operation, sustained initialization, scheduling, physical storage
and userspace remain unverified. These are frozen-kernel diagnostics, not an
integrated native timer platform. Reset source is independently unproven.

## P19M 最新实机结果

[startup/timer/CPU/processor分组诊断](docs/V23P19_STARTUP_INIT.md)已仅在boot_b完成测试：
原始debug/startup、timer_call_init、cpu_init和processor_bootstrap返回，六槽及终点均到达。
9条记录CRC独立匹配，终点SCTLR=0x3454593d（M1），日志经一次连续录像观察到的重启后保留。
终点预设请求重启，复位来源未独立证明。下一步审查arm_init剩余流程及arm_vm_init的映射/栈生命周期；
timer IRQ、GIC、调度及用户态尚未验证。

## P20M 待实机测试

[arm_vm_init早期VM诊断](docs/V23P20_VM_INIT.md)已构建，通过27项内核、5项依赖及31项链接reader主机检查；已仅刷入boot_b，重启命令成功，初始reader待确认。
继续原始VM入口、初始表分配、内核段处理和arm_vm_prot_init返回；在TCR/TTBR切换前主动终止。
下一次切换需要完成日志、探针及栈在新表中的映射。P19实机结果保留。

## Sources and results

- [Current progress and upstream/sheng comparison](docs/PROGRESS_AFTER_V23P16_20261026.md)
- [P16 procedure and hardware observations](docs/V23P16_TIMER_WFE.md)
- [P16 build validation](docs/v23p16-validation.json)
- [P18 historical hardware result](docs/V23P18_BOOTSTRAP_CLOCK.md)
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

## P17M independent historical result

[Grouped chosen/stall/CPU-topology diagnostic](docs/V23P17_CHOSEN_TOPOLOGY.md)
reached all six witnesses and terminal SCTLR0x3454593d/M1 on boot_b. Nine CRCs
match fresh ID1/ARM6303C7DA; full logical header0x380 and records survived one
video-observed reboot. The terminal intentionally requests reset before boot CPU
publication/max-CPU checks. Its evidence remains separate from P18;
native provider/timer IRQ/scheduler qualification is still pending.

## P18M historical hardware result

[Boot CPU/current-thread/early-clock grouped diagnostic](docs/V23P18_BOOTSTRAP_CLOCK.md)
built and host audited: 50 actual-kernel and 31 linked-reader mocked cases.
Tested only on boot_b: all six witnesses and the terminal reached, actual SCTLR
0x3454593d/M1. Nine CRCs bind fresh ID1/ARM209C5114 and match independent
reconstruction; header0x380 and 14 records survived one continuous-video-observed
reboot. P18 deliberately requests reset after rtclock_early_init return; reset
source remains independently unproven. No native timer IRQ or full cpu_init claim.
P19 subsequently reached these original function returns; see the current result above.
