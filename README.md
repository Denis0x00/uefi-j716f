# uefi-j716f

联想小新 Pad Pro 2021（TB-J716F，j716f，SM8250/Kona）的 Mu-Silicium UEFI 固件移植仓库。
本仓库维护板级固件、内存映射、屏幕与按键交接、固件卷及版本专属诊断读取器。
配套设备与内核配置：[UniXNU-j716f](https://github.com/Denis0x00/UniXNU-j716f)；
上游启动框架：[UniXNU](https://github.com/UniXNU/UniXNU)。

**截至 2026-10-27：固件已支持多轮早期内核诊断，尚未启动完整 iPadOS。**
P21 为最新实机里程碑；P22 已构建并通过主机校验，按用户要求暂缓刷入。

配套内核仓库正推进[原生 IRQ 观测候选](https://github.com/Denis0x00/UniXNU-j716f/blob/main/docs/NATIVE_IRQ_OBSERVER.md)：
参照同 SM8250 的 pipa/umi，保留原始 timer dispatch 并记录进入、rtclock 返回及 EOI 后状态。
本轮主机校验和[完整内核编译/链接门槛 CI](https://github.com/Denis0x00/UniXNU-j716f/actions/runs/40445461837)
已通过（受测设备源码 `9ae5d75`）；无预链接、持久化采集或实机 IRQ 结果。
当前固件与 P22 读取器没有读取该内核静态观测数组的功能。

## 固件当前支持什么

| 项目 | 已完成或已观察到 | 范围与限制 |
| --- | --- | --- |
| UEFI 与启动菜单 | 实机进入 UniXNU 菜单并使用硬件按键选择 | 不代表 XNU 输入驱动可用 |
| 屏幕交接 | GOP 与诊断页面输出；保留较小菜单字体 | 不代表原生显示、刷新率或背光驱动已完成 |
| 负载与加载器 | 嵌入式 RAM FAT、DarwinBoot、Mach-O/ADT 交接 | 当前诊断 Android ramdisk 为空 |
| 内存窗口 | 一个 256 MiB 窗口完成两轮读写 | 不代表全部 RAM 所有权或 DMA 可用 |
| 诊断读取器 | 版本专用 Query/ARM、CRC、独立槽和重启后只读读取 | 保留结论限定于已记录的测试流程 |
| 重启与关机菜单 | 已加入独立电源应用；22 项链接应用主机检查通过 | 新应用的全部实机行为，尤其 SYSTEM_OFF，仍待验证 |
| 最新固件产物 | P22 两轮 UEFI 构建、读取器及最终镜像审计通过 | P22 未刷入、无本轮设备结论 |

最新诊断菜单无倒计时、默认进入读取器；包含内核诊断、重启、关机和退出项。
菜单电源操作使用经 Query 检查的直接 PSCI 调用，不假定旧 RESET2 路径可用。
历史要求与实现范围见[电源菜单记录](docs/NEXT_MENU_BUILD.md)。

## 内核侧已经推进到哪里

以下是配套项目的实机诊断结果，固件用于交接和读取证据；不把内核结果等同于固件外设支持。

| 版本 | 已确认的实机边界 |
| --- | --- |
| P13 | MMU 开启后直接读回 SCTLR_EL1 M=1 |
| P14/P15 | 原始 cpu_data_init 返回，首次 PE_init_platform(0, bootArgs) 进入并返回 |
| P16 | 原始 WFE configure/init、misc 返回，CNTKCTL_EL1=0x4f |
| P17 | chosen/stall 查询、CPU 拓扑及 boot CPU 编号读取返回 |
| P18 | boot CPU 发布/校验、当前线程设置、rtclock_early_init 返回 |
| P19 | debug/startup、timer_call_init、cpu_init、processor_bootstrap 返回 |
| P20 | arm_vm_init 早期分配、布局与 arm_vm_prot_init 返回 |
| P21 | 原始 TTBR1/TTBR0 设置函数返回，执行到 TLBI/DSB/ISB 之后 |

P21 六个独立槽各命中一次，九条 CRC 独立重算一致，终点 SCTLR_EL1=0x3454593d（M=1）；
日志在一次连续录像观察到的重启后保留。终点按设计请求 SYSTEM_RESET，复位来源尚未独立测定。
完整 arm_vm_init 返回、真实 timer IRQ、GIC/ArmCpu 运行、调度、UFS/根卷及用户态尚未验证。
目前仍是上游阶段 5 的早期内核与平台接入。
详见 [P21 实机结果](docs/V23P21_VM_SWITCH.md)及[完整进度对照](docs/PROGRESS_AFTER_V23P22_20261027.md)。

## P22：剩余 VM 流程与返回诊断

P22 保留原始 pmap_bootstrap、VM 收尾、栈保护检查及 RET，新增三个检查点：
`PMAP_BOOTSTRAP_RETURN`、`VM_FINAL_READY`、`ARM_VM_RETURN`。
主机检查共 26 项内核、8 项依赖、31 项实际链接读取器；系统寄存器、TLB、缓存、SMC 与固件交互仍为模型。

日志子表位于静态 bootstrap 数组中，生命周期在主机模型中审查至立即返回终点；
这不证明后续回收、线程切换或真实硬件缓存一致性。
P22 当前尚未刷入，不能将主机完整返回写成实机成功。

| P22 入口 | 路径 |
| --- | --- |
| 固件配置 | `Platforms/Lenovo/j716fPkg/j716f-vm-return-trace-v23p22.dsc` |
| 固件卷配置 | `Platforms/Lenovo/j716fPkg/j716f-vm-return-trace-v23p22.fdf` |
| 读取器源码 | `Platforms/Lenovo/j716fPkg/Application/J716fVmReturnTrace/` |
| 本地生成 FAT 输入 | `Platforms/Lenovo/j716fPkg/Resources/J716fVmReturnTraceV23P22.fat`，不随 Git 发布 |
| 测试说明 | [P22 VM 返回](docs/V23P22_VM_RETURN.md) |
| 构建审计快照 | [P22 校验摘要](docs/v23p22-validation.json)，不随设备测试改写为实机证据 |

生成启动镜像为 Android header v2、4096 字节页，使用固定 BootShim、压缩 FD 与固件 DTB。
P22 启动镜像大小 8,347,648 字节，SHA-256：

```text
c49fb7542528ca3a8aca005a2f0f4fb304e6a43f379da65555a6525519e3ced2
```

## 与上游及小米平板 6S Pro 的关系

本次核对 UniXNU `f85b5a60` 和 uefi-sheng `20ac23d5`；主仓库最近一次变化刷新设备记录，
未新增当前 VM 路径的代码修复。sheng、SM8550 公共层和 ArmCpu 版本与上次审查一致。

sheng 可参考的固件分工是：公共 UniXNURamDiskDxe 暴露负载块设备，板级模块负责内存策略。
J716F 当前诊断仍使用嵌入式 FAT；历史配置中的标准 initrd 声明不表示 P22 已切换到该路径。
内核侧应借鉴 ArmDTPlatform/ArmCpu 接入、真实 IRQ 计数和存储日志流程；
SM8550 地址、保留区、CPU/SMP、UFS/SMMU 与电源常数不能直接当作 SM8250 的硬件事实。

sheng 已有维护者报告的系统和外设运行结果，也记录 Wi-Fi、音频及低电量关机等限制；
本项目未独立复测。版本、来源和适用范围见[上游对照](docs/PROGRESS_AFTER_V23P22_20261027.md)。

## 仓库结构

| 路径 | 用途 |
| --- | --- |
| `Platforms/Lenovo/j716fPkg/` | J716F 板级 DSC/FDF、内存库、按键与诊断应用 |
| `Resources/Configs/j716f.toml` | 固件和 Android 启动镜像参数 |
| `Resources/MuPatches/` | 固件集成补丁 |
| `BootShim/`、`Silicon/`、`Common/`、`Mu_Basecore/` | 继承的 Mu-Silicium 框架与固定子模块 |
| `docs/` | 各版本测试说明与脱敏构建审计 |
| `.github/workflows/` | J716F 源码检查及独立手动构建流程 |

各诊断版本使用独立配置和读取器，历史镜像与设备结果保留。
设备硬件事实、组件选择和权威移植记录集中维护在
[设备仓库 porting.toml](https://github.com/Denis0x00/UniXNU-j716f/blob/main/porting.toml)。

## 构建与持续集成

### 获取源码

```sh
git clone --recurse-submodules https://github.com/Denis0x00/uefi-j716f.git
cd uefi-j716f
```

建议使用 Linux 构建。通用工具依赖与入口见[继承的构建说明](Building.md)，
但默认 `build_uefi.py -d j716f` 不等于选择最新 P22 诊断配置。
必须按所选版本明确 DSC/FDF、关联内核/ADT、FAT 文件、厂商输入和构建哈希。

完整固件复现还依赖固定子模块、匹配的工具链、`Binaries/j716f` 厂商输入、UniXNU/DarwinBoot 模块、
对应探针/读取器/电源应用以及本地生成资源。公开仓库没有一条下载后即可生成完整 P22 的无输入构建命令。
设备源码配置与原生平台候选见
[配套仓库构建说明](https://github.com/Denis0x00/UniXNU-j716f#构建入口与持续集成)。

### Actions 验证范围

[J716F 源码检查](.github/workflows/j716f-source-checks.yml)核对 Git 子模块和可执行位、
脚本/配置语法、内存窗口策略及 Linux Python 入口；**不编译完整固件卷，也不验证实机启动**。
P22 源码提交已有[通过记录](https://github.com/Denis0x00/uefi-j716f/actions/runs/40442169963)。
继承的全设备构建是单独的[手动工作流](.github/workflows/active_devices.yml)，其状态不能替代 J716F 检查。
历史失败与修复见 [CI 记录](docs/CI_STATUS_20261026.md)；
最新状态查看 [Actions](https://github.com/Denis0x00/uefi-j716f/actions)。

## 测试顺序与后续工作

1. 用户恢复 P22 测试后，仅刷 boot_b、核对 B 槽并进入无倒计时菜单；先拍 P22 reader 初始页。
2. 同一版本重新 Query→ARM，核对 ID、CRC 与模板后只启动一次；连续录像。
3. 重启后先进入 reader 保存完整页，先不 Query/ARM；若停住，先保存当前画面。
4. P22 结果核对后继续原生平台预链接、单核驱动匹配、timebase/真实 IRQ 和调度验证。
5. 再推进受控存储读取、根卷及用户态；持久化日志和返回 bootloader 手动闭环完成后，才引入自动循环。

只读 RAM 日志与上游 Advanced Logger 固件二维码、存储持久化日志是不同机制。
Advanced Logger 依赖平台协议及固件集成，无法直接替代 ExitBootServices 后的 XNU 日志；
当前不声称已接入该二维码导出器或 sheng 的无人值守流程。

测试范围保持 boot_b；boot_a、vbmeta、userdata 不在诊断写入范围内，TzDxe 暂不处理。
公开内容限源码、配置、哈希及脱敏摘要；厂商/Apple 二进制、完整 DTB/ADT、FAT/FD/boot 镜像、
照片视频、原始日志、设备标识、随机种子和凭据保留在本地。

## 文档入口与项目来源

- [当前进度与上游对照](docs/PROGRESS_AFTER_V23P22_20261027.md)
- [P21 实机结果](docs/V23P21_VM_SWITCH.md) · [P22 待测方案](docs/V23P22_VM_RETURN.md)
- [历史进度归档](docs/HISTORY_THROUGH_V23P16.md) · [平台包说明](Platforms/Lenovo/j716fPkg/README.md)
- [Project Silicium / Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) · [Project Mu](https://github.com/microsoft/mu)
- [上游固件集成指南](https://unixnu.com/zh/porting/uefi-firmware) · [uefi-sheng](https://github.com/UniXNU/uefi-sheng)

许可证沿用仓库及各组件声明，详见 [LICENSE](LICENSE)。
