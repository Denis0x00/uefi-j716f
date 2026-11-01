// SPDX-License-Identifier: BSD-2-Clause-Patent
// Only the embedded test FAT is exposed; no physical disk access.
#include <Uefi.h>
#include <Pi/PiFirmwareFile.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC UINT8 *mBase;
STATIC EFI_GUID mPayload = {0x90f40472,0x1975,0x40e2,{0x80,0x28,0xc7,0x6d,0x53,0x0d,0x15,0xa7}};
STATIC EFI_BLOCK_IO_MEDIA mMedia;
STATIC struct { VENDOR_DEVICE_PATH Vendor; EFI_DEVICE_PATH_PROTOCOL End; } mPath = {
  {{HARDWARE_DEVICE_PATH, HW_VENDOR_DP, {sizeof (VENDOR_DEVICE_PATH), 0}},
   {0x3aac9276,0x97ee,0x4442,{0x81,0xd5,0x94,0x3e,0x24,0x15,0x47,0xbb}}},
  {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {4, 0}}
};
STATIC EFI_STATUS EFIAPI Reset (EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN Extended) { return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Flush (EFI_BLOCK_IO_PROTOCOL *This) { return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Read (EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba, UINTN Size, VOID *Buffer) {
  UINT64 Blocks;
  if (MediaId != mMedia.MediaId) { return EFI_MEDIA_CHANGED; }
  if (Size == 0) { return EFI_SUCCESS; }
  if (Buffer == NULL) { return EFI_INVALID_PARAMETER; }
  if (Size % 512 != 0) { return EFI_BAD_BUFFER_SIZE; }
  Blocks = Size / 512;
  if (Lba > mMedia.LastBlock || Blocks > mMedia.LastBlock - Lba + 1) { return EFI_INVALID_PARAMETER; }
  CopyMem (Buffer, mBase + (UINTN)Lba * 512, Size);
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Write (EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba, UINTN Size, VOID *Buffer) {
  return MediaId == mMedia.MediaId ? EFI_WRITE_PROTECTED : EFI_MEDIA_CHANGED;
}
STATIC EFI_BLOCK_IO_PROTOCOL mBlock = { EFI_BLOCK_IO_PROTOCOL_REVISION, &mMedia, Reset, Read, Write, Flush };

EFI_STATUS EFIAPI J716fNativeFatInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;
  UINTN Size = 0;
  // GetSectionFromAnyFv allocates BootServicesData. Keep it until firmware exits.
  Status = GetSectionFromAnyFv (&mPayload, EFI_SECTION_RAW, 0, (VOID **)&mBase, &Size);
  if (EFI_ERROR (Status)) { return Status; }
  // Accept only the locally generated 64 MiB FAT32 superfloppy.
  if (Size != 0x4000000 || mBase[510] != 0x55 || mBase[511] != 0xaa ||
      mBase[11] != 0 || mBase[12] != 2 || mBase[13] != 1 ||
      mBase[16] != 2 || mBase[19] != 0 || mBase[20] != 0 ||
      mBase[22] != 0 || mBase[23] != 0 ||
      mBase[32] != 0 || mBase[33] != 0 || mBase[34] != 2 || mBase[35] != 0 ||
      CompareMem (mBase + 82, "FAT32   ", 8) != 0) {
    FreePool (mBase); mBase = NULL; return EFI_VOLUME_CORRUPTED;
  }
  mMedia.MediaId = 0x716f1001;
  mMedia.MediaPresent = TRUE; mMedia.ReadOnly = TRUE;
  mMedia.BlockSize = 512; mMedia.IoAlign = 1; mMedia.LastBlock = Size / 512 - 1;
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiBlockIoProtocolGuid, &mBlock,
                                                   &gEfiDevicePathProtocolGuid, &mPath, NULL);
  if (EFI_ERROR (Status)) { FreePool (mBase); mBase = NULL; }
  DEBUG ((DEBUG_ERROR, "J716F NATIVE FAT 64MiB readonly: %r\n", Status));
  return Status;
}
