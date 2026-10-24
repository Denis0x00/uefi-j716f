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

STATIC UINTN mFailed;
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
  CHAR16 Line[96];
  UnicodeSPrint (Line, sizeof (Line), L"%s  %s", Good ? L"PASS" : L"FAIL", Name);
  ++mResult->Checks;
  if (Good) { ++mResult->Passed; } else { ++mFailed; ++mResult->Failed; }
  DrawLine (Line, Good);
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
EFI_STATUS EFIAPI J716fGopIoProbeMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL *Loaded = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_BLOCK_IO_PROTOCOL *Block;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_FILE_PROTOCOL *Root = NULL, *File = NULL;
  EFI_GUID DiskGuid = {0x3aac9276,0x97ee,0x4442,{0x81,0xd5,0x94,0x3e,0x24,0x15,0x47,0xbb}};
  UINT8 Buffer[8194];
  UINTN Size, I;
  UINT64 Position;
  BOOLEAN Good;
  CHAR16 Summary[96];
  mFailed = 0;
  Status = BeginResults ();
  if (EFI_ERROR (Status)) { return Status; }
  DrawLine (L"J716F V11 - EXTERNAL EFI GOP TEST", TRUE);
  DrawLine (L"Loaded from FAT; no physical disk test", TRUE);
  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded);
  if (EFI_ERROR (Status) || Loaded->DeviceHandle == NULL) { Check (L"loaded image device", FALSE); goto Done; }
  Status = gBS->HandleProtocol (Loaded->DeviceHandle, &gEfiDevicePathProtocolGuid, (VOID **)&Path);
  Good = !EFI_ERROR (Status) && Path->Type == HARDWARE_DEVICE_PATH && Path->SubType == HW_VENDOR_DP &&
         CompareMem (&((VENDOR_DEVICE_PATH *)Path)->Guid, &DiskGuid, sizeof (DiskGuid)) == 0;
  Check (L"loaded from J716F RAM disk device", Good);
  if (!Good) { goto Done; } // Never issue the write-denial probe on another device.
  Status = gBS->HandleProtocol (Loaded->DeviceHandle, &gEfiBlockIoProtocolGuid, (VOID **)&Block);
  Good = !EFI_ERROR (Status) && Block->Media->ReadOnly && Block->Media->BlockSize == 512 &&
         Block->Media->LastBlock == 8191 && Block->Media->MediaId == 0x716f0009;
  Check (L"4MiB read-only BlockIO geometry", Good);
  if (!Good) { goto Done; }
  Status = Block->ReadBlocks (Block, Block->Media->MediaId, 8191, 512, Buffer);
  Check (L"last sector readable", !EFI_ERROR (Status));
  Status = Block->ReadBlocks (Block, Block->Media->MediaId, 8191, 1024, Buffer);
  Check (L"out-of-range BlockIO read rejected", Status == EFI_INVALID_PARAMETER);
  Status = Block->WriteBlocks (Block, Block->Media->MediaId, 0, 512, Buffer);
  Check (L"BlockIO write rejected", Status == EFI_WRITE_PROTECTED);
  Status = gBS->HandleProtocol (Loaded->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
  if (EFI_ERROR (Status)) { Check (L"SimpleFileSystem", FALSE); goto Done; }
  Status = Fs->OpenVolume (Fs, &Root);
  if (EFI_ERROR (Status)) { Check (L"open FAT root", FALSE); goto Done; }
  Status = Root->Open (Root, &File, L"\\PATTERN.BIN", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) { Check (L"open pattern file", FALSE); goto Done; }
  Size = sizeof (Buffer);
  Status = File->Read (File, &Size, Buffer);
  Good = !EFI_ERROR (Status) && Size == 8193;
  for (I = 0; Good && I < Size; ++I) { Good = Buffer[I] == (UINT8)((I * 37 + 11) & 255); }
  Check (L"8193-byte file read and byte comparison", Good);
  Status = File->SetPosition (File, 509);
  if (!EFI_ERROR (Status)) { Size = 37; Status = File->Read (File, &Size, Buffer); }
  Good = !EFI_ERROR (Status) && Size == 37;
  for (I = 0; Good && I < Size; ++I) { Good = Buffer[I] == (UINT8)(((I + 509) * 37 + 11) & 255); }
  Check (L"seek across sector boundary", Good);
  Status = File->SetPosition (File, MAX_UINT64);
  if (!EFI_ERROR (Status)) { Status = File->GetPosition (File, &Position); }
  Good = !EFI_ERROR (Status) && Position == 8193;
  if (Good) { Size = 1; Status = File->Read (File, &Size, Buffer); Good = !EFI_ERROR (Status) && Size == 0; }
  Check (L"seek EOF and zero-byte read", Good);
  File->Close (File); File = NULL;
  Status = Root->Open (Root, &File, L"\\PATTERN.BIN", EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
  if (!EFI_ERROR (Status)) {
    Size = 1; Status = File->Write (File, &Size, Buffer);
    File->Close (File); File = NULL;
  }
  Check (L"filesystem write rejected", Status == EFI_WRITE_PROTECTED || Status == EFI_ACCESS_DENIED);
Done:
  if (File != NULL) { File->Close (File); }
  if (Root != NULL) { Root->Close (Root); }
  mResult->Complete = 1;
  UnicodeSPrint (Summary, sizeof (Summary), L"V11 RESULT: %s  PASS=%u FAIL=%u COUNT=%u",
    mFailed == 0 && mResult->Checks == 9 ? L"PASS" : L"FAIL/INCOMPLETE",
    mResult->Passed, mResult->Failed, mResult->Checks);
  DrawLine (Summary, mFailed == 0 && mResult->Checks == 9);
  Pause ();
  return mFailed == 0 && mResult->Checks == 9 && !EFI_ERROR (mResult->GopStatus) ? EFI_SUCCESS : EFI_ABORTED;
}
