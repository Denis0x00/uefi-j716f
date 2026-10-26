# V23P5 matched STR/NOP diagnostics

See the device repository's `docs/V23P5_MATCHED_MMIO_CONTROL.md` for the upstream
alignment, experiment and host checks. Upstream main was reviewed at
`16e592abee8a550dec8154ed1f38b5ebc4f16213`; the diagnostic kernel/firmware baseline
remains frozen. This tests the early part of bring-up stage 5; it does not
establish platform-expert or MMU-on continuation.

Both variants use menu scale 3 rather than 4, as requested after P4 r1 video.
The two firmware builds have identical non-payload modules. Versus P4 r1,
UniXNU.efi is explicitly allowed to change for this menu adjustment and the
embedded FAT changes for the diagnostic identity/kernel. Other modules match.

P5A kernel is exactly P4 r1. P5B changes only the relocated legacy STR to NOP
and its crypto HMAC. Both keep the initialized 96x96 before/after markers,
clear filter, mode 5, checkpoints and title DSB. Host audits and two Unicorn
cases passed; the actual MMIO page is unmapped for the B emulator case.
P5A was flashed only to boot_b in fastbootd; B activation and reboot command succeeded. A video confirms the compact A menu, both early markers and prefix 1/2, without prefix 3 or EL/SCTLR. Screen loss occurs about 17.47s after entry, followed by Lenovo/Android splash and A menu return. PMIC changes 0x02000001 to 0x02000041; reset cause is unknown. P5B was subsequently flashed and reviewed; see the current video checkpoint below.

Original r1 and next-build source/menu evidence are preserved separately.
Advanced Logger/QR export, QEMU firmware/entry testing and standard source
recipe integration remain separate work items; the emulator is not a substitute
for those gates or for hardware execution.

V23P5B was flashed only to boot_b in fastbootd; B activation, current-slot b and slot-unbootable:b=no were verified; reboot command succeeded. Its menu identity and diagnostic progress await video. Reviewed P5A remains the visible control; no P5B functional, PSCI or MMU-on result is claimed.

## Current P5B video checkpoint

V23P5B video confirms the compact B menu, both lower envelope markers and partial title in frame 56; the diagnostic is captured in only one camera frame. Top prefix 1 completion, prefix 2/3, EL/SCTLR and A0/pre-SMC are not established. Entry band to display loss is about 0.067s (camera observation), followed by Lenovo/Android splash and return to B menu; PMIC 0x02000001 -> 0x02000041. Rapid intentional PSCI reset is a hypothesis, not a verified result. Next proposed control retains P5B and replaces only its SMC with an in-place branch, preserving the preceding DSB; not yet built. This checkpoint supersedes earlier pending/unflashed notes; no new device operations or push occurred during video review.

Prepare a P5B-derived single-instruction control replacing SMC at 0xfffffff0073fc798 with B . (bytes 00000014), retaining the preceding DSB, markers, menu and all other instructions; reseal HMAC and audit before any boot_b test. Do not branch to the existing halt routine, which adds a DSB. This control is not yet built.
