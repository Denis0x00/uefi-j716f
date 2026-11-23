// SPDX-License-Identifier: BSD-2-Clause-Patent
// Board adapter, invoked exactly once from Kona PlatformInitialize before detach.
#include <Uefi.h>
#include <Library/ArmLib.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/ArmSmmuDetachLib.h>
#include <Library/J716fR35RecordLib.h>

STATIC R35Workspace mWork;
STATIC BOOLEAN mLive;
STATIC UINT32 mAccesses;
STATIC BOOLEAN mAttempted;

STATIC int Live (void *Context)
{
  (VOID)Context;
  return mLive && mAccesses < 20000;
}
STATIC BOOLEAN Read32Allowed (UINT64 Address)
{
  UINT32 I, Bank;
  for (I=0; I<R35_HCI_WORDS; ++I) if (Address==r35_hci_address(I)) return TRUE;
  if (Address==R35_SMMU_BASE || Address==R35_SMMU_BASE+0x20 ||
      Address==R35_SMMU_BASE+0x24 || Address==R35_SMMU_BASE+0x28) return TRUE;
  if ((Address&3)==0 && ((Address>=R35_SMMU_BASE+0x800 && Address<R35_SMMU_BASE+0x800+4*R35_GROUPS) ||
      (Address>=R35_SMMU_BASE+0xc00 && Address<R35_SMMU_BASE+0xc00+4*R35_GROUPS))) return TRUE;
  for (Bank=2; Bank<=4; ++Bank) for (I=0; I<R35_CONTEXT_WORDS; ++I)
    if (Address==r35_context_address(Bank,I)) return TRUE;
  return FALSE;
}
STATIC int Read32 (void *Context, R35U64 Address, R35U32 *Value)
{
  if (!Value || !Live(Context) || !Read32Allowed(Address)) return 0;
  ++mAccesses; *Value=MmioRead32((UINTN)Address); return 1;
}
STATIC int Read64 (void *Context, R35U64 Address, R35U64 *Value)
{
  UINT32 Bank;
  if (!Value || !Live(Context)) return 0;
  for (Bank=2; Bank<=4; ++Bank) {
    UINT64 Base=R35_SMMU_BASE+0x80000+((UINT64)Bank<<12);
    if (Address==Base+0x20 || Address==Base+0x28) {
      ++mAccesses; *Value=MmioRead64((UINTN)Address); return 1;
    }
  }
  return 0;
}
STATIC int Permit (void *Context, CONST R35Record *Record, R35U64 Address, R35U32 Value)
{
  UINT32 Bank;
  if (!Live(Context) || Record!=&mWork.record || Record->status!=R35Waiting) return 0;
  if (!Record->old_namespace_captured || Record->selected_banks[0]!=2 ||
      Record->selected_banks[1]!=3) return 0;
  if (Record->phase==R35StopHci && Value==0 &&
      (Address==R35_HCI_BASE+0x24 || Address==R35_HCI_BASE+0x60 || Address==R35_HCI_BASE+0x80)) return 1;
  if (!Record->host_stopped) return 0;
  for (Bank=2; Bank<=3; ++Bank) {
    UINT64 Base=R35_SMMU_BASE+0x80000+((UINT64)Bank<<12);
    if (Record->phase==R35DisableContexts && Address==Base &&
        Value==(Record->before.context[Bank-2].words[R35Sctlr]&~1U)) return 1;
    if (Record->phase==R35SyncContexts && Value==MAX_UINT32 &&
        (Address==Base+0x618 || Address==Base+0x7f0)) return 1;
  }
  return 0;
}
STATIC int Write32 (void *Context, R35U64 Address, R35U32 Value)
{
  if (!Permit(Context,&mWork.record,Address,Value)) return 0;
  ++mAccesses; MmioWrite32((UINTN)Address,Value); return 1;
}
STATIC int Barrier (void *Context)
{
  if (!Live(Context)) return 0;
  __asm__ __volatile__("dsb sy" ::: "memory"); return 1;
}
STATIC int Ticks (void *Context, R35U64 *Value)
{
  if (!Value || !Live(Context)) return 0;
  __asm__ __volatile__("mrs %0, cntpct_el0" : "=r"(*Value)); return 1;
}

// Only compare the portions the r35 detacher was required to preserve. Other
// device routes intentionally undergo the baseline cleanup. Any mismatch
// keeps the record unusable; it is not repaired or retried.
STATIC BOOLEAN PostDetachMatches (VOID)
{
  CONST R35Record *R=&mWork.record;
  UINT32 I, Bank, Value; UINT64 Wide;
  if (R->status!=R35Maintained) return FALSE;
  for (I=0; I<R35_HCI_WORDS; ++I) {
    if (!Read32(NULL,r35_hci_address(I),&Value) || Value!=R->after.hci[I]) return FALSE;
  }
  for (I=0; I<R35_GROUPS; ++I) {
    if (!(R->preserve_slots[I/32]&(1U<<(I%32)))) continue;
    if (!Read32(NULL,R35_SMMU_BASE+0x800+4*I,&Value) || Value!=R->after.streams[I].smr) return FALSE;
    if (!Read32(NULL,R35_SMMU_BASE+0xc00+4*I,&Value) || Value!=R->after.streams[I].s2cr) return FALSE;
  }
  for (Bank=2; Bank<=4; ++Bank) {
    for (I=0; I<R35_CONTEXT_WORDS; ++I)
      if (!Read32(NULL,r35_context_address(Bank,I),&Value) || Value!=R->after.context[Bank-2].words[I]) return FALSE;
    for (I=0; I<2; ++I)
      if (!Read64(NULL,R35_SMMU_BASE+0x80000+((UINT64)Bank<<12)+0x20+8*I,&Wide) || Wide!=R->after.context[Bank-2].ttbr[I]) return FALSE;
  }
  return TRUE;
}

VOID J716fR35BeforeDetach (VOID)
{
  CONST UINT16 MdpStreams[]={0x820,0x821,0x822,0xc20,0xc21,0xc22};
  R35Io Io={NULL,Live,Read32,Read64,Write32,Barrier,Ticks,Permit,NULL};
  R35Config Config;
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  UINT64 Frequency=0, CurrentEl=0, Sctlr=MAX_UINT64;
  BOOLEAN Preserve=FALSE, Matches=FALSE;
  EFI_STATUS Status;
  // SEC-only, before ordinary firmware services and constructors. The current
  // execution/access contract does not assert exclusion of secure-world users.
  if (mAttempted) return;
  mAttempted=TRUE; mAccesses=0; mLive=FALSE;
  ZeroMem(&Config,sizeof(Config)); ZeroMem(&mWork,sizeof(mWork));
  __asm__ __volatile__("mrs %0, CurrentEL" : "=r"(CurrentEl));
  if (CurrentEl==4) __asm__ __volatile__("mrs %0, sctlr_el1" : "=r"(Sctlr));
  if (CurrentEl==8) __asm__ __volatile__("mrs %0, sctlr_el2" : "=r"(Sctlr));
  __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(Frequency));
  DEBUG((EFI_D_WARN,"J35 SEC entry EL=%lu SCTLR=%016lx frequency=%lu\n",CurrentEl>>2,Sctlr,Frequency));
  Status=LocateMemoryRegionByName("SMMU",&Region);
  // New physical MMIO is permitted only with an observed disabled stage-1 MMU.
  // A software deadline cannot recover a synchronous bus abort or stalled bus.
  if ((CurrentEl==4 || CurrentEl==8) && !(Sctlr&1) &&
      !EFI_ERROR(Status) && Region.Address==R35_SMMU_BASE && Region.Length>=0x85000 &&
      Frequency>=1000000 && Frequency<=1000000000 &&
      J716fR35GetCurrentBootIdentity(&Config.boot_id,&Config.boot_sequence)) {
    Config.mode=R35StopAndMaintain;
    Config.timeout_ticks=Frequency*2;
    Config.max_operations=10000; Config.max_polls=1024;
    mLive=TRUE;
    (VOID)r35_handoff_run(&mWork,&Io,&Config);
    Preserve=mWork.record.smmu_mutation_possible!=0;
  }
  // CB4 and slot4 are always explicitly preserved in this candidate. Once
  // SMMU maintenance might have started, preserve UFS namespace/old slots too.
  ArmSmmuDetachR35(MdpStreams,ARRAY_SIZE(MdpStreams),Preserve,
                  mWork.record.selected_slots[0],mWork.record.selected_slots[1]);
  if (Preserve && mWork.record.status==R35Maintained) Matches=PostDetachMatches();
  mLive=FALSE;
  if (mWork.record.finished) {
    r35_record_post_detacher(&mWork.record,Matches);
    Status=J716fR35RecordStage(&mWork.record);
    DEBUG((EFI_D_WARN,"J35 SEC result=%u stopped=%u sync=%x post=%u staged=%r; owner=0 dma=0\n",
      mWork.record.status,mWork.record.host_stopped,mWork.record.synchronized_mask,Matches,Status));
  } else {
    DEBUG((EFI_D_ERROR,"J35 SEC producer not entered; no DMA handoff\n"));
  }
}
