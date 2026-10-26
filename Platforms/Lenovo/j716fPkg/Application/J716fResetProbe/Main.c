// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/Reset.h>
#include <Protocol/ResetNotification.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/ArmLib.h>
#include <Library/DevicePathLib.h>
#include "Ring.h"

#define PSCI_VERSION  0x84000000U
#define PSCI_FEATURES 0x8400000AU
#define PSCI_RESET    0x84000009U
#define PSCI_RESET2   0xC4000012U
#define RESET_RVA    0x10540U

extern CONST UINT8 gCaFontGlyphs[95][19];
VOID J716fSmc (UINT64 *Args);
UINT64 J716fCurrentEl (VOID);
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL mBg = {15, 12, 11, 0};
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL mFg = {245, 243, 242, 0};
STATIC volatile J716F_RING *mRing = (volatile J716F_RING *)(UINTN)J716F_CONSOLE_BASE;
STATIC CONST UINT8 mP6[] = "[J716F-V23P6-r1 seq=";
STATIC CONST UINT8 mP7[] = "[J716F-V23P7-r1 ";
STATIC CONST UINT8 mResetPrefix[] = {
  0xf3, 0x53, 0xbb, 0xa9, 0xf5, 0x5b, 0x01, 0xa9,
  0xf7, 0x63, 0x02, 0xa9, 0xf9, 0x1b, 0x00, 0xf9
};
STATIC CONST EFI_GUID mResetGuid = {0x4b28e4c7,0xff36,0x4e10,{0x93,0xcf,0xa8,0x21,0x59,0xe7,0x77,0xc5}};
STATIC UINTN mSelected, mResetPointer, mOwnerBase, mOwnerSize, mResetOffset;
STATIC EFI_GUID mOwnerGuid;
STATIC BOOLEAN mOwnerFound, mExpectedRoute, mResetArch, mResetNotify, mQueried;
STATIC UINT64 mVersion, mResetFeature, mReset2Feature, mLastArgs[8];
STATIC UINT32 mLastFunction, mSequence, mEntryP7;
STATIC CONST CHAR16 *mResult = L"Inventory only. Select Query PSCI to collect return values.";
STATIC CONST CHAR16 *mOptions[] = {
  L"Refresh inventory (read only)",
  L"Query PSCI + record (NO RESET)",
  L"EFI Warm restart + record (manual)",
  L"Direct RESET2 + record (requires support)",
  L"Direct SYSTEM_RESET + record (manual)",
  L"Return to UniXNU menu"
};
STATIC VOID Barrier (VOID) { ArmDataSynchronizationBarrier (); }
#include "Panel.h"

STATIC VOID Inventory (VOID)
{
  EFI_HANDLE *Handles;
  EFI_LOADED_IMAGE_PROTOCOL *Image;
  EFI_DEVICE_PATH_PROTOCOL *Node;
  VOID *Interface;
  UINTN Count, Index, Nodes, Bytes, Length;
  mResetPointer = (UINTN)gRT->ResetSystem;
  mResetArch = !EFI_ERROR (gBS->LocateProtocol (&gEfiResetArchProtocolGuid, NULL, &Interface));
  mResetNotify = !EFI_ERROR (gBS->LocateProtocol (&gEfiResetNotificationProtocolGuid, NULL, &Interface));
  mOwnerFound = FALSE;
  mExpectedRoute = FALSE;
  mOwnerBase = mOwnerSize = mResetOffset = 0;
  ZeroMem (&mOwnerGuid, sizeof (mOwnerGuid));
  if (EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &Count, &Handles))) {
    return;
  }
  for (Index = 0; Index < Count; ++Index) {
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiLoadedImageProtocolGuid, (VOID **)&Image))) { continue; }
    if (mResetPointer < (UINTN)Image->ImageBase ||
        mResetPointer - (UINTN)Image->ImageBase >= Image->ImageSize) { continue; }
    mOwnerFound = TRUE;
    mOwnerBase = (UINTN)Image->ImageBase;
    mOwnerSize = (UINTN)Image->ImageSize;
    mResetOffset = mResetPointer - mOwnerBase;
    Node = Image->FilePath;
    for (Nodes = 0, Bytes = 0; Node != NULL && Nodes < 32 && Bytes < 4096; ++Nodes) {
      if (IsDevicePathEnd (Node)) { break; }
      Length = DevicePathNodeLength (Node);
      if (Length < sizeof (*Node) || Length > 4096 - Bytes) { break; }
      if (DevicePathType (Node) == MEDIA_DEVICE_PATH && DevicePathSubType (Node) == MEDIA_PIWG_FW_FILE_DP &&
          Length == sizeof (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH)) {
        CopyMem (&mOwnerGuid, &((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)Node)->FvFileName, sizeof (mOwnerGuid));
        break;
      }
      Bytes += Length;
      Node = NextDevicePathNode (Node);
    }
    // Inspect only bytes inside the owning loaded image. This is a prefix match,
    // not an integrity hash or evidence that the function actually ran.
    if (mResetOffset == RESET_RVA && mOwnerSize - mResetOffset >= sizeof (mResetPrefix) &&
        CompareGuid (&mOwnerGuid, &mResetGuid) &&
        CompareMem ((VOID *)mResetPointer, mResetPrefix, sizeof (mResetPrefix)) == 0) {
      mExpectedRoute = TRUE;
    }
    break;
  }
  FreePool (Handles);
}

STATIC VOID Render (VOID)
{
  CHAR16 Text[140];
  UINTN Index;
  Barrier ();
  mGop->Blt (mGop, &mBg, EfiBltVideoFill, 0, 0, 0, 0,
              mGop->Mode->Info->HorizontalResolution, mGop->Mode->Info->VerticalResolution, 0);
  Line (0, L"J716F V23P7 RESET r1 - firmware / direct PSCI comparison");
  UnicodeSPrint (Text, sizeof (Text), L"EL%u  ResetArch=%u ResetNotify=%u ownerFound=%u",
                 (UINT32)(J716fCurrentEl () >> 2), mResetArch, mResetNotify, mOwnerFound); Line (1, Text);
  UnicodeSPrint (Text, sizeof (Text), L"Reset=%016lx base=%016lx RVA=%lx", (UINT64)mResetPointer, (UINT64)mOwnerBase, (UINT64)mResetOffset); Line (2, Text);
  UnicodeSPrint (Text, sizeof (Text), L"Owner=%g  P6 driver GUID/RVA/prefix match=%u", &mOwnerGuid, mExpectedRoute); Line (3, Text);
  UnicodeSPrint (Text, sizeof (Text), L"RAM: %s next=%08x size=%08x; P6=%u P7 entry=%u now=%u",
    RingValid (mRing) ? L"VALID" : L"INVALID / CALLS BLOCKED", mRing->Start, mRing->Size,
    RingCount (mRing, mP6, sizeof (mP6)-1), mEntryP7, RingCount (mRing, mP7, sizeof (mP7)-1)); Line (4, Text);
  UnicodeSPrint (Text, sizeof (Text), L"Queries complete=%u  VERSION x0=%016lx", mQueried, mVersion); Line (5, Text);
  UnicodeSPrint (Text, sizeof (Text), L"FEATURES SYSTEM_RESET x0=%016lx status32=%d", mResetFeature, (INT32)(UINT32)mResetFeature); Line (6, Text);
  UnicodeSPrint (Text, sizeof (Text), L"FEATURES RESET2(A64) x0=%016lx status32=%d", mReset2Feature, (INT32)(UINT32)mReset2Feature); Line (7, Text);
  UnicodeSPrint (Text, sizeof (Text), L"Last FID=%08x x0=%016lx x1=%016lx", mLastFunction, mLastArgs[0], mLastArgs[1]); Line (8, Text);
  UnicodeSPrint (Text, sizeof (Text), L"x2=%016lx x3=%016lx; all unused input arguments ZERO", mLastArgs[2], mLastArgs[3]); Line (9, Text);
  for (Index = 0; Index < ARRAY_SIZE (mOptions); ++Index) {
    UnicodeSPrint (Text, sizeof (Text), L"%s %s", Index == mSelected ? L">" : L" ", mOptions[Index]); Line (11+Index, Text);
  }
  Line (18, mResult);
  Line (20, L"Volume +/-: select; Power: enter. Manual actions only.");
  Line (21, L"Query first and record the page. No reset occurs on entry.");
}

STATIC BOOLEAN Record (CONST CHAR8 *Step, UINT32 Function, CONST UINT64 *Args)
{
  CHAR8 Text[256];
  UINTN Length;
  Length = AsciiSPrint (Text, sizeof (Text),
    "\n[J716F-V23P7-r1 seq=%08u step=%a fid=%08x x0=%016lx x1=%016lx x2=%016lx x3=%016lx]\n",
    mSequence, Step, Function, Args[0], Args[1], Args[2], Args[3]);
  if (!RingAppend (mRing, (CONST UINT8 *)Text, Length, Barrier)) {
    mResult = L"RAM record refused. No subsequent call made.";
    return FALSE;
  }
  ++mSequence;
  if (RingCount (mRing, (CONST UINT8 *)Text, Length) == 0) {
    mResult = L"RAM record readback failed. No subsequent call made.";
    return FALSE;
  }
  return TRUE;
}

STATIC BOOLEAN Call (UINT32 Function, UINT64 Arg1, CONST CHAR16 *Label)
{
  ZeroMem (mLastArgs, sizeof (mLastArgs));
  mLastArgs[0] = Function;
  mLastArgs[1] = Arg1;
  mLastFunction = Function;
  if (!Record ("PRE_SMC", Function, mLastArgs)) { return FALSE; }
  mResult = Label;
  Render ();
  J716fSmc (mLastArgs);
  // Query returns are normal; reset returns require investigation.
  return Record ("RETURN_SMC", Function, mLastArgs);
}

STATIC VOID Query (VOID)
{
  mQueried = FALSE;
  if (!Call (PSCI_VERSION, 0, L"Calling PSCI_VERSION (SMC, zero unused arguments)...")) { return; }
  mVersion = mLastArgs[0];
  if (!Call (PSCI_FEATURES, PSCI_RESET, L"Calling PSCI_FEATURES(SYSTEM_RESET)...")) { return; }
  mResetFeature = mLastArgs[0];
  if (!Call (PSCI_FEATURES, PSCI_RESET2, L"Calling PSCI_FEATURES(RESET2 AArch64)...")) { return; }
  mReset2Feature = mLastArgs[0];
  mQueried = TRUE;
  mResult = L"Queries returned and recorded. Photograph this page before a reset test.";
}

STATIC VOID FirmwareWarm (VOID)
{
  UINT64 Args[8];
  ZeroMem (Args, sizeof (Args));
  Args[0] = EfiResetWarm;
  Args[1] = mResetPointer;
  if (!Record ("PRE_EFI_WARM", 0, Args)) { return; }
  mResult = L"Calling firmware EFI Warm restart; PRE record committed...";
  Render ();
  gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
  if (Record ("RETURN_EFI_WARM", 0, Args)) {
    mResult = L"EFI Warm returned; no successful reset or retention claim.";
  }
}

EFI_STATUS EFIAPI UefiMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  UINTN Event;
  if (gST->ConIn == NULL) { return EFI_UNSUPPORTED; }
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&mGop);
  if (EFI_ERROR (Status) || mGop->Mode == NULL || mGop->Mode->Info == NULL) { return EFI_UNSUPPORTED; }
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  Inventory ();
  mEntryP7 = RingCount (mRing, mP7, sizeof (mP7)-1);
  mSequence = mEntryP7 + 1;
  for (;;) {
    Render ();
    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Event);
    if (EFI_ERROR (Status)) { return Status; }
    if (EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) { continue; }
    if (Key.ScanCode == SCAN_UP) { mSelected = (mSelected + ARRAY_SIZE (mOptions)-1) % ARRAY_SIZE (mOptions); }
    if (Key.ScanCode == SCAN_DOWN) { mSelected = (mSelected + 1) % ARRAY_SIZE (mOptions); }
    if (Key.ScanCode == SCAN_ESC) { return EFI_SUCCESS; }
    if (Key.UnicodeChar != L'\r') { continue; }
    switch (mSelected) {
      case 0: Inventory (); mResult = L"Inventory refreshed; no RAM writes or monitor calls."; break;
      case 1: Query (); break;
      case 2: FirmwareWarm (); break;
      case 3:
        if (!mQueried || (INT32)(UINT32)mReset2Feature != 0) {
          mResult = L"RESET2 blocked: query first; FEATURES(RESET2) must return 0."; break;
        }
        if (Call (PSCI_RESET2, 0, L"Direct RESET2 type=0 cookie=0; PRE record committed...")) {
          mResult = L"Direct RESET2 RETURNED. Photograph x0..x3; reset not proven.";
        }
        break;
      case 4:
        if (!mQueried || (INT32)(UINT32)mVersion < 2) {
          mResult = L"SYSTEM_RESET blocked: query first; require PSCI version >= 0.2."; break;
        }
        if (Call (PSCI_RESET, 0, L"Direct SYSTEM_RESET; all unused arguments zero; PRE recorded...")) {
          mResult = L"Direct SYSTEM_RESET RETURNED. Photograph x0..x3.";
        }
        break;
      case 5: return EFI_SUCCESS;
    }
  }
}
