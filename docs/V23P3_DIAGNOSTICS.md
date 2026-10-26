# V23P3 diagnostic payload controls

V23P3 uses the same V23 firmware profile and library sources as V23P2.
Only the embedded FAT's kernel and menu identity change; the packaging audit
requires every other extracted firmware module to match the actual P2 FD.

- P3A replaces one BCM legacy MMIO store with NOP and retains all barriers.
- P3B retains that store and routes the shared renderer barrier through a
  register/flag-preserving dispatcher: title row DMB SY, every other row DSB SY.
- Both retain P2's on-screen mode 5 and twelve checkpoint squares. The menu
  identifies P3A/B. B's square 3 confirms the DMB path, not DSB completion.
- Both remain terminal pre-MMU PSCI diagnostics, without counter dependencies.
  A later restart alone cannot qualify PSCI or identify the reset source.

Build the separate device repository's `scripts/build-v23p3-payload.py` for
each variant, supply its local FAT to the existing `j716f-entry-v23.dsc/.fdf`,
and inspect the actual embedded output after the build. Private binary inputs
are required and must not be committed. No firmware source changes are needed
for the P3 controls; standard initrd and QR logging migration remain separate.

See `docs/v23p3-validation.json` for host results and sanitized device commands.
P3A flash to boot_b, B activation and reboot command succeeded via fastbootd.
Video and kernel progress are pending; P3B remains unflashed. No 9008 operation is part of P3.

V23P3B was flashed only to boot_b via fastbootd, B activated and reboot command succeeded. User report for P3A was brief screen then reboot; no checkpoint or PSCI result established. P3B video is pending.

V23P3A 视频已确认菜单身份；约1.7秒后画面变黑，未见诊断方块或EL/SCTLR，PSCI与复位源仍未知。

V23P3B 视频确认标题 mode 5 和方块 1/2；方块 3、EL/SCTLR 未见。约20.9秒后画面消失，随后 Lenovo/Android 启动并回到 UniXNU 菜单。标题 DMB 对照未显示出越过第2格的硬件效果，PSCI/复位源仍未知。
