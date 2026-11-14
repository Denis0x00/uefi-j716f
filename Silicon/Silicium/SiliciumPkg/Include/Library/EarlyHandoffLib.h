// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include <Library/MemoryMapLib.h>

// Called before HOB/heap initialization. Must not allocate or use DebugLib.
EFI_STATUS EFIAPI PrepareEarlyHandoff (IN UINT64 EntryArgument);
// Called after console/log initialization; also reports a rejected placement.
VOID EFIAPI ReportEarlyHandoff (VOID);
// Applied before MemoryPeim emits resource/allocation HOBs or allocates tables.
EFI_STATUS EFIAPI ApplyEarlyHandoffMemoryMap (
  IN OUT EFI_MEMORY_REGION_DESCRIPTOR **Map, IN OUT UINT8 *Count);
