// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_R35_EPOCH_H
#define J716F_R35_EPOCH_H
#include <J716fEarlyLogRegion.h>

// Diagnostic epoch cross-check, never an owner or security-world capability.
// This is valid only before XNU replaces JLOG with the early-console header.
STATIC BOOLEAN J716fR35CurrentSequence (OUT UINT32 *Sequence)
{
  CONST volatile J716F_EL_U8 *Current =
    (CONST volatile J716F_EL_U8 *)(UINTN)J716F_EL_BASE;
  UINT32 Value;
  if (Sequence == NULL) return FALSE;
  *Sequence = 0;
  if (!J716fEarlyLogRegionValid ()) return FALSE;
  MemoryFence ();
  Value = J716fEarlyLogGet32 (Current + 12);
  if (Value == 0 ||
      J716fEarlyLogGet32 (Current) != J716F_EL_JLOG_MAGIC ||
      J716fEarlyLogGet32 (Current + 8) != J716F_EL_JLOG_CAPACITY ||
      J716fEarlyLogGet32 (Current + 4) > J716F_EL_JLOG_CAPACITY ||
      J716fEarlyLogGet32 (Current + J716F_EL_OWNER_OFFSET) != J716F_EL_JOWN_MAGIC ||
      J716fEarlyLogGet32 (Current + J716F_EL_OWNER_OFFSET + 4) != Value) return FALSE;
  MemoryFence ();
  if (J716fEarlyLogGet32 (Current + 12) != Value) return FALSE;
  *Sequence = Value;
  return TRUE;
}
#endif
