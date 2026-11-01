// SPDX-License-Identifier: BSD-2-Clause-Patent
// Experimental compatibility for a second UEFI after the ZUI boot chain.
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/EFIScm.h>

#define J716F_TZAPPS_BASE  0x82400000U
#define J716F_TZAPPS_SIZE  0x03E00000U

STATIC QCOM_SCM_PROTOCOL     *mScm;
STATIC QCOM_SCM_SEND_COMMAND  mOriginalSend;
STATIC EFI_EVENT             mExitEvent;
STATIC BOOLEAN               mBusy;

STATIC
VOID
LogDecision (
  IN CONST CHAR8 *Mode,
  IN EFI_STATUS  QueryStatus,
  IN UINT32      AppId,
  IN EFI_STATUS  Status)
{
  CHAR8  Line[192];
  UINTN  Length;

  Length = AsciiSPrint (Line, sizeof (Line),
             "J716F SCM V7 %a query=%016lx id=%08x status=%016lx\n",
             Mode, (UINT64)QueryStatus, AppId, (UINT64)Status);
  SerialPortWrite ((UINT8 *)Line, Length);
}

STATIC
EFI_STATUS
EFIAPI
J716fSendCommand (
  IN QCOM_SCM_PROTOCOL *This,
  IN AppCmdType        CmdId,
  IN UINT32           *AppId,
  IN OUT VOID         *Req,
  IN UINTN             ReqLen,
  IN OUT VOID         *Rsp,
  IN UINTN             RspLen)
{
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  EFI_STATUS                   Status;
  EFI_STATUS                   QueryStatus;
  UINT32                       Request[2];
  UINT32                       ResidentId;
  // These are the legacy and full Keymaster lookup names. No app is loaded.
  CHAR8                        LegacyName[] = "keymaste";
  CHAR8                        FullName[] = "keymaster";

  if ((This != mScm) || (CmdId != APP_REGION_NOTIFICATION_CMD) || mBusy ||
      (AppId == NULL) || (Req == NULL) || (ReqLen != sizeof (Request)) ||
      (Rsp != NULL) || (RspLen != 0)) {
    return mOriginalSend (This, CmdId, AppId, Req, ReqLen, Rsp, RspLen);
  }

  // Interpret only the exact 32-bit request used by the stock J716F TzDxe.
  CopyMem (Request, Req, sizeof (Request));
  Status = LocateMemoryRegionByName ("TZApps Reserved", &Region);
  if (EFI_ERROR (Status) ||
      (Region.Address != J716F_TZAPPS_BASE) ||
      (Region.Length != J716F_TZAPPS_SIZE) ||
      (Request[0] != J716F_TZAPPS_BASE) ||
      (Request[1] != J716F_TZAPPS_SIZE)) {
    return mOriginalSend (This, CmdId, AppId, Req, ReqLen, Rsp, RspLen);
  }

  mBusy = TRUE;
  ResidentId = 0;
  QueryStatus = mOriginalSend (This, APP_QUERY_APPID_CMD, &ResidentId,
                              LegacyName, AsciiStrLen (LegacyName), NULL, 0);
  if (EFI_ERROR (QueryStatus) || (ResidentId == 0)) {
    ResidentId = 0;
    QueryStatus = mOriginalSend (This, APP_QUERY_APPID_CMD, &ResidentId,
                                FullName, AsciiStrLen (FullName), NULL, 0);
  }

  if (!EFI_ERROR (QueryStatus) && (ResidentId != 0)) {
    // A resident app is evidence of an initialized QSEE app environment.
    // Reusing that environment is a hardware-test candidate; it does not
    // establish that the secure allocator has exactly this region extent.
    Status = EFI_SUCCESS;
    LogDecision ("INHERITED_KEYMASTER", QueryStatus, ResidentId, Status);
  } else {
    // Never turn a failed/unavailable query into a successful registration.
    Status = mOriginalSend (This, CmdId, AppId, Req, ReqLen, Rsp, RspLen);
    LogDecision ("STOCK_REGION_RESULT", QueryStatus, ResidentId, Status);
  }

  mBusy = FALSE;
  return Status;
}

STATIC
VOID
EFIAPI
RestoreScm (
  IN EFI_EVENT Event,
  IN VOID     *Context)
{
  // The adapter has boot-services lifetime. Restore the vendor entry before
  // the OS can call through a pointer into boot-services code.
  if ((mScm != NULL) && (mScm->ScmSendCommand == J716fSendCommand)) {
    mScm->ScmSendCommand = mOriginalSend;
  }
}

EFI_STATUS
EFIAPI
J716fScmCompatEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;

  Status = gBS->LocateProtocol (&gQcomScmProtocolGuid, NULL, (VOID **)&mScm);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // The stock V1.2 table and newer tables share the prefix through SendCommand.
  if ((mScm->Revision < 0x00010002) || (mScm->ScmSendCommand == NULL)) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_NOTIFY, RestoreScm,
                              NULL, &gEfiEventExitBootServicesGuid, &mExitEvent);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mOriginalSend = mScm->ScmSendCommand;
  mScm->ScmSendCommand = J716fSendCommand;
  LogDecision ("ADAPTER_READY", EFI_NOT_STARTED, 0, EFI_SUCCESS);
  return EFI_SUCCESS;
}
