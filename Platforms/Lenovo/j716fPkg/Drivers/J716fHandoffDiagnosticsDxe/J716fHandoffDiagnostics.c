/** @file
  Read-only, pre-UniXNU diagnostics for the experimental J716F firmware.

  The map is a boot-services snapshot, NOT the final ExitBootServices map and
  NOT an ownership grant to XNU. Logging and later loader allocations can change
  it. No AllocateAddress, memory-type conversion, firmware-variable or disk
  writes are performed here. Only our temporary map buffer/event are released.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Protocol/AdvancedLogger.h>
#include <Protocol/GraphicsOutput.h>
#include <AdvancedLoggerInternal.h>
#include <AdvancedLoggerInternalProtocol.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryMapLib.h>
#include <Library/UefiBootServicesTableLib.h>

VOID J716fImportPreviousEarlyLog (VOID);

#define MAX_MAP_BYTES  (1024 * 1024)
#define MAX_MAP_TRIES  8
#define MAX_FDT_BYTES  (16 * 1024 * 1024)
#define MAX_INITRD_CRC_BYTES  (80 * 1024 * 1024)

typedef struct {
  EFI_MEMORY_DESCRIPTOR  *Buffer;
  UINTN                   Size;
  UINTN                   Capacity;
  UINTN                   Key;
  UINTN                   DescriptorSize;
  UINT32                  Version;
} MAP_SNAPSHOT;

STATIC BOOLEAN  mCaptured;

STATIC
CONST CHAR8 *
MemoryTypeName (IN UINT32 Type)
{
  switch (Type) {
    case EfiReservedMemoryType:      return "Reserved";
    case EfiLoaderCode:              return "LoaderCode";
    case EfiLoaderData:              return "LoaderData";
    case EfiBootServicesCode:        return "BootServicesCode";
    case EfiBootServicesData:        return "BootServicesData";
    case EfiRuntimeServicesCode:     return "RuntimeServicesCode";
    case EfiRuntimeServicesData:     return "RuntimeServicesData";
    case EfiConventionalMemory:      return "Conventional";
    case EfiUnusableMemory:          return "Unusable";
    case EfiACPIReclaimMemory:        return "ACPIReclaim";
    case EfiACPIMemoryNVS:           return "ACPINVS";
    case EfiMemoryMappedIO:          return "MMIO";
    case EfiMemoryMappedIOPortSpace: return "MMIOPort";
    case EfiPalCode:                 return "PAL";
    case EfiPersistentMemory:        return "Persistent";
    default:                        return "Unknown";
  }
}

STATIC
EFI_STATUS
CaptureMap (OUT MAP_SNAPSHOT *Map)
{
  EFI_STATUS  Status;
  UINTN       Attempt;
  UINTN       Slack;

  ZeroMem (Map, sizeof (*Map));
  for (Attempt = 0; Attempt < MAX_MAP_TRIES; Attempt++) {
    Map->Size = Map->Capacity;
    Status = gBS->GetMemoryMap (&Map->Size, Map->Buffer, &Map->Key,
                               &Map->DescriptorSize, &Map->Version);
    if (!EFI_ERROR (Status)) {
      if ((Map->Buffer == NULL) || (Map->DescriptorSize < sizeof (EFI_MEMORY_DESCRIPTOR)) ||
          (Map->Size > Map->Capacity) || (Map->Size % Map->DescriptorSize != 0)) {
        Status = EFI_COMPROMISED_DATA;
        break;
      }
      return EFI_SUCCESS;
    }
    if (Status != EFI_BUFFER_TOO_SMALL) {
      break;
    }
    // Free/reallocate can split descriptors. Reserve slack but still retry on
    // BUFFER_TOO_SMALL; do not assume the first size query remains sufficient.
    if ((Map->DescriptorSize < sizeof (EFI_MEMORY_DESCRIPTOR)) ||
        (Map->DescriptorSize > MAX_MAP_BYTES / 8)) {
      Status = EFI_COMPROMISED_DATA;
      break;
    }
    Slack = Map->DescriptorSize * 8;
    if (Map->Size > MAX_MAP_BYTES - Slack) {
      Status = EFI_OUT_OF_RESOURCES;
      break;
    }
    if (Map->Buffer != NULL) {
      FreePool (Map->Buffer);
      Map->Buffer = NULL;
    }
    Map->Capacity = Map->Size + Slack;
    Map->Buffer = AllocatePool (Map->Capacity);
    if (Map->Buffer == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }
  if (Map->Buffer != NULL) {
    FreePool (Map->Buffer);
    Map->Buffer = NULL;
  }
  return Status;
}

STATIC
BOOLEAN
DescriptorEnd (IN CONST EFI_MEMORY_DESCRIPTOR *Descriptor, OUT UINT64 *End)
{
  if ((Descriptor->NumberOfPages == 0) ||
      (Descriptor->NumberOfPages > (MAX_UINT64 - Descriptor->PhysicalStart) / EFI_PAGE_SIZE)) {
    return FALSE;
  }
  *End = Descriptor->PhysicalStart + Descriptor->NumberOfPages * EFI_PAGE_SIZE;
  return TRUE;
}

STATIC
VOID
ReportRange (IN CONST MAP_SNAPSHOT *Map, IN CONST CHAR8 *Name, IN UINT64 Base, IN UINT64 Size)
{
  EFI_MEMORY_DESCRIPTOR  Descriptor;
  UINTN                  Offset;
  UINT64                 End;
  UINT64                 Limit;
  BOOLEAN                Found;

  if ((Size == 0) || (Size > MAX_UINT64 - Base)) {
    DEBUG ((DEBUG_INFO, "J716FDIAG RANGE %a invalid base=0x%lx size=0x%lx\n", Name, Base, Size));
    return;
  }
  Limit = Base + Size;
  Found = FALSE;
  DEBUG ((DEBUG_INFO, "J716FDIAG RANGE %a [0x%lx,0x%lx)\n", Name, Base, Limit));
  for (Offset = 0; Offset < Map->Size; Offset += Map->DescriptorSize) {
    CopyMem (&Descriptor, (UINT8 *)Map->Buffer + Offset, sizeof (Descriptor));
    if (DescriptorEnd (&Descriptor, &End) && (Base < End) && (Descriptor.PhysicalStart < Limit)) {
      Found = TRUE;
      DEBUG ((DEBUG_INFO, "J716FDIAG OVERLAP %a map=%u type=%u/%a [0x%lx,0x%lx)\n",
              Name, (UINT32)(Offset / Map->DescriptorSize), Descriptor.Type,
              MemoryTypeName (Descriptor.Type), Descriptor.PhysicalStart, End));
      if (Descriptor.Type == EfiConventionalMemory) {
        DEBUG ((DEBUG_WARN, "J716FDIAG CONFLICT %a overlaps Conventional; live object is NOT reserved by this descriptor\n", Name));
      }
    }
  }
  if (!Found) {
    DEBUG ((DEBUG_WARN, "J716FDIAG RANGE %a absent from EFI snapshot; reservation unknown\n", Name));
  }
}

// A diagnostic must not dereference an arbitrary incoming x0. Require both a
// board-declared, normally mapped RAM range and EFI map coverage without RP.
// This is a conservative guard, not validation of the bootloader's RAM claims.
STATIC
BOOLEAN
CanReadFdt (IN CONST MAP_SNAPSHOT *Map, IN UINT64 Base, IN UINT64 Size)
{
  EFI_MEMORY_REGION_DESCRIPTOR  *Regions;
  EFI_MEMORY_DESCRIPTOR         Descriptor;
  UINT8                         Count;
  UINTN                         Index;
  UINTN                         Offset;
  UINT64                        Limit;
  UINT64                        Cursor;
  UINT64                        Next;
  UINT64                        End;
  BOOLEAN                       BoardRam;

  if ((Base == 0) || (Size == 0) || (Size > MAX_UINT64 - Base)) {
    return FALSE;
  }
  Limit = Base + Size;
  BoardRam = FALSE;
  GetMemoryMap (&Regions, &Count);
  for (Index = 0; Index < Count; Index++) {
    if ((Regions[Index].ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) &&
        (Regions[Index].HobOption != NoHob) &&
        (Regions[Index].ArmAttributes != ARM_MEMORY_REGION_ATTRIBUTE_DEVICE) &&
        (Regions[Index].Length <= MAX_UINT64 - Regions[Index].Address) &&
        (Base >= Regions[Index].Address) &&
        (Limit <= Regions[Index].Address + Regions[Index].Length)) {
      BoardRam = TRUE;
      break;
    }
  }
  if (!BoardRam) {
    return FALSE;
  }
  Cursor = Base;
  while (Cursor < Limit) {
    Next = Cursor;
    for (Offset = 0; Offset < Map->Size; Offset += Map->DescriptorSize) {
      CopyMem (&Descriptor, (UINT8 *)Map->Buffer + Offset, sizeof (Descriptor));
      if (DescriptorEnd (&Descriptor, &End) &&
          (Descriptor.PhysicalStart <= Cursor) && (Cursor < End)) {
        if ((Descriptor.Attribute & EFI_MEMORY_RP) ||
            (Descriptor.Type == EfiUnusableMemory) ||
            (Descriptor.Type == EfiMemoryMappedIO) ||
            (Descriptor.Type == EfiMemoryMappedIOPortSpace)) {
          return FALSE;
        }
        Next = MIN (End, Limit);
        break;
      }
    }
    if (Next == Cursor) {
      return FALSE;
    }
    Cursor = Next;
  }
  return TRUE;
}

// The checksum is optional evidence at ReadyToBoot. Fail closed before reading
// a single payload byte: only normally mapped board RAM that is wholly Reserved
// in this EFI snapshot is eligible. Conventional/loader data are not ownership
// evidence. Do not assume EFI descriptors are ordered or non-overlapping.
STATIC
BOOLEAN
CanReadInitrd (IN CONST MAP_SNAPSHOT *Map, IN UINT64 Base, IN UINT64 Size)
{
  EFI_MEMORY_REGION_DESCRIPTOR  *Regions;
  EFI_MEMORY_DESCRIPTOR         Descriptor;
  UINT8                         Count;
  UINTN                         Index;
  UINTN                         Offset;
  UINT64                        Limit;
  UINT64                        Cursor;
  UINT64                        Next;
  UINT64                        End;
  BOOLEAN                       BoardRam;
  BOOLEAN                       Found;

  if ((Base == 0) || (Size == 0) || (Size > MAX_INITRD_CRC_BYTES) ||
      (Base > MAX_UINTN) || (Size > MAX_UINTN - Base) ||
      (Map == NULL) || (Map->Buffer == NULL) || (Map->Size == 0) ||
      (Map->Size > MAX_MAP_BYTES) || (Map->Size > Map->Capacity) ||
      (Map->DescriptorSize < sizeof (Descriptor)) ||
      (Map->Size % Map->DescriptorSize != 0)) {
    return FALSE;
  }
  Limit = Base + Size;
  BoardRam = FALSE;
  GetMemoryMap (&Regions, &Count);
  for (Index = 0; (Regions != NULL) && (Index < Count); Index++) {
    if ((Regions[Index].ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) &&
        (Regions[Index].HobOption == AddMem) &&
        ((Regions[Index].ArmAttributes == ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK) ||
         (Regions[Index].ArmAttributes == ARM_MEMORY_REGION_ATTRIBUTE_WRITE_THROUGH) ||
         (Regions[Index].ArmAttributes == ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED)) &&
        (Regions[Index].Length <= MAX_UINT64 - Regions[Index].Address) &&
        (Base >= Regions[Index].Address) &&
        (Limit <= Regions[Index].Address + Regions[Index].Length)) {
      BoardRam = TRUE;
      break;
    }
  }
  if (!BoardRam) {
    return FALSE;
  }
  Cursor = Base;
  while (Cursor < Limit) {
    Found = FALSE;
    Next = Limit;
    for (Offset = 0; Offset < Map->Size; Offset += Map->DescriptorSize) {
      CopyMem (&Descriptor, (UINT8 *)Map->Buffer + Offset, sizeof (Descriptor));
      if (!DescriptorEnd (&Descriptor, &End) ||
          ((Descriptor.PhysicalStart & (EFI_PAGE_SIZE - 1)) != 0)) {
        return FALSE;
      }
      if ((Descriptor.PhysicalStart >= Limit) || (End <= Base)) {
        continue;
      }
      if ((Descriptor.Type != EfiReservedMemoryType) ||
          ((Descriptor.Attribute & EFI_MEMORY_RP) != 0)) {
        return FALSE;
      }
      if ((Descriptor.PhysicalStart <= Cursor) && (Cursor < End)) {
        if (Found) {
          return FALSE;
        }
        Found = TRUE;
        Next = MIN (Next, End);
      } else if (Descriptor.PhysicalStart > Cursor) {
        // Stop at every boundary, including a second descriptor starting in
        // the middle of the first, so overlapping entries cannot be hidden.
        Next = MIN (Next, Descriptor.PhysicalStart);
      }
    }
    if (!Found) {
      return FALSE;
    }
    Cursor = Next;
  }
  return TRUE;
}

STATIC
VOID
ReportInitrdCrc32 (IN CONST MAP_SNAPSHOT *Map, IN UINT64 Base, IN UINT64 Size)
{
  EFI_STATUS  Status;
  UINT32      Crc32;

  if ((Base == 0) || (Size == 0) || (Size > MAX_INITRD_CRC_BYTES) ||
      (Base > MAX_UINTN) || (Size > MAX_UINTN - Base)) {
    DEBUG ((DEBUG_WARN, "J716FDIAG INITRD CRC32 skipped base=0x%lx bytes=0x%lx reason=bounds\n", Base, Size));
    return;
  }
  if (!CanReadInitrd (Map, Base, Size)) {
    DEBUG ((DEBUG_WARN, "J716FDIAG INITRD CRC32 skipped base=0x%lx bytes=0x%lx reason=board-ram-or-reserved-map\n", Base, Size));
    return;
  }
  DEBUG ((DEBUG_INFO, "J716FDIAG INITRD CRC32 begin [0x%lx,0x%lx) bytes=0x%lx phase=ReadyToBoot\n",
          Base, Base + Size, Size));
  Status = gBS->CalculateCrc32 ((VOID *)(UINTN)Base, (UINTN)Size, &Crc32);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "J716FDIAG INITRD CRC32 failed [0x%lx,0x%lx) bytes=0x%lx status=%r\n",
            Base, Base + Size, Size, Status));
    return;
  }
  DEBUG ((DEBUG_INFO, "J716FDIAG INITRD CRC32 ok [0x%lx,0x%lx) bytes=0x%lx crc32=%08x phase=ReadyToBoot\n",
          Base, Base + Size, Size, Crc32));
}

STATIC
VOID
ReportFdt (IN CONST MAP_SNAPSHOT *Map)
{
  EFI_HOB_GUID_TYPE  *Hob;
  UINT64             Address;
  UINT64             Start;
  UINT64             End;
  UINT32             Size;
  VOID               *Fdt;
  CONST VOID         *StartProp;
  CONST VOID         *EndProp;
  INT32              Chosen;
  INT32              StartLength;
  INT32              EndLength;

  Hob = GetFirstGuidHob (&gSiliciumEntryArgHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) < sizeof (Address))) {
    DEBUG ((DEBUG_WARN, "J716FDIAG FDT missing/short entry-argument HOB\n"));
    return;
  }
  CopyMem (&Address, GET_GUID_HOB_DATA (Hob), sizeof (Address));
  DEBUG ((DEBUG_INFO, "J716FDIAG FDT incoming-x0=0x%lx\n", Address));
  // The FDT v17 header is 40 bytes. Check this range before any libfdt call.
  if ((Address & 7) || !CanReadFdt (Map, Address, 40)) {
    DEBUG ((DEBUG_WARN, "J716FDIAG FDT inspection skipped: pointer outside readable board RAM/map\n"));
    return;
  }
  Fdt = (VOID *)(UINTN)Address;
  if (FdtCheckHeader (Fdt) != 0) {
    DEBUG ((DEBUG_WARN, "J716FDIAG FDT invalid header\n"));
    return;
  }
  Size = FdtTotalSize (Fdt);
  if ((Size < 40) || (Size > MAX_FDT_BYTES) || !CanReadFdt (Map, Address, Size)) {
    DEBUG ((DEBUG_WARN, "J716FDIAG FDT inspection skipped: unreadable/oversize blob bytes=0x%x\n", Size));
    return;
  }
  ReportRange (Map, "incoming-fdt", Address, Size);
  Chosen = FdtPathOffset (Fdt, "/chosen");
  if (Chosen < 0) {
    DEBUG ((DEBUG_WARN, "J716FDIAG FDT /chosen missing\n"));
    return;
  }
  StartProp = FdtGetProp (Fdt, Chosen, "linux,initrd-start", &StartLength);
  EndProp = FdtGetProp (Fdt, Chosen, "linux,initrd-end", &EndLength);
  if ((StartProp == NULL) || (EndProp == NULL) ||
      ((StartLength != 4) && (StartLength != 8)) ||
      ((EndLength != 4) && (EndLength != 8))) {
    DEBUG ((DEBUG_WARN, "J716FDIAG INITRD absent/invalid address properties\n"));
    return;
  }
  Start = (StartLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (StartProp)) : Fdt32ToCpu (ReadUnaligned32 (StartProp));
  End = (EndLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (EndProp)) : Fdt32ToCpu (ReadUnaligned32 (EndProp));
  if (End <= Start) {
    DEBUG ((DEBUG_WARN, "J716FDIAG INITRD invalid [0x%lx,0x%lx)\n", Start, End));
    return;
  }
  ReportRange (Map, "incoming-initrd", Start, End - Start);
  DEBUG ((DEBUG_INFO, "J716FDIAG INITRD bytes=0x%lx sector-remainder=%u crc32-follows\n",
          End - Start, (UINT32)((End - Start) % 512)));
  ReportInitrdCrc32 (Map, Start, End - Start);
}

STATIC
VOID
ReportGop (IN CONST MAP_SNAPSHOT *Map)
{
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  EFI_STATUS                    Status;

  // Prefer the BDS-selected output handle without allocating a handle list.
  Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&Gop);
  if (EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
  }
  if (EFI_ERROR (Status) || (Gop == NULL) || (Gop->Mode == NULL) || (Gop->Mode->Info == NULL)) {
    DEBUG ((DEBUG_WARN, "J716FDIAG GOP unavailable %r\n", Status));
    return;
  }
  Info = Gop->Mode->Info;
  DEBUG ((DEBUG_INFO, "J716FDIAG GOP mode=%u max=%u width=%u height=%u stride=%u format=%u\n",
          Gop->Mode->Mode, Gop->Mode->MaxMode, Info->HorizontalResolution,
          Info->VerticalResolution, Info->PixelsPerScanLine, (UINT32)Info->PixelFormat));
  DEBUG ((DEBUG_INFO, "J716FDIAG GOP masks R=%08x G=%08x B=%08x reserved=%08x\n",
          Info->PixelInformation.RedMask, Info->PixelInformation.GreenMask,
          Info->PixelInformation.BlueMask, Info->PixelInformation.ReservedMask));
  ReportRange (Map, "gop-framebuffer", Gop->Mode->FrameBufferBase, Gop->Mode->FrameBufferSize);
}

STATIC
VOID
ReportLogger (IN CONST MAP_SNAPSHOT *Map)
{
  ADVANCED_LOGGER_PROTOCOL  *Protocol;
  ADVANCED_LOGGER_INFO      *Info;
  EFI_STATUS                Status;

  Status = gBS->LocateProtocol (&gAdvancedLoggerProtocolGuid, NULL, (VOID **)&Protocol);
  if (EFI_ERROR (Status) || (Protocol == NULL) || (Protocol->Signature != ADVANCED_LOGGER_PROTOCOL_SIGNATURE) ||
      (Protocol->Version != ADVANCED_LOGGER_PROTOCOL_VERSION)) {
    DEBUG ((DEBUG_WARN, "J716FDIAG LOGGER protocol unavailable/incompatible\n"));
    return;
  }
  // Same pinned Mu internal container used by AdvancedLoggerAccessLib.
  Info = LOGGER_INFO_FROM_PROTOCOL (Protocol);
  if ((Info == NULL) || (Info->Signature != ADVANCED_LOGGER_SIGNATURE) ||
      (Info->Version != ADVANCED_LOGGER_INFO_VER) ||
      (Info->LogBufferOffset != EXPECTED_LOG_BUFFER_OFFSET (Info))) {
    DEBUG ((DEBUG_WARN, "J716FDIAG LOGGER layout unavailable/incompatible\n"));
    return;
  }
  ReportRange (Map, "advanced-logger", (UINTN)Info,
               (UINT64)Info->LogBufferOffset + Info->LogBufferSize);
  DEBUG ((DEBUG_INFO, "J716FDIAG LOGGER version=%u current-offset=0x%x discarded=0x%x\n",
          (UINT32)Info->Version, Info->LogCurrentOffset, Info->DiscardedSize));
}

STATIC
VOID
EFIAPI
OnReadyToBoot (IN EFI_EVENT Event, IN VOID *Context)
{
  MAP_SNAPSHOT                   Map;
  EFI_STATUS                     Status;
  EFI_MEMORY_DESCRIPTOR          Descriptor;
  EFI_MEMORY_REGION_DESCRIPTOR   *Regions;
  EFI_HOB_HANDOFF_INFO_TABLE      *HobList;
  UINT8                          Count;
  UINTN                          Offset;
  UINTN                          Index;
  UINT64                         End;

  (VOID)Context;
  if (mCaptured) {
    return;
  }
  mCaptured = TRUE;
  gBS->CloseEvent (Event);
  // Consume only the SEC snapshot; never read or modify last-boot RAM here.
  J716fImportPreviousEarlyLog ();
  DEBUG ((DEBUG_INFO, "J716FDIAG BEGIN v1 phase=ReadyToBoot-before-UniXNU\n"));
  Status = CaptureMap (&Map);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "J716FDIAG MAP capture failed %r\n", Status));
    return;
  }
  DEBUG ((DEBUG_INFO, "J716FDIAG MAP key=0x%lx bytes=0x%lx stride=%u version=%u count=%u\n",
          (UINT64)Map.Key, (UINT64)Map.Size, (UINT32)Map.DescriptorSize,
          Map.Version, (UINT32)(Map.Size / Map.DescriptorSize)));
  DEBUG ((DEBUG_INFO, "J716FDIAG NOTE snapshot only; Conventional is not an XNU ownership grant; map buffer remains allocated\n"));
  for (Offset = 0; Offset < Map.Size; Offset += Map.DescriptorSize) {
    CopyMem (&Descriptor, (UINT8 *)Map.Buffer + Offset, sizeof (Descriptor));
    if (!DescriptorEnd (&Descriptor, &End)) {
      DEBUG ((DEBUG_WARN, "J716FDIAG MAP[%u] invalid descriptor range\n", (UINT32)(Offset / Map.DescriptorSize)));
      continue;
    }
    DEBUG ((DEBUG_INFO, "J716FDIAG MAP[%u] type=%u/%a [0x%lx,0x%lx) pages=0x%lx attr=0x%lx virt=0x%lx\n",
            (UINT32)(Offset / Map.DescriptorSize), Descriptor.Type, MemoryTypeName (Descriptor.Type),
            Descriptor.PhysicalStart, End, Descriptor.NumberOfPages, Descriptor.Attribute, Descriptor.VirtualStart));
  }
  ReportRange (&Map, "diagnostic-map-buffer", (UINTN)Map.Buffer, Map.Capacity);
  ReportRange (&Map, "system-table", (UINTN)gST, gST->Hdr.HeaderSize);
  HobList = GetHobList ();
  if ((HobList != NULL) && (HobList->Header.HobType == EFI_HOB_TYPE_HANDOFF) &&
      (HobList->EfiEndOfHobList >= (UINTN)HobList) &&
      (HobList->EfiEndOfHobList - (UINTN)HobList < MAX_MAP_BYTES)) {
    ReportRange (&Map, "hob-list", (UINTN)HobList,
                 HobList->EfiEndOfHobList - (UINTN)HobList + sizeof (EFI_HOB_GENERIC_HEADER));
  }
  ReportGop (&Map);
  ReportLogger (&Map);
  ReportFdt (&Map);
  // Static platform descriptions are printed separately from the EFI snapshot.
  // Even a region named XNU Window is only a proposal until loader allocation.
  GetMemoryMap (&Regions, &Count);
  for (Index = 0; Index < Count; Index++) {
    if ((Regions[Index].ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) ||
        (Regions[Index].ResourceType == EFI_RESOURCE_MEMORY_RESERVED)) {
      DEBUG ((DEBUG_INFO, "J716FDIAG PLATFORM %a base=0x%lx bytes=0x%lx hob=%u resource=%u type=%u/%a\n",
              Regions[Index].Name, Regions[Index].Address, Regions[Index].Length,
              (UINT32)Regions[Index].HobOption, (UINT32)Regions[Index].ResourceType,
              (UINT32)Regions[Index].MemoryType, MemoryTypeName (Regions[Index].MemoryType)));
    }
  }
  DEBUG ((DEBUG_INFO, "J716FDIAG END snapshot; freeing diagnostic buffer now; later allocations and ExitBootServices map differ\n"));
  FreePool (Map.Buffer);
}

EFI_STATUS
EFIAPI
J716fHandoffDiagnosticsInitialize (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT   Event;
  EFI_STATUS  Status;

  (VOID)ImageHandle;
  (VOID)SystemTable;
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnReadyToBoot, NULL,
                              &gEfiEventReadyToBootGuid, &Event);
  DEBUG ((DEBUG_INFO, "J716FDIAG ReadyToBoot registration %r\n", Status));
  return Status;
}
