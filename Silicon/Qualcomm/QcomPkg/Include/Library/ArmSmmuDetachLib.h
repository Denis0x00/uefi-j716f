#ifndef _ARM_SMMU_DETACH_LIB_H_
#define _ARM_SMMU_DETACH_LIB_H_

/**
  This Function Detaches all IOMMU Domains from the SMMU.

  @param[in] SkipStreams                   - The SIDs to be Skiped.
  @param[in] SkipStreamCount               - The Number of SIDs to be Skiped.
**/
VOID
ArmSmmuDetach (
  IN CONST UINT16 *SkipStreams,
  IN UINT8         SkipStreamCount
  );

// TB-J716F diagnostic handoff: additionally preserve actual CB2/CB3 routes
// and context registers. This function does not grant DMA ownership.
VOID
ArmSmmuDetachR35 (
  IN CONST UINT16 *SkipStreams,
  IN UINT8 SkipStreamCount,
  IN BOOLEAN PreserveUfs,
  IN UINT32 UfsSlot0,
  IN UINT32 UfsSlot1
  );

#endif /* _ARM_SMMU_DETACH_LIB_H_ */
