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
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL mBg={15,12,11,0}, mFg={245,243,242,0};
STATIC volatile J716F_RING *mRing=(volatile J716F_RING *)(UINTN)J716F_CONSOLE_BASE;
STATIC UINT32 mEntrySig, mEntryStart, mEntrySize, mEntryCrc, mId, mInitCrc;
STATIC UINT32 mInitSig, mInitStart, mInitSize, mLastCrc;
STATIC TRACE_SCAN mEntryLogical, mEntryRaw;
STATIC BOOLEAN mInitArmed, mResetArmed, mBaseline, mQueried;
STATIC UINT32 mResetCrc;
STATIC UINT64 mVersion, mFeature, mLastReturn;
STATIC UINTN mSelected;
STATIC CONST CHAR16 *mResult=L"Read only. Photograph ENTRY before any action.";
STATIC CONST CHAR16 *mOptions[]={
  L"Refresh (read only)", L"Initialize INVALID header (press twice)",
  L"Append BASELINE marker (once this run)", L"Query PSCI + record (NO RESET)",
  L"Direct SYSTEM_RESET (requires baseline + query)", L"Return to UniXNU menu"
};
STATIC VOID Barrier (VOID) { ArmDataSynchronizationBarrier (); }
#include "Panel.h"

STATIC VOID Render (VOID) {
  CHAR16 Text[140]; UINTN I; TRACE_SCAN Now, Raw;
  Barrier(); Now=TraceScan(mRing,0); Raw=TraceScan(mRing,1);
  mGop->Blt(mGop,&mBg,EfiBltVideoFill,0,0,0,0,
             mGop->Mode->Info->HorizontalResolution,mGop->Mode->Info->VerticalResolution,0);
  Line(0,L"J716F V23P8 RETENTION r1 - no XNU / no automatic writes");
  UnicodeSPrint(Text,sizeof(Text),L"ENTRY sig=%08x next=%08x size=%08x CRC32=%08x",mEntrySig,mEntryStart,mEntrySize,mEntryCrc); Line(1,Text);
  UnicodeSPrint(Text,sizeof(Text),L"ENTRY logical: count=%u id=%08x phase=%u crc=%08x",mEntryLogical.Count,mEntryLogical.Id,mEntryLogical.Phase,mEntryLogical.Crc); Line(2,Text);
  UnicodeSPrint(Text,sizeof(Text),L"ENTRY raw: count=%u id=%08x phase=%u crc=%08x (may be stale)",mEntryRaw.Count,mEntryRaw.Id,mEntryRaw.Phase,mEntryRaw.Crc); Line(3,Text);
  UnicodeSPrint(Text,sizeof(Text),L"NOW %s sig=%08x next=%08x size=%08x",RingValid(mRing)?L"VALID":L"INVALID",mRing->Signature,mRing->Start,mRing->Size); Line(4,Text);
  UnicodeSPrint(Text,sizeof(Text),L"NOW logical: count=%u id=%08x phase=%u crc=%08x",Now.Count,Now.Id,Now.Phase,Now.Crc); Line(5,Text);
  UnicodeSPrint(Text,sizeof(Text),L"NOW raw: count=%u id=%08x phase=%u crc=%08x",Raw.Count,Raw.Id,Raw.Phase,Raw.Crc); Line(6,Text);
  UnicodeSPrint(Text,sizeof(Text),L"THIS RUN id=%08x baseline=%u query=%u last-record-crc=%08x",mId,mBaseline,mQueried,mLastCrc); Line(7,Text);
  UnicodeSPrint(Text,sizeof(Text),L"EL%u PSCI=%016lx FEATURES(reset)=%016lx last-x0=%016lx",(UINT32)(J716fCurrentEl()>>2),mVersion,mFeature,mLastReturn); Line(8,Text);
  for(I=0; I<ARRAY_SIZE(mOptions); ++I) {
    UnicodeSPrint(Text,sizeof(Text),L"%s %s",I==mSelected?L">":L" ",mOptions[I]); Line(10+I,Text);
  }
  Line(17,mResult);
  Line(19,L"After restart: reopen P8; photograph ENTRY BEFORE init/append/query.");
  Line(20,L"Match PRE_RESET phase=3 ID + CRC. Raw alone proves payload bytes only.");
  Line(21,L"Console B00BE000+40000 only. No RESET2, EFI Warm or forced reset.");
}

STATIC BOOLEAN Append (UINT32 Phase, UINT64 Value) {
  UINT8 P[TRACE_BYTES];
  if(mId==0) { mResult=L"Record ID exhausted; no write or call."; return FALSE; }
  TraceMake(P,mId,Phase,(UINT32)Value,(UINT32)(Value>>32),
            mRing->Signature,mRing->Start,mRing->Size);
  if(!RingAppend(mRing,P,sizeof(P),Barrier) || RingCount(mRing,P,sizeof(P))==0) {
    mResult=L"Record refused/readback failed. No subsequent monitor call."; return FALSE;
  }
  mLastCrc=Read32(P+48); return TRUE;
}

STATIC VOID Initialize (VOID) {
  UINT8 P[TRACE_BYTES];
  if(RingValid(mRing)) { mInitArmed=FALSE; mResult=L"Already VALID. Initialization refused; existing logs preserved."; return; }
  if(mId==0) { mResult=L"Record ID exhausted; initialization blocked."; return; }
  if(!mInitArmed) {
    mInitSig=mRing->Signature; mInitStart=mRing->Start; mInitSize=mRing->Size;
    mInitCrc=TraceCrc((volatile UINT8 *)mRing,J716F_CONSOLE_BYTES);
    mInitArmed=TRUE;
    mResult=L"ARMED: photograph ENTRY. Press again to replace INVALID header only.";
    return;
  }
  mInitArmed=FALSE;
  if(mInitSig!=mRing->Signature || mInitStart!=mRing->Start || mInitSize!=mRing->Size ||
     mInitCrc!=TraceCrc((volatile UINT8 *)mRing,J716F_CONSOLE_BYTES)) {
    mResult=L"RAM changed since arming. Initialization cancelled; refresh first."; return;
  }
  if(!TraceInitialize(mRing,Barrier)) { mResult=L"Initialization refused."; return; }
  // Store the pre-initialization header and full-region CRC in the INIT record.
  TraceMake(P,mId,TRACE_INIT,mInitCrc,0,mInitSig,mInitStart,mInitSize);
  if(!RingAppend(mRing,P,sizeof(P),Barrier) || RingCount(mRing,P,sizeof(P))==0) {
    mResult=L"INIT readback failed. No reset permitted."; return;
  }
  mLastCrc=Read32(P+48);
  mResult=L"Header initialized; INIT readback OK. Append BASELINE next.";
}

STATIC BOOLEAN Monitor (UINT32 Fid, UINT32 Arg, UINT32 Pre, UINT32 Ret, UINT64 *Value) {
  UINT64 Args[8]; ZeroMem(Args,sizeof(Args)); Args[0]=Fid; Args[1]=Arg;
  if(!Append(Pre,Fid)) { return FALSE; }
  J716fSmc(Args); *Value=Args[0]; mLastReturn=Args[0];
  return Append(Ret,Args[0]);
}
STATIC VOID Query (VOID) {
  mQueried=FALSE;
  if(!mBaseline || (J716fCurrentEl()>>2)!=1) { mResult=L"Query blocked: append BASELINE first; require EL1."; return; }
  if(!Monitor(0x84000000U,0,TRACE_PRE_VERSION,TRACE_VERSION_RETURN,&mVersion)) { return; }
  if((INT32)(UINT32)mVersion<2) { mResult=L"PSCI version unsupported. Reset blocked."; return; }
  if(!Monitor(0x8400000aU,0x84000009U,TRACE_PRE_FEATURE,TRACE_FEATURE_RETURN,&mFeature)) { return; }
  mQueried=(mFeature==0);
  mResult=mQueried?L"Query recorded. Photograph baseline; then select SYSTEM_RESET.":L"FEATURES query failed. Reset blocked.";
}
STATIC VOID Reset (VOID) {
  UINT64 Args[8]; TRACE_SCAN Last;
  if(!mBaseline || !mQueried || (J716fCurrentEl()>>2)!=1) {
    mResult=L"Reset blocked: BASELINE + successful Query required this invocation."; return;
  }
  if(!mResetArmed) {
    if(!Append(TRACE_PRE_RESET,0x84000009U)) { return; }
    mResetCrc=mLastCrc; mResetArmed=TRUE;
    mResult=L"PRE_RESET ready. Photograph ID/CRC, then press again to reset.";
    return;
  }
  mResetArmed=FALSE; Last=TraceScan(mRing,0);
  if(!RingValid(mRing) || Last.Id!=mId || Last.Phase!=TRACE_PRE_RESET || Last.Crc!=mResetCrc) {
    mResult=L"PRE_RESET marker changed. Reset blocked."; return;
  }
  // The exact phase-3 ID/CRC shown here must be present on the next ENTRY.
  mQueried=FALSE;
  mResult=L"PRE_RESET committed. Calling SYSTEM_RESET with zero unused inputs...";
  Render(); Barrier(); ZeroMem(Args,sizeof(Args)); Args[0]=0x84000009U;
  J716fSmc(Args); mLastReturn=Args[0];
  if(Append(TRACE_RESET_RETURN,Args[0])) { mResult=L"SYSTEM_RESET RETURNED. Photograph x0. Retention not proven."; }
}
EFI_STATUS EFIAPI UefiMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status; EFI_INPUT_KEY Key; UINTN Event;
  if(gST->ConIn==NULL) { return EFI_UNSUPPORTED; }
  Status=gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID **)&mGop);
  if(EFI_ERROR(Status) || mGop->Mode==NULL || mGop->Mode->Info==NULL) { return EFI_UNSUPPORTED; }
  gBS->SetWatchdogTimer(0,0,0,NULL); Barrier();
  mEntrySig=mRing->Signature; mEntryStart=mRing->Start; mEntrySize=mRing->Size;
  mEntryCrc=TraceCrc((volatile UINT8 *)mRing,J716F_CONSOLE_BYTES);
  mEntryLogical=TraceScan(mRing,0); mEntryRaw=TraceScan(mRing,1);
  mId=MAX(mEntryLogical.Id,mEntryRaw.Id)+1U;
  for(;;) {
    Render();
    Status=gBS->WaitForEvent(1,&gST->ConIn->WaitForKey,&Event);
    if(EFI_ERROR(Status)) { return Status; }
    if(EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn,&Key))) { continue; }
    if(Key.ScanCode==SCAN_UP || Key.ScanCode==SCAN_DOWN) {
      mInitArmed=FALSE;
      mResetArmed=FALSE;
      mSelected=(mSelected+(Key.ScanCode==SCAN_UP?ARRAY_SIZE(mOptions)-1:1))%ARRAY_SIZE(mOptions);
    }
    if(Key.ScanCode==SCAN_ESC) { return EFI_SUCCESS; }
    if(Key.UnicodeChar!=L'\r') { continue; }
    if(mSelected!=1) { mInitArmed=FALSE; }
    if(mSelected!=4) { mResetArmed=FALSE; }
    switch(mSelected) {
      case 0: mResult=L"Refreshed; frozen ENTRY preserved. No RAM writes or SMC."; break;
      case 1: Initialize(); break;
      case 2:
        if(mBaseline) { mResult=L"Already appended BASELINE this run."; break; }
        if(Append(TRACE_BASELINE,0)) { mBaseline=TRUE; mResult=L"BASELINE readback OK. Photograph ID/CRC, then Query."; }
        break;
      case 3: Query(); break;
      case 4: Reset(); break;
      case 5: return EFI_SUCCESS;
    }
  }
}
