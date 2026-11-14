// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Library/EarlyHandoffLib.h>
EFI_STATUS EFIAPI PrepareEarlyHandoff (IN UINT64 EntryArgument) { return EFI_SUCCESS; }
VOID EFIAPI ReportEarlyHandoff (VOID) { }
EFI_STATUS EFIAPI ApplyEarlyHandoffMemoryMap (
  IN OUT EFI_MEMORY_REGION_DESCRIPTOR **Map, IN OUT UINT8 *Count)
{ return EFI_SUCCESS; }
