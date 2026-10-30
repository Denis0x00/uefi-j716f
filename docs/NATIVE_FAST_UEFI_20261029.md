# NATIVE FAST-V1..V4 固件与应用集成（2026-10-29）

## 1. 概述

本仓库在保留全部历史 `V14..V23P22` 诊断模块与配置的基础上，新增面向原生预链接候选 `40458123294`（`ArmDTPlatform` + `ArmCpu` + native timer）的专用固件与快速闭环 UEFI 应用（[`Platforms/Lenovo/j716fPkg/Application/J716fNativeFast/`](../Platforms/Lenovo/j716fPkg/Application/J716fNativeFast)）：

- **`J716fNativeRead.efi`（`Read.inf` / `FastRead.c`）**：
  - 全区物理扫描 `0xb00be000 + 0x40000` 内的 `JL_MAGIC` 会话（无惧 `fastbootd` 覆写外层 12 字节 `DBGC` 头）；
  - 直接在平板屏幕显示每个非空槽位的阶段名称（覆盖 `0..7`、`33..39`、`45..49`、`54..62`）、异常寄存器（`ESR_EL1`、`FAR_EL1`、`ELR_EL1`）或 Panic / `_kernel_debug_string_early` ASCII 摘要；
  - 支持 4 秒 8 帧 QR（`J716FS1/...`，8 $\times$ 1024 字节 = 8,192 字节）导出当前完整会话。
- **`J716fNativeArm.efi`（`Arm.inf` / `FastArm.c`）**：
  - 3 秒完成冷启动/脏 `DBGC` 头清理（`JLF_MODE_CLEAN_INIT`）、线性追加（`JLF_MODE_APPEND`）或导出后显式回滚（`JLF_MODE_ROLLOVER`）并写入 8,192 字节 `JL_ARMED` 会话头；
  - 当全区已存在未导出的 `JL_USED` 会话且线性尾部不足 8 KiB 时，默认拒绝覆盖，防止误删实机证据。
- **`J716fPowerAction.efi`（`Power.inf` / `FastPower.c`）**：
  - 主菜单内双次 Power 确认执行 `PSCI_SYSTEM_RESET (0x84000009)` 或 `PSCI_SYSTEM_OFF (0x84000008)`。
- **平台固件配置**：
  - `Platforms/Lenovo/j716fPkg/j716f-native.dsc` 与 `j716f-native.fdf`
  - `Platforms/Lenovo/j716fPkg/Drivers/J716fNativeFatDxe/`（GUID `90f40472-1975-40e2-8028-c76d530d15a7`）
  - `Platforms/Lenovo/j716fPkg/Include/NATIVE_APRIORI.inc` 与 `NATIVE_DXE.inc`

---

## 2. 实机验证结果（2026-10-29）

配套设备仓库：[`UniXNU-j716f`](https://github.com/Denis0x00/UniXNU-j716f)（详见 `docs/NATIVE_FAST_VM_BRINGUP_20261029.md` 与 `research/native-fast-vm-done-20261029.json`）。

| 版本 | `boot.img` SHA256 | 内核 SHA256 | `boot_b` 实机进度 | 结论 |
| :--- | :--- | :--- | :--- | :--- |
| `FAST-V1` | `eca22352...fae09b73` | `150c5c5f...97132d074` | `0:start_begin -> 43:BOOT_MAP_ERR` | 定位 `TCR_EL1.T1SZ=26`（256 项 L1 根表）索引掩码缺陷 |
| `FAST-V2` | `28463fde...06680084f` | `1d686761...aa252c16` | `0..3 -> 44:PERM_MAP_ERR -> 41:EL1_SYNC_FLT` | 定位 `TTBR1_EL1` 切换前 `_phystokv` 返回永久表 KVA 缺陷 |
| `FAST-V3` | `d367d4ee...c3cf55e8` | `a0b548d0...24bd7242` | **`0:start_begin -> 1:boot_pt_map -> 2:arm_init -> 3:vm_entry -> 4:vm_perm_map -> 5:vm_ttbr_sw (TTBR1_EL1=0xd4229000) -> 6:pmap_boot -> 7:vm_done`（8/8 槽位 CRC 全过，零异常）** | **实机 100% 完成 XNU ARM64 VM 初始化、永久页表切换与 `pmap_bootstrap`** |
| `FAST-V4` | `8ac33597...409e428` | `2a45d7d0...c3ffa978` | 主机反解审计通过（待 `boot_b` 实机测试） | 新增 21 个槽位覆盖 `_arm_init` 尾声、`_kernel_debug_string_early`、`_kernel_bootstrap`、`_PE_init_iokit` 与 `_panic` |
