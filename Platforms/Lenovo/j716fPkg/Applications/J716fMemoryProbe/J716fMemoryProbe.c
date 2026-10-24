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

EFI_STATUS EFIAPI J716fMemoryProbeMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_MEMORY_DESCRIPTOR *Map, *D;
  EFI_STATUS Status;
  UINTN Size, Stride, I, Page, Pages, Count, FreeCount, Row;
  UINT64 Bytes, End, Start, TotalFree, Best, BestStart, El;
  CHAR16 Line[120];
  (VOID)ImageHandle; (VOID)SystemTable;
  Status = BeginResults ();
  if (EFI_ERROR (Status)) { return Status; }
  mScale = 2;
  Status = Snapshot (&Map, &Size, &Stride);
  Clear ();
  DrawLine (L"J716F V13 - UEFI MEMORY SURVEY", TRUE);
  if (EFI_ERROR (Status)) {
    UnicodeSPrint (Line, sizeof (Line), L"GetMemoryMap failed: 0x%lx", (UINT64)Status);
    DrawLine (Line, FALSE); WaitForPower (); return Status;
  }
  TotalFree = 0; Best = 0; BestStart = 0; FreeCount = 0;
  Count = Size / Stride;
  for (I = 0; I < Count; ++I) {
    D = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + I * Stride);
    if (D->Type != EfiConventionalMemory || D->NumberOfPages > (MAX_UINT64 >> 12)) { continue; }
    Bytes = EFI_PAGES_TO_SIZE (D->NumberOfPages);
    if (D->PhysicalStart > MAX_UINT64 - Bytes || D->PhysicalStart > MAX_UINT64 - 0x1fffff) { continue; }
    End = D->PhysicalStart + Bytes;
    Start = ALIGN_VALUE (D->PhysicalStart, 0x200000);
    ++FreeCount; TotalFree += Bytes;
    if (End > Start && End - Start > Best) { Best = End - Start; BestStart = Start; }
  }
  __asm__ volatile ("mrs %0, CurrentEL" : "=r" (El));
  UnicodeSPrint (Line, sizeof (Line), L"EL%u  GOP %ux%u  FB=0x%lx", (UINTN)(El >> 2),
                mGop->Mode->Info->HorizontalResolution, mGop->Mode->Info->VerticalResolution,
                mGop->Mode->FrameBufferBase);
  DrawLine (Line, TRUE);
  UnicodeSPrint (Line, sizeof (Line), L"Map descriptors=%u  conventional=%u", Count, FreeCount);
  DrawLine (Line, TRUE);
  UnicodeSPrint (Line, sizeof (Line), L"Total free: 0x%lx (%lu MiB)", TotalFree, TotalFree >> 20);
  DrawLine (Line, TRUE);
  UnicodeSPrint (Line, sizeof (Line), L"Largest 2MiB-aligned run: 0x%lx (%lu MiB)", Best, Best >> 20);
  DrawLine (Line, Best >= 0x10000000);
  UnicodeSPrint (Line, sizeof (Line), L"Run starts at 0x%lx", BestStart);
  DrawLine (Line, TRUE);
  DrawLine (Best >= 0x10000000 ? L"256 MiB upper-bound capacity: YES" : L"256 MiB upper-bound capacity: NO", Best >= 0x10000000);
  DrawLine (L"Kernel base residue not checked; no window allocated.", TRUE);
  DrawLine (L"This snapshot is for memory planning, not a hand-off test.", TRUE);
  DrawLine (L"No kernel load, ExitBootServices or partition writes.", TRUE);
  DrawLine (L"Photo this page FIRST. Short power: memory-map pages.", TRUE);
  WaitForPower ();
  Pages = (Count + 19) / 20;
  for (Page = 0; Page < Pages; ++Page) {
    Clear ();
    UnicodeSPrint (Line, sizeof (Line), L"J716F V13 - MEMORY MAP page %u/%u", Page + 1, Pages);
    DrawLine (Line, TRUE);
    DrawLine (L"idx type  physical start     bytes              attributes", TRUE);
    for (Row = 0; Row < 20 && Page * 20 + Row < Count; ++Row) {
      I = Page * 20 + Row;
      D = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + I * Stride);
      UnicodeSPrint (Line, sizeof (Line), L"%03u %02u   %016lx   %016lx   %016lx", I, D->Type,
                    D->PhysicalStart, EFI_PAGES_TO_SIZE (D->NumberOfPages), D->Attribute);
      DrawLine (Line, D->Type == EfiConventionalMemory);
    }
    DrawLine (L"Type 07=Conventional (free); 03/04=BootServices; 00=Reserved", TRUE);
    DrawLine (Page + 1 < Pages ? L"Photo first. Short power: NEXT page." : L"Photo first. Short power: RETURN to menu.", TRUE);
    WaitForPower ();
  }
  FreePool (Map);
  return EFI_SUCCESS;
}
