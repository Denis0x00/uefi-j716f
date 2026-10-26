// SPDX-License-Identifier: BSD-2-Clause-Patent
.text
.balign 4

// UINT64 J716fCurrentEl(void). No counter access.
.globl J716fCurrentEl
J716fCurrentEl:
  mrs x0, CurrentEL
  ret

// void J716fSmc(UINT64 Args[8]); single SMC conduit, no HVC fallback.
// Args is zero-initialized in C. x8..x17 are also explicitly cleared.
// Preserve the platform register x18 and all C ABI callee-saved state used here.
.globl J716fSmc
J716fSmc:
  stp x18, x19, [sp, #-32]!
  stp x29, x30, [sp, #16]
  mov x29, sp
  mov x19, x0
  ldp x0, x1, [x19]
  ldp x2, x3, [x19, #16]
  ldp x4, x5, [x19, #32]
  ldp x6, x7, [x19, #48]
  mov x8, xzr
  mov x9, xzr
  mov x10, xzr
  mov x11, xzr
  mov x12, xzr
  mov x13, xzr
  mov x14, xzr
  mov x15, xzr
  mov x16, xzr
  mov x17, xzr
  dsb sy
  smc #0
  stp x0, x1, [x19]
  stp x2, x3, [x19, #16]
  stp x4, x5, [x19, #32]
  stp x6, x7, [x19, #48]
  ldp x29, x30, [sp, #16]
  ldp x18, x19, [sp], #32
  ret
