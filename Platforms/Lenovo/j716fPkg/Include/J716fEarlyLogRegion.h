// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_EARLY_LOG_REGION_H
#define J716F_EARLY_LOG_REGION_H
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <J716fEarlyLog.h>
STATIC BOOLEAN J716fEarlyLogRegionValid (VOID)
{
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  ZeroMem (&Region, sizeof (Region));
  return !EFI_ERROR (LocateMemoryRegionByName ("Boot Log", &Region)) &&
    Region.Address == J716F_EL_BASE && Region.Length == J716F_EL_REGION_BYTES &&
    Region.MemoryType == EfiReservedMemoryType &&
    Region.ResourceType == EFI_RESOURCE_MEMORY_RESERVED &&
    Region.ArmAttributes == ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED;
}
#endif
