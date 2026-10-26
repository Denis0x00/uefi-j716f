// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/ArmLib.h>
#include "Trace.h"
extern CONST UINT8 gCaFontGlyphs[95][19];
VOID J716fSmc (UINT64 *Args);
UINT64 J716fCurrentEl (VOID);
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL mBg={15,12,11,0},mFg={245,243,242,0};
STATIC volatile J716F_RING *mRing=(volatile J716F_RING *)(UINTN)J716F_CONSOLE_BASE;
STATIC UINT32 mEntrySig,mEntryStart,mEntrySize,mEntryCrc,mEntryId,mId,mLastCrc;
STATIC TRACE_SCAN mEntryLogical,mEntryRaw;
STATIC TRACE_DETAIL mLogical[6],mRaw[6];
STATIC UINT64 mVersion,mFeature;
STATIC BOOLEAN mQueried,mArmed;
STATIC UINTN mSelected;
STATIC CONST CHAR16 *mResult=L"Read only. Photograph ENTRY before Query or ARM.";
STATIC CONST CHAR16 *mOptions[]={L"Refresh (read only)",L"Query PSCI + record (NO RESET)",
  L"ARM next kernel attempt (requires Query)",L"Return to menu (then select kernel E or M)"};
STATIC CONST CHAR16 *mNames[]={L"ARM",L"KERNEL_ENTRY",L"PRE_MMU",L"POST_MMU",L"MAP_FAILURE",L"RESET_RETURN"};
STATIC VOID Barrier (VOID) { ArmDataSynchronizationBarrier(); }
#include "Panel.h"

STATIC VOID Render (VOID) {
  CHAR16 T[160]; UINTN I; TRACE_SCAN Now,Raw;
  Barrier(); Now=TraceScan(mRing,0); Raw=TraceScan(mRing,1);
  mGop->Blt(mGop,&mBg,EfiBltVideoFill,0,0,0,0,
    mGop->Mode->Info->HorizontalResolution,mGop->Mode->Info->VerticalResolution,0);
  Line(0,L"J716F V23P9 KERNEL TRACE r1 - ENTRY frozen / no automatic writes");
  UnicodeSPrint(T,sizeof(T),L"ENTRY sig=%08x next=%08x size=%08x CRC32=%08x",mEntrySig,mEntryStart,mEntrySize,mEntryCrc); Line(1,T);
  UnicodeSPrint(T,sizeof(T),L"ENTRY logical count=%u id=%08x phase=%u crc=%08x",mEntryLogical.Count,mEntryLogical.Id,mEntryLogical.Phase,mEntryLogical.Crc); Line(2,T);
  UnicodeSPrint(T,sizeof(T),L"ENTRY raw count=%u id=%08x phase=%u crc=%08x (may be stale)",mEntryRaw.Count,mEntryRaw.Id,mEntryRaw.Phase,mEntryRaw.Crc); Line(3,T);
  UnicodeSPrint(T,sizeof(T),L"NOW %s next=%08x size=%08x logical=%u raw=%u",RingValid(mRing)?L"VALID":L"INVALID",mRing->Start,mRing->Size,Now.Count,Raw.Count); Line(4,T);
  UnicodeSPrint(T,sizeof(T),L"FROZEN checkpoint ID=%08x; rows show logical/raw hits, CRC and value",mEntryId); Line(5,T);
  for(I=0; I<6; ++I) {
    UnicodeSPrint(T,sizeof(T),L"%s L=%u R=%u crc=%08x/%08x val=%016lx/%016lx",mNames[I],mLogical[I].Count,mRaw[I].Count,mLogical[I].Crc,mRaw[I].Crc,mLogical[I].Value,mRaw[I].Value); Line(6+I,T);
  }
  UnicodeSPrint(T,sizeof(T),L"THIS RUN id=%08x query=%u armed=%u crc=%08x EL%u PSCI=%08x FEATURES=%016lx",mId,mQueried,mArmed,mLastCrc,(UINT32)(J716fCurrentEl()>>2),(UINT32)mVersion,mFeature); Line(12,T);
  for(I=0; I<ARRAY_SIZE(mOptions); ++I) {
    UnicodeSPrint(T,sizeof(T),L"%s %s",I==mSelected?L">":L" ",mOptions[I]); Line(14+I,T);
  }
  Line(19,mResult);
  Line(20,L"After reset: open reader FIRST, photograph ENTRY. Do not Query/ARM.");
  Line(21,L"E: entry+reset. M: entry/pre/post+reset. Missing row alone is not a PC.");
}
STATIC BOOLEAN Append (UINT32 Phase,UINT64 Value) {
  UINT8 P[TRACE_BYTES];
  if(!mId || !RingValid(mRing)) { mResult=L"Invalid header or exhausted ID. No write/call. Use preserved P8 if needed."; return FALSE; }
  TraceMake(P,mId,Phase,(UINT32)Value,(UINT32)(Value>>32),mRing->Signature,mRing->Start,mRing->Size);
  if(!RingAppend(mRing,P,sizeof(P),Barrier) || !RingCount(mRing,P,sizeof(P))) {
    mResult=L"Record readback failed. No subsequent call or ARM."; return FALSE;
  }
  mLastCrc=Read32(P+48); return TRUE;
}
STATIC BOOLEAN Monitor (UINT32 Fid,UINT32 Arg,UINT32 Pre,UINT32 Ret,UINT64 *Value) {
  UINT64 A[8]; ZeroMem(A,sizeof(A)); A[0]=Fid; A[1]=Arg;
  if(!Append(Pre,Fid)) { return FALSE; }
  J716fSmc(A); *Value=A[0]; return Append(Ret,A[0]);
}
STATIC VOID Query (VOID) {
  mQueried=FALSE;
  if(mArmed) { mResult=L"Already ARMED. Return to menu; launch kernel without more writes."; return; }
  if((J716fCurrentEl()>>2)!=1) { mResult=L"EL1 required. No SMC or ARM."; return; }
  if(!Monitor(0x84000000U,0,TRACE_PRE_VERSION,TRACE_VERSION_RETURN,&mVersion)) { return; }
  if((INT32)(UINT32)mVersion<2) { mResult=L"Unsupported PSCI version. ARM blocked."; return; }
  if(!Monitor(0x8400000aU,0x84000009U,TRACE_PRE_FEATURE,TRACE_FEATURE_RETURN,&mFeature)) { return; }
  mQueried=(mFeature==0);
  mResult=mQueried?L"Query OK. Select ARM, photograph ID/CRC, then return to menu.":L"FEATURES failed. ARM blocked.";
}
STATIC VOID Arm (VOID) {
  if(mArmed) { mResult=L"Already ARMED. No duplicate write. Return to menu and select kernel."; return; }
  if(!mQueried) { mResult=L"ARM blocked: successful Query required this invocation."; return; }
  if(Append(TRACE_ARM_KERNEL,TRACE_ARM_COOKIE)) {
    mArmed=TRUE; mResult=L"ARM readback OK. Photograph ID/CRC; return to menu and start kernel.";
  }
}
EFI_STATUS EFIAPI UefiMain (EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS S; EFI_INPUT_KEY Key; UINTN Event,I;
  if(gST->ConIn==NULL) { return EFI_UNSUPPORTED; }
  S=gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID **)&mGop);
  if(EFI_ERROR(S) || mGop->Mode==NULL || mGop->Mode->Info==NULL) { return EFI_UNSUPPORTED; }
  gBS->SetWatchdogTimer(0,0,0,NULL); Barrier();
  mEntrySig=mRing->Signature; mEntryStart=mRing->Start; mEntrySize=mRing->Size;
  mEntryCrc=TraceCrc((volatile UINT8 *)mRing,J716F_CONSOLE_BYTES);
  mEntryLogical=TraceScan(mRing,0); mEntryRaw=TraceScan(mRing,1);
  mEntryId=MAX(mEntryLogical.Id,mEntryRaw.Id); mId=mEntryId+1U;
  for(I=0; I<6; ++I) {
    mLogical[I]=TraceFind(mRing,0,mEntryId,TRACE_ARM_KERNEL+(UINT32)I);
    mRaw[I]=TraceFind(mRing,1,mEntryId,TRACE_ARM_KERNEL+(UINT32)I);
  }
  for(;;) {
    Render(); S=gBS->WaitForEvent(1,&gST->ConIn->WaitForKey,&Event);
    if(EFI_ERROR(S)) { return S; }
    if(EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn,&Key))) { continue; }
    if(Key.ScanCode==SCAN_UP || Key.ScanCode==SCAN_DOWN) {
      mSelected=(mSelected+(Key.ScanCode==SCAN_UP?ARRAY_SIZE(mOptions)-1:1))%ARRAY_SIZE(mOptions);
    }
    if(Key.ScanCode==SCAN_ESC) { return EFI_SUCCESS; }
    if(Key.UnicodeChar!=L'\r') { continue; }
    switch(mSelected) {
      case 0: mResult=L"Refreshed. Frozen ENTRY unchanged. No write/SMC."; break;
      case 1: Query(); break;
      case 2: Arm(); break;
      case 3: return EFI_SUCCESS;
    }
  }
}
