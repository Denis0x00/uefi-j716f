// SPDX-License-Identifier: BSD-2-Clause-Patent
// Explicitly selected application, never a DXE/FDF automatic component.
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <J716fEarlyLogRegion.h>

#define MAP_CAPACITY (1024 * 1024)
#define MAX_EXIT_TRIES 4
#define PROBE_BUILD "j716f-ram-retention-20261114"

STATIC VOID StopAfterExitAttempt (VOID)
{
  // Never return through a potentially torn-down boot-services environment.
  __asm__ volatile ("msr daifset, #0xf\n dsb sy\n isb" ::: "memory");
  for (;;) __asm__ volatile ("wfe" ::: "memory");
}

STATIC BOOLEAN IdentityNonCacheMapping (VOID)
{
  UINT64 CurrentEl;
  UINT64 Control;
  UINT64 Address;
  UINT64 Par;
  __asm__ volatile ("mrs %0, CurrentEL" : "=r" (CurrentEl));
  if (CurrentEl != 4) return FALSE;
  __asm__ volatile ("mrs %0, sctlr_el1" : "=r" (Control));
  if ((Control & 1) == 0) return FALSE;
  for (Address = J716F_EL_BASE; Address < J716F_EL_BASE + J716F_EL_REGION_BYTES; Address += 4096) {
    // AT checks write accessibility; ISB is required before consuming PAR_EL1.
    __asm__ volatile ("at s1e1w, %1\n isb\n mrs %0, par_el1" : "=r" (Par) : "r" (Address) : "memory");
    if ((Par & 1) != 0 || (Par & 0x0000FFFFFFFFF000ULL) != Address || (Par >> 56) != 0x44) return FALSE;
  }
  return TRUE;
}

STATIC BOOLEAN BootLogMapValid (CONST UINT8 *Map, UINTN Bytes, UINTN Stride)
{
  EFI_MEMORY_DESCRIPTOR D;
  UINTN Offset;
  UINT64 End;
  UINT32 Matches = 0;
  if (Map == NULL || Stride < sizeof (D) || Stride > MAP_CAPACITY || Bytes == 0 ||
      Bytes > MAP_CAPACITY || Bytes % Stride != 0) return FALSE;
  for (Offset = 0; Offset < Bytes; Offset += Stride) {
    CopyMem (&D, Map + Offset, sizeof (D));
    if (D.NumberOfPages == 0 || D.NumberOfPages > (MAX_UINT64 - D.PhysicalStart) / 4096) return FALSE;
    End = D.PhysicalStart + D.NumberOfPages * 4096;
    if (D.PhysicalStart < J716F_EL_BASE + J716F_EL_REGION_BYTES && J716F_EL_BASE < End) {
      if (D.PhysicalStart != J716F_EL_BASE || End != J716F_EL_BASE + J716F_EL_REGION_BYTES ||
          D.Type != EfiReservedMemoryType ||
          (D.Attribute & (EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB |
                          EFI_MEMORY_RP | EFI_MEMORY_RO | EFI_MEMORY_WP)) != EFI_MEMORY_UC) return FALSE;
      ++Matches;
    }
  }
  return Matches == 1;
}

STATIC EFI_STATUS ParseMode (EFI_HANDLE ImageHandle, BOOLEAN *Hold)
{
  EFI_LOADED_IMAGE_PROTOCOL *Loaded;
  EFI_STATUS Status;
  CONST CHAR16 *Options;
  *Hold = FALSE;
  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded);
  if (EFI_ERROR (Status)) return Status;
  if (Loaded->LoadOptionsSize == 0) return EFI_SUCCESS;
  if (Loaded->LoadOptions == NULL || Loaded->LoadOptionsSize % sizeof (CHAR16) != 0) return EFI_INVALID_PARAMETER;
  Options = Loaded->LoadOptions;
  if (Loaded->LoadOptionsSize == sizeof (CHAR16) && Options[0] == 0) return EFI_SUCCESS;
  if (Loaded->LoadOptionsSize != 5 * sizeof (CHAR16) || CompareMem (Options, L"hold", 5 * sizeof (CHAR16)) != 0)
    return EFI_INVALID_PARAMETER;
  *Hold = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI J716fEarlyLogProbeEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  UINT8 *Map;
  UINTN MapBytes;
  UINTN MapKey;
  UINTN Stride;
  UINTN Attempt;
  UINT32 Version;
  UINT64 BootId;
  BOOLEAN Hold;
  BOOLEAN ExitAttempted = FALSE;
  J716F_EL_STATE State;
  STATIC CONST CHAR8 Sentinel[] = "J716F EARLY LOG RETENTION SENTINEL v1\nsource=firmware-after-EBS; no XNU execution\n";
  STATIC CONST CHAR8 ResetMarker[] = "mode=PSCI_SYSTEM_RESET_SMC\n";
  STATIC CONST CHAR8 HoldMarker[] = "mode=MANUAL_FORCED_RESET_HOLD\n";
  (VOID)SystemTable;
  Status = ParseMode (ImageHandle, &Hold);
  if (EFI_ERROR (Status)) return Status;
  if (!J716fEarlyLogRegionValid () || !IdentityNonCacheMapping ()) return EFI_UNSUPPORTED;
  Status = gBS->SetWatchdogTimer (0, 0, 0, NULL);
  if (EFI_ERROR (Status)) return Status;
  Map = AllocatePool (MAP_CAPACITY);
  if (Map == NULL) return EFI_OUT_OF_RESOURCES;
  BootId = GetPerformanceCounter ();
  if (BootId == 0) BootId = 1;
  Print (L"J716F RAM retention self-test: %s. Read Advanced Logger after the next boot.\n",
         Hold ? L"hold after capture; manually force a restart" : L"one PSCI reset after capture");
  for (Attempt = 0; Attempt < MAX_EXIT_TRIES; ++Attempt) {
    MapBytes = MAP_CAPACITY;
    Status = gBS->GetMemoryMap (&MapBytes, (EFI_MEMORY_DESCRIPTOR *)Map, &MapKey, &Stride, &Version);
    if (EFI_ERROR (Status) || Version != EFI_MEMORY_DESCRIPTOR_VERSION || !BootLogMapValid (Map, MapBytes, Stride) ||
        !IdentityNonCacheMapping ()) {
      if (ExitAttempted) StopAfterExitAttempt ();
      FreePool (Map);
      return EFI_ERROR (Status) ? Status : EFI_COMPROMISED_DATA;
    }
    ExitAttempted = TRUE;
    Status = gBS->ExitBootServices (ImageHandle, MapKey);
    if (!EFI_ERROR (Status)) break;
    if (Status != EFI_INVALID_PARAMETER) StopAfterExitAttempt ();
    // Only GetMemoryMap and ExitBootServices are called after a failed attempt.
  }
  if (EFI_ERROR (Status)) StopAfterExitAttempt ();
  // No allocations, logging protocol calls, storage access or gBS/gRT below.
  __asm__ volatile ("msr daifset, #0xf\n isb" ::: "memory");
  if (J716fEarlyLogBegin (&State, (volatile J716F_EL_U8 *)(UINTN)J716F_EL_BASE,
        J716F_EL_REGION_BYTES, J716F_EL_SOURCE_PROBE, BootId, PROBE_BUILD) != J716F_EL_OK ||
      J716fEarlyLogAppend (&State, Sentinel, sizeof (Sentinel) - 1) != J716F_EL_OK ||
      J716fEarlyLogAppend (&State, Hold ? HoldMarker : ResetMarker,
        Hold ? sizeof (HoldMarker) - 1 : sizeof (ResetMarker) - 1) != J716F_EL_OK) StopAfterExitAttempt ();
  if (!Hold) {
    register UINT64 Arg0 __asm__ ("x0") = 0x84000009;
    __asm__ volatile ("dsb sy\n smc #0" : "+r" (Arg0) : : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "memory");
  }
  StopAfterExitAttempt ();
  return EFI_ABORTED; // Unreachable; keeps the EFI entry-point type explicit.
}
