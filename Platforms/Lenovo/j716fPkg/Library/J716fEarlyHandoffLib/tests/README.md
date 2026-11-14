# Early handoff host tests

Run on the Linux build host, with the pinned Mu_Basecore libfdt sources and the
stock normalized board DTB:

```sh
python3 Platforms/Lenovo/j716fPkg/Library/J716fEarlyHandoffLib/tests/test_early_handoff.py \
  --libfdt /home/test/unixnu-work/UniXNU/out/feeds/mu_basecore-8f10084c/MdePkg/Library/BaseFdtLib/libfdt/libfdt \
  --dtb /home/test/unixnu-work/work/UniXNU-j716f-menu/firmware/j716f-stock-base00-overlay01-normalized.dtb
```

The command assumes the working directory is the board firmware checkout. Adjust
only the DTB/checkout paths to match the existing workspace. No network, device,
firmware installation or QEMU is used. Input DTB and board descriptors are never
modified. Fixture DTB mutations occur in private mapped process memory.

The runner compiles the actual `J716fEarlyHandoff.c`, actual board
`MemoryMapLib.c`, actual `MemoryMapLib.h`, actual `AddHob` from `MemoryInitPei.c`,
and real pinned libfdt C files. It replaces only UEFI host ABI/library plumbing.
The library algorithms are not reimplemented in Python or copied into tests.
Source hooks are checked for ordering, early allocation/logging prohibition and
rejection before HOB creation. Those static checks are not an emulation of SEC.

Synthetic valid trees and malformed trees exercise validation, both big-endian
cell widths, partial properties, absence/empty initrd, arithmetic, sector padding,
physical banks and holes, every static non-input/live region, page conflicts,
byte overlap versus page merging, descriptor capacity, exact split ranges and
actual HOB emitter inputs. A copy of the normalized stock DTB is also exercised.
Malformed aligned-layout cases include non-word structure offsets/sizes,
non-eight-byte reservation offsets, block overlap and truncation, missing reserve
terminators, and unsupported versions. These call the actual production layout
guard before native `fdt_check_full`, matching production ordering. A header
mutation regression originally exposed a misaligned load in the pinned libfdt;
the production guard prevents it rather than suppressing the sanitizer finding.
Successful preparations and rejection paths assert the original map is intact;
failed preparations cannot expose a previously successful map.

Two explicit GCC builds run with halt-on-error:

1. **UBSan full production path:** `mmap(MAP_FIXED_NOREPLACE)` at `0xa1000000`
   and `0xd0000000` lets production code dereference actual board-like addresses.
   It covers successful and failing `PrepareEarlyHandoff` calls and all scenarios
   above. An unavailable mapping is a failure, not a skipped test.
2. **ASan + UBSan range/map/rejection tests:** actual range checks, descriptor
   splitting, capacity boundaries, HOB emission and entry-pointer rejection run
   without fixed mappings. The pinned full libfdt validator is separately tested
   on exact-sized heap copies of the stock DTB and malformed variants.

The successful fixed-address `PrepareEarlyHandoff` path is **not ASan-tested**.
On x86-64 its addresses conflict with ASan shadow mappings, including shadow for
the gap when `protect_shadow_gap=0` is used. The test does not overwrite sanitizer
mappings, relocate production pointers, or disable ASan gap protection. These
tests do not establish ABL placement, hardware RAM ownership or runtime boot success.
