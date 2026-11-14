# J716F firmware handoff diagnostics

This DEBUG-only driver adds a read-only boot-services diagnostic snapshot to
the existing Advanced Logger. It does not reserve memory for XNU, edit an FDT,
change any memory type, write storage or variables, or launch a kernel.

The platform's APRIORI list dispatches it before `J716fIoConsoleBdsDxe`.
It registers one ReadyToBoot callback. The existing board BDS connects firmware
handles and consoles, loads the UniXNU application, signals ReadyToBoot, then
calls StartImage. Thus this snapshot is available in Tools → Advanced Logger
before a user launches the payload. No BDS change is necessary.

`J716FDIAG BEGIN` / `END` delimit these records:

- The actual `gBS->GetMemoryMap` key, descriptor size/version, and every EFI
  descriptor's type, physical range, pages, attributes and virtual address.
- GOP geometry, pixel format/masks and framebuffer coverage in the snapshot.
- EFI system table, HOB list and diagnostic map-buffer addresses/coverage.
- The Advanced Logger allocation and version, using the pinned Mu producer's
  internal layout, also used by AdvancedLoggerAccessLib.
- Original SEC `x0` from the entry-argument HOB; a bounded, guarded FDT read;
  `/chosen` initrd addresses and map overlaps. Initrd contents are not read.
- Static platform RAM and reserved-region descriptions, separately labeled
  `PLATFORM`, including the FD, heap, stack, page tables and proposed XNU window.

Capture first queries the required size, allocates BootServicesData with eight
descriptor slots of slack, and retries up to eight times if the map grows. It
caps allocation at 1 MiB, honors returned descriptor stride, rejects invalid
lengths, and retains the buffer while reporting it. It releases only this
buffer and its own event. Firmware build validation is required before use.

The FDT guard accepts only a board-declared, non-device system-RAM range fully
covered by EFI descriptors without read protection, unusable or MMIO types.
It checks the header range before libfdt and caps the full blob at 16 MiB.
An unrecognized placement is reported and skipped rather than probed. This
guard cannot prove that the bootloader or static board memory map is correct.

`Conventional` describes the EFI type at capture time; it never means this
driver has handed the range to XNU. A live FDT/initrd/framebuffer/logger overlap
with Conventional is explicitly reported as a reservation conflict. Other
overlaps are informational and do not establish ownership. A range absent
from the map has unknown reservation status. Full map records remain the
source for detecting partial coverage and other conflicts.

The snapshot includes allocations already made by DXE and LoadImage, plus the
temporary diagnostic buffer. Later logging, application/payload loading and
DarwinBoot allocations can change the map; the buffer is freed after reporting.
This is not the final ExitBootServices map and does not determine
`payload.adt.owned_memory_size` on its own. It must be checked alongside live
handoff objects and the loader's actual allocations before setting that value.

This diagnostic is firmware-only. It cannot capture XNU output after
ExitBootServices. Kernel entry still needs its own UART or framebuffer evidence.

Integration follows the official
[firmware guide](https://unixnu.com/porting/uefi-firmware) and
[Advanced Logger guide](https://unixnu.com/porting/log-capture).
