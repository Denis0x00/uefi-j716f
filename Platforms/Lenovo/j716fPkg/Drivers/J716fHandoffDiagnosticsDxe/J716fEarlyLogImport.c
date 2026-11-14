// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <PiDxe.h>
#include <Protocol/AdvancedLogger.h>
#include <AdvancedLoggerInternal.h>
#include <AdvancedLoggerInternalProtocol.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <J716fEarlyLogHob.h>
#include <J716fEarlyLogRegion.h>
#include <J716fEarlyLogText.h>

STATIC BOOLEAN mImported;
typedef struct {
  ADVANCED_LOGGER_PROTOCOL *Protocol;
  ADVANCED_LOGGER_INFO *Info;
  UINT32 InitialDiscarded;
} IMPORT_CONTEXT;

STATIC int ImportText (void *Context, const char *Bytes, J716F_EL_U32 Length)
{
  IMPORT_CONTEXT *Import = (IMPORT_CONTEXT *)Context;
  if (Length == 0 || Length > 512 || Import->Info->DiscardedSize != Import->InitialDiscarded) return 1;
  // Level zero is memory-only in the pinned Mu protocol. Never DEBUG()/serial.
  Import->Protocol->AdvancedLoggerWriteProtocol (Import->Protocol, 0, Bytes, Length);
  return Import->Info->DiscardedSize != Import->InitialDiscarded;
}

VOID J716fImportPreviousEarlyLog (VOID)
{
  CONST VOID *Hob;
  CONST J716F_EARLY_LOG_HOB *Snapshot;
  CONST volatile J716F_EL_U8 *Current = (CONST volatile J716F_EL_U8 *)(UINTN)J716F_EL_BASE;
  J716F_EL_HEADER Header;
  ADVANCED_LOGGER_PROTOCOL *Protocol;
  ADVANCED_LOGGER_INFO *Info;
  IMPORT_CONTEXT Context;
  UINT32 ExpectedSequence;
  UINT32 Used;
  EFI_STATUS Status;
  if (mImported) return;
  mImported = TRUE;
  Hob = GetFirstGuidHob (&gJ716fEarlyLogSnapshotGuid);
  if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (J716F_EARLY_LOG_HOB)) return;
  Snapshot = GET_GUID_HOB_DATA (Hob);
  if (Snapshot->Magic != J716F_EL_HOB_MAGIC || Snapshot->Version != 1 ||
      Snapshot->SnapshotBytes != J716F_EL_TOTAL_BYTES ||
      Snapshot->SnapshotCrc != J716fEarlyLogCrc32 (Snapshot->Snapshot, J716F_EL_TOTAL_BYTES) ||
      J716fEarlyLogHeaderValidate (Snapshot->Snapshot, J716F_EL_TOTAL_BYTES, &Header) != J716F_EL_OK) return;
  ExpectedSequence = Header.boot_sequence + 1;
  if (ExpectedSequence == 0) ExpectedSequence = 1;
  if (!J716fEarlyLogRegionValid () ||
      J716fEarlyLogGet32 (Current) != J716F_EL_JLOG_MAGIC ||
      J716fEarlyLogGet32 (Current + 8) != J716F_EL_JLOG_CAPACITY ||
      J716fEarlyLogGet32 (Current + 4) > J716F_EL_JLOG_CAPACITY ||
      J716fEarlyLogGet32 (Current + 12) != ExpectedSequence) return;
  Status = gBS->LocateProtocol (&gAdvancedLoggerProtocolGuid, NULL, (VOID **)&Protocol);
  if (EFI_ERROR (Status) || Protocol == NULL ||
      Protocol->Signature != ADVANCED_LOGGER_PROTOCOL_SIGNATURE ||
      Protocol->Version != ADVANCED_LOGGER_PROTOCOL_VERSION ||
      Protocol->AdvancedLoggerWriteProtocol == NULL) return;
  Info = LOGGER_INFO_FROM_PROTOCOL (Protocol);
  if (Info == NULL || Info->Signature != ADVANCED_LOGGER_SIGNATURE ||
      Info->Version != ADVANCED_LOGGER_INFO_VER ||
      Info->LogBufferOffset != EXPECTED_LOG_BUFFER_OFFSET (Info) ||
      Info->LogCurrentOffset < Info->LogBufferOffset) return;
  Used = Info->LogCurrentOffset - Info->LogBufferOffset;
  // At most 243 records, each <=512 bytes plus Mu record header/alignment.
  if (Used > Info->LogBufferSize || Info->LogBufferSize - Used < 243 * 552) return;
  Context.Protocol = Protocol;
  Context.Info = Info;
  Context.InitialDiscarded = Info->DiscardedSize;
  (VOID)J716fEarlyLogExportText (Snapshot->Snapshot, J716F_EL_TOTAL_BYTES, ImportText, &Context);
}
