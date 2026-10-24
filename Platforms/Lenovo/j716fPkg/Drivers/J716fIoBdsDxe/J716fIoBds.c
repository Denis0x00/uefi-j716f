/** @file
  Minimal BDS launching the official UniXNU FV application. Console connection
  follows the bounded-pass approach in ht94911/uefi-cas CasBdsDxe.c
  (a491522d8f5fecd57be706df1e42e8bd3ca79b20), BSD-2-Clause-Patent.
  No persistent boot backend, storage controller or vendor firmware is selected.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Protocol/Bds.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

STATIC EFI_HANDLE mImageHandle;
STATIC VOID EFIAPI EmptyNotify (EFI_EVENT Event, VOID *Context) { }

STATIC VOID ConnectFirmwareHandles (VOID)
{
  EFI_HANDLE *Handles;
  UINTN Count, Index, Pass;
  for (Pass = 0; Pass < 3; ++Pass) {
    if (!EFI_ERROR (gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &Count, &Handles))) {
      for (Index = 0; Index < Count; ++Index) {
        gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
      }
      FreePool (Handles);
    }
  }
}

STATIC VOID SetConsole (EFI_GUID *Protocol, EFI_HANDLE *Handle, VOID **Interface)
{
  EFI_HANDLE *Handles;
  UINTN Count;
  if (!EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, Protocol, NULL, &Count, &Handles))) {
    if (Count > 0) {
      *Handle = Handles[0];
      gBS->HandleProtocol (*Handle, Protocol, Interface);
    }
    FreePool (Handles);
  }
}

STATIC VOID SetInputConsole (VOID)
{
  EFI_GUID Keypad = {0x846513b2,0x4287,0x4055,{0xa6,0x8f,0xa8,0x8e,0x86,0x07,0xc6,0x25}};
  EFI_HANDLE *Handles;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  UINTN Count, I;
  if (!EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleTextInProtocolGuid, NULL, &Count, &Handles))) {
    for (I = 0; I < Count; ++I) {
      Path = DevicePathFromHandle (Handles[I]);
      if (Path != NULL && DevicePathType (Path) == HARDWARE_DEVICE_PATH &&
          DevicePathSubType (Path) == HW_VENDOR_DP &&
          CompareGuid (&((VENDOR_DEVICE_PATH *)Path)->Guid, &Keypad)) {
        gST->ConsoleInHandle = Handles[I];
        gBS->HandleProtocol (Handles[I], &gEfiSimpleTextInProtocolGuid, (VOID **)&gST->ConIn);
        FreePool (Handles);
        return;
      }
    }
    FreePool (Handles);
  }
  SetConsole (&gEfiSimpleTextInProtocolGuid, &gST->ConsoleInHandle, (VOID **)&gST->ConIn);
}

STATIC EFI_STATUS BootUniXnu (VOID)
{
  EFI_STATUS Status = EFI_NOT_FOUND;
  EFI_STATUS ReadStatus;
  EFI_HANDLE *Handles;
  EFI_HANDLE Image;
  UINTN Count, Index, Size;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv;
  EFI_FV_FILETYPE Type;
  EFI_FV_FILE_ATTRIBUTES Attributes;
  UINT32 Auth;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH Node;
  EFI_GUID *File = (EFI_GUID *)PcdGetPtr (PcdUniXNUApplicationFile);

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiFirmwareVolume2ProtocolGuid,
                                    NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) { return Status; }
  Status = EFI_NOT_FOUND;
  for (Index = 0; Index < Count; ++Index) {
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv)) ||
        (DevicePathFromHandle (Handles[Index]) == NULL)) {
      continue;
    }
    Size = 0;
    ReadStatus = Fv->ReadFile (Fv, File, NULL, &Size, &Type, &Attributes, &Auth);
    if (EFI_ERROR (ReadStatus)) { continue; }
    EfiInitializeFwVolDevicepathNode (&Node, File);
    Path = AppendDevicePathNode (DevicePathFromHandle (Handles[Index]), (EFI_DEVICE_PATH_PROTOCOL *)&Node);
    if (Path == NULL) { Status = EFI_OUT_OF_RESOURCES; break; }
    Status = gBS->LoadImage (FALSE, mImageHandle, Path, NULL, 0, &Image);
    FreePool (Path);
    if (!EFI_ERROR (Status)) {
      EfiSignalEventReadyToBoot ();
      DEBUG ((DEBUG_ERROR, "J716F V9 START_UNIXNU\n"));
      // Application host mode avoids a second ConnectAll; BDS connected above.
      Status = gBS->StartImage (Image, NULL, NULL);
    }
    break;
  }
  FreePool (Handles);
  return Status;
}

STATIC VOID EFIAPI BdsEntry (EFI_BDS_ARCH_PROTOCOL *This)
{
  EFI_STATUS Status;
  EFI_EVENT Event;
  DEBUG ((DEBUG_ERROR, "J716F V9 BDS_ENTER\n"));
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  if (!EFI_ERROR (gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, EmptyNotify, NULL,
                                     &gEfiEndOfDxeEventGroupGuid, &Event))) {
    gBS->SignalEvent (Event);
    gBS->CloseEvent (Event);
  }
  ConnectFirmwareHandles ();
  SetConsole (&gEfiSimpleTextOutProtocolGuid, &gST->ConsoleOutHandle, (VOID **)&gST->ConOut);
  SetInputConsole ();
  if (gST->ConOut != NULL) {
    gST->StandardErrorHandle = gST->ConsoleOutHandle;
    gST->StdErr = gST->ConOut;
  }
  gST->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gST, gST->Hdr.HeaderSize, &gST->Hdr.CRC32);
  Status = BootUniXnu ();
  DEBUG ((DEBUG_ERROR, "J716F V9 UNIXNU_RETURNED %r - HALTED\n", Status));
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  // Keep evidence visible. Do not reboot, repeat the launch or boot another OS.
  for (;;) { gBS->Stall (100000); }
}

STATIC EFI_BDS_ARCH_PROTOCOL mBds = { BdsEntry };

EFI_STATUS EFIAPI J716fIoBdsInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HANDLE Handle = NULL;
  mImageHandle = ImageHandle;
  return gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiBdsArchProtocolGuid, &mBds, NULL);
}
