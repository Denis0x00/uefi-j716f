// SPDX-License-Identifier: BSD-2-Clause-Patent
// J716F PM8150 keys. Portable observer/filter logic derived from uefi-cas,
// a491522d8f5fecd57be706df1e42e8bd3ca79b20. No cas reserved RAM is used.
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleTextIn.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "J716fInput.h"

#define QUEUE_SIZE 16
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL mInput;
STATIC EFI_EVENT mPollEvent, mExitEvent;
STATIC EFI_INPUT_KEY mQueue[QUEUE_SIZE];
STATIC UINTN mHead, mCount;
STATIC J716F_KEY_STATE mKeys;
STATIC J716F_SPMI_STATE mSpmi;
STATIC UINT32 mFailures, mLogs;
STATIC BOOLEAN mStopped;
STATIC struct { VENDOR_DEVICE_PATH Vendor; EFI_DEVICE_PATH_PROTOCOL End; } mPath = {
  {{HARDWARE_DEVICE_PATH, HW_VENDOR_DP, {sizeof (VENDOR_DEVICE_PATH), 0}},
   {0x846513b2,0x4287,0x4055,{0xa6,0x8f,0xa8,0x8e,0x86,0x07,0xc6,0x25}}},
  {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {4, 0}}
};
STATIC J716F_INPUT_U32 Read32 (VOID *Context, J716F_INPUT_U64 Address) { return MmioRead32 ((UINTN)Address); }
STATIC VOID Write32 (VOID *Context, J716F_INPUT_U64 Address, J716F_INPUT_U32 Value) {
  MmioWrite32 ((UINTN)Address, Value); MemoryFence ();
}
STATIC VOID DelayUs (VOID *Context, J716F_INPUT_U32 Us) { MicroSecondDelay (Us); }
STATIC J716F_SPMI_BUS mBus = { Read32, Write32, DelayUs, NULL };

STATIC UINT32 Sample (UINT32 *Raw) {
  UINT32 Pon, Gpio, Result;
  *Raw = MAX_UINT32;
  Result = J716fSpmiReadByte (&mBus, &mSpmi, J716F_PON_RT_STS, &Pon);
  if (Result == J716fSpmiOk) { Result = J716fSpmiReadByte (&mBus, &mSpmi, J716F_GPIO6_RT_STS, &Gpio); }
  if (Result == J716fSpmiOk) { *Raw = J716fKeySample (Pon, Gpio); }
  return Result;
}
STATIC VOID Push (UINT32 Event) {
  EFI_INPUT_KEY Key = {0, 0};
  if (Event == J716fKeyFlush) { mHead = 0; mCount = 0; return; }
  if (Event == J716fKeyUp) { Key.ScanCode = SCAN_UP; }
  else if (Event == J716fKeyDown) { Key.ScanCode = SCAN_DOWN; }
  else if (Event == J716fKeyEscape) { Key.ScanCode = SCAN_ESC; }
  else if (Event == J716fKeyEnter) { Key.UnicodeChar = L'\r'; }
  else { return; }
  if (mCount == QUEUE_SIZE) { return; }
  mQueue[(mHead + mCount) % QUEUE_SIZE] = Key; ++mCount;
  gBS->SignalEvent (mInput.WaitForKey);
}
STATIC VOID EFIAPI Poll (EFI_EVENT Event, VOID *Context) {
  UINT32 Raw, Result;
  UINT64 Now;
  if (mStopped) { return; }
  Now = GetTimeInNanoSecond (GetPerformanceCounter ()) / 1000000;
  Result = Sample (&Raw);
  if (Result != J716fSpmiOk) {
    if (++mFailures == 1 && mLogs++ < 8) {
      DEBUG ((DEBUG_ERROR, "J716F V9 key read failed: %u status=%x\n", Result, mSpmi.LastStatus));
    }
    if (mFailures == 5) { gBS->SetTimer (mPollEvent, TimerPeriodic, 1000000); }
  } else if (mFailures) {
    mFailures = 0; gBS->SetTimer (mPollEvent, TimerPeriodic, J716F_KEY_POLL_100NS);
  }
  // Failed samples flush the queue and require a clean release before rearming.
  Push (J716fKeysUpdate (&mKeys, Raw, Now));
}
STATIC EFI_STATUS EFIAPI Reset (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, BOOLEAN ExtendedVerification) {
  EFI_TPL Tpl;
  if (This != &mInput) { return EFI_INVALID_PARAMETER; }
  Tpl = gBS->RaiseTPL (TPL_NOTIFY);
  mHead = 0; mCount = 0; ZeroMem (&mKeys, sizeof (mKeys));
  gBS->RestoreTPL (Tpl);
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI ReadKey (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key) {
  EFI_TPL Tpl;
  if (This != &mInput || Key == NULL) { return EFI_INVALID_PARAMETER; }
  Tpl = gBS->RaiseTPL (TPL_NOTIFY);
  if (!mCount) { gBS->RestoreTPL (Tpl); return mFailures ? EFI_DEVICE_ERROR : EFI_NOT_READY; }
  *Key = mQueue[mHead]; mHead = (mHead + 1) % QUEUE_SIZE; --mCount;
  gBS->RestoreTPL (Tpl);
  return EFI_SUCCESS;
}
STATIC VOID EFIAPI Wait (EFI_EVENT Event, VOID *Context) {
  if (mCount) { gBS->SignalEvent (Event); }
}
STATIC VOID EFIAPI Stop (EFI_EVENT Event, VOID *Context) {
  mStopped = TRUE;
  if (mPollEvent != NULL) { gBS->SetTimer (mPollEvent, TimerCancel, 0); }
}
EFI_STATUS EFIAPI J716fKeysInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;
  UINT32 Result, Raw;
  Result = J716fSpmiInitialize (&mBus, &mSpmi);
  DEBUG ((DEBUG_ERROR, "J716F V9 SPMI=%08x init=%u PON=%u GPIO6=%u\n", mSpmi.Version, Result, mSpmi.PonApid, mSpmi.GpioApid));
  if (Result != J716fSpmiOk) { return EFI_UNSUPPORTED; }
  if (Sample (&Raw) != J716fSpmiOk) { return EFI_DEVICE_ERROR; }
  J716fKeysUpdate (&mKeys, Raw, GetTimeInNanoSecond (GetPerformanceCounter ()) / 1000000);
  mInput.Reset = Reset; mInput.ReadKeyStroke = ReadKey;
  Status = gBS->CreateEvent (EVT_NOTIFY_WAIT, TPL_NOTIFY, Wait, NULL, &mInput.WaitForKey);
  if (EFI_ERROR (Status)) { return Status; }
  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_NOTIFY, Poll, NULL, &mPollEvent);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_NOTIFY, Stop, NULL, &gEfiEventExitBootServicesGuid, &mExitEvent);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->SetTimer (mPollEvent, TimerPeriodic, J716F_KEY_POLL_100NS);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiDevicePathProtocolGuid, &mPath,
                                                   &gEfiSimpleTextInProtocolGuid, &mInput, NULL);
  if (!EFI_ERROR (Status)) { return EFI_SUCCESS; }
Failure:
  if (mExitEvent != NULL) { gBS->CloseEvent (mExitEvent); }
  if (mPollEvent != NULL) { gBS->CloseEvent (mPollEvent); }
  gBS->CloseEvent (mInput.WaitForKey);
  return Status;
}
