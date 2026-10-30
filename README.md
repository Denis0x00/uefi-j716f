# uefi-j716f

联想小新 Pad Pro 2021（TB-J716F / j716f / SM8250）的 Mu-Silicium UEFI 移植。
配套设备/内核仓库：[UniXNU-j716f](https://github.com/Denis0x00/UniXNU-j716f)。

## 当前状态（2026-10-30）

原生候选 `40458123294`（含 `ArmDTPlatform` + `ArmCpu` + native timer）配合本仓库新增的 `J716fNativeFast`（3 秒冷启动/脏头 ARM + 4 秒 8 帧 QR 会话导出 + 物理全区扫描）已完成 11 轮 `boot_b` 实机迭代（`FAST-V1 -> ... -> FAST-V11`）：

- **Run 1（`FAST-V1`）**：捕获 `0:start_begin -> 43:BOOT_MAP_ERR`，定位并修复 `TCR_EL1.T1SZ=26`（38-bit VA，256 项 L1 根表）索引掩码缺陷；
- **Run 2（`FAST-V2`）**：捕获 `0..3 -> 44:PERM_MAP_ERR -> 41:EL1_SYNC_FLT`，定位并修复 `TTBR1_EL1` 切换前 `_phystokv` 返回永久表 KVA 的缺陷；
- **Run 3（`FAST-V3`）**：实机恢复会话 `0x000000004a9650ec`（8/8 槽位 CRC 全过，零异常），100% 跑通 `0:start_begin -> ... -> 7:vm_done`；
- **Run 6（`FAST-V7`）**：解决 `_register_and_init_prng` 双重注册 panic；
- **Run 7（`FAST-V8`）**：解决 `_cckprng_generate` 只读内存写入 Data Abort；
- **Run 8（`FAST-V9`）**：解决 `_idle_thread_create` 寄存器破坏空指针崩溃；
- **Run 9（`FAST-V10`）**：跳过 `AMFI: No PMGR?` 苹果专属电源管理协处理器强依赖；
- **Run 10**：进入 `_bsd_init`，但在 `_proc_ro_alloc` 中因非 PPL 物理页保护遭遇 Level 3 Permission Fault（`_bzero(0xfffffff7ed254000)`）；
- **Run 11（`FAST-V11`，最新重大里程碑）**：
  - **`Slot 18: proc_ro_ok` 与 `Slot 19: vfs_ok` 全部成功通过！**
  - **Mach 微内核、SMP 8核拓扑、GICv3 中断与时钟、IOKit 驱动匹配、BSD 核心系统（proc/proc_ro/credentials/vfs）全部完成初始化！**
  - **实机 58/58 槽位有效，成功推进至 RootFS 挂载点：`panic("Failed to mount root device @%s:%d", "IOKitBSDInit.cpp", 984)`！**

详见 [NATIVE FAST-V1..V11 固件与应用集成](docs/NATIVE_FAST_UEFI_20261029.md)。历史 P22M 过程保留在 [本地历史过程](docs/V23P22_VM_RETURN.md) 与 `docs/v23p22-validation.json`。

## 固件支持与边界

| 项目 | 已有证据 | 未完成 |
| --- | --- | --- |
| UEFI 菜单、GOP、硬件键 | 多轮实机使用（可读 `scale=2` 6 项手动菜单） | 不代表 XNU 原生显示/输入 |
| RAM FAT 与 DarwinBoot | `J716fNativeFatDxe` 嵌入 FAT 实机加载原生候选 `40458123294` | 未验证标准 initrd 链迁移 |
| 保留 RAM / `J716fNativeFast` | **实机验证 3 秒 ARM + 4 秒 8 帧 QR 恢复 `0..7:vm_done`（永久页表 `TTBR1_EL1=0xd4229000` 映射生效且零异常）** | `7:vm_done` 之后的 `_kernel_bootstrap` / `IOKit` 待 `FAST-V4` 实机测试 |
| 重启/关机应用 | `FastPower.c` 双次确认 `PSCI_SYSTEM_RESET` / `PSCI_SYSTEM_OFF` 实机可用 | |
| 固件日志导出 | `FastRead.c` 8 帧 QR（`J716FS1/...`）4 秒导出 8 KiB 会话 | 上游 Advanced Logger 接入 |
| 原生平台/IRQ | 原生候选已链接 `ArmDTPlatform` + `ArmCpu` 并完成实机 `arm_vm_init` | 待 `FAST-V4` 观测 `_PE_init_iokit`、真实 IRQ、调度、根卷与用户态 |

## 新主线

保留全部 P22 及早期源码/镜像/照片/录像，不默认制作新的逐函数复位版。
配套设备默认改为 ArmDTPlatform + ArmCpu + native timer。
选定接收 CI40448429664，先完成来源核验、预链接和专用打包；
该产物只有静态 IRQ observer，仍缺可恢复早期日志，不能直接当上机候选。

固件后续必须提供匹配的原生只读 reader，绑定候选与本次会话，覆盖入口、VM、
平台、异常和真实 IRQ。不能把 P22 的临时 TTBR0 日志桥接当作原生永久映射。
在同一候选验证平台/CPU/GIC/timer/调度后再进入 UFS。

当前未新增 native FD/boot.img，未推送、未刷机、未重启。
仍仅允许另行核验后的 boot_b 测试，保留 boot_a/vbmeta/userdata；TzDxe 不在范围。

## 构建与规范

板级文件位于 `Platforms/Lenovo/j716fPkg/`，历史 P22 的 DSC/FDF 为
`j716f-vm-return-trace-v23p22.dsc/.fdf`，reader 为 `Application/J716fVmReturnTrace/`。
已有版本按其专属文档和源/产物清单复现，不覆盖结果或重跑 build-only recorder。

[上游固件集成](https://unixnu.com/zh/porting/uefi-firmware)、
[日志规范](https://unixnu.com/zh/porting/log-capture)与配套设备 `docs/NATIVE_MAINLINE.md`
共同作为后续验收清单。标准配置声明不等于已构建进当前 FD。

厂商二进制、完整 DTB/ADT、FAT/镜像、照片视频、原始日志、设备标识和凭据不公开。
冻结前 README 保留在 `docs/archive/README-before-native-mainline.md`，仅作为历史记录。
