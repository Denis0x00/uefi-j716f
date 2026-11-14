// SPDX-License-Identifier: BSD-2-Clause-Patent
// No allocation, HOB access, logging or FDT writes during PrepareEarlyHandoff.
#include <Library/EarlyHandoffLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>

#define MAX_FDT_BYTES  (16 * 1024 * 1024)
#define PAGE_MASK      0xfffULL

// BaseFdtLib at pinned Mu_Basecore cfb93724 includes libfdt/fdt_check.c.
// Its complete validator is exported but not wrapped by FdtLib.h. The native
// libfdt ABI uses int and size_t (INT32 and UINTN on this AARCH64 target).
INT32 fdt_check_full (CONST VOID *Fdt, UINTN BufSize);

typedef struct { UINT64 Base; UINT64 End; } HANDOFF_RANGE;
STATIC EFI_MEMORY_REGION_DESCRIPTOR mMap[MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT];
STATIC UINT8 mCount;
STATIC HANDOFF_RANGE mRanges[2];
STATIC UINTN mRangeCount;
STATIC UINT64 mFdt;
STATIC UINT64 mFdtSize;
STATIC UINT64 mInitrd;
STATIC UINT64 mInitrdEnd;
STATIC UINT64 mPaddedEnd;
STATIC BOOLEAN mReady;
STATIC CONST CHAR8 *mReason = "not prepared";

STATIC BOOLEAN
RangeEnd (UINT64 Base, UINT64 Size, UINT64 *End)
{
  if ((Base == 0) || (Size == 0) || (Size > MAX_UINT64 - Base)) {
    return FALSE;
  }
  *End = Base + Size;
  return TRUE;
}

STATIC BOOLEAN
Overlaps (UINT64 Base, UINT64 End, UINT64 OtherBase, UINT64 OtherEnd)
{
  return (Base < OtherEnd) && (OtherBase < End);
}

// Do not infer ownership from /memory or from an arbitrary Reserved entry.
// The old Kernel staging area is already reserved. Only conventional RAM
// outside the entire PrePi heap may acquire new handoff reservations.
STATIC BOOLEAN
InputRegion (CONST EFI_MEMORY_REGION_DESCRIPTOR *Region)
{
  if ((Region->ResourceType != EFI_RESOURCE_SYSTEM_MEMORY) ||
      (Region->HobOption != AddMem) ||
      (Region->ArmAttributes == ARM_MEMORY_REGION_ATTRIBUTE_DEVICE)) {
    return FALSE;
  }
  if ((AsciiStrCmp (Region->Name, "DXE Heap") == 0) ||
      (AsciiStrCmp (Region->Name, "DXE_Heap") == 0)) {
    return FALSE;
  }
  return (Region->MemoryType == EfiConventionalMemory) ||
    ((Region->MemoryType == EfiReservedMemoryType) &&
     (AsciiStrCmp (Region->Name, "Kernel") == 0));
}

STATIC BOOLEAN
InputRange (CONST EFI_MEMORY_REGION_DESCRIPTOR *Map, UINTN Count,
            UINT64 Base, UINT64 Size)
{
  UINT64 End, Cursor, Next, RegionEnd;
  UINTN Index;
  if (!RangeEnd (Base, Size, &End)) {
    return FALSE;
  }
  // Known physical banks, including the 52 MiB hole; never SKU rounding.
  if (!(((Base >= 0x80000000ULL) && (End <= 0xbcc00000ULL)) ||
        ((Base >= 0xc0000000ULL) && (End <= 0x200000000ULL)))) {
    return FALSE;
  }
  // Explicitly reject every intersecting non-input region, even if a bad
  // overlapping table also advertises conventional RAM over that address.
  for (Index = 0; Index < Count; Index++) {
    if (!RangeEnd (Map[Index].Address, Map[Index].Length, &RegionEnd)) {
      return FALSE;
    }
    if (Overlaps (Base, End, Map[Index].Address, RegionEnd) &&
        !InputRegion (&Map[Index])) {
      return FALSE;
    }
  }
  Cursor = Base;
  while (Cursor < End) {
    Next = Cursor;
    for (Index = 0; Index < Count; Index++) {
      RegionEnd = Map[Index].Address + Map[Index].Length;
      if (InputRegion (&Map[Index]) && (Map[Index].Address <= Cursor) &&
          (Cursor < RegionEnd)) {
        Next = MAX (Next, MIN (End, RegionEnd));
      }
    }
    if (Next == Cursor) {
      return FALSE;
    }
    Cursor = Next;
  }
  return TRUE;
}

STATIC BOOLEAN
PageRange (UINT64 Base, UINT64 End, HANDOFF_RANGE *Range)
{
  if ((End <= Base) || (End > MAX_UINT64 - PAGE_MASK)) {
    return FALSE;
  }
  Range->Base = Base & ~PAGE_MASK;
  Range->End = (End + PAGE_MASK) & ~PAGE_MASK;
  return TRUE;
}

// The pinned libfdt checks blob bounds but fdt_next_tag still loads aligned
// UINT32s from off_dt_struct. Reject malformed block alignment/layout before
// invoking it, including a bounded reservation terminator. This port uses v17.
STATIC BOOLEAN
ValidateFdtLayout (CONST VOID *Fdt, UINTN Size)
{
  UINTN Structure, StructureSize, Strings, StringsSize, Reserve, Limit, Cursor;
  CONST UINT8 *Bytes = Fdt;
  if (((UINTN)Fdt & 7) || (Size < sizeof (FDT_HEADER)) ||
      (Size > MAX_FDT_BYTES) || (FdtCheckHeader (Fdt) != 0) ||
      (FdtTotalSize (Fdt) != Size) || (FdtVersion (Fdt) != 17)) {
    return FALSE;
  }
  Structure = FdtOffsetDtStruct (Fdt);
  StructureSize = FdtSizeDtStruct (Fdt);
  Strings = FdtOffsetDtStrings (Fdt);
  StringsSize = FdtSizeDtStrings (Fdt);
  Reserve = FdtOffsetMemRsvmap (Fdt);
  if ((Structure < sizeof (FDT_HEADER)) || (Structure & 3) ||
      (Structure > Size) || (StructureSize < 4) || (StructureSize & 3) ||
      (StructureSize > Size - Structure) ||
      (Strings < sizeof (FDT_HEADER)) || (Strings > Size) ||
      (StringsSize > Size - Strings) ||
      (Reserve < sizeof (FDT_HEADER)) || (Reserve & 7) ||
      (Reserve > Size - 16)) {
    return FALSE;
  }
  if ((StringsSize != 0) &&
      Overlaps (Structure, Structure + StructureSize, Strings, Strings + StringsSize)) {
    return FALSE;
  }
  if (((Reserve >= Structure) && (Reserve < Structure + StructureSize)) ||
      ((StringsSize != 0) && (Reserve >= Strings) && (Reserve < Strings + StringsSize))) {
    return FALSE;
  }
  Limit = Size;
  if (Structure > Reserve) {
    Limit = MIN (Limit, Structure);
  }
  if ((StringsSize != 0) && (Strings > Reserve)) {
    Limit = MIN (Limit, Strings);
  }
  for (Cursor = Reserve; (Cursor <= Limit) && (Limit - Cursor >= 16); Cursor += 16) {
    if (ReadUnaligned64 ((CONST UINT64 *)(Bytes + Cursor + 8)) == 0) {
      return ReadUnaligned64 ((CONST UINT64 *)(Bytes + Cursor)) == 0;
    }
  }
  return FALSE;
}

STATIC EFI_STATUS
Reject (CONST CHAR8 *Reason)
{
  mReason = Reason;
  mReady = FALSE;
  mCount = 0;
  return EFI_COMPROMISED_DATA;
}

STATIC BOOLEAN
AppendRegion (CONST EFI_MEMORY_REGION_DESCRIPTOR *Original,
              UINT64 Base, UINT64 End, BOOLEAN Reserved)
{
  if (Base == End) {
    return TRUE;
  }
  // MemoryPeim needs one trailing zero MMU descriptor.
  if ((End < Base) || (mCount >= MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT - 1)) {
    return FALSE;
  }
  mMap[mCount] = *Original;
  mMap[mCount].Address = Base;
  mMap[mCount].Length = End - Base;
  if (Reserved) {
    mMap[mCount].MemoryType = EfiReservedMemoryType;
    AsciiStrCpyS (mMap[mCount].Name, sizeof (mMap[mCount].Name), "Handoff FDT/initrd");
  }
  mCount++;
  return TRUE;
}

STATIC BOOLEAN
BuildProtectedMap (CONST EFI_MEMORY_REGION_DESCRIPTOR *Map, UINTN Count)
{
  UINTN Index, Range;
  UINT64 Cursor, End, Start, Stop;
  mCount = 0;
  for (Index = 0; Index < Count; Index++) {
    Cursor = Map[Index].Address;
    if (!RangeEnd (Cursor, Map[Index].Length, &End)) {
      return FALSE;
    }
    if (Map[Index].MemoryType == EfiConventionalMemory) {
      for (Range = 0; Range < mRangeCount; Range++) {
        Start = MAX (Cursor, mRanges[Range].Base);
        Stop = MIN (End, mRanges[Range].End);
        if (Start < Stop) {
          if (!AppendRegion (&Map[Index], Cursor, Start, FALSE) ||
              !AppendRegion (&Map[Index], Start, Stop, TRUE)) {
            return FALSE;
          }
          Cursor = Stop;
        }
      }
    }
    if (!AppendRegion (&Map[Index], Cursor, End, FALSE)) {
      return FALSE;
    }
  }
  return TRUE;
}

EFI_STATUS EFIAPI
PrepareEarlyHandoff (IN UINT64 EntryArgument)
{
  EFI_MEMORY_REGION_DESCRIPTOR *Map;
  UINT8 Count;
  CONST VOID *Fdt;
  CONST VOID *StartProp;
  CONST VOID *EndProp;
  INT32 Chosen, StartLength, EndLength;
  UINT64 End, Length;
  UINTN Index;
  HANDOFF_RANGE Swap;

  mReady = FALSE;
  mCount = 0;
  mRangeCount = 0;
  mFdt = EntryArgument;
  mFdtSize = mInitrd = mInitrdEnd = mPaddedEnd = 0;
  GetMemoryMap (&Map, &Count);
  if ((Count == 0) || (Count >= MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT)) {
    return Reject ("invalid static map count");
  }
  if ((mFdt & 7) || !InputRange (Map, Count, mFdt, sizeof (FDT_HEADER))) {
    return Reject ("FDT header outside input RAM or overlaps live region");
  }
  Fdt = (CONST VOID *)(UINTN)mFdt;
  if (FdtCheckHeader (Fdt) != 0) {
    return Reject ("invalid FDT header");
  }
  mFdtSize = FdtTotalSize (Fdt);
  if ((mFdtSize < sizeof (FDT_HEADER)) || (mFdtSize > MAX_FDT_BYTES) ||
      !InputRange (Map, Count, mFdt, mFdtSize)) {
    return Reject ("FDT blob outside input RAM or exceeds size limit");
  }
  if (!ValidateFdtLayout (Fdt, (UINTN)mFdtSize)) {
    return Reject ("invalid FDT block layout/version");
  }
  if (fdt_check_full (Fdt, (UINTN)mFdtSize) != 0) {
    return Reject ("invalid FDT structure or strings");
  }
  End = mFdt + mFdtSize;
  if (!PageRange (mFdt, End, &mRanges[mRangeCount++])) {
    return Reject ("FDT page range overflow");
  }
  Chosen = FdtPathOffset (Fdt, "/chosen");
  if (Chosen >= 0) {
    StartProp = FdtGetProp (Fdt, Chosen, "linux,initrd-start", &StartLength);
    EndProp = FdtGetProp (Fdt, Chosen, "linux,initrd-end", &EndLength);
    if ((StartProp == NULL) && (EndProp == NULL) &&
        (StartLength == -FDT_ERR_NOTFOUND) && (EndLength == -FDT_ERR_NOTFOUND)) {
      // Empty-ramdisk menu: no input volume to preserve.
    } else {
      if ((StartProp == NULL) || (EndProp == NULL) ||
          ((StartLength != 4) && (StartLength != 8)) ||
          ((EndLength != 4) && (EndLength != 8))) {
        return Reject ("incomplete or malformed initrd properties");
      }
      mInitrd = (StartLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (StartProp)) : Fdt32ToCpu (ReadUnaligned32 (StartProp));
      mInitrdEnd = (EndLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (EndProp)) : Fdt32ToCpu (ReadUnaligned32 (EndProp));
      if (mInitrdEnd < mInitrd) {
        return Reject ("reversed initrd range");
      }
      if (mInitrdEnd != mInitrd) {
        // The shared RAM disk exposes complete 512-byte sectors rounded from
        // its start address. Protect those padding bytes as well as the file.
        Length = mInitrdEnd - mInitrd;
        if ((Length > MAX_UINT64 - 511) ||
            !RangeEnd (mInitrd, (Length + 511) & ~511ULL, &mPaddedEnd) ||
            !InputRange (Map, Count, mInitrd, mPaddedEnd - mInitrd)) {
          return Reject ("initrd/padding outside input RAM or overlaps live region");
        }
        if (Overlaps (mFdt, End, mInitrd, mPaddedEnd)) {
          return Reject ("initrd/padding overlaps FDT bytes");
        }
        if (!PageRange (mInitrd, mPaddedEnd, &mRanges[mRangeCount++])) {
          return Reject ("initrd page range overflow");
        }
      }
    }
  } else if (Chosen != -FDT_ERR_NOTFOUND) {
    return Reject ("invalid chosen node");
  }
  for (Index = 0; Index < mRangeCount; Index++) {
    if (!InputRange (Map, Count, mRanges[Index].Base,
                     mRanges[Index].End - mRanges[Index].Base)) {
      return Reject ("page-rounded reservation overlaps live region or hole");
    }
  }
  if (mRangeCount == 2) {
    if (mRanges[1].Base < mRanges[0].Base) {
      Swap = mRanges[0]; mRanges[0] = mRanges[1]; mRanges[1] = Swap;
    }
    if (mRanges[1].Base <= mRanges[0].End) {
      mRanges[0].End = MAX (mRanges[0].End, mRanges[1].End);
      mRangeCount = 1;
    }
  }
  if (!BuildProtectedMap (Map, Count)) {
    return Reject ("protected map capacity or range failure");
  }
  mReason = "accepted; input pages reserved before heap allocation";
  mReady = TRUE;
  return EFI_SUCCESS;
}

VOID EFIAPI
ReportEarlyHandoff (VOID)
{
  UINTN Index;
  DEBUG ((DEBUG_ERROR, "J716FHANDOFF %a: %a\n", mReady ? "ACCEPT" : "REJECT", mReason));
  DEBUG ((DEBUG_ERROR, "J716FHANDOFF FDT=0x%lx bytes=0x%lx initrd=[0x%lx,0x%lx) padded-end=0x%lx\n",
          mFdt, mFdtSize, mInitrd, mInitrdEnd, mPaddedEnd));
  if (mReady) {
    for (Index = 0; Index < mRangeCount; Index++) {
      DEBUG ((DEBUG_ERROR, "J716FHANDOFF RESERVED [0x%lx,0x%lx)\n", mRanges[Index].Base, mRanges[Index].End));
    }
  }
}

EFI_STATUS EFIAPI
ApplyEarlyHandoffMemoryMap (
  IN OUT EFI_MEMORY_REGION_DESCRIPTOR **Map, IN OUT UINT8 *Count)
{
  if (!mReady) {
    return EFI_NOT_READY;
  }
  *Map = mMap;
  *Count = mCount;
  return EFI_SUCCESS;
}
