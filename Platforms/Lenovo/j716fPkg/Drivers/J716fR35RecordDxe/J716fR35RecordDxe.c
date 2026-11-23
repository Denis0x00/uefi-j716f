// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <PiDxe.h>
#include <Protocol/AdvancedLogger.h>
#include <AdvancedLoggerInternal.h>
#include <AdvancedLoggerInternalProtocol.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/J716fR35CodecLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <J716fR35RecordGuid.h>
#include <J716fR35Epoch.h>

#define R35_MAX_LINES 192U
#define R35_MAX_LINE 512U
#define R35_LOG_BUDGET (R35_MAX_LINES * 552U)
STATIC BOOLEAN mDone;
STATIC R35Record mRecord;
// A fixed snapshot avoids allocating/freeing pool memory while bounding HOB
// reads. Failure/insufficient capacity rejects the export; there is no retry.
STATIC UINT64 mHobMemoryMap[8192];
STATIC_ASSERT (sizeof (R35Record) <= 4096, "R35 RAW export exceeds reviewed bound");

typedef struct {
  ADVANCED_LOGGER_PROTOCOL *Protocol;
  ADVANCED_LOGGER_INFO *Info;
  UINT32 InitialDiscarded;
  UINT32 Lines;
  BOOLEAN Failed;
} R35_LOG;

STATIC BOOLEAN Emit (R35_LOG *Log, CONST CHAR8 *Format, ...)
{
  CHAR8 Buffer[R35_MAX_LINE];
  UINTN Length;
  VA_LIST Args;
  UINT32 Used;
  if (Log->Failed || Log->Lines >= R35_MAX_LINES ||
      Log->Info->DiscardedSize != Log->InitialDiscarded ||
      Log->Info->LogCurrentOffset < Log->Info->LogBufferOffset) {
    Log->Failed = TRUE;
    return FALSE;
  }
  Used = Log->Info->LogCurrentOffset - Log->Info->LogBufferOffset;
  if (Used > Log->Info->LogBufferSize || Log->Info->LogBufferSize - Used < 552U) {
    Log->Failed = TRUE;
    return FALSE;
  }
  VA_START (Args, Format);
  Length = AsciiVSPrint (Buffer, sizeof (Buffer), Format, Args);
  VA_END (Args);
  if (Length == 0 || Length >= sizeof (Buffer) - 1) {
    Log->Failed = TRUE;
    return FALSE;
  }
  // Level zero is memory-only in the pinned Mu logger. No DEBUG/serial redraw.
  Log->Protocol->AdvancedLoggerWriteProtocol (Log->Protocol, 0, Buffer, Length);
  Log->Lines++;
  if (Log->Info->DiscardedSize != Log->InitialDiscarded) Log->Failed = TRUE;
  return !Log->Failed;
}

STATIC BOOLEAN OpenLogger (R35_LOG *Log)
{
  UINT32 Used;
  ZeroMem (Log, sizeof (*Log));
  if (EFI_ERROR (gBS->LocateProtocol (&gAdvancedLoggerProtocolGuid, NULL,
                                    (VOID **)&Log->Protocol)) ||
      Log->Protocol == NULL ||
      Log->Protocol->Signature != ADVANCED_LOGGER_PROTOCOL_SIGNATURE ||
      Log->Protocol->Version != ADVANCED_LOGGER_PROTOCOL_VERSION ||
      Log->Protocol->AdvancedLoggerWriteProtocol == NULL) return FALSE;
  Log->Info = LOGGER_INFO_FROM_PROTOCOL (Log->Protocol);
  if (Log->Info == NULL || Log->Info->Signature != ADVANCED_LOGGER_SIGNATURE ||
      Log->Info->Version != ADVANCED_LOGGER_INFO_VER ||
      Log->Info->LogBufferOffset != EXPECTED_LOG_BUFFER_OFFSET (Log->Info) ||
      Log->Info->LogCurrentOffset < Log->Info->LogBufferOffset) return FALSE;
  Used = Log->Info->LogCurrentOffset - Log->Info->LogBufferOffset;
  if (Used > Log->Info->LogBufferSize ||
      Log->Info->LogBufferSize - Used < R35_LOG_BUDGET) return FALSE;
  Log->InitialDiscarded = Log->Info->DiscardedSize;
  return TRUE;
}

STATIC CONST CHAR8 *HobReadableLimit (IN CONST UINT8 *Hobs, OUT UINTN *Limit)
{
  UINTN MapSize = sizeof (mHobMemoryMap), MapKey, DescriptorSize = 0;
  UINTN Offset, Count, Matches = 0;
  UINT32 Version = 0;
  UINT64 Base = (UINTN)Hobs, Bytes, End, Available = 0;
  EFI_MEMORY_DESCRIPTOR Descriptor;
  EFI_STATUS Status;
  *Limit = 0;
  Status = gBS->GetMemoryMap (&MapSize, (EFI_MEMORY_DESCRIPTOR *)mHobMemoryMap,
                             &MapKey, &DescriptorSize, &Version);
  if (EFI_ERROR (Status)) return "hob-map-service";
  if (MapSize == 0 || MapSize > sizeof (mHobMemoryMap) ||
      Version != EFI_MEMORY_DESCRIPTOR_VERSION ||
      DescriptorSize < sizeof (Descriptor) || (DescriptorSize & 7) ||
      MapSize % DescriptorSize != 0) return "hob-map-format";
  Count = MapSize / DescriptorSize;
  if (Count > 2048) return "hob-map-count";
  for (Offset = 0; Offset < MapSize; Offset += DescriptorSize) {
    CopyMem (&Descriptor, (CONST UINT8 *)mHobMemoryMap + Offset, sizeof (Descriptor));
    if (Descriptor.NumberOfPages == 0 ||
        (Descriptor.PhysicalStart & (EFI_PAGE_SIZE - 1)) ||
        Descriptor.NumberOfPages > MAX_UINT64 / EFI_PAGE_SIZE) return "hob-map-range";
    Bytes = Descriptor.NumberOfPages * EFI_PAGE_SIZE;
    if (Bytes > MAX_UINT64 - Descriptor.PhysicalStart) return "hob-map-range";
    End = Descriptor.PhysicalStart + Bytes;
    if (Base < Descriptor.PhysicalStart || Base >= End) continue;
    if (++Matches != 1) return "hob-map-overlap";
    // Pinned DXE Core relocates the list with AllocateCopyPool. Do not widen
    // this CPU-read permission to MMIO, free RAM, or arbitrary reserved memory.
    if (Descriptor.Type != EfiBootServicesData) return "hob-map-type";
    if ((Descriptor.Attribute & EFI_MEMORY_RP) != 0) return "hob-map-read-protected";
    Available = End - Base;
  }
  if (Matches != 1) return "hob-map-missing";
  *Limit = (UINTN)MIN (Available, (UINT64)SIZE_16MB);
  return NULL;
}

STATIC CONST CHAR8 *FindRecord (OUT R35Record *Record, IN OUT R35_LOG *Log)
{
  CONST UINT8 *Hobs = GetHobList ();
  CONST CHAR8 *Reason;
  UINT64 OldEnd;
  UINTN Limit, Offset, Count = 0;
  UINT16 Type, Length;
  BOOLEAN Found = FALSE;
  if (Hobs == NULL || ((UINTN)Hobs & 7) != 0) return "hob-list-missing-or-unaligned";
  // Do not read even PHIT until the current BootServices memory map has
  // established a complete readable interval for this relocated list.
  Reason = HobReadableLimit (Hobs, &Limit);
  if (Reason != NULL) return Reason;
  if (Limit < sizeof (EFI_HOB_HANDOFF_INFO_TABLE)) return "hob-list-short-phit";
  if (ReadUnaligned16 ((CONST UINT16 *)Hobs) != EFI_HOB_TYPE_HANDOFF ||
      ReadUnaligned16 ((CONST UINT16 *)(Hobs + 2)) != sizeof (EFI_HOB_HANDOFF_INFO_TABLE))
    return "hob-list-header";
  // DXE Core copies PHIT without rebasing its absolute fields. OldEnd is only
  // a diagnostic number; never dereference it or use it as a new-list bound.
  OldEnd = ReadUnaligned64 ((CONST UINT64 *)(Hobs + OFFSET_OF (EFI_HOB_HANDOFF_INFO_TABLE, EfiEndOfHobList)));
  if (!Emit (Log, "J716FR35 HOB base=%016lx bound_bytes=%08x old_end=%016lx bounds_source=memory-map\n",
             (UINT64)(UINTN)Hobs, (UINT32)Limit, OldEnd)) return "hob-log-budget";
  for (Offset = 0; Offset <= Limit && Limit - Offset >= sizeof (EFI_HOB_GENERIC_HEADER); Offset += Length) {
    if (++Count > 4096) return "hob-list-count";
    Type = ReadUnaligned16 ((CONST UINT16 *)(Hobs + Offset));
    Length = ReadUnaligned16 ((CONST UINT16 *)(Hobs + Offset + 2));
    if (Length < sizeof (EFI_HOB_GENERIC_HEADER) || (Length & 7) || Length > Limit - Offset)
      return "hob-length";
    if (ReadUnaligned32 ((CONST UINT32 *)(Hobs + Offset + 4)) != 0) return "hob-reserved";
    if (Type == EFI_HOB_TYPE_END_OF_HOB_LIST) {
      // The descriptor can contain other pool allocations after this END.
      if (Length != sizeof (EFI_HOB_GENERIC_HEADER)) return "hob-end";
      return Found ? NULL : "record-absent";
    }
    if (Type == EFI_HOB_TYPE_GUID_EXTENSION && Length >= sizeof (EFI_HOB_GUID_TYPE) &&
        CompareMem (Hobs + Offset + sizeof (EFI_HOB_GENERIC_HEADER), &gJ716fR35RecordGuid, sizeof (EFI_GUID)) == 0) {
      if (Found) return "record-duplicate";
      if (Length != sizeof (EFI_HOB_GUID_TYPE) + sizeof (*Record)) return "record-size";
      CopyMem (Record, Hobs + Offset + sizeof (EFI_HOB_GUID_TYPE), sizeof (*Record));
      Found = TRUE;
    }
  }
  return "hob-no-end";
}

STATIC VOID ExportContext (R35_LOG *Log, CONST CHAR8 *Phase, CONST R35Context *Cb)
{
  if (Cb->valid32 != R35_CONTEXT_ALL || Cb->valid64 != 3) {
    Emit (Log, "J716FR35 CB phase=%a bank=%u valid32=%08x valid64=%08x incomplete=1 details_in_RAW=1\n",
          Phase, Cb->bank, Cb->valid32, Cb->valid64);
    return;
  }
  Emit (Log, "J716FR35 CB phase=%a bank=%u valid32=%08x valid64=%08x sctlr=%08x actlr=%08x cbar=%08x cba2r=%08x\n",
        Phase, Cb->bank, Cb->valid32, Cb->valid64, Cb->words[R35Sctlr], Cb->words[R35Actlr],
        Cb->words[R35Cbar], Cb->words[R35Cba2r]);
  Emit (Log, "J716FR35 CB phase=%a bank=%u tcr=%08x tcr2=%08x contextidr=%08x mair0=%08x mair1=%08x fsr=%08x tlbstatus=%08x\n",
        Phase, Cb->bank, Cb->words[R35Tcr], Cb->words[R35Tcr2], Cb->words[R35Contextidr],
        Cb->words[R35Mair0], Cb->words[R35Mair1], Cb->words[R35Fsr], Cb->words[R35Tlbstatus]);
  Emit (Log, "J716FR35 CB phase=%a bank=%u ttbr0=%016lx ttbr1=%016lx valid64=%08x\n",
        Phase, Cb->bank, Cb->ttbr[0], Cb->ttbr[1], Cb->valid64);
}

STATIC VOID ExportRecord (R35_LOG *Log)
{
  STATIC CONST CHAR8 Hex[] = "0123456789abcdef";
  CHAR8 Bytes[65];
  CONST UINT8 *Raw = (CONST UINT8 *)&mRecord;
  UINTN Offset, Count, I;
  if (!Emit (Log, "J716FR35 BEGIN version=%u bytes=%u crc=%08x boot_seq=%08x boot_id=%016lx current_SEC_record=1\n",
             mRecord.version, mRecord.bytes, mRecord.crc32, mRecord.boot_sequence, mRecord.boot_id)) return;
  Emit (Log, "J716FR35 NOTE crc_is_corruption_check_only; no_owner_or_TZ_grant; reset_creates_new_SEC_record; respect_field_valid_masks\n");
  Emit (Log, "J716FR35 RESULT status=%u phase=%u mode=%u finished=%u host_stopped=%u disabled=%08x synced=%08x\n",
        mRecord.status, mRecord.phase, mRecord.mode, mRecord.finished, mRecord.host_stopped,
        mRecord.disabled_mask, mRecord.synchronized_mask);
  Emit (Log, "J716FR35 RESULT old_namespace=%u routes_unchanged=%u display_config_unchanged=%u mutation=%u smmu_mutation=%u\n",
        mRecord.old_namespace_captured, mRecord.routes_unchanged, mRecord.display_config_unchanged,
        mRecord.mutation_possible, mRecord.smmu_mutation_possible);
  Emit (Log, "J716FR35 DETACH policy=%u preserve_banks=%08x recorded=%u preserved=%u maintenance_record_valid=%u\n",
        mRecord.detacher_policy, mRecord.preserve_banks, mRecord.post_detacher_recorded,
        mRecord.post_detacher_preserved, mRecord.maintenance_record_valid);
  Emit (Log, "J716FR35 COUNTS read32=%u read64=%u write_attempt=%u write_complete=%u barrier=%u clock=%u operations=%u polls=%u\n",
        mRecord.reads32, mRecord.reads64, mRecord.writes_attempted, mRecord.writes_completed,
        mRecord.barriers, mRecord.clock_queries, mRecord.operations, mRecord.polls);
  Emit (Log, "J716FR35 TIME start=%016lx last=%016lx failed_address=%016lx tz=%u dma_owner=%u global_drain=%u root=%u\n",
        mRecord.started_ticks, mRecord.last_ticks, mRecord.failed_address, mRecord.tz_exclusion_proven,
        mRecord.dma_ownership_granted, mRecord.global_dma_drain_proven, mRecord.root_ready);
  for (I = 0; I < 3 && !Log->Failed; I++) {
    ExportContext (Log, "before", &mRecord.before.context[I]);
    ExportContext (Log, "after", &mRecord.after.context[I]);
  }
  // Complete fixed ABI payload, including actual HCI, all selected stream rows,
  // valid masks and write journal. A decoder requires BEGIN + every RAW + END.
  for (Offset = 0; Offset < sizeof (mRecord) && !Log->Failed; Offset += Count) {
    Count = MIN ((UINTN)32, sizeof (mRecord) - Offset);
    for (I = 0; I < Count; I++) {
      Bytes[2 * I] = Hex[Raw[Offset + I] >> 4];
      Bytes[2 * I + 1] = Hex[Raw[Offset + I] & 15];
    }
    Bytes[2 * Count] = 0;
    Emit (Log, "J716FR35 RAW offset=%04x bytes=%02x data=%a\n", (UINT32)Offset, (UINT32)Count, Bytes);
  }
  if (!Log->Failed)
    Emit (Log, "J716FR35 END bytes=%u crc=%08x boot_seq=%08x complete=1\n",
          mRecord.bytes, mRecord.crc32, mRecord.boot_sequence);
}

STATIC VOID EFIAPI OnReadyToBoot (EFI_EVENT Event, VOID *Context)
{
  R35_LOG Log;
  CONST CHAR8 *Reason;
  UINT32 Sequence;
  (VOID)Context;
  if (mDone) return;
  mDone = TRUE;
  gBS->CloseEvent (Event);
  if (!OpenLogger (&Log)) return;
  ZeroMem (&mRecord, sizeof (mRecord));
  Reason = FindRecord (&mRecord, &Log);
  if (Reason != NULL) { Emit (&Log, "J716FR35 REJECT reason=%a\n", Reason); return; }
  if (!r35_record_validate (&mRecord)) {
    Emit (&Log, "J716FR35 REJECT reason=record-validation\n"); return;
  }
  if (mRecord.tz_exclusion_proven || mRecord.dma_ownership_granted ||
      mRecord.global_dma_drain_proven || mRecord.root_ready) {
    Emit (&Log, "J716FR35 REJECT reason=unsupported-grant-claim\n"); return;
  }
  if (!J716fR35CurrentSequence (&Sequence) || Sequence != mRecord.boot_sequence) {
    Emit (&Log, "J716FR35 REJECT reason=current-boot-epoch\n"); return;
  }
  ExportRecord (&Log);
}

EFI_STATUS EFIAPI J716fR35RecordInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;
  (VOID)ImageHandle;
  (VOID)SystemTable;
  return gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnReadyToBoot, NULL,
                            &gEfiEventReadyToBootGuid, &Event);
}
