/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "R35Handoff.h"
#define HCI_VALID ((1U<<R35_HCI_WORDS)-1U)
#define FSR_FAULT 0xc00001feU
#define NONE_INDEX 0xffffffffU

static const unsigned hci_offsets[R35_HCI_WORDS]={
  0x00,0x08,0x34,0x30,0x20,0x24,0x50,0x54,0x58,0x60,0x70,0x74,0x78,0x80
};
static const unsigned cb_offsets[R35_CONTEXT_WORDS]={
  0,4,0,0,0x30,0x10,0x34,0x38,0x3c,0x58,0x7f4
};
static void zero_bytes(void *p,unsigned bytes)
{
  R35U8 *v=(R35U8 *)p;
  unsigned i;
  for(i=0;i<bytes;++i) v[i]=0;
}
R35U64 r35_hci_address(unsigned index)
{
  return index<R35_HCI_WORDS ? R35_HCI_BASE+hci_offsets[index] : 0;
}
R35U64 r35_context_address(unsigned bank,unsigned word)
{
  if(bank<2 || bank>4 || word>=R35_CONTEXT_WORDS) return 0;
  if(word==R35Cbar) return R35_SMMU_BASE+0x1000+4*bank;
  if(word==R35Cba2r) return R35_SMMU_BASE+0x1800+4*bank;
  return R35_SMMU_BASE+0x80000+((R35U64)bank<<12)+cb_offsets[word];
}
static int fail(R35Workspace *w,enum R35Status status,R35U64 pa)
{
  if(w->record.status==R35Waiting) { w->record.status=status; w->record.failed_address=pa; }
  return 0;
}
static int live(R35Workspace *w)
{
  return w->io.live(w->io.context) || fail(w,R35Access,0);
}
static int time_ok(R35Workspace *w)
{
  R35U64 now=0;
  if(!live(w)) return 0;
  ++w->record.clock_queries;
  if(!w->io.ticks(w->io.context,&now)) return fail(w,R35Clock,0);
  if(!live(w)) return 0;
  if(!w->clock_started) {
    w->clock_started=1; w->record.started_ticks=w->record.last_ticks=now;
  }
  if(now<w->record.last_ticks) return fail(w,R35TimeReversed,0);
  w->record.last_ticks=now;
  if(now-w->record.started_ticks>=w->config.timeout_ticks) return fail(w,R35Deadline,0);
  return 1;
}
static int step(R35Workspace *w)
{
  if(!time_ok(w)) return 0;
  if(w->record.operations>=w->config.max_operations) return fail(w,R35Budget,0);
  ++w->record.operations; return 1;
}
static int note(R35Workspace *w,enum R35Event event)
{
  if(w->io.note) w->io.note(w->io.context,event,&w->record);
  return time_ok(w);
}
static int readable32(R35U64 pa)
{
  unsigned i,b;
  for(i=0;i<R35_HCI_WORDS;++i) if(pa==r35_hci_address(i)) return 1;
  if(pa==R35_SMMU_BASE || pa==R35_SMMU_BASE+0x20 ||
     pa==R35_SMMU_BASE+0x24 || pa==R35_SMMU_BASE+0x28) return 1;
  if(!(pa&3) && ((pa>=R35_SMMU_BASE+0x800 && pa<R35_SMMU_BASE+0x800+4*R35_GROUPS) ||
     (pa>=R35_SMMU_BASE+0xc00 && pa<R35_SMMU_BASE+0xc00+4*R35_GROUPS))) return 1;
  for(b=2;b<=4;++b) for(i=0;i<R35_CONTEXT_WORDS;++i) if(pa==r35_context_address(b,i)) return 1;
  return 0;
}
static int read32(R35Workspace *w,R35U64 pa,R35U32 *out)
{
  R35U32 v=0;
  if(!out || !readable32(pa)) return fail(w,R35Argument,pa);
  if(!step(w)) return 0;
  if(!w->io.read32(w->io.context,pa,&v)) return fail(w,R35Read,pa);
  ++w->record.reads32;
  if(!time_ok(w)) return 0;
  *out=v; return 1;
}
static int read64(R35Workspace *w,R35U64 pa,R35U64 *out)
{
  unsigned b; int allowed=0;
  R35U64 value=0;
  for(b=2;b<=4;++b) {
    const R35U64 base=R35_SMMU_BASE+0x80000+((R35U64)b<<12);
    if(pa==base+0x20 || pa==base+0x28) allowed=1;
  }
  if(!out || !allowed) return fail(w,R35Argument,pa);
  if(!step(w)) return 0;
  if(!w->io.read64(w->io.context,pa,&value)) return fail(w,R35Read,pa);
  ++w->record.reads64;
  if(!time_ok(w)) return 0;
  *out=value; return 1;
}
static int barrier(R35Workspace *w)
{
  if(!step(w)) return 0;
  ++w->record.barriers;
  if(!w->io.barrier(w->io.context)) return fail(w,R35Barrier,0);
  return time_ok(w);
}
static int writable(R35Workspace *w,R35U64 pa,R35U32 value)
{
  unsigned b;
  if(w->record.phase==R35StopHci && !value &&
    (pa==r35_hci_address(R35Ie) || pa==r35_hci_address(R35TaskRunStop) ||
     pa==r35_hci_address(R35TransferRunStop))) return 1;
  for(b=2;b<=3;++b) {
    const R35U64 base=R35_SMMU_BASE+0x80000+((R35U64)b<<12);
    if(w->record.phase==R35DisableContexts && pa==base &&
       value==(w->record.before.context[b-2].words[R35Sctlr]&~1U)) return 1;
    if(w->record.phase==R35SyncContexts && value==0xffffffffU &&
       (pa==base+0x618 || pa==base+0x7f0)) return 1;
  }
  return 0;
}
static int write32(R35Workspace *w,R35U64 pa,R35U32 value)
{
  R35WriteRecord *entry;
  if(!writable(w,pa,value)) return fail(w,R35Argument,pa);
  if(!w->io.permit || !w->io.permit(w->io.context,&w->record,pa,value)) return fail(w,R35Denied,pa);
  if(!step(w)) return 0;
  if(w->record.writes_attempted==R35_MAX_WRITES) return fail(w,R35Budget,pa);
  entry=&w->record.writes[w->record.writes_attempted++];
  entry->address=pa; entry->value=value; entry->phase=w->record.phase;
  w->record.mutation_possible=1;
  if(pa>=R35_SMMU_BASE) {
    w->record.smmu_mutation_possible=1;
    w->record.detacher_policy=R35PreserveUfsBanks;
    w->record.preserve_banks=0x1c; /* CB2/3 and display CB4, even on failure. */
  }
  if(!note(w,R35BeforeWrite)) return 0;
  /* Authorization is rechecked after bounded diagnostics, before the store. */
  if(!w->io.permit(w->io.context,&w->record,pa,value)) return fail(w,R35Denied,pa);
  if(!time_ok(w)) return 0;
  if(!w->io.write32(w->io.context,pa,value)) return fail(w,R35Write,pa);
  entry->completed=1; ++w->record.writes_completed;
  return note(w,R35AfterWrite);
}
static int capture_hci(R35Workspace *w,R35Snapshot *s)
{
  unsigned i;
  s->hci_valid=0;
  if(!barrier(w)) return 0;
  for(i=0;i<R35_HCI_WORDS;++i) {
    if(!read32(w,r35_hci_address(i),&s->hci[i])) return 0;
    s->hci_valid|=1U<<i;
  }
  return barrier(w);
}
static int capture_context(R35Workspace *w,R35Context *c,unsigned bank)
{
  unsigned i;
  const R35U64 base=R35_SMMU_BASE+0x80000+((R35U64)bank<<12);
  zero_bytes(c,sizeof(*c)); c->bank=bank;
  for(i=0;i<R35_CONTEXT_WORDS;++i) {
    if(!read32(w,r35_context_address(bank,i),&c->words[i])) return 0;
    c->valid32|=1U<<i;
  }
  for(i=0;i<2;++i) {
    if(!read64(w,base+0x20+8*i,&c->ttbr[i])) return 0;
    c->valid64|=1U<<i;
  }
  return 1;
}
static int capture(R35Workspace *w,R35Snapshot *s)
{
  unsigned i;
  static const unsigned header_offsets[4]={0x20,0x24,0x28,0};
  zero_bytes(s,sizeof(*s));
  if(!capture_hci(w,s)) return 0;
  for(i=0;i<4;++i) {
    if(!read32(w,R35_SMMU_BASE+header_offsets[i],&s->headers[i])) return 0;
    s->header_valid|=1U<<i;
  }
  if(s->headers[0]!=0x4c017e57 || s->headers[1]!=0x6000004a || s->headers[2]!=0x5111 ||
     (s->headers[3]&(0x80000000U|8U|1U))) return fail(w,R35Geometry,0);
  s->stream_count=R35_GROUPS;
  for(i=0;i<R35_GROUPS;++i) {
    if(!read32(w,R35_SMMU_BASE+0x800+4*i,&s->streams[i].smr)) return 0;
    s->streams[i].valid|=1;
    if(!read32(w,R35_SMMU_BASE+0xc00+4*i,&s->streams[i].s2cr)) return 0;
    s->streams[i].valid|=2;
  }
  for(i=0;i<3;++i) if(!capture_context(w,&s->context[i],i+2)) return 0;
  return barrier(w);
}
static int host_valid(const R35Snapshot *s)
{
  const R35U32 *v=s->hci;
  return s->hci_valid==HCI_VALID && v[R35Ver]==0x300 && !(v[R35Cap]&(1U<<30)) &&
    v[R35Hce]==1 && (v[R35Hcs]&15U)==15U && !(v[R35Hcs]&0x30U) &&
    !(v[R35Is]&0x71884U) && !v[R35TransferDoorbell] && !v[R35TaskDoorbell] &&
    v[R35TransferRunStop]<=1 && v[R35TaskRunStop]<=1;
}
static int streams_same(const R35Snapshot *a,const R35Snapshot *b)
{
  unsigned i;
  if(a->header_valid!=15 || b->header_valid!=15 || a->stream_count!=R35_GROUPS || b->stream_count!=R35_GROUPS) return 0;
  for(i=0;i<4;++i) if(a->headers[i]!=b->headers[i]) return 0;
  for(i=0;i<R35_GROUPS;++i) if(a->streams[i].valid!=3 || b->streams[i].valid!=3 ||
    a->streams[i].smr!=b->streams[i].smr || a->streams[i].s2cr!=b->streams[i].s2cr) return 0;
  return 1;
}
static int context_same(const R35Context *a,const R35Context *b,R35U32 sctlr)
{
  unsigned i;
  if(a->bank!=b->bank || a->valid32!=R35_CONTEXT_ALL || b->valid32!=R35_CONTEXT_ALL ||
     a->valid64!=3 || b->valid64!=3 || b->words[R35Sctlr]!=sctlr) return 0;
  for(i=1;i<=R35Mair1;++i) if(a->words[i]!=b->words[i]) return 0;
  for(i=0;i<2;++i) if(a->ttbr[i]!=b->ttbr[i]) return 0;
  return 1;
}
static int fault_free(const R35Context *c)
{
  return !(c->words[R35Fsr]&FSR_FAULT) && !(c->words[R35Fsr]&~0x400U) &&
    !c->words[R35Tlbstatus];
}
static int namespace_valid(R35Workspace *w,const R35Context *c)
{
  const R35U32 cbar=c->words[R35Cbar];
  if(c->valid32!=R35_CONTEXT_ALL || c->valid64!=3)
    return fail(w,R35Namespace,r35_context_address(c->bank,R35Cbar));
  if(((cbar>>16)&3)!=1 || (cbar&~0xff01f3ffU))
    return fail(w,R35Namespace,r35_context_address(c->bank,R35Cbar));
  if(c->words[R35Cba2r]!=1)
    return fail(w,R35Namespace,r35_context_address(c->bank,R35Cba2r));
  if(c->words[R35Sctlr]&~0x11f7U)
    return fail(w,R35Namespace,r35_context_address(c->bank,R35Sctlr));
  /* This fixed J716F profile has already passed ID geometry and route checks.
   * Keep the legacy zero value and r36's observed 3; do not generalize this
   * Qualcomm value to other implementations or write ACTLR to normalize it.
   * context_same() requires the complete original value on every recheck. */
  if(c->words[R35Actlr]!=0 && c->words[R35Actlr]!=3)
    return fail(w,R35Namespace,r35_context_address(c->bank,R35Actlr));
  /* FSR.Format=0x400 is AArch64 format, not a fault and never W1C-cleared. */
  if((c->words[R35Fsr]&FSR_FAULT) || (c->words[R35Fsr]&~0x400U))
    return fail(w,R35Fault,r35_context_address(c->bank,R35Fsr));
  if(c->words[R35Tlbstatus])
    return fail(w,R35Fault,r35_context_address(c->bank,R35Tlbstatus));
  return 1;
}
static int select_routes(R35Workspace *w)
{
  static const R35U32 sids[2]={0x4e0,0xe0};
  const R35Snapshot *s=&w->record.before;
  unsigned i,j;
  R35U32 matches[2]={NONE_INDEX,NONE_INDEX};
  if(s->streams[4].smr!=0x84020820U || s->streams[4].s2cr!=4) return fail(w,R35Routes,0);
  for(i=0;i<R35_GROUPS;++i) {
    const R35U32 smr=s->streams[i].smr,s2cr=s->streams[i].s2cr;
    const R35U32 mask=(smr>>16)&0x7fffU,bank=s2cr&255U,type=(s2cr>>16)&3U;
    int matched=-1;
    if(!(smr&0x80000000U)) continue;
    if(type==3 || (!type && bank>=74) || ((smr&0xffffU)|mask)&~0x7fffU) return fail(w,R35Routes,0);
    for(j=0;j<2;++j) if(((smr^sids[j])&0xffffU&~mask)==0) {
      if(mask || matches[j]!=NONE_INDEX || s2cr!=j+2) return fail(w,R35Routes,R35_SMMU_BASE+0x800+4*i);
      matches[j]=i; matched=(int)j;
    }
    if(!type && (bank==2 || bank==3) && matched<0) return fail(w,R35SharedContext,R35_SMMU_BASE+0xc00+4*i);
  }
  for(j=0;j<2;++j) {
    if(matches[j]==NONE_INDEX) return fail(w,R35Routes,0);
    w->record.selected_slots[j]=matches[j]; w->record.selected_banks[j]=j+2;
    w->record.preserve_slots[matches[j]/32]|=1U<<(matches[j]%32);
  }
  w->record.preserve_slots[0]|=1U<<4; /* display route as well as its CB4 */
  return 1;
}
static int hci_expected(R35Workspace *w,const R35Snapshot *s,unsigned pending)
{
  unsigned i;
  if(!host_valid(s)) return fail(w,R35HostState,0);
  for(i=0;i<R35_HCI_WORDS;++i)
    if(s->hci[i]!=w->expected_hci[i] && !(i==pending && s->hci[i]==0))
      return fail(w,R35StateChanged,r35_hci_address(i));
  return 1;
}
static int state_expected(R35Workspace *w,const R35Snapshot *s)
{
  unsigned i;
  if(!hci_expected(w,s,R35_HCI_WORDS)) return 0;
  if(!streams_same(&w->record.before,s)) return fail(w,R35StateChanged,R35_SMMU_BASE);
  for(i=0;i<3;++i) {
    const R35U32 expected=i<2?w->expected_sctlr[i]:w->record.before.context[i].words[R35Sctlr];
    if(!context_same(&w->record.before.context[i],&s->context[i],expected))
      return fail(w,R35StateChanged,r35_context_address(i+2,R35Sctlr));
    if(i<2 && !fault_free(&s->context[i])) return fail(w,R35Fault,r35_context_address(i+2,R35Fsr));
  }
  return 1;
}
static int stop_control(R35Workspace *w,unsigned control)
{
  unsigned poll;
  if(!capture_hci(w,&w->scratch) || !hci_expected(w,&w->scratch,R35_HCI_WORDS)) return 0;
  if(!w->expected_hci[control]) return 1;
  if(!barrier(w) || !write32(w,r35_hci_address(control),0) || !barrier(w)) return 0;
  for(poll=0;poll<w->config.max_polls;++poll) {
    ++w->record.polls;
    if(!capture_hci(w,&w->scratch) || !hci_expected(w,&w->scratch,control)) return 0;
    if(!w->scratch.hci[control]) { w->expected_hci[control]=0; return 1; }
  }
  return fail(w,R35Readback,r35_hci_address(control));
}
static int maintain_context(R35Workspace *w,unsigned bank)
{
  const unsigned index=bank-2;
  const R35U64 base=R35_SMMU_BASE+0x80000+((R35U64)bank<<12);
  const R35U32 old=w->expected_sctlr[index],disabled=old&~1U;
  R35U32 value=0; unsigned poll;
  if(!capture(w,&w->scratch) || !state_expected(w,&w->scratch)) return 0;
  w->record.phase=R35DisableContexts;
  if(old&1U) {
    if(!write32(w,base,disabled) || !barrier(w)) return 0;
    for(poll=0;poll<w->config.max_polls;++poll) {
      ++w->record.polls;
      if(!read32(w,base,&value)) return 0;
      if(value==disabled) break;
      if(value!=old) return fail(w,R35StateChanged,base);
    }
    if(value!=disabled) return fail(w,R35Readback,base);
  } else if(!read32(w,base,&value) || value!=disabled) return fail(w,R35StateChanged,base);
  w->expected_sctlr[index]=disabled;
  w->record.disabled_mask|=1U<<bank;
  if(!note(w,R35ContextDisabled)) return 0;
  /* Read exact old namespace again with SCTLR.M cleared. No CBAR, TTBR,
   * CBA2R, TCR, ASID/VMID, stream or fault-state rewrite is performed. */
  if(!capture_context(w,&w->scratch.context[index],bank) ||
     !context_same(&w->record.before.context[index],&w->scratch.context[index],disabled))
    return fail(w,R35StateChanged,base);
  if(!fault_free(&w->scratch.context[index])) return fail(w,R35Fault,base+0x58);
  w->record.phase=R35SyncContexts;
  if(!barrier(w) || !write32(w,base+0x618,0xffffffffU) || !barrier(w) ||
     !write32(w,base+0x7f0,0xffffffffU) || !barrier(w)) return 0;
  for(poll=0;poll<w->config.max_polls;++poll) {
    ++w->record.polls;
    if(!read32(w,base+0x7f4,&value)) return 0;
    if(value&~1U) return fail(w,R35Namespace,base+0x7f4);
    if(!value) break;
  }
  if(value) return fail(w,R35SyncTimeout,base+0x7f4);
  if(!barrier(w) || !read32(w,base+0x58,&value)) return 0;
  if((value&FSR_FAULT) || (value&~0x400U)) return fail(w,R35Fault,base+0x58);
  w->record.synchronized_mask|=1U<<bank;
  return note(w,R35ContextSynced);
}
static enum R35Status perform(R35Workspace *w)
{
  unsigned i;
  w->record.phase=R35CaptureBefore;
  if(!note(w,R35Begin) || !capture(w,&w->record.before)) return (enum R35Status)w->record.status;
  w->record.old_namespace_captured=1;
  if(!note(w,R35BeforeCaptured)) return (enum R35Status)w->record.status;
  if(w->config.mode==R35CaptureOnly) {
    w->record.phase=R35CaptureAfter;
    if(!capture(w,&w->record.after)) return (enum R35Status)w->record.status;
    w->record.routes_unchanged=streams_same(&w->record.before,&w->record.after);
    w->record.display_config_unchanged=context_same(&w->record.before.context[2],&w->record.after.context[2],
      w->record.before.context[2].words[R35Sctlr]);
    w->record.status=R35Observed; return R35Observed;
  }
  w->record.phase=R35Validate;
  if(!host_valid(&w->record.before)) { fail(w,R35HostState,0); return (enum R35Status)w->record.status; }
  if(!select_routes(w)) return (enum R35Status)w->record.status;
  for(i=0;i<2;++i) if(!namespace_valid(w,&w->record.before.context[i])) return (enum R35Status)w->record.status;
  for(i=0;i<R35_HCI_WORDS;++i) w->expected_hci[i]=w->record.before.hci[i];
  for(i=0;i<2;++i) w->expected_sctlr[i]=w->record.before.context[i].words[R35Sctlr];
  if(!capture(w,&w->scratch) || !state_expected(w,&w->scratch)) return (enum R35Status)w->record.status;
  w->record.phase=R35StopHci;
  if(!stop_control(w,R35Ie) || !stop_control(w,R35TaskRunStop) || !stop_control(w,R35TransferRunStop))
    return (enum R35Status)w->record.status;
  w->record.phase=R35VerifyStopped;
  if(!capture(w,&w->scratch) || !state_expected(w,&w->scratch)) return (enum R35Status)w->record.status;
  w->record.host_stopped=1;
  if(!note(w,R35QueueStopped) || !maintain_context(w,2) || !maintain_context(w,3)) return (enum R35Status)w->record.status;
  w->record.phase=R35CaptureAfter;
  if(!capture(w,&w->record.after) || !state_expected(w,&w->record.after)) return (enum R35Status)w->record.status;
  w->record.routes_unchanged=w->record.display_config_unchanged=1;
  w->record.status=R35Maintained;
  return R35Maintained;
}
/* The SEC adapter executes once on the boot CPU before MMU setup; exclusive
 * accesses to MMU-off RAM have no guaranteed monitor/progress. This branch
 * requires that single-CPU execution contract and is not a concurrent lock. */
static unsigned claim_busy(R35Workspace *w)
{
#ifdef R35_EDK2_SEC
  unsigned old=*(volatile R35U32 *)&w->busy;
  if(!old) *(volatile R35U32 *)&w->busy=1;
  __asm__ __volatile__("" ::: "memory");
  return old;
#else
  return __atomic_exchange_n(&w->busy,1U,__ATOMIC_ACQUIRE);
#endif
}
static void release_busy(R35Workspace *w)
{
#ifdef R35_EDK2_SEC
  __asm__ __volatile__("" ::: "memory");
  *(volatile R35U32 *)&w->busy=0;
#else
  __atomic_store_n(&w->busy,0U,__ATOMIC_RELEASE);
#endif
}
enum R35Status r35_handoff_run(R35Workspace *w,const R35Io *io,const R35Config *config)
{
  enum R35Status status;
  if(!w || !io || !config) return R35Argument;
  if(claim_busy(w)) return R35Busy;
  if(w->used) { release_busy(w); return R35AlreadyAttempted; }
  if(!io->live || !io->read32 || !io->read64 || !io->barrier || !io->ticks ||
    (config->mode==R35StopAndMaintain && (!io->write32 || !io->permit)) ||
    !config->boot_id || !config->boot_sequence || !config->timeout_ticks ||
    config->mode>R35StopAndMaintain || !config->max_operations || config->max_operations>1000000 ||
    !config->max_polls || config->max_polls>65536) {
    release_busy(w); return R35Argument;
  }
  w->used=1; w->io=*io; w->config=*config; w->clock_started=0;
  zero_bytes(&w->record,sizeof(w->record)); zero_bytes(&w->scratch,sizeof(w->scratch));
  w->record.magic=R35_MAGIC; w->record.version=R35_VERSION; w->record.bytes=R35_RECORD_BYTES;
  w->record.profile=R35_PROFILE; w->record.source=1;
  w->record.boot_id=config->boot_id; w->record.boot_sequence=config->boot_sequence; w->record.mode=config->mode;
  w->record.selected_slots[0]=w->record.selected_slots[1]=NONE_INDEX;
  w->record.selected_banks[0]=w->record.selected_banks[1]=NONE_INDEX;
  status=perform(w);
  /* Never perform fresh MMIO after failure solely to populate a pretty final
   * snapshot. Before, partial after, write journal and failed PA remain exact. */
  if(status==R35Observed || status==R35Maintained) w->record.phase=R35Done;
  w->record.finished=1; w->record.crc32=r35_record_crc(&w->record);
  if(w->io.note) w->io.note(w->io.context,R35Finalized,&w->record);
  zero_bytes(&w->io,sizeof(w->io));
  release_busy(w);
  return status;
}
