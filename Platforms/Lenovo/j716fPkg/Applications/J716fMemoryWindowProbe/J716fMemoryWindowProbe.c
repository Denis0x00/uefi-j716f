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
#include "WindowPolicy.h"
#include <Library/CacheMaintenanceLib.h>


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
      Old->Magic == J716F_IO_RESULT_MAGIC && Old->Version == 2 && Old->Size == sizeof (*Old)) {
    Status = gBS->InstallConfigurationTable (&mResultGuid, NULL);
    if (EFI_ERROR (Status)) { return Status; }
    FreePool (Old);
  }
  mResult = AllocateZeroPool (sizeof (*mResult));
  if (mResult == NULL) { return EFI_OUT_OF_RESOURCES; }
  mResult->Magic = J716F_IO_RESULT_MAGIC; mResult->Version = 2; mResult->Size = sizeof (*mResult);
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

STATIC EFI_STATUS Snapshot (EFI_MEMORY_DESCRIPTOR **Map, UINTN *Size, UINTN *Stride)
{
  EFI_STATUS Status;
  UINTN Key, Capacity, Attempt;
  UINT32 Version;
  *Map = NULL; *Size = 0; *Stride = 0;
  Status = gBS->GetMemoryMap (Size, NULL, &Key, Stride, &Version);
  if (Status != EFI_BUFFER_TOO_SMALL || *Stride < sizeof (EFI_MEMORY_DESCRIPTOR)) {
    return EFI_LOAD_ERROR;
  }
  for (Attempt = 0; Attempt < 4; ++Attempt) {
    if (*Size > MAX_UINTN - 32 * *Stride) { return EFI_OUT_OF_RESOURCES; }
    Capacity = *Size + 32 * *Stride;
    *Map = AllocatePool (Capacity);
    if (*Map == NULL) { return EFI_OUT_OF_RESOURCES; }
    *Size = Capacity;
    Status = gBS->GetMemoryMap (Size, *Map, &Key, Stride, &Version);
    if (!EFI_ERROR (Status)) {
      if (*Stride < sizeof (EFI_MEMORY_DESCRIPTOR) || *Size % *Stride != 0) {
        FreePool (*Map); *Map = NULL; return EFI_LOAD_ERROR;
      }
      return EFI_SUCCESS;
    }
    FreePool (*Map); *Map = NULL;
    if (Status != EFI_BUFFER_TOO_SMALL) { return Status; }
  }
  return EFI_OUT_OF_RESOURCES;
}

STATIC VOID WaitForPower (VOID)
{
  EFI_INPUT_KEY Key;
  UINTN Index;
  if (gST->ConIn == NULL) { for (;;) { gBS->Stall (100000); } }
  gST->ConIn->Reset (gST->ConIn, FALSE);
  for (;;) {
    if (EFI_ERROR (gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index))) { continue; }
    if (!EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key)) && Key.UnicodeChar == L'\r') { return; }
  }
}

STATIC VOID Clear (VOID)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0,0,0,0};
  mGop->Blt (mGop, &Black, EfiBltVideoFill, 0, 0, 0, 0,
              mGop->Mode->Info->HorizontalResolution, mGop->Mode->Info->VerticalResolution, 0);
  mRow = 0;
}


STATIC VOID Check (CONST CHAR16 *Name, BOOLEAN Passed)
{
  CHAR16 Line[120];
  ++mResult->Checks;
  if (Passed) { ++mResult->Passed; } else { ++mResult->Failed; }
  UnicodeSPrint (Line, sizeof (Line), L"%s: %s", Name, Passed ? L"PASS" : L"FAIL");
  DrawLine (Line, Passed);
}

EFI_STATUS EFIAPI J716fMemoryWindowProbeMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status, FreeStatus;
  EFI_MEMORY_DESCRIPTOR *Map, *D;
  EFI_PHYSICAL_ADDRESS Address;
  UINTN Size, Stride, I, Offset, Round;
  UINT64 El, Choice, Expected, Actual, BadAddress;
  BOOLEAN Found, Good;
  CHAR16 Line[120];
  (VOID)ImageHandle; (VOID)SystemTable;
  Status = BeginResults ();
  if (EFI_ERROR (Status)) { return Status; }
  mScale = 2;
  Clear ();
  DrawLine (L"J716F V14 - 256 MiB MEMORY WINDOW", TRUE);
  DrawLine (L"Experimental RAM qualification; no XNU or ExitBootServices.", TRUE);
  DrawLine (L"Candidate region: D0000000 .. E8000000 (384 MiB)", TRUE);
  DrawLine (L"Kernel base residue: 0007C000; full-window 64-bit patterns.", TRUE);
  __asm__ volatile ("mrs %0, CurrentEL" : "=r" (El));
  Check (L"Executing at EL1", (El >> 2) == 1);
  if ((El >> 2) != 1) { goto Done; }
  Status = Snapshot (&Map, &Size, &Stride);
  Found = FALSE; Choice = 0;
  if (!EFI_ERROR (Status)) {
    for (I = 0; I < Size / Stride; ++I) {
      D = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + I * Stride);
      if (D->Type == EfiConventionalMemory &&
          J716fChooseWindow (D->PhysicalStart, D->NumberOfPages, &Choice)) {
        Found = TRUE; break;
      }
    }
    FreePool (Map);
  }
  Check (L"Map has bounded 256 MiB candidate", Found);
  if (!Found) { goto Done; }
  Address = Choice;
  UnicodeSPrint (Line, sizeof (Line), L"Allocate %016lx .. %016lx", Address, Address + J716F_WINDOW_SIZE);
  DrawLine (Line, TRUE);
  Status = gBS->AllocatePages (AllocateAddress, EfiLoaderCode,
                             EFI_SIZE_TO_PAGES (J716F_WINDOW_SIZE), &Address);
  Check (L"AllocatePages 256 MiB", !EFI_ERROR (Status));
  if (EFI_ERROR (Status)) {
    UnicodeSPrint (Line, sizeof (Line), L"Allocation status: %r", Status);
    DrawLine (Line, FALSE); goto Done;
  }
  mResult->WindowBase = Address; mResult->WindowBytes = J716F_WINDOW_SIZE;
  Good = Address == Choice && Address >= J716F_REGION_BASE &&
         Address <= J716F_REGION_END - J716F_WINDOW_SIZE &&
         (Address & 0x1fffff) == J716F_BASE_RESIDUE;
  Check (L"Allocated address and alignment", Good);
  if (!Good) { goto Release; }
  for (Round = 0; Round < 2; ++Round) {
    UnicodeSPrint (Line, sizeof (Line), L"Round %u: WRITE 256 MiB, cache flush, READ 256 MiB", Round + 1);
    DrawLine (Line, TRUE);
    for (Offset = 0; Offset < J716F_WINDOW_SIZE; Offset += sizeof (UINT64)) {
      *(volatile UINT64 *)(UINTN)(Address + Offset) = J716fPattern (Address + Offset, Round);
    }
    WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)Address, J716F_WINDOW_SIZE);
    Good = TRUE; BadAddress = 0; Actual = 0; Expected = 0;
    for (Offset = 0; Offset < J716F_WINDOW_SIZE; Offset += sizeof (UINT64)) {
      Expected = J716fPattern (Address + Offset, Round);
      Actual = *(volatile UINT64 *)(UINTN)(Address + Offset);
      if (Actual != Expected) { Good = FALSE; BadAddress = Address + Offset; break; }
    }
    if (Good) { mResult->VerifiedBytes += J716F_WINDOW_SIZE; }
    Check (Round == 0 ? L"Round 1 full-window readback" : L"Round 2 inverted readback", Good);
    if (!Good) {
      mResult->FailureAddress = BadAddress;
      UnicodeSPrint (Line, sizeof (Line), L"Mismatch @%016lx got=%016lx", BadAddress, Actual);
      DrawLine (Line, FALSE);
      UnicodeSPrint (Line, sizeof (Line), L"Expected %016lx", Expected); DrawLine (Line, FALSE);
      break;
    }
  }
Release:
  // Leave allocated memory clean before returning it to the firmware allocator.
  WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)Address, J716F_WINDOW_SIZE);
  FreeStatus = gBS->FreePages (Address, EFI_SIZE_TO_PAGES (J716F_WINDOW_SIZE));
  Check (L"FreePages 256 MiB", !EFI_ERROR (FreeStatus));
Done:
  mResult->Complete = 1;
  UnicodeSPrint (Line, sizeof (Line), L"RESULT checks=%u PASS=%u FAIL=%u verified=%lu MiB",
                mResult->Checks, mResult->Passed, mResult->Failed, mResult->VerifiedBytes >> 20);
  DrawLine (Line, mResult->Checks == 7 && mResult->Passed == 7 && mResult->Failed == 0);
  DrawLine (L"Photo this screen. Short power: RETURN to menu diagnosis.", TRUE);
  WaitForPower ();
  return mResult->Checks == 7 && mResult->Passed == 7 && mResult->Failed == 0 ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}
