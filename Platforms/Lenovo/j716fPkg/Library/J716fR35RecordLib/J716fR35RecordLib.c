// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/TimerLib.h>
#include <Library/J716fR35RecordLib.h>
#include <J716fR35RecordGuid.h>
#include <J716fR35Epoch.h>
#include <J716fR35RamSlot.h>

STATIC R35Record mRecord;
STATIC UINT32 mSequence;
STATIC UINT64 mBootId;
STATIC BOOLEAN mReady, mStageAttempted, mStaged, mPublishAttempted;
STATIC_ASSERT (sizeof (R35Record) <= J716F_R35_RAM_BYTES, "R35 diagnostic record exceeds 4 KiB RAM handoff slot");
STATIC_ASSERT ((sizeof (R35Record) & 3U) == 0, "R35 record size must be 32-bit word aligned");
STATIC_ASSERT (sizeof (R35Record) + sizeof (EFI_HOB_GUID_TYPE) < 0xFFF8,
               "R35 record exceeds GUID HOB length field");

STATIC VOID ClearRamHandoffSlot (VOID)
{
  volatile UINT32 *Slot;
  UINTN Index;
  if (!J716fEarlyLogRegionValid ()) return;
  Slot = (volatile UINT32 *)(UINTN)J716F_R35_RAM_BASE;
  Slot[0] = 0;
  MemoryFence ();
  for (Index = 1; Index < J716F_R35_RAM_BYTES / sizeof (UINT32); Index++) {
    Slot[Index] = 0;
  }
  MemoryFence ();
}

STATIC BOOLEAN PublishRamHandoffSlot (IN CONST R35Record *Record)
{
  volatile UINT32 *Slot;
  CONST UINT32 *Words;
  R35Record Verify;
  UINTN Index;
  UINTN WordCount = sizeof (R35Record) / sizeof (UINT32);
  if (!J716fEarlyLogRegionValid () || Record == NULL) return FALSE;
  Slot = (volatile UINT32 *)(UINTN)J716F_R35_RAM_BASE;
  Words = (CONST UINT32 *)Record;
  // Invalidate magic first, write body and zero-pad remainder, fence, then publish magic last.
  Slot[0] = 0;
  MemoryFence ();
  for (Index = 1; Index < WordCount; Index++) {
    Slot[Index] = Words[Index];
  }
  for (Index = WordCount; Index < J716F_R35_RAM_BYTES / sizeof (UINT32); Index++) {
    Slot[Index] = 0;
  }
  MemoryFence ();
  Slot[0] = Words[0];
  MemoryFence ();
  // Read back into aligned local buffer and verify complete record + CRC.
  ZeroMem (&Verify, sizeof (Verify));
  for (Index = 0; Index < WordCount; Index++) {
    ((UINT32 *)&Verify)[Index] = Slot[Index];
  }
  MemoryFence ();
  if (!r35_record_validate (&Verify) || CompareMem (&Verify, Record, sizeof (R35Record)) != 0) {
    ClearRamHandoffSlot ();
    return FALSE;
  }
  return TRUE;
}

VOID EFIAPI J716fR35RecordReset (VOID)
{
  // SEC may precede constructors. Always initialize the complete lifetime state
  // and clear any prior-boot record from the reserved RAM handoff slot.
  mReady = FALSE;
  mStageAttempted = mStaged = mPublishAttempted = FALSE;
  mSequence = 0;
  mBootId = 0;
  ZeroMem (&mRecord, sizeof (mRecord));
  ClearRamHandoffSlot ();
  mReady = J716fR35CurrentSequence (&mSequence);
  if (mReady) {
    // No entropy/security claim. Fresh SEC execution and a fresh HOB list bind
    // this observation to the boot; this value only correlates its log records.
    mBootId = GetPerformanceCounter () ^ LShiftU64 ((UINT64)mSequence, 32) ^ R35_PROFILE;
    if (mBootId == 0) mReady = FALSE;
  }
}

BOOLEAN EFIAPI J716fR35GetCurrentBootIdentity (OUT UINT64 *BootId, OUT UINT32 *Sequence)
{
  UINT32 Current;
  if (BootId == NULL || Sequence == NULL) return FALSE;
  *BootId = 0;
  *Sequence = 0;
  if (!mReady || mStageAttempted || mPublishAttempted ||
      !J716fR35CurrentSequence (&Current) || Current != mSequence) return FALSE;
  *BootId = mBootId;
  *Sequence = mSequence;
  return TRUE;
}

EFI_STATUS EFIAPI J716fR35RecordStage (IN CONST R35Record *Record)
{
  UINT32 Current;
  if (!mReady || mStageAttempted || mPublishAttempted) return EFI_NOT_READY;
  mStageAttempted = TRUE;
  if (Record == NULL || ((UINTN)Record & 7) != 0) return EFI_INVALID_PARAMETER;
  if (!J716fR35CurrentSequence (&Current) || Current != mSequence ||
      Record->boot_sequence != Current || Record->boot_id != mBootId || !r35_record_validate (Record) ||
      Record->tz_exclusion_proven || Record->dma_ownership_granted ||
      Record->global_dma_drain_proven || Record->root_ready) return EFI_COMPROMISED_DATA;
  CopyMem (&mRecord, Record, sizeof (mRecord));
  MemoryFence ();
  if (!r35_record_validate (&mRecord)) {
    ZeroMem (&mRecord, sizeof (mRecord));
    return EFI_COMPROMISED_DATA;
  }
  if (!PublishRamHandoffSlot (&mRecord)) {
    ZeroMem (&mRecord, sizeof (mRecord));
    return EFI_DEVICE_ERROR;
  }
  mStaged = TRUE;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI J716fR35RecordPublish (VOID)
{
  VOID *HobData;
  UINT32 Current;
  if (!mReady || mPublishAttempted) return EFI_NOT_READY;
  mPublishAttempted = TRUE;
  if (!mStaged || !J716fR35CurrentSequence (&Current) || Current != mSequence ||
      mRecord.boot_sequence != Current || mRecord.boot_id != mBootId || !r35_record_validate (&mRecord)) {
    ZeroMem (&mRecord, sizeof (mRecord));
    mStaged = FALSE;
    return EFI_COMPROMISED_DATA;
  }
  // Detect accidental second producers. Never choose the first/last of duplicates.
  if (GetFirstGuidHob (&gJ716fR35RecordGuid) != NULL) {
    ZeroMem (&mRecord, sizeof (mRecord));
    mStaged = FALSE;
    return EFI_ALREADY_STARTED;
  }
  HobData = BuildGuidDataHob (&gJ716fR35RecordGuid, &mRecord, sizeof (mRecord));
  ZeroMem (&mRecord, sizeof (mRecord));
  mStaged = FALSE;
  return HobData != NULL ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
}
