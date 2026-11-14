// Included after the unmodified production functions in the host translation unit.
#define FDT_A 0xa1000000ULL
#define FDT_B 0xd0000000ULL
#define MAP_BYTES (2 * 1024 * 1024)
#define TREE_BYTES 4096
static unsigned checks;
static const char *current_test;
static EFI_MEMORY_REGION_DESCRIPTOR board_snapshot[128];
static EFI_MEMORY_REGION_DESCRIPTOR custom_map[128];
static UINT8 board_count;
static void fail(const char *expr, int line) {
  fprintf(stderr, "FAIL %s:%d: %s (reason=%s)\n", current_test, line, expr, mReason);
  exit(1);
}
#define CHECK(x) do { if (!(x)) fail(#x, __LINE__); } while (0)
#define FDT_OK(x) CHECK((x) == 0)
static void begin(const char *name) {
  current_test = name;
  test_map = NULL;
  test_count = 0;
}
static void finish(void) {
  CHECK(memcmp(gMemoryDescriptor, board_snapshot, board_count * sizeof(board_snapshot[0])) == 0);
  ++checks;
  printf("PASS %03u %s\n", checks, current_test);
}
static void *tree_at(UINT64 address, int chosen) {
  void *fdt = (void *)(uintptr_t)address;
  memset(fdt, 0, TREE_BYTES);
  FDT_OK(fdt_create_empty_tree(fdt, TREE_BYTES));
  if (chosen) CHECK(fdt_add_subnode(fdt, 0, "chosen") >= 0);
  return fdt;
}
static void prop(void *fdt, const char *name, UINT64 value, int width) {
  int chosen = fdt_path_offset(fdt, "/chosen");
  CHECK(chosen >= 0);
  if (width == 4) {
    fdt32_t v = cpu_to_fdt32((UINT32)value);
    FDT_OK(fdt_setprop(fdt, chosen, name, &v, sizeof(v)));
  } else if (width == 8) {
    fdt64_t v = cpu_to_fdt64(value);
    FDT_OK(fdt_setprop(fdt, chosen, name, &v, sizeof(v)));
  } else {
    char bytes[16] = {0};
    CHECK(width >= 0 && width <= (int)sizeof(bytes));
    FDT_OK(fdt_setprop(fdt, chosen, name, bytes, width));
  }
}
static void initrd(void *fdt, UINT64 start, UINT64 end, int start_width, int end_width) {
  prop(fdt, "linux,initrd-start", start, start_width);
  prop(fdt, "linux,initrd-end", end, end_width);
}
static void pack(void *fdt) { FDT_OK(fdt_pack(fdt)); }
static void accepted(UINT64 address) {
  EFI_MEMORY_REGION_DESCRIPTOR *map = NULL;
  UINT8 count = 0;
  CHECK(PrepareEarlyHandoff(address) == EFI_SUCCESS);
  CHECK(mReady);
  CHECK(ApplyEarlyHandoffMemoryMap(&map, &count) == EFI_SUCCESS);
  CHECK(map == mMap && count == mCount && count > 0 && count < 128);
}
static void rejected(UINT64 address, const char *reason) {
  EFI_MEMORY_REGION_DESCRIPTOR *map = gMemoryDescriptor;
  UINT8 count = board_count;
  CHECK(PrepareEarlyHandoff(address) == EFI_COMPROMISED_DATA);
  CHECK(!mReady && mCount == 0);
  CHECK(ApplyEarlyHandoffMemoryMap(&map, &count) == EFI_NOT_READY);
  CHECK(map == gMemoryDescriptor && count == board_count);
  if (reason) CHECK(strstr(mReason, reason) != NULL);
}
static EFI_MEMORY_REGION_DESCRIPTOR conventional(UINT64 base, UINT64 size) {
  EFI_MEMORY_REGION_DESCRIPTOR out = {"Test RAM", 0, 0, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN};
  out.Address = base;
  out.Length = size;
  return out;
}
static void custom_one(UINT64 base, UINT64 size) {
  custom_map[0] = conventional(base, size);
  test_map = custom_map;
  test_count = 1;
}
static void expect_region(size_t i, UINT64 base, UINT64 end, UINT32 type) {
  CHECK(i < mCount);
  CHECK(mMap[i].Address == base && mMap[i].Length == end - base);
  CHECK(mMap[i].MemoryType == type);
  CHECK(mMap[i].HobOption == AddMem && mMap[i].ResourceType == SYS_MEM);
  CHECK(mMap[i].ResourceAttribute == (SYS_MEM_CAP) && mMap[i].ArmAttributes == WRITE_BACK_XN);
}
static void assert_hobs_match_map(void) {
  resource_count = allocation_count = 0;
  for (UINTN i = 0; i < mCount; ++i) AddHob(mMap[i]);
  size_t ri = 0, ai = 0;
  for (UINTN i = 0; i < mCount; ++i) {
    const EFI_MEMORY_REGION_DESCRIPTOR *r = &mMap[i];
    if (r->HobOption == AddMem || r->HobOption == AddDev || r->HobOption == HobOnlyNoCacheSetting) {
      CHECK(ri < resource_count);
      TEST_HOB h = resource_hobs[ri++];
      CHECK(h.base == r->Address && h.size == r->Length && h.type == r->ResourceType && h.attrs == r->ResourceAttribute);
    }
    if (r->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY || r->MemoryType == EfiRuntimeServicesData) {
      CHECK(ai < allocation_count);
      TEST_HOB h = allocation_hobs[ai++];
      CHECK(h.base == r->Address && h.size == r->Length && h.type == r->MemoryType);
    }
  }
  CHECK(ri == resource_count && ai == allocation_count);
}
static void stock_dtb(const char *filename) {
  FILE *file = fopen(filename, "rb");
  CHECK(file != NULL);
  CHECK(fseek(file, 0, SEEK_END) == 0);
  long size = ftell(file);
  CHECK(size > 0 && size <= MAP_BYTES / 2);
  rewind(file);
  void *original = malloc((size_t)size);
  CHECK(original != NULL);
  CHECK(fread(original, 1, (size_t)size, file) == (size_t)size);
  CHECK(fclose(file) == 0);
  CHECK(fdt_check_full(original, (size_t)size) == 0);
  void *fdt = (void *)(uintptr_t)FDT_A;
  FDT_OK(fdt_open_into(original, fdt, MAP_BYTES));
  int chosen = fdt_path_offset(fdt, "/chosen");
  if (chosen < 0) chosen = fdt_add_subnode(fdt, 0, "chosen");
  CHECK(chosen >= 0);
  int status = fdt_delprop(fdt, chosen, "linux,initrd-start");
  CHECK(status == 0 || status == -FDT_ERR_NOTFOUND);
  status = fdt_delprop(fdt, chosen, "linux,initrd-end");
  CHECK(status == 0 || status == -FDT_ERR_NOTFOUND);
  pack(fdt);
  accepted(FDT_A);
  CHECK(mFdtSize == fdt_totalsize(fdt) && mRangeCount == 1 && mInitrd == 0);
  CHECK(mCount == board_count); // Already-reserved Kernel staging region.
  CHECK(memcmp(mMap, board_snapshot, board_count * sizeof(board_snapshot[0])) == 0);
  free(original);
}

int main(int argc, char **argv) {
  current_test = "setup";
  CHECK(argc == 2);
  void *a = mmap((void *)(uintptr_t)FDT_A, MAP_BYTES, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (a == MAP_FAILED) { perror("mmap FDT_A"); return 2; }
  void *b = mmap((void *)(uintptr_t)FDT_B, MAP_BYTES, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (b == MAP_FAILED) { perror("mmap FDT_B"); return 2; }
  CHECK((uintptr_t)a == FDT_A && (uintptr_t)b == FDT_B);
  EFI_MEMORY_REGION_DESCRIPTOR *original;
  GetBoardMemoryMap(&original, &board_count);
  CHECK(original == gMemoryDescriptor && board_count < 128);
  memcpy(board_snapshot, original, board_count * sizeof(board_snapshot[0]));

  begin("unprepared state leaves caller map/count untouched");
  EFI_MEMORY_REGION_DESCRIPTOR *output = original;
  UINT8 output_count = board_count;
  CHECK(ApplyEarlyHandoffMemoryMap(&output, &output_count) == EFI_NOT_READY);
  CHECK(output == original && output_count == board_count);
  finish();

  begin("copied normalized stock DTB, empty initrd, reserved Kernel unchanged");
  stock_dtb(argv[1]);
  finish();

  begin("missing chosen node accepted as empty ramdisk");
  void *fdt = tree_at(FDT_A, 0); pack(fdt); accepted(FDT_A);
  CHECK(mRangeCount == 1 && mInitrd == 0); finish();

  begin("chosen without either initrd property accepted");
  fdt = tree_at(FDT_A, 1); pack(fdt); accepted(FDT_A);
  CHECK(mRangeCount == 1 && mInitrd == 0); finish();

  for (int missing = 0; missing < 2; ++missing) {
    begin(missing ? "initrd start missing rejected" : "initrd end missing rejected");
    fdt = tree_at(FDT_A, 1);
    prop(fdt, missing ? "linux,initrd-end" : "linux,initrd-start", 0xd0100000, 8);
    pack(fdt); rejected(FDT_A, "incomplete"); finish();
  }
  const int widths[] = {0,1,3,5,7,9,16};
  for (size_t i=0; i<ARRAY_SIZE(widths); ++i) {
    char label[80]; snprintf(label,sizeof(label),"invalid initrd property width %d rejected",widths[i]); begin(label);
    fdt=tree_at(FDT_A,1); initrd(fdt,0xd0100000,0xd0100200,widths[i],8);
    pack(fdt); rejected(FDT_A,"malformed"); finish();
  }
  const int combinations[][2] = {{4,4},{8,8},{4,8},{8,4}};
  for (size_t i=0; i<ARRAY_SIZE(combinations); ++i) {
    char label[80]; snprintf(label,sizeof(label),"big-endian initrd decode %d/%d",combinations[i][0],combinations[i][1]); begin(label);
    fdt=tree_at(FDT_A,1); initrd(fdt,0xd0100001,0xd0100202,combinations[i][0],combinations[i][1]);
    pack(fdt); accepted(FDT_A);
    CHECK(mInitrd==0xd0100001 && mInitrdEnd==0xd0100202 && mPaddedEnd==0xd0100401);
    CHECK(mRangeCount==2); finish();
  }
  begin("64-bit initrd above 4 GiB decoded and reserved");
  fdt=tree_at(FDT_A,1); initrd(fdt,0x100001000ULL,0x100001201ULL,8,8); pack(fdt); accepted(FDT_A);
  CHECK(mInitrd==0x100001000ULL && mPaddedEnd==0x100001400ULL); finish();

  begin("equal zero initrd bounds accepted as absent");
  fdt=tree_at(FDT_A,1); initrd(fdt,0,0,8,8); pack(fdt); accepted(FDT_A); CHECK(mRangeCount==1); finish();
  begin("equal nonzero initrd bounds accepted as empty");
  fdt=tree_at(FDT_A,1); initrd(fdt,0xd0100001,0xd0100001,8,8); pack(fdt); accepted(FDT_A); CHECK(mRangeCount==1 && mPaddedEnd==0); finish();
  begin("reversed initrd rejected");
  fdt=tree_at(FDT_A,1); initrd(fdt,0xd0101000,0xd0100000,8,8); pack(fdt); rejected(FDT_A,"reversed"); finish();
  begin("nonempty zero-base initrd rejected");
  fdt=tree_at(FDT_A,1); initrd(fdt,0,512,8,8); pack(fdt); rejected(FDT_A,"outside"); finish();
  begin("initrd length plus sector padding overflow rejected");
  fdt=tree_at(FDT_A,1); initrd(fdt,1,UINT64_MAX,8,8); pack(fdt); rejected(FDT_A,"outside"); finish();
  begin("initrd start plus sector padding overflow rejected");
  fdt=tree_at(FDT_A,1); initrd(fdt,UINT64_MAX-10,UINT64_MAX-1,8,8); pack(fdt); rejected(FDT_A,"outside"); finish();

  begin("512-byte padding is relative to unaligned initrd start");
  fdt=tree_at(FDT_A,1); initrd(fdt,0xd0100001,0xd0100201,8,8); pack(fdt); accepted(FDT_A);
  CHECK(mPaddedEnd==0xd0100201); finish();
  begin("sector padding extending beyond permitted region rejected");
  fdt=tree_at(FDT_A,1); initrd(fdt,0xefffff01,0xf0000000,8,8); pack(fdt); rejected(FDT_A,"padding"); finish();
  begin("last complete sector of permitted region accepted");
  fdt=tree_at(FDT_A,1); initrd(fdt,0xeffffe00,0xf0000000,8,8); pack(fdt); accepted(FDT_A); CHECK(mPaddedEnd==0xf0000000); finish();
  begin("unmapped RAM hole initrd rejected without dereference");
  fdt=tree_at(FDT_A,1); initrd(fdt,0xbcc00000,0xbcc00200,8,8); pack(fdt); rejected(FDT_A,"outside"); finish();
  begin("initrd crossing physical RAM hole rejected");
  fdt=tree_at(FDT_A,1); initrd(fdt,0xbcbffe00,0xc0000200,8,8); pack(fdt); rejected(FDT_A,"outside"); finish();

  begin("initrd byte overlap with FDT rejected");
  fdt=tree_at(FDT_A,1); initrd(fdt,FDT_A,FDT_A+512,8,8); pack(fdt); rejected(FDT_A,"overlaps FDT"); finish();
  begin("initrd sector padding overlap with FDT rejected");
  fdt=tree_at(FDT_A+256,1); initrd(fdt,FDT_A,FDT_A+1,8,8); pack(fdt); rejected(FDT_A+256,"overlaps FDT"); finish();
  begin("disjoint bytes sharing page merge reservation");
  fdt=tree_at(FDT_B+0x100,1); initrd(fdt,FDT_B+0x800,FDT_B+0xa00,8,8); pack(fdt); accepted(FDT_B+0x100);
  CHECK(mRangeCount==1 && mRanges[0].Base==FDT_B && mRanges[0].End==FDT_B+0x1000); finish();
  begin("adjacent pages merge reservation");
  fdt=tree_at(FDT_B+0x100,1); initrd(fdt,FDT_B+0x1000,FDT_B+0x1200,8,8); pack(fdt); accepted(FDT_B+0x100);
  CHECK(mRangeCount==1 && mRanges[0].Base==FDT_B && mRanges[0].End==FDT_B+0x2000); finish();
  begin("lower initrd range sorted before FDT");
  fdt=tree_at(FDT_B+0x8100,1); initrd(fdt,FDT_B+0x1000,FDT_B+0x1200,8,8); pack(fdt); accepted(FDT_B+0x8100);
  CHECK(mRangeCount==2 && mRanges[0].Base==FDT_B+0x1000 && mRanges[1].Base==FDT_B+0x8000); finish();

  const UINT64 invalid_fdt[] = {0, FDT_A+1, FDT_A+4, 0xbcc00000ULL, 0xbffff000ULL, 0x200000000ULL, UINT64_MAX-7};
  for (size_t i=0;i<ARRAY_SIZE(invalid_fdt);++i) {
    char label[100]; snprintf(label,sizeof(label),"invalid/unmapped/unaligned FDT address 0x%" PRIx64 " rejected",invalid_fdt[i]); begin(label);
    rejected(invalid_fdt[i],"FDT header"); finish();
  }
  begin("bad FDT magic rejected");
  fdt=tree_at(FDT_A,1); fdt_set_magic(fdt,0); rejected(FDT_A,"header"); finish();
  begin("too-small FDT total rejected");
  fdt=tree_at(FDT_A,1); fdt_set_totalsize(fdt,8); rejected(FDT_A,NULL); finish();
  begin("FDT total over 16 MiB cap rejected before body access");
  fdt=tree_at(FDT_A,1); fdt_set_totalsize(fdt,16*1024*1024+1); rejected(FDT_A,"size limit"); finish();
  begin("FDT full range outside accepted board regions rejected before body access");
  fdt=tree_at(FDT_B,1); custom_one(FDT_B,0x1000); fdt_set_totalsize(fdt,0x2000); rejected(FDT_B,"blob outside"); finish();
  begin("malformed FDT structure tag rejected by real fdt_check_full");
  fdt=tree_at(FDT_A,1); pack(fdt);
  *(fdt32_t *)((char *)fdt+fdt_off_dt_struct(fdt))=cpu_to_fdt32(0xdeadbeef);
  CHECK(fdt_check_header(fdt)==0); rejected(FDT_A,"structure"); finish();
  begin("malformed property string index rejected by real fdt_check_full");
  fdt=tree_at(FDT_A,1); prop(fdt,"sample",1,4); pack(fdt);
  int poff=fdt_first_property_offset(fdt,fdt_path_offset(fdt,"/chosen")); CHECK(poff>=0);
  struct fdt_property *p=(struct fdt_property *)fdt_offset_ptr_w(fdt,poff,sizeof(*p)); CHECK(p!=NULL);
  p->nameoff=cpu_to_fdt32(fdt_size_dt_strings(fdt)+10); CHECK(fdt_check_header(fdt)==0);
  rejected(FDT_A,"structure"); finish();
  begin("truncated structure declared within valid header rejected");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_size_dt_struct(fdt,4);
  CHECK(fdt_check_header(fdt)==0); rejected(FDT_A,"structure"); finish();

  begin("misaligned structure offset rejected before native libfdt access");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_off_dt_struct(fdt,fdt_off_dt_struct(fdt)+1);
  rejected(FDT_A,NULL); finish();
  begin("two-byte misaligned structure offset rejected before native libfdt access");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_off_dt_struct(fdt,fdt_off_dt_struct(fdt)+2);
  rejected(FDT_A,NULL); finish();
  begin("misaligned reservation offset rejected before native libfdt access");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_off_mem_rsvmap(fdt,fdt_off_mem_rsvmap(fdt)+4);
  rejected(FDT_A,NULL); finish();
  begin("non-word structure size rejected before native libfdt access");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_size_dt_struct(fdt,fdt_size_dt_struct(fdt)-1);
  rejected(FDT_A,NULL); finish();
  begin("structure block overlapping header rejected");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_off_dt_struct(fdt,32);
  rejected(FDT_A,NULL); finish();
  begin("reservation block overlapping header rejected");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_off_mem_rsvmap(fdt,32);
  rejected(FDT_A,NULL); finish();
  begin("strings overlapping structure rejected");
  fdt=tree_at(FDT_A,1); prop(fdt,"sample",1,4); pack(fdt); fdt_set_off_dt_strings(fdt,fdt_off_dt_struct(fdt));
  rejected(FDT_A,NULL); finish();
  begin("structure overlapping reservation table rejected");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_off_dt_struct(fdt,fdt_off_mem_rsvmap(fdt));
  rejected(FDT_A,NULL); finish();
  begin("unterminated reservation table rejected before structure bytes");
  fdt=tree_at(FDT_A,1); pack(fdt);
  fdt64_t *reservation=(fdt64_t *)((char *)fdt+fdt_off_mem_rsvmap(fdt));
  reservation[0]=cpu_to_fdt64(1); reservation[1]=cpu_to_fdt64(1);
  rejected(FDT_A,NULL); finish();
  begin("reservation record truncated at end of FDT rejected");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_totalsize(fdt,128); fdt_set_off_mem_rsvmap(fdt,120);
  rejected(FDT_A,NULL); finish();
  begin("version 16 lacks required explicit structure length and is rejected");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_version(fdt,16); fdt_set_last_comp_version(fdt,16);
  rejected(FDT_A,NULL); finish();
  begin("future FDT version outside supported layout rejected");
  fdt=tree_at(FDT_A,1); pack(fdt); fdt_set_version(fdt,18);
  rejected(FDT_A,NULL); finish();

  // Every static live/non-input row is separately challenged as an FDT address
  // and as initrd; FDT rejection must occur before touching unmapped memory.
  for (UINTN i=0;i<board_count;++i) {
    const EFI_MEMORY_REGION_DESCRIPTOR *r=&board_snapshot[i];
    if (InputRegion(r)) continue;
    char label[140]; snprintf(label,sizeof(label),"static live region '%s' rejects FDT and initrd",r->Name); begin(label);
    CHECK(!InputRange(board_snapshot,board_count,r->Address,8));
    rejected(r->Address,"FDT header");
    fdt=tree_at(FDT_A,1); initrd(fdt,r->Address,r->Address+1,8,8); pack(fdt); rejected(FDT_A,"outside");
    finish();
  }
  begin("overlapping live row defeats conventional coverage");
  fdt=tree_at(FDT_B+0x1000,1); pack(fdt);
  custom_one(FDT_B,0x10000); custom_map[1]=custom_map[0];
  custom_map[1].Address=FDT_B+0x1000; custom_map[1].Length=0x1000; custom_map[1].MemoryType=EfiBootServicesData;
  test_count=2; rejected(FDT_B+0x1000,"live region"); finish();
  begin("page rounding below input region rejected");
  fdt=tree_at(FDT_B+0x100,1); pack(fdt); custom_one(FDT_B+0x80,0xff80);
  rejected(FDT_B+0x100,"page-rounded"); finish();
  begin("page rounding beyond input region rejected");
  fdt=tree_at(FDT_B+0x100,1); pack(fdt); custom_one(FDT_B,0x800);
  rejected(FDT_B+0x100,"page-rounded"); finish();
  begin("page rounding into adjacent live row rejected");
  fdt=tree_at(FDT_B+0x100,1); pack(fdt); custom_one(FDT_B,0x800);
  custom_map[1]=conventional(FDT_B+0x800,0x800); custom_map[1].MemoryType=EfiBootServicesData;
  test_count=2; rejected(FDT_B+0x100,"page-rounded"); finish();
  begin("initrd page rounding beyond input region rejected");
  fdt=tree_at(FDT_B+0x100,1); initrd(fdt,FDT_B+0x1100,FDT_B+0x1200,8,8); pack(fdt);
  custom_one(FDT_B,0x1800); rejected(FDT_B+0x100,"page-rounded"); finish();

  begin("exact two-range split preserves attributes and actual HOB source intervals");
  fdt=tree_at(FDT_B+0x1100,1); initrd(fdt,FDT_B+0x4080,FDT_B+0x4481,8,8); pack(fdt);
  custom_one(FDT_B,0x10000); EFI_MEMORY_REGION_DESCRIPTOR saved=custom_map[0]; accepted(FDT_B+0x1100);
  CHECK(mCount==5 && mRangeCount==2);
  expect_region(0,FDT_B,FDT_B+0x1000,Conv);
  expect_region(1,FDT_B+0x1000,FDT_B+0x2000,Reserv);
  expect_region(2,FDT_B+0x2000,FDT_B+0x4000,Conv);
  expect_region(3,FDT_B+0x4000,FDT_B+0x5000,Reserv);
  expect_region(4,FDT_B+0x5000,FDT_B+0x10000,Conv);
  CHECK(memcmp(&saved,&custom_map[0],sizeof(saved))==0);
  assert_hobs_match_map(); CHECK(resource_count==5 && allocation_count==5); finish();
  begin("reservation spanning adjacent input descriptors splits each once");
  fdt=tree_at(FDT_B+0x100,1); initrd(fdt,FDT_B+0x3000,FDT_B+0x5000,8,8); pack(fdt);
  custom_one(FDT_B,0x4000); custom_map[1]=conventional(FDT_B+0x4000,0xc000); test_count=2; accepted(FDT_B+0x100);
  CHECK(mCount==5); expect_region(0,FDT_B,FDT_B+0x1000,Reserv);
  expect_region(1,FDT_B+0x1000,FDT_B+0x3000,Conv); expect_region(2,FDT_B+0x3000,FDT_B+0x4000,Reserv);
  expect_region(3,FDT_B+0x4000,FDT_B+0x5000,Reserv); expect_region(4,FDT_B+0x5000,FDT_B+0x10000,Conv);
  assert_hobs_match_map(); finish();
  begin("actual board map protected split emits full static and dynamic HOBs");
  fdt=tree_at(FDT_B+0x1100,1); initrd(fdt,FDT_B+0x4080,FDT_B+0x4481,8,8); pack(fdt); accepted(FDT_B+0x1100);
  CHECK(mCount==board_count+4); assert_hobs_match_map(); finish();

  begin("zero map count rejected without FDT access");
  test_map=custom_map; test_count=0; rejected(FDT_B,"map count"); finish();
  begin("map count consuming required terminator rejected");
  test_map=custom_map; test_count=128; rejected(FDT_B,"map count"); finish();
  begin("output capacity failure invalidates prior success and leaves source map intact");
  fdt=tree_at(FDT_B+0x1100,1); pack(fdt); accepted(FDT_B+0x1100);
  custom_one(FDT_B,0x10000);
  for (unsigned i=1;i<127;++i) {
    custom_map[i]=board_snapshot[0]; custom_map[i].Address=0x10000ULL+i*0x10000ULL; custom_map[i].Length=0x1000;
  }
  test_count=127;
  EFI_MEMORY_REGION_DESCRIPTOR *backup=malloc(sizeof(custom_map)); CHECK(backup!=NULL); memcpy(backup,custom_map,sizeof(custom_map));
  rejected(FDT_B+0x1100,"capacity"); CHECK(memcmp(backup,custom_map,sizeof(custom_map))==0); free(backup); finish();
  begin("invalid static range rejected before input dereference");
  fdt=tree_at(FDT_B,1); pack(fdt); custom_one(FDT_B,0x10000);
  custom_map[1]=conventional(UINT64_MAX-4,16); test_count=2; rejected(FDT_B,"header outside"); finish();
  begin("fresh valid prepare recovers after rejection without stale initrd ranges");
  fdt=tree_at(FDT_A,0); pack(fdt); accepted(FDT_A);
  CHECK(mRangeCount==1 && mInitrd==0 && mInitrdEnd==0 && mPaddedEnd==0); finish();
  begin("range and page overflow helpers reject wrapping bounds");
  UINT64 end; HANDOFF_RANGE range;
  CHECK(!RangeEnd(0,1,&end) && !RangeEnd(1,0,&end) && !RangeEnd(UINT64_MAX-1,2,&end));
  CHECK(RangeEnd(1,UINT64_MAX-1,&end) && end==UINT64_MAX);
  CHECK(!PageRange(0x1000,0x1000,&range) && !PageRange(0x2000,0x1000,&range) && !PageRange(1,UINT64_MAX,&range));
  CHECK(PageRange(0x1001,0x1fff,&range) && range.Base==0x1000 && range.End==0x2000); finish();

  CHECK(munmap(a,MAP_BYTES)==0 && munmap(b,MAP_BYTES)==0);
  printf("PASS %u C scenarios; base board map unchanged after every scenario\n",checks);
  return 0;
}
