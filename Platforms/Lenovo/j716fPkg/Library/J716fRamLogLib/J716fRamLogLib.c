// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Uefi.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PrintLib.h>
#include <Library/RamLogLib.h>
#include "DisplayOwnership.h"
#include <J716fEarlyLog.h>

#define J716F_LOG_MAGIC       0x474F4C4A // 'JLOG'
#define J716F_LOG_BASE        0xB0000000ULL
#define J716F_LOG_REGION_SIZE 0x00080000U
// Keep the existing 16-byte header and payload capacity for existing readers.
#define J716F_LOG_MAX         (J716F_LOG_REGION_SIZE - 32)

typedef struct {
  UINT32 Magic;
  UINT32 Length;
  UINT32 MaxCapacity;
  UINT32 BootSequence; // Previously reserved; increments if a valid header survives.
  CHAR8  Buffer[];
} J716F_BOOT_LOG_HDR;

STATIC_ASSERT (sizeof (J716F_BOOT_LOG_HDR) == 16, "JLOG header layout changed");
STATIC_ASSERT (sizeof (J716F_BOOT_LOG_HDR) + J716F_LOG_MAX + 1 <= J716F_LOG_REGION_SIZE,
               "JLOG payload/terminator exceeds the boot-log reservation");

// The existing JLOG header/capacity are unchanged. The last possible payload
// terminator is at region+0x7fff0; owner state is [region+0x7fff4,region+0x80000).
// This is the actual 512 KiB Boot Log reservation, never display/dfps RAM.
#define J716F_OWNER_MAGIC  0x4E574F4A // 'JOWN'
typedef struct {
  UINT32 Magic;
  UINT32 BootSequence;
  UINT32 Lease; // even=serial, odd=graphics; never wraps in this boot
} J716F_DISPLAY_OWNER;
#define J716F_OWNER_OFFSET (J716F_LOG_REGION_SIZE - sizeof (J716F_DISPLAY_OWNER))
STATIC_ASSERT (sizeof (J716F_DISPLAY_OWNER) == 12, "Owner tail size changed");
STATIC_ASSERT (J716F_OWNER_OFFSET % sizeof (UINT32) == 0, "Owner tail unaligned");
STATIC_ASSERT (sizeof (J716F_BOOT_LOG_HDR) + J716F_LOG_MAX + 1 <= J716F_OWNER_OFFSET,
               "Owner overlaps existing JLOG header/payload/terminator");

STATIC
volatile J716F_BOOT_LOG_HDR *
GetLogHeader (VOID)
{
  EFI_MEMORY_REGION_DESCRIPTOR Region;

  ZeroMem (&Region, sizeof (Region));
  if (EFI_ERROR (LocateMemoryRegionByName ("Boot Log", &Region)) ||
      (Region.Address != J716F_LOG_BASE) || (Region.Length != J716F_LOG_REGION_SIZE) ||
      (Region.MemoryType != EfiReservedMemoryType)) {
    return NULL;
  }
  return (volatile J716F_BOOT_LOG_HDR *)(UINTN)Region.Address;
}

STATIC
volatile J716F_DISPLAY_OWNER *
GetDisplayOwner (VOID)
{
  volatile J716F_BOOT_LOG_HDR *Hdr = GetLogHeader ();
  volatile J716F_DISPLAY_OWNER *Owner;
  if ((Hdr == NULL) || (Hdr->Magic != J716F_LOG_MAGIC) ||
      (Hdr->MaxCapacity != J716F_LOG_MAX) || (Hdr->Length > J716F_LOG_MAX) ||
      (Hdr->BootSequence == 0)) {
    return NULL;
  }
  Owner = (volatile J716F_DISPLAY_OWNER *)((UINT8 *)(UINTN)Hdr + J716F_OWNER_OFFSET);
  MemoryFence ();
  if ((Owner->Magic != J716F_OWNER_MAGIC) || (Owner->BootSequence != Hdr->BootSequence)) {
    return NULL;
  }
  return Owner;
}

BOOLEAN EFIAPI J716fSerialMayDraw (VOID)
{
  volatile J716F_DISPLAY_OWNER *Owner = GetDisplayOwner ();
  return (Owner != NULL) && ((Owner->Lease & 1U) == 0);
}

BOOLEAN EFIAPI J716fDisplayClaim (OUT UINT64 *Token)
{
  volatile J716F_DISPLAY_OWNER *Owner;
  BOOLEAN Interrupts;
  BOOLEAN Success = FALSE;
  if (Token == NULL) {
    return FALSE;
  }
  *Token = 0;
  Interrupts = ArmGetInterruptState ();
  ArmDisableInterrupts ();
  Owner = GetDisplayOwner ();
  if ((Owner != NULL) && ((Owner->Lease & 1U) == 0) && (Owner->Lease < 0xFFFFFFFEU)) {
    Owner->Lease++;
    MemoryFence ();
    *Token = ((UINT64)Owner->BootSequence << 32) | Owner->Lease;
    Success = TRUE;
  }
  if (Interrupts) {
    ArmEnableInterrupts ();
  }
  return Success;
}

BOOLEAN EFIAPI J716fDisplayRelease (IN UINT64 Token)
{
  volatile J716F_DISPLAY_OWNER *Owner;
  BOOLEAN Interrupts = ArmGetInterruptState ();
  BOOLEAN Success = FALSE;
  ArmDisableInterrupts ();
  Owner = GetDisplayOwner ();
  if ((Owner != NULL) && ((Owner->Lease & 1U) != 0) && (Owner->Lease != 0xFFFFFFFFU) &&
      (Token == (((UINT64)Owner->BootSequence << 32) | Owner->Lease))) {
    Owner->Lease++;
    MemoryFence ();
    Success = TRUE;
  }
  if (Interrupts) {
    ArmEnableInterrupts ();
  }
  return Success;
}

VOID
EFIAPI
RamLogReset (VOID)
{
  volatile J716F_BOOT_LOG_HDR *Hdr;
  UINT32                     Sequence;
  J716F_EL_HEADER            PreviousXnu;
  CHAR8                      Marker[80];
  UINTN                      MarkerLength;
  volatile J716F_DISPLAY_OWNER *Owner;

  Hdr = GetLogHeader ();
  if (Hdr == NULL) {
    return;
  }
  Owner = (volatile J716F_DISPLAY_OWNER *)((UINT8 *)(UINTN)Hdr + J716F_OWNER_OFFSET);
  Owner->Magic = 0;
  MemoryFence ();
  Sequence = 1;
  // A valid XNU epoch supplies the previous sequence, never stale tail bytes.
  if (J716fEarlyLogHeaderValidate ((volatile J716F_EL_U8 *)Hdr, J716F_EL_TOTAL_BYTES, &PreviousXnu) == J716F_EL_OK) {
    Sequence = PreviousXnu.boot_sequence + 1;
    if (Sequence == 0) {
      Sequence = 1;
    }
  }
  if ((Hdr->Magic == J716F_LOG_MAGIC) && (Hdr->MaxCapacity == J716F_LOG_MAX) &&
      (Hdr->Length <= J716F_LOG_MAX)) {
    Sequence = Hdr->BootSequence + 1;
    if (Sequence == 0) {
      Sequence = 1;
    }
  }

  // SEC is the only reset caller. Do not clear the entire reserved buffer:
  // Length and the terminator delimit this boot, regardless of stale tail bytes.
  Hdr->Magic       = 0;
  Hdr->Length      = 0;
  Hdr->MaxCapacity = J716F_LOG_MAX;
  Hdr->BootSequence = Sequence;
  Hdr->Buffer[0]   = '\0';
  MemoryFence ();
  Hdr->Magic = J716F_LOG_MAGIC;
  MemoryFence ();
  Owner->BootSequence = Sequence;
  Owner->Lease = 0;
  MemoryFence ();
  Owner->Magic = J716F_OWNER_MAGIC;
  MemoryFence ();

  MarkerLength = AsciiSPrint (Marker, sizeof (Marker), "J716F SEC BOOT START seq=%08x\n", Sequence);
  RamLogWrite ((CONST UINT8 *)Marker, MarkerLength);
}

VOID
EFIAPI
RamLogWrite (
  IN CONST UINT8 *Buffer,
  IN UINTN        NumberOfBytes)
{
  volatile J716F_BOOT_LOG_HDR *Hdr;
  UINT32                     Length;
  UINTN                      Available;
  UINTN                      ToCopy;

  if ((Buffer == NULL) || (NumberOfBytes == 0)) {
    return;
  }
  Hdr = GetLogHeader ();
  // A writer never repairs/resets a header. Invalid or legacy headers must wait
  // for the one SEC initialization, so later module constructors cannot erase it.
  if ((Hdr == NULL) || (Hdr->Magic != J716F_LOG_MAGIC) ||
      (Hdr->MaxCapacity != J716F_LOG_MAX) || (Hdr->BootSequence == 0)) {
    return;
  }
  Length = Hdr->Length;
  if (Length >= J716F_LOG_MAX) {
    return;
  }

  // Preserve the beginning of this boot. Once full, drop new bytes until the
  // next explicit SEC reset; do not wrap or narrow a large UINTN before clipping.
  Available = J716F_LOG_MAX - Length;
  ToCopy = (NumberOfBytes < Available) ? NumberOfBytes : Available;
  CopyMem ((VOID *)&Hdr->Buffer[Length], Buffer, ToCopy);
  Hdr->Buffer[Length + ToCopy] = '\0';
  MemoryFence ();
  Hdr->Length = Length + (UINT32)ToCopy;
  MemoryFence ();
}
