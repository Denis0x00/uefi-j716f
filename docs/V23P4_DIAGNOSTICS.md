# V23P4 r1 early MMIO envelope

V23P4 keeps the V23P2 firmware profile and replaces only the FAT payload.
The payload puts a before/after framebuffer envelope around the legacy BCM
store at `0xfffffff0073fe32c`. The original `str w1, [x0]` and the following
kernel instructions are preserved; the title-row DSB is unchanged.

The before marker is `(1280,192)` and the after marker is `(1384,192)`, both
96×96 white squares in the validated framebuffer reservation. The host build
passed source-crypto HMAC, FD/FAT/PE/Android-v2 checks and the exact P2 rollback
guard. A Unicorn run executed the real stub, observed the original store once,
observed the actual return target and checked 18432 marker pixels. r1 first
clears both marker regions to reject stale pixels, and filters the later P2
clear to preserve only those two rectangles. The emulator executes pre/setup_fb
and the full clear/filter, checking all 16384000 bytes. The intervening
privileged kernel path is excluded; MMIO is RAM in this test.

The original P4 was never flashed: preflight found its markers would be erased
by the later clear. Its image is privately archived and superseded by r1.
Restoring both the legacy store and clear STP, removing the extra stub and
resealing reconstructs the exact P2 kernel. Non-payload firmware modules match P2.

V23P4 r1 flash to `boot_b` in fastbootd, B activation and reboot command succeeded.
Video confirms the r1 menu, both early MMIO markers and prefix squares 1/2.
Square 3, EL/SCTLR and PSCI progress are not observed. The diagnostic screen
disappears about 15.9 seconds after the entry band, then Lenovo/Android splash
and the r1 menu return. PMIC changes 0x02000001 to 0x02000041; reset cause is
unknown. The splash does not establish an Android OS boot. Only `boot_b` was written;
boot_a, vbmeta, userdata, GPT and TzDxe remain untouched.

The after marker establishes execution past STR, not external MMIO transaction
completion. Retain the envelope and clear filter for a matched STR/NOP control;
the absent square 3 does not by itself prove DSB is stalled.

For the next build, the menu source now selects scale 3 instead of 4 (25% smaller
linear glyph dimensions), as requested after this video. This has not been
compiled or flashed. Kernel diagnostic glyphs are unchanged. The original r1
library source is privately snapshotted with matching remote-build hashes.
Audit the changed menu module explicitly on the next build.
