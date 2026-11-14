// Captures the arguments emitted by the actual production AddHob function.
typedef struct { UINT64 base, size; UINT32 type; UINT64 attrs; } TEST_HOB;
static TEST_HOB resource_hobs[128], allocation_hobs[128];
static size_t resource_count, allocation_count;
static void BuildResourceDescriptorHob(UINT32 type, UINT64 attrs, UINT64 base, UINT64 size) {
  if (resource_count >= ARRAY_SIZE(resource_hobs)) abort();
  resource_hobs[resource_count++] = (TEST_HOB){base, size, type, attrs};
}
static void BuildMemoryAllocationHob(UINT64 base, UINT64 size, UINT32 type) {
  if (allocation_count >= ARRAY_SIZE(allocation_hobs)) abort();
  allocation_hobs[allocation_count++] = (TEST_HOB){base, size, type, 0};
}
