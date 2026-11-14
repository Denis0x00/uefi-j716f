// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_EARLY_LOG_HOB_H
#define J716F_EARLY_LOG_HOB_H
#include <Uefi.h>
#include <J716fEarlyLog.h>
extern EFI_GUID gJ716fEarlyLogSnapshotGuid;
#define J716F_EL_HOB_MAGIC SIGNATURE_32('J','X','S','1')
typedef struct {
  UINT32 Magic;
  UINT32 Version;
  UINT32 SnapshotBytes;
  UINT32 SnapshotCrc;
  UINT8 Snapshot[J716F_EL_TOTAL_BYTES];
} J716F_EARLY_LOG_HOB;
STATIC_ASSERT (sizeof (J716F_EARLY_LOG_HOB) == 30864, "Snapshot HOB layout changed");
STATIC_ASSERT (sizeof (J716F_EARLY_LOG_HOB) + 24 < 0xFFF8, "Snapshot exceeds HOB UINT16 size");
#endif
