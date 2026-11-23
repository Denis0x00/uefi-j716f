// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_R35_RAM_SLOT_H
#define J716F_R35_RAM_SLOT_H

#include <J716fEarlyLog.h>

// Fixed 4 KiB non-overlapping handoff slot inside the existing 512 KiB Boot Log
// reservation [0xB0000000, 0xB0080000) (EfiReservedMemoryType, UC, DT no-map).
// JXLG occupies [0x00000, 0x07880); RamLogWrite is soft-clamped below 0x7DF00;
// R35Record slot occupies [0x7E000, 0x7F000); JOWN occupies [0x7FFF4, 0x80000).
#define J716F_RAMLOG_SOFT_LIMIT 0x7DF00U
#define J716F_R35_RAM_OFFSET    0x7E000U
#define J716F_R35_RAM_BYTES     0x1000U
#define J716F_R35_RAM_BASE      (J716F_EL_BASE + J716F_R35_RAM_OFFSET)

STATIC_ASSERT (J716F_EL_TOTAL_BYTES < J716F_RAMLOG_SOFT_LIMIT,
               "JXLG overlaps RamLog soft limit");
STATIC_ASSERT (16U + J716F_RAMLOG_SOFT_LIMIT + 1U <= J716F_R35_RAM_OFFSET,
               "RamLog soft limit overlaps R35 handoff slot");
STATIC_ASSERT (J716F_R35_RAM_OFFSET + J716F_R35_RAM_BYTES <= J716F_EL_OWNER_OFFSET,
               "R35 handoff slot overlaps JOWN tail");
STATIC_ASSERT ((J716F_R35_RAM_BASE & 0xFFFULL) == 0,
               "R35 handoff slot must be 4 KiB aligned");

#endif
