// SPDX-License-Identifier: BSD-2-Clause-Patent
// Explicit XUI menu actions only; no log, partition or persistent-state writes.
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

VOID J716fSmc (UINT64 Args[8]);
UINT64 J716fCurrentEl (VOID);

// Exported functions are retained in the final map for actual-instruction tests.
UINT64 EFIAPI
PowerCall (UINT64 Fid, UINT64 Arg1)
{
  UINT64 Args[8] = {0};
  Args[0] = Fid;
  Args[1] = Arg1;
  J716fSmc (Args);
  return Args[0];
}

EFI_STATUS EFIAPI
UefiMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_LOADED_IMAGE_PROTOCOL *Loaded;
  EFI_STATUS Status;
  CHAR16 *Options;
  UINT32 Fid;
  UINT64 Version;
  UINTN Count;
  (VOID)SystemTable;
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded);
  if (EFI_ERROR (Status) || Loaded == NULL || Loaded->LoadOptions == NULL ||
      Loaded->LoadOptionsSize < sizeof (CHAR16) || Loaded->LoadOptionsSize > 32 ||
      (Loaded->LoadOptionsSize & 1) != 0) {
    return EFI_INVALID_PARAMETER;
  }
  Options = Loaded->LoadOptions;
  Count = Loaded->LoadOptionsSize / sizeof (CHAR16);
  if (Options[Count - 1] != L'\0') {
    return EFI_INVALID_PARAMETER;
  }
  if (StrCmp (Options, L"reboot") == 0 && Count == 7) {
    Fid = 0x84000009U;
  } else if (StrCmp (Options, L"shutdown") == 0 && Count == 9) {
    Fid = 0x84000008U;
  } else {
    return EFI_INVALID_PARAMETER;
  }
  // J716F PSCI 1.1/SMC is qualified; query anew for this invocation.
  if (J716fCurrentEl () != 4) {
    return EFI_UNSUPPORTED;
  }
  Version = PowerCall (0x84000000U, 0);
  if ((INT64)Version < 0 || Version < 0x00010000U || Version > 0x7fffffffU ||
      PowerCall (0x8400000aU, Fid) != 0) {
    Print (L"Power action blocked: PSCI query failed.\r\n");
    return EFI_UNSUPPORTED;
  }
  Print (Fid == 0x84000009U ? L"Rebooting...\r\n" : L"Powering off...\r\n");
  PowerCall (Fid, 0); // all unused x1..x17 explicitly zeroed, no RESET2 path
  Print (L"Power request returned; action did not complete.\r\n");
  return EFI_DEVICE_ERROR;
}
