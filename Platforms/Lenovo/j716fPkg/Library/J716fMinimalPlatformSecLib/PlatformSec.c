// SPDX-License-Identifier: BSD-2-Clause-Patent
// Derived from Kona PlatformSecLib. Preserve the bootloader's display SIDs.
#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PlatformSecLib.h>

VOID
PlatformInitialize (VOID)
{
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  EFI_STATUS Status;

  Status = LocateMemoryRegionByName ("APSS_HM", &Region);
  if (EFI_ERROR (Status)) {
    return;
  }
  // Same Kona APSS watchdog register as the platform library. No SMMU writes.
  if ((Region.Address == 0x17800000) && (Region.Length > 0x41000B)) {
    MmioWrite32 ((UINTN)Region.Address + 0x410008, 0);
  }
}
