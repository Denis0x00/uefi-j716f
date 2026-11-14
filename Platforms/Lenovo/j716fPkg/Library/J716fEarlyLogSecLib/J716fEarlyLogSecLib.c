// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <PiPei.h>
#include <Library/EarlyLogSecLib.h>
#include <Library/BaseLib.h>
#include <Library/HobLib.h>
#include <J716fEarlyLogHob.h>
#include <J716fEarlyLogRegion.h>

// This instance is linked into SEC only, never into serial/logger consumers.
STATIC J716F_EARLY_LOG_HOB mSnapshot;
STATIC BOOLEAN mValid;

VOID EFIAPI EarlyLogSecCapture (VOID)
{
  CONST volatile J716F_EL_U8 *Ram = (CONST volatile J716F_EL_U8 *)(UINTN)J716F_EL_BASE;
  UINTN Index;
  // SEC can run before constructors. Explicit initialization is mandatory.
  mValid = FALSE;
  ZeroMem (&mSnapshot, sizeof (mSnapshot));
  if (!J716fEarlyLogRegionValid () ||
      J716fEarlyLogHeaderValidate (Ram, J716F_EL_TOTAL_BYTES, NULL) != J716F_EL_OK) {
    return;
  }
  for (Index = 0; Index < J716F_EL_TOTAL_BYTES; Index++) {
    mSnapshot.Snapshot[Index] = Ram[Index];
  }
  MemoryFence ();
  if (J716fEarlyLogHeaderValidate (mSnapshot.Snapshot, J716F_EL_TOTAL_BYTES, NULL) != J716F_EL_OK) {
    ZeroMem (&mSnapshot, sizeof (mSnapshot));
    return;
  }
  mSnapshot.Version = 1;
  mSnapshot.SnapshotBytes = J716F_EL_TOTAL_BYTES;
  mSnapshot.SnapshotCrc = J716fEarlyLogCrc32 (mSnapshot.Snapshot, J716F_EL_TOTAL_BYTES);
  mSnapshot.Magic = J716F_EL_HOB_MAGIC;
  mValid = TRUE;
}

VOID EFIAPI EarlyLogSecPublish (VOID)
{
  if (!mValid) {
    return;
  }
  // Do not retry or publish duplicate HOBs, including on allocation failure.
  mValid = FALSE;
  BuildGuidDataHob (&gJ716fEarlyLogSnapshotGuid, &mSnapshot, sizeof (mSnapshot));
  ZeroMem (&mSnapshot, sizeof (mSnapshot));
}
