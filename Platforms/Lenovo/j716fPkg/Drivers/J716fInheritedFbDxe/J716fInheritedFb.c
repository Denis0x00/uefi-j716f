/** @file
  J716F inherited framebuffer GOP, based on reAAAq/uefi-umi InheritedFb.c
  (727f6c26347055ab70eac2104e6a4edc80c8d556), BSD-2-Clause-Patent.
  Candidate geometry is from this board's panel XML and existing diagnostics.
  Scanout, stride and colour order remain subject to hardware verification.
  No clock, panel, DSI, PMIC or SMMU programming is performed.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Configuration/BootDevices.h>

STATIC FRAME_BUFFER_CONFIGURE *mBlt;
STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mInfo;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode;

STATIC EFI_STATUS EFIAPI QueryMode (
  EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber,
  UINTN *SizeOfInfo, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)
{
  if ((This == NULL) || (SizeOfInfo == NULL) || (Info == NULL) || (ModeNumber != 0)) {
    return EFI_INVALID_PARAMETER;
  }
  *Info = AllocateCopyPool (sizeof (mInfo), &mInfo);
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  *SizeOfInfo = sizeof (mInfo);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI Blt (
  EFI_GRAPHICS_OUTPUT_PROTOCOL *This, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer,
  EFI_GRAPHICS_OUTPUT_BLT_OPERATION Operation, UINTN SourceX, UINTN SourceY,
  UINTN DestinationX, UINTN DestinationY, UINTN Width, UINTN Height, UINTN Delta)
{
  EFI_STATUS Status;
  EFI_TPL OldTpl;
  UINTN Offset, Bytes;
  if ((This == NULL) || (mBlt == NULL) || (Operation >= EfiGraphicsOutputBltOperationMax) ||
      ((Buffer == NULL) && (Operation != EfiBltVideoToVideo))) {
    return EFI_INVALID_PARAMETER;
  }
  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  Status = FrameBufferBlt (mBlt, Buffer, Operation, SourceX, SourceY,
                          DestinationX, DestinationY, Width, Height, Delta);
  // Only flush a successfully validated destination rectangle in framebuffer RAM.
  if (!EFI_ERROR (Status) && (Operation != EfiBltVideoToBltBuffer) &&
      (Width > 0) && (Height > 0)) {
    Offset = (DestinationY * mInfo.PixelsPerScanLine + DestinationX) * 4;
    Bytes = ((Height - 1) * mInfo.PixelsPerScanLine + Width) * 4;
    WriteBackDataCacheRange ((VOID *)(UINTN)(mMode.FrameBufferBase + Offset), Bytes);
  }
  gBS->RestoreTPL (OldTpl);
  return Status;
}

STATIC EFI_STATUS EFIAPI SetMode (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = { 0, 0, 0, 0 };
  if (This == NULL) { return EFI_INVALID_PARAMETER; }
  if (ModeNumber != 0) { return EFI_UNSUPPORTED; }
  return Blt (This, &Black, EfiBltVideoFill, 0, 0, 0, 0,
              mInfo.HorizontalResolution, mInfo.VerticalResolution, 0);
}

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGop = { QueryMode, SetMode, Blt, &mMode };

EFI_STATUS EFIAPI J716fInheritedFbEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  EFI_STATUS Status;
  EFI_HANDLE Handle = NULL;
  UINTN Size = 0;

  ZeroMem (&Region, sizeof (Region));
  Status = LocateMemoryRegionByName ("Display Reserved", &Region);
  if (EFI_ERROR (Status)) { return Status; }
  mInfo.HorizontalResolution = FixedPcdGet32 (PcdFrameBufferWidth);
  mInfo.VerticalResolution = FixedPcdGet32 (PcdFrameBufferHeight);
  mInfo.PixelsPerScanLine = mInfo.HorizontalResolution;
  mInfo.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  if ((Region.Address != 0x9C000000) ||
      (mInfo.HorizontalResolution != 2560) || (mInfo.VerticalResolution != 1600) ||
      (FixedPcdGet8 (PcdFrameBufferColorDepth) != 32)) {
    return EFI_UNSUPPORTED;
  }
  mMode.FrameBufferBase = Region.Address;
  mMode.FrameBufferSize = (UINTN)mInfo.PixelsPerScanLine * mInfo.VerticalResolution * 4;
  if (Region.Length < mMode.FrameBufferSize) { return EFI_UNSUPPORTED; }
  mMode.MaxMode = 1;
  mMode.Info = &mInfo;
  mMode.SizeOfInfo = sizeof (mInfo);
  Status = FrameBufferBltConfigure ((VOID *)(UINTN)Region.Address, &mInfo, NULL, &Size);
  if ((Status != EFI_BUFFER_TOO_SMALL) || (Size == 0)) { return EFI_DEVICE_ERROR; }
  mBlt = AllocatePool (Size);
  if (mBlt == NULL) { return EFI_OUT_OF_RESOURCES; }
  Status = FrameBufferBltConfigure ((VOID *)(UINTN)Region.Address, &mInfo, mBlt, &Size);
  if (!EFI_ERROR (Status)) {
    Status = gBS->InstallMultipleProtocolInterfaces (&Handle,
               &gEfiDevicePathProtocolGuid, &DisplayDevicePath,
               &gEfiGraphicsOutputProtocolGuid, &mGop, NULL);
  }
  if (EFI_ERROR (Status)) {
    FreePool (mBlt);
    mBlt = NULL;
    return Status;
  }
  DEBUG ((DEBUG_ERROR, "J716F V8 INHERITED_GOP 9C000000 2560x1600 BGRX32; scanout unverified\n"));
  return EFI_SUCCESS;
}
