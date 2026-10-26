// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/ArmLib.h>
#include "Ring.h"

extern CONST UINT8 gCaFontGlyphs[95][19];
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC CONST CHAR8 mBuild[] = "J716F V23P6 RAMLOG r1 20261025";
STATIC CONST CHAR8 mPrefix[] = "[J716F-V23P6-r1 seq=";
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL mBg = {15, 12, 11, 0};
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL mFg = {245, 243, 242, 0};

STATIC VOID Barrier (VOID) { ArmDataSynchronizationBarrier (); }

STATIC VOID Line (UINTN Row, CONST CHAR16 *Text)
{
  UINTN Width, Height, Count, I, X, Y, Sx, Sy, Pixel;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
  CONST UINT8 *Glyph;
  Width = mGop->Mode->Info->HorizontalResolution;
  Height = mGop->Mode->Info->VerticalResolution;
  if (Width < 96 || 40 + Row * 72 + 57 > Height) { return; }
  Count = MIN (StrLen (Text), (Width - 80) / 24);
  if (!Count) { return; }
  Pixels = AllocatePool (Count * 24 * 57 * sizeof (*Pixels));
  if (Pixels == NULL) { return; }
  for (I = 0; I < Count * 24 * 57; ++I) { Pixels[I] = mBg; }
  for (I = 0; I < Count; ++I) {
    Glyph = gCaFontGlyphs[Text[I] >= 32 && Text[I] <= 126 ? Text[I] - 32 : '?' - 32];
    for (Y = 0; Y < 19; ++Y) {
      for (X = 0; X < 8; ++X) {
        if ((Glyph[Y] & (0x80 >> X)) == 0) { continue; }
        for (Sy = 0; Sy < 3; ++Sy) {
          for (Sx = 0; Sx < 3; ++Sx) {
            Pixel = (Y * 3 + Sy) * Count * 24 + I * 24 + X * 3 + Sx;
            Pixels[Pixel] = mFg;
          }
        }
      }
    }
  }
  mGop->Blt (mGop, Pixels, EfiBltBufferToVideo, 0, 0, 40, 40 + Row * 72, Count * 24, 57, 0);
  FreePool (Pixels);
}

EFI_STATUS EFIAPI UefiMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  volatile J716F_RING *Ring = (volatile J716F_RING *)(UINTN)J716F_CONSOLE_BASE;
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  UINTN Event, Selected = 0, I;
  UINT32 Before, Matches, Sequence;
  BOOLEAN Written = FALSE;
  CHAR16 Text[120];
  CHAR8 Marker[128];
  UINTN Length;
  CONST CHAR16 *Result = L"Read only. Select APPEND to write one short marker.";
  CONST CHAR16 *Options[] = {L"Refresh header / read", L"Append marker (once this run)",
                            L"Warm restart (manual)", L"Return to UniXNU menu"};
  if (gST->ConIn == NULL) { return EFI_UNSUPPORTED; }
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&mGop);
  if (EFI_ERROR (Status) || mGop->Mode == NULL || mGop->Mode->Info == NULL) {
    return EFI_UNSUPPORTED;
  }
  // Boot Services remain running; this app has no timer/counter/SMC/XNU path.
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  Barrier ();
  Before = RingCount (Ring, (CONST UINT8 *)mPrefix, sizeof (mPrefix) - 1);
  for (;;) {
    Barrier ();
    Matches = RingCount (Ring, (CONST UINT8 *)mPrefix, sizeof (mPrefix) - 1);
    mGop->Blt (mGop, &mBg, EfiBltVideoFill, 0, 0, 0, 0,
               mGop->Mode->Info->HorizontalResolution, mGop->Mode->Info->VerticalResolution, 0);
    UnicodeSPrint (Text, sizeof (Text), L"%a", mBuild); Line (0, Text);
    Line (1, L"console: B00BE000 .. B00FDFFF; capacity 0003FFF4");
    UnicodeSPrint (Text, sizeof (Text), L"sig=%08x next=%08x size=%08x : %s",
                   Ring->Signature, Ring->Start, Ring->Size, RingValid (Ring) ? L"VALID" : L"INVALID / WRITES BLOCKED");
    Line (2, Text);
    UnicodeSPrint (Text, sizeof (Text), L"Markers on entry=%u  now=%u  appended this run=%u", Before, Matches, Written);
    Line (3, Text);
    Line (4, L"Retention is unverified until reboot + independent readback.");
    for (I = 0; I < 4; ++I) {
      UnicodeSPrint (Text, sizeof (Text), L"%s %s", I == Selected ? L">" : L" ", Options[I]);
      Line (6 + I, Text);
    }
    Line (11, Result);
    Line (13, L"Volume +/-: select; Power: enter. No automatic restart.");
    Line (14, L"Full ring append replaces only the oldest marker-length bytes.");
    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Event);
    if (EFI_ERROR (Status)) { return Status; }
    if (EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) { continue; }
    if (Key.ScanCode == SCAN_UP) { Selected = (Selected + 3) % 4; }
    if (Key.ScanCode == SCAN_DOWN) { Selected = (Selected + 1) % 4; }
    if (Key.ScanCode == SCAN_ESC) { return EFI_SUCCESS; }
    if (Key.UnicodeChar != L'\r') { continue; }
    if (Selected == 0) { Result = L"Refreshed. No RAM writes."; }
    if (Selected == 1) {
      if (Written) { Result = L"Already appended once. Repeat write blocked."; continue; }
      Sequence = Matches + 1;
      Length = AsciiSPrint (Marker, sizeof (Marker), "\n%aseq=%08u build=20261025]\n", "[J716F-V23P6-r1 ", Sequence);
      // Verify the exact marker prefix used by the reader.
      if (RingAppend (Ring, (CONST UINT8 *)Marker, Length, Barrier)) {
        Written = TRUE;
        Result = RingCount (Ring, (CONST UINT8 *)Marker, Length) != 0 ?
                 L"APPEND READBACK OK. Record this screen, then select Warm restart." :
                 L"APPEND READBACK FAILED. Do not infer retention.";
      } else { Result = L"Invalid header: append refused; no initialization performed."; }
    }
    if (Selected == 2) {
      Barrier ();
      gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
      Result = L"Warm reset returned. Reset/retention NOT proven.";
    }
    if (Selected == 3) { return EFI_SUCCESS; }
  }
}
