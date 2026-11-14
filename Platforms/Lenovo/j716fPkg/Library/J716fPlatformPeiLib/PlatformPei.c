/** @file
  Firmware-volume handoff for the J716F menu-stage firmware.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>

#include <Library/HobLib.h>
#include <Library/PcdLib.h>

EFI_STATUS
EFIAPI
PlatformPeim (VOID)
{
  // SEC decompresses this firmware volume before loading DXE Core. The menu
  // uses native drivers and does not need inherited XBL protocols or locks.
  BuildFvHob (PcdGet64 (PcdFvBaseAddress), PcdGet32 (PcdFvSize));

  return EFI_SUCCESS;
}
