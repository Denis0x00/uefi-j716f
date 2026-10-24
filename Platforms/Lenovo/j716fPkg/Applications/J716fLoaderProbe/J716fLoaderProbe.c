// SPDX-License-Identifier: BSD-2-Clause-Patent
// Executed only as a file on the embedded FAT, not as an FV executable.
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Protocol/GraphicsOutput.h>
#include "IoResult.h"
#include <Protocol/UniXNULoader.h>

STATIC UINTN mFailed;
STATIC CONST CHAR16 *mNames[11];
STATIC BOOLEAN mGood[11];
STATIC J716F_IO_RESULT *mResult;
STATIC EFI_GUID mResultGuid = J716F_IO_RESULT_GUID;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC UINTN mScale, mRow;
extern CONST UINT8 gCaFontGlyphs[95][19];

STATIC VOID DrawLine (CONST CHAR16 *Text, BOOLEAN Good)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Fore = Good ?
    (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x70,0xff,0xa0,0} :
    (EFI_GRAPHICS_OUTPUT_BLT_PIXEL){0x70,0x70,0xff,0};
  UINTN W, H, Margin, Len, I, X, Y, Dx, Dy, Ch, At;
  EFI_STATUS Status;
  if (mGop == NULL) { return; }
  Margin = 8 * mScale;
  W = mGop->Mode->Info->HorizontalResolution - Margin * 2;
  H = 19 * mScale;
  Y = Margin + mRow++ * 24 * mScale;
  if (Y + H > mGop->Mode->Info->VerticalResolution) { mResult->GopStatus = EFI_BAD_BUFFER_SIZE; return; }
  Pixels = AllocateZeroPool (W * H * sizeof (*Pixels));
  if (Pixels == NULL) { mResult->GopStatus = EFI_OUT_OF_RESOURCES; return; }
  Len = StrLen (Text);
  if (Len > W / (8 * mScale)) { Len = W / (8 * mScale); }
  for (I = 0; I < Len; ++I) {
    Ch = Text[I]; if (Ch < 32 || Ch > 126) { Ch = '?'; }
    for (Y = 0; Y < 19; ++Y) {
      for (X = 0; X < 8; ++X) {
        if ((gCaFontGlyphs[Ch - 32][Y] & (0x80 >> X)) == 0) { continue; }
        for (Dy = 0; Dy < mScale; ++Dy) {
          for (Dx = 0; Dx < mScale; ++Dx) {
            At = (Y * mScale + Dy) * W + (I * 8 + X) * mScale + Dx;
            Pixels[At] = Fore;
          }
        }
      }
    }
  }
  Status = mGop->Blt (mGop, Pixels, EfiBltBufferToVideo, 0, 0, Margin,
                       Margin + (mRow - 1) * 24 * mScale, W, H, W * sizeof (*Pixels));
  if (EFI_ERROR (Status)) { mResult->GopStatus = Status; }
  FreePool (Pixels);
}

STATIC EFI_STATUS BeginResults (VOID)
{
  EFI_STATUS Status;
  J716F_IO_RESULT *Old;
  EFI_HANDLE *Handles;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0,0,0,0};
  UINTN Count, I;
  if (!EFI_ERROR (EfiGetSystemConfigurationTable (&mResultGuid, (VOID **)&Old)) &&
      Old->Magic == J716F_IO_RESULT_MAGIC && Old->Version == 1 && Old->Size == sizeof (*Old)) {
    Status = gBS->InstallConfigurationTable (&mResultGuid, NULL);
    if (EFI_ERROR (Status)) { return Status; }
    FreePool (Old);
  }
  mResult = AllocateZeroPool (sizeof (*mResult));
  if (mResult == NULL) { return EFI_OUT_OF_RESOURCES; }
  mResult->Magic = J716F_IO_RESULT_MAGIC; mResult->Version = 1; mResult->Size = sizeof (*mResult);
  Status = gBS->InstallConfigurationTable (&mResultGuid, mResult);
  if (EFI_ERROR (Status)) { FreePool (mResult); mResult = NULL; return Status; }
  mGop = NULL; mRow = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    for (I = 0; I < Count; ++I) {
      if (!EFI_ERROR (gBS->HandleProtocol (Handles[I], &gEfiGraphicsOutputProtocolGuid, (VOID **)&Gop)) &&
          Gop->Mode != NULL && Gop->Mode->Info != NULL && Gop->Mode->FrameBufferBase == 0x9C000000) {
        mGop = Gop; break;
      }
    }
    FreePool (Handles);
  }
  if (mGop == NULL) { mResult->GopStatus = EFI_NOT_FOUND; return EFI_NOT_FOUND; }
  mScale = MAX (1, MIN (mGop->Mode->Info->HorizontalResolution / 640,
                        mGop->Mode->Info->VerticalResolution / 400));
  mResult->GopStatus = mGop->Blt (mGop, &Black, EfiBltVideoFill, 0, 0, 0, 0,
    mGop->Mode->Info->HorizontalResolution, mGop->Mode->Info->VerticalResolution, 0);
  return EFI_SUCCESS;
}
STATIC VOID Check (CONST CHAR16 *Name, BOOLEAN Good) {
  UINTN At = mResult->Checks;
  if (At < ARRAY_SIZE (mNames)) { mNames[At] = Name; mGood[At] = Good; }
  ++mResult->Checks;
  if (Good) { ++mResult->Passed; } else { ++mFailed; ++mResult->Failed; }
}
STATIC VOID Pause (VOID) {
  EFI_INPUT_KEY Key;
  UINTN Index;
  DrawLine (L"Photo results FIRST; short power returns.", TRUE);
  if (gST->ConIn == NULL) { for (;;) { gBS->Stall (100000); } }
  gST->ConIn->Reset (gST->ConIn, FALSE);
  for (;;) {
    if (EFI_ERROR (gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index))) { gBS->Stall (100000); continue; }
    if (!EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key)) && Key.UnicodeChar == L'\r') { return; }
  }
}

EFI_STATUS EFIAPI J716fLoaderProbeMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL *Loaded = NULL;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_GUID DiskGuid = {0x3aac9276,0x97ee,0x4442,{0x81,0xd5,0x94,0x3e,0x24,0x15,0x47,0xbb}};
  UNIXNU_LOADER_PROTOCOL *Loader;
  UNIXNU_LOADER_BOOT_REQUEST Request;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0,0,0,0};
  UINT64 CurrentEl;
  UINTN Size, Key, Stride, I;
  UINT32 Version;
  CHAR16 Line[96];
  BOOLEAN Good;
  (VOID)SystemTable;
  mFailed = 0;
  Status = BeginResults ();
  if (EFI_ERROR (Status)) { return Status; }
  DrawLine (L"J716F V12 - DarwinBoot protocol tests", TRUE);
  DrawLine (L"Calling official loader; please wait", TRUE);
  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded);
  if (EFI_ERROR (Status) || Loaded->DeviceHandle == NULL) { Check (L"probe RAM FAT origin", FALSE); goto Done; }
  Status = gBS->HandleProtocol (Loaded->DeviceHandle, &gEfiDevicePathProtocolGuid, (VOID **)&Path);
  Good = !EFI_ERROR (Status) && Path->Type == HARDWARE_DEVICE_PATH && Path->SubType == HW_VENDOR_DP &&
    CompareMem (&((VENDOR_DEVICE_PATH *)Path)->Guid, &DiskGuid, sizeof (DiskGuid)) == 0;
  Check (L"probe loaded from J716F RAM FAT", Good);
  if (!Good) { goto Done; }
  Status = gBS->LocateProtocol (&gUniXNULoaderProtocolGuid, NULL, (VOID **)&Loader);
  Good = !EFI_ERROR (Status) && Loader->Revision == UNIXNU_LOADER_REVISION && Loader->Boot != NULL;
  Check (L"official DarwinBoot loader v2 registered", Good);
  if (!Good) { goto Done; }
  __asm__ volatile ("mrs %0, CurrentEL" : "=r" (CurrentEl));
  Check (L"firmware executing at EL1", CurrentEl == 4);
  if (CurrentEl != 4) { goto Done; }
  Check (L"NULL request rejected: INVALID_PARAMETER", Loader->Boot (Loader, NULL) == EFI_INVALID_PARAMETER);
  ZeroMem (&Request, sizeof (Request));
  Check (L"empty request rejected: INVALID_PARAMETER", Loader->Boot (Loader, &Request) == EFI_INVALID_PARAMETER);
  Request.Volume = Loaded->DeviceHandle;
  // This path is intentionally absent from this RAM FAT.
  Request.KernelPath = L"\\MISSING.MAC";
  Request.KextCount = 1;
  Check (L"unsupported kext request rejected", Loader->Boot (Loader, &Request) == EFI_UNSUPPORTED);
  Request.KextCount = 0;
  Request.DeviceTreeSize = 1;
  Check (L"unsupported FDT input rejected", Loader->Boot (Loader, &Request) == EFI_UNSUPPORTED);
  Request.DeviceTreeSize = 0;
  Request.RamdiskSize = 1;
  Check (L"inconsistent ramdisk rejected", Loader->Boot (Loader, &Request) == EFI_BAD_BUFFER_SIZE);
  Request.RamdiskSize = 0;
  Check (L"missing kernel rejected: NOT_FOUND", Loader->Boot (Loader, &Request) == EFI_NOT_FOUND);
  // The existing deterministic byte pattern has no Mach-O header. ParseKernel
  // must refuse it before ADT reads, AllocateWindow or ExitBootServices.
  Request.KernelPath = L"\\PATTERN.BIN";
  Check (L"non Mach-O file rejected: UNSUPPORTED", Loader->Boot (Loader, &Request) == EFI_UNSUPPORTED);
  Size = 0; Key = 0; Stride = 0; Version = 0;
  Status = gBS->GetMemoryMap (&Size, NULL, &Key, &Stride, &Version);
  Check (L"boot services retained after refusals", Status == EFI_BUFFER_TOO_SMALL && Size >= Stride && Stride >= sizeof (EFI_MEMORY_DESCRIPTOR));
Done:
  // Refusal logging uses Print(). Redraw all results after the calls so both
  // a working text console and a disconnected one leave the same visible page.
  Status = mGop->Blt (mGop, &Black, EfiBltVideoFill, 0, 0, 0, 0,
    mGop->Mode->Info->HorizontalResolution, mGop->Mode->Info->VerticalResolution, 0);
  if (EFI_ERROR (Status)) { mResult->GopStatus = Status; }
  mRow = 0;
  DrawLine (L"J716F V12 - DarwinBoot protocol tests", TRUE);
  DrawLine (L"No real kernel entry attempted", TRUE);
  for (I = 0; I < MIN (mResult->Checks, ARRAY_SIZE (mNames)); ++I) {
    UnicodeSPrint (Line, sizeof (Line), L"%s  %s", mGood[I] ? L"PASS" : L"FAIL", mNames[I]);
    DrawLine (Line, mGood[I]);
  }
  mResult->Complete = 1;
  UnicodeSPrint (Line, sizeof (Line), L"V12 RESULT: %s PASS=%u FAIL=%u COUNT=%u",
    mFailed == 0 && mResult->Checks == 11 ? L"PASS" : L"FAIL/INCOMPLETE",
    mResult->Passed, mResult->Failed, mResult->Checks);
  DrawLine (Line, mFailed == 0 && mResult->Checks == 11);
  Pause ();
  return mFailed == 0 && mResult->Checks == 11 && !EFI_ERROR (mResult->GopStatus) ? EFI_SUCCESS : EFI_ABORTED;
}
