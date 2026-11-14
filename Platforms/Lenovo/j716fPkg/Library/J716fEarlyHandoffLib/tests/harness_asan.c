// ASan cannot map the board's 2--4 GiB addresses: those are shadow mappings.
// Exercise production range/map/rejection functions without fixed mappings,
// plus the same pinned libfdt validator on ordinary ASan-tracked allocations.
static const char *current_test;
static unsigned checks;
static void fail(const char *expr, int line) {
  fprintf(stderr,"FAIL ASan %s:%d: %s\n",current_test,line,expr); exit(1);
}
#define CHECK(x) do { if (!(x)) fail(#x,__LINE__); } while (0)
static void begin(const char *name) { current_test=name; test_map=NULL; test_count=0; }
static void finish(void) { printf("PASS ASan %03u %s\n",++checks,current_test); }
static EFI_MEMORY_REGION_DESCRIPTOR ram(UINT64 base, UINT64 size) {
  EFI_MEMORY_REGION_DESCRIPTOR out={"Test RAM",0,0,AddMem,SYS_MEM,SYS_MEM_CAP,Conv,WRITE_BACK_XN};
  out.Address=base; out.Length=size; return out;
}
static void rejects_without_read(UINT64 address) {
  EFI_MEMORY_REGION_DESCRIPTOR *map=gMemoryDescriptor;
  UINT8 count=ARRAY_SIZE(gMemoryDescriptor);
  CHECK(PrepareEarlyHandoff(address)==EFI_COMPROMISED_DATA);
  CHECK(!mReady && mCount==0);
  CHECK(ApplyEarlyHandoffMemoryMap(&map,&count)==EFI_NOT_READY);
  CHECK(map==gMemoryDescriptor && count==ARRAY_SIZE(gMemoryDescriptor));
}
static void expected(size_t i,UINT64 base,UINT64 end,UINT32 type) {
  CHECK(i<mCount && mMap[i].Address==base && mMap[i].Length==end-base && mMap[i].MemoryType==type);
  CHECK(mMap[i].HobOption==AddMem && mMap[i].ResourceType==SYS_MEM);
  CHECK(mMap[i].ResourceAttribute==(SYS_MEM_CAP) && mMap[i].ArmAttributes==WRITE_BACK_XN);
}
static void *read_dtb(const char *path,size_t *size) {
  FILE *file=fopen(path,"rb"); CHECK(file!=NULL);
  CHECK(fseek(file,0,SEEK_END)==0); long length=ftell(file); CHECK(length>0);
  rewind(file); void *data=malloc((size_t)length); CHECK(data!=NULL);
  CHECK(fread(data,1,(size_t)length,file)==(size_t)length); CHECK(fclose(file)==0);
  *size=(size_t)length; return data;
}

int main(int argc,char **argv) {
  begin("setup"); CHECK(argc==2);
  size_t bytes=sizeof(gMemoryDescriptor);
  void *snapshot=malloc(bytes); CHECK(snapshot!=NULL); memcpy(snapshot,gMemoryDescriptor,bytes);
  UINTN count=ARRAY_SIZE(gMemoryDescriptor);

  begin("board descriptor allowed input regions and uncovered hole");
  CHECK(InputRange(gMemoryDescriptor,count,0xa1000000,0x10000));
  CHECK(InputRange(gMemoryDescriptor,count,0xd0000000,0x10000));
  CHECK(InputRange(gMemoryDescriptor,count,0x100000000ULL,0x10000));
  CHECK(!InputRange(gMemoryDescriptor,count,0xbcc00000,0x1000));
  CHECK(!InputRange(gMemoryDescriptor,count,0xbcbff000,0x3410000));
  CHECK(!InputRange(gMemoryDescriptor,count,0x200000000ULL,1)); finish();

  for (UINTN i=0;i<count;++i) {
    if (InputRegion(&gMemoryDescriptor[i])) continue;
    char label[100]; snprintf(label,sizeof(label),"static '%s' rejected without dereference",gMemoryDescriptor[i].Name); begin(label);
    rejects_without_read(gMemoryDescriptor[i].Address); finish();
  }
  const UINT64 bad[]={0,0xa1000001,0xa1000004,0xbcc00000,0x200000000ULL,UINT64_MAX-7};
  for(size_t i=0;i<ARRAY_SIZE(bad);++i) {
    char label[100]; snprintf(label,sizeof(label),"invalid entry pointer 0x%" PRIx64,bad[i]); begin(label);
    rejects_without_read(bad[i]); finish();
  }
  begin("RangeEnd/PageRange zero and overflow boundaries");
  UINT64 end; HANDOFF_RANGE range;
  CHECK(!RangeEnd(0,1,&end) && !RangeEnd(1,0,&end) && !RangeEnd(UINT64_MAX-1,2,&end));
  CHECK(RangeEnd(1,UINT64_MAX-1,&end) && end==UINT64_MAX);
  CHECK(!PageRange(1,UINT64_MAX,&range) && !PageRange(2,1,&range) && !PageRange(1,1,&range));
  CHECK(PageRange(0x1001,0x1fff,&range) && range.Base==0x1000 && range.End==0x2000); finish();

  EFI_MEMORY_REGION_DESCRIPTOR *map=calloc(128,sizeof(*map)); CHECK(map!=NULL);
  begin("exact split uses bounded map writes and preserves source");
  map[0]=ram(0xd0000000,0x10000); EFI_MEMORY_REGION_DESCRIPTOR original=map[0];
  mRangeCount=2; mRanges[0]=(HANDOFF_RANGE){0xd0001000,0xd0002000}; mRanges[1]=(HANDOFF_RANGE){0xd0004000,0xd0005000};
  CHECK(BuildProtectedMap(map,1)); CHECK(mCount==5);
  expected(0,0xd0000000,0xd0001000,Conv); expected(1,0xd0001000,0xd0002000,Reserv);
  expected(2,0xd0002000,0xd0004000,Conv); expected(3,0xd0004000,0xd0005000,Reserv); expected(4,0xd0005000,0xd0010000,Conv);
  CHECK(memcmp(map,&original,sizeof(original))==0);
  resource_count=allocation_count=0;
  for(UINTN i=0;i<mCount;++i) AddHob(mMap[i]);
  CHECK(resource_count==5 && allocation_count==5);
  for(UINTN i=0;i<mCount;++i) {
    CHECK(resource_hobs[i].base==mMap[i].Address && resource_hobs[i].size==mMap[i].Length);
    CHECK(allocation_hobs[i].base==mMap[i].Address && allocation_hobs[i].size==mMap[i].Length && allocation_hobs[i].type==mMap[i].MemoryType);
  }
  finish();
  begin("adjacent descriptor split retains exact boundary");
  map[0]=ram(0xd0000000,0x4000); map[1]=ram(0xd0004000,0x4000);
  mRangeCount=1; mRanges[0]=(HANDOFF_RANGE){0xd0003000,0xd0005000};
  CHECK(BuildProtectedMap(map,2) && mCount==4);
  expected(0,0xd0000000,0xd0003000,Conv); expected(1,0xd0003000,0xd0004000,Reserv);
  expected(2,0xd0004000,0xd0005000,Reserv); expected(3,0xd0005000,0xd0008000,Conv); finish();
  begin("non-conventional descriptors remain byte-identical");
  map[0]=ram(0xd0000000,0x4000); map[0].MemoryType=EfiReservedMemoryType; original=map[0];
  mRangeCount=1; mRanges[0]=(HANDOFF_RANGE){0xd0001000,0xd0002000};
  CHECK(BuildProtectedMap(map,1) && mCount==1 && memcmp(&mMap[0],&original,sizeof(original))==0); finish();
  begin("AppendRegion output boundary cannot overrun terminator slot");
  map[0]=ram(0xd0000000,0x4000); mCount=126;
  CHECK(AppendRegion(map,0xd0000000,0xd0001000,TRUE) && mCount==127);
  EFI_MEMORY_REGION_DESCRIPTOR final=mMap[127];
  CHECK(!AppendRegion(map,0xd0001000,0xd0002000,TRUE) && mCount==127 && memcmp(&final,&mMap[127],sizeof(final))==0);
  CHECK(AppendRegion(map,1,1,FALSE) && mCount==127); CHECK(!AppendRegion(map,2,1,FALSE)); finish();
  begin("descriptor capacity fails before out-of-bounds write");
  for(UINTN i=0;i<128;++i) map[i]=ram(0xd0000000+i*0x1000,0x1000);
  mRangeCount=0; CHECK(!BuildProtectedMap(map,128)); CHECK(mCount==127); finish();
  begin("overflowing map descriptor fails without write");
  map[0]=ram(UINT64_MAX-7,16); mRangeCount=0; CHECK(!BuildProtectedMap(map,1) && mCount==0); finish();
  begin("overlapping live descriptor overrides conventional coverage");
  map[0]=ram(0xd0000000,0x4000); map[1]=ram(0xd0001000,0x1000); map[1].MemoryType=EfiBootServicesData;
  CHECK(!InputRange(map,2,0xd0001000,8)); CHECK(InputRange(map,2,0xd0000000,8)); finish();
  begin("malformed unrelated map range rejected instead of overflowing");
  map[0]=ram(0xd0000000,0x4000); map[1]=ram(UINT64_MAX-7,16);
  CHECK(!InputRange(map,2,0xd0000000,8)); finish();
  begin("bad count rejected without traversing incomplete map");
  test_map=map; test_count=0; rejects_without_read(0xd0000000);
  test_count=128; rejects_without_read(0xd0000000); finish();
  begin("rejection invalidates formerly available protected map");
  mReady=TRUE; mCount=1; rejects_without_read(0); finish();

  size_t dtb_size; void *dtb=read_dtb(argv[1],&dtb_size);
  begin("pinned full validator accepts exact-sized stock allocation");
  CHECK(ValidateFdtLayout(dtb,dtb_size) && fdt_check_full(dtb,dtb_size)==0); finish();
  begin("pinned full validator rejects insufficient caller size");
  CHECK(!ValidateFdtLayout(dtb,0) && !ValidateFdtLayout(dtb,8) && !ValidateFdtLayout(dtb,39));
  CHECK(!ValidateFdtLayout(dtb,fdt_totalsize(dtb)-1)); finish();
  void *copy=malloc(dtb_size); CHECK(copy!=NULL);
  begin("pinned full validator rejects malformed structure in ASan allocation");
  memcpy(copy,dtb,dtb_size); *(fdt32_t *)((char *)copy+fdt_off_dt_struct(copy))=cpu_to_fdt32(0xdeadbeef);
  CHECK(ValidateFdtLayout(copy,dtb_size) && fdt_check_full(copy,dtb_size)!=0); finish();
  begin("production layout guard bounds header mutations before native full validation");
  for(unsigned i=0;i<sizeof(struct fdt_header);++i) {
    memcpy(copy,dtb,dtb_size); ((unsigned char *)copy)[i]^=0xff;
    if (ValidateFdtLayout(copy,dtb_size)) (void)fdt_check_full(copy,dtb_size);
  }
  finish();
  begin("production layout guard rejects structure alignment regression");
  memcpy(copy,dtb,dtb_size); fdt_set_off_dt_struct(copy,fdt_off_dt_struct(copy)+1);
  CHECK(!ValidateFdtLayout(copy,dtb_size)); finish();
  begin("production layout guard rejects reservation alignment regression");
  memcpy(copy,dtb,dtb_size); fdt_set_off_mem_rsvmap(copy,fdt_off_mem_rsvmap(copy)+4);
  CHECK(!ValidateFdtLayout(copy,dtb_size)); finish();
  begin("production layout guard rejects partial-word structure size");
  memcpy(copy,dtb,dtb_size); fdt_set_size_dt_struct(copy,fdt_size_dt_struct(copy)-1);
  CHECK(!ValidateFdtLayout(copy,dtb_size)); finish();
  begin("production layout guard rejects overlapping strings and structure");
  memcpy(copy,dtb,dtb_size); fdt_set_off_dt_strings(copy,fdt_off_dt_struct(copy));
  CHECK(!ValidateFdtLayout(copy,dtb_size)); finish();
  begin("production layout guard rejects reservation table inside header");
  memcpy(copy,dtb,dtb_size); fdt_set_off_mem_rsvmap(copy,32);
  CHECK(!ValidateFdtLayout(copy,dtb_size)); finish();
  begin("production layout guard rejects non-v17 blobs");
  memcpy(copy,dtb,dtb_size); fdt_set_version(copy,16); fdt_set_last_comp_version(copy,16);
  CHECK(!ValidateFdtLayout(copy,dtb_size)); finish();
  begin("unaligned endian primitives use bounded actual host shims");
  unsigned char values[17]={0}; fdt64_t v64=cpu_to_fdt64(0x100001234ULL); fdt32_t v32=cpu_to_fdt32(0xd0012345);
  memcpy(values+1,&v64,8); memcpy(values+10,&v32,4);
  CHECK(Fdt64ToCpu(ReadUnaligned64(values+1))==0x100001234ULL);
  CHECK(Fdt32ToCpu(ReadUnaligned32(values+10))==0xd0012345); finish();
  begin("board source descriptors unchanged after all ASan cases");
  CHECK(memcmp(snapshot,gMemoryDescriptor,bytes)==0); finish();
  free(copy); free(dtb); free(map); free(snapshot);
  printf("PASS %u ASan/UBSan scenarios; fixed-address Prepare success path is separately UBSan tested\n",checks);
  return 0;
}
