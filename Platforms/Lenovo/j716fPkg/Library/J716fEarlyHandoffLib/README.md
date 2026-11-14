# J716F incoming FDT/initrd lifetime

This board-specific SEC library validates the incoming x0 FDT and plans its
reservations before RamLogReset, display initialization, HobConstructor or any
PrePi heap allocation. Preparation uses only static storage and bounded reads;
it neither changes the FDT nor copies/reads the initrd contents. The pinned
BaseFdtLib exports libfdt's `fdt_check_full`, used after the entire declared blob
has passed the board range guard. FDT size is capped at 16 MiB; this port
accepts format v17. Explicit block alignment, bounds, non-overlap and a bounded
reservation-table terminator are checked before libfdt: its header validator
alone does not reject all misaligned structure-block offsets.

The acceptable inputs are the existing `Kernel` SYS_MEM/Reserved staging area
or conventional RAM outside the entire fixed `DXE Heap`. Every byte must also
be inside a recorded physical bank. Other reservations, unmapped gaps, MMIO,
FD, stack, log, display and heap collisions are rejected. The fixed PrePi
allocator cannot avoid holes in its heap, so this implementation rejects heap
collisions instead of pretending a later allocation HOB protects them.

`/chosen` may be absent. Both initrd properties may be absent or specify an
empty range. Otherwise both must be present with a 4- or 8-byte big-endian
address. The range must be forward and safely rounded to the shared RAM disk's
512-byte sector length **relative to its start**, then to 4 KiB pages. Overlap
of FDT bytes with initrd/padding is rejected; page-only overlap is merged.

Only after every check succeeds does MemoryPeim receive the completed copy of
the static map. Conventional entries are split at reservation boundaries and
marked Reserved; the original RAM resource/cache attributes are retained.
Existing Kernel reservations stay reserved. There are no overlapping late
allocation HOBs and no change to the fixed PrePi heap. Static map storage remains
unchanged; a rejected preparation cannot be applied. Other boards use the null
library and retain their existing maps.

After console initialization `J716FHANDOFF ACCEPT/REJECT`, input addresses and
reserved page ranges are logged. A rejected handoff stops SEC before heap/HOB
initialization and cannot reach the raw x0 consumer in UniXNURamDiskDxe. On a
rejection, capture the last screen line; Advanced Logger DXE is not running.
On success, compare these records with the ReadyToBoot `J716FDIAG` EFI map.

## Limits

- BootShim has already copied the FD and assembly has already cleared the
  stack before C validation. Collisions there are unsupported; this cannot
  recover already overwritten input. BootShim's forward-only overlapping-copy
  limitation is unchanged and still needs actual source/destination evidence.
- This protects accepted incoming objects against this firmware's allocators;
  it does not validate ABL's physical RAM claims, repair an incompatible image
  layout, import every FDT reservation or grant XNU ownership of physical RAM.
- DXE's MemoryMapLib remains the board's original static table. The real EFI
  map derives from SEC's protected HOBs; diagnostic static regions are labeled
  separately and are not the allocation map.
- Empty-ramdisk testing does not establish placement of the later 80 MiB initrd.
- Host tests/builds do not prove hardware input, memory mappings or boot success.

中文：本库在早期堆初始化前校验并保留传入的 FDT/initrd，接受范围仅限原
Kernel 保留区及堆外普通 RAM。碰到固件、栈、日志、显示、堆或空洞即停止
SEC；不猜测新的加载地址。成功时输出保护区间，随后由 ReadyToBoot 诊断
输出实际 EFI 内存图。主机检查
不能代替实机布局验证，且不能恢复进入 C 代码前已被覆盖的数据。
