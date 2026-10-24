// SPDX-License-Identifier: BSD-2-Clause-Patent
// Executed only as a file on the embedded FAT, not as an FV executable.
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

STATIC UINTN mFailed;
STATIC VOID Check (CONST CHAR16 *Name, BOOLEAN Good) {
  Print (L"%s  %s\r\n", Good ? L"PASS" : L"FAIL", Name);
  if (!Good) { ++mFailed; }
}
STATIC VOID Pause (VOID) {
  EFI_INPUT_KEY Key;
  UINTN Index;
  Print (L"\r\nShort press power to return to UniXNU menu.\r\n");
  if (gST->ConIn == NULL) { for (;;) { gBS->Stall (100000); } }
  gST->ConIn->Reset (gST->ConIn, FALSE);
  for (;;) {
    if (EFI_ERROR (gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index))) { gBS->Stall (100000); continue; }
    if (!EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key)) && Key.UnicodeChar == L'\r') { return; }
  }
}
EFI_STATUS EFIAPI J716fIoProbeMain (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL *Loaded = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_BLOCK_IO_PROTOCOL *Block;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_FILE_PROTOCOL *Root = NULL, *File = NULL;
  EFI_GUID DiskGuid = {0x3aac9276,0x97ee,0x4442,{0x81,0xd5,0x94,0x3e,0x24,0x15,0x47,0xbb}};
  UINT8 Buffer[8194];
  UINTN Size, I;
  UINT64 Position;
  BOOLEAN Good;
  mFailed = 0;
  gST->ConOut->SetAttribute (gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
  gST->ConOut->ClearScreen (gST->ConOut);
  Print (L"J716F V9 - EXTERNAL EFI / READ-ONLY RAM FAT TEST\r\n\r\n");
  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded);
  if (EFI_ERROR (Status) || Loaded->DeviceHandle == NULL) { Check (L"loaded image device", FALSE); goto Done; }
  Status = gBS->HandleProtocol (Loaded->DeviceHandle, &gEfiDevicePathProtocolGuid, (VOID **)&Path);
  Good = !EFI_ERROR (Status) && Path->Type == HARDWARE_DEVICE_PATH && Path->SubType == HW_VENDOR_DP &&
         CompareMem (&((VENDOR_DEVICE_PATH *)Path)->Guid, &DiskGuid, sizeof (DiskGuid)) == 0;
  Check (L"loaded from J716F RAM disk device", Good);
  if (!Good) { goto Done; } // Never issue the write-denial probe on another device.
  Status = gBS->HandleProtocol (Loaded->DeviceHandle, &gEfiBlockIoProtocolGuid, (VOID **)&Block);
  Good = !EFI_ERROR (Status) && Block->Media->ReadOnly && Block->Media->BlockSize == 512 &&
         Block->Media->LastBlock == 8191 && Block->Media->MediaId == 0x716f0009;
  Check (L"4MiB read-only BlockIO geometry", Good);
  if (!Good) { goto Done; }
  Status = Block->ReadBlocks (Block, Block->Media->MediaId, 8191, 512, Buffer);
  Check (L"last sector readable", !EFI_ERROR (Status));
  Status = Block->ReadBlocks (Block, Block->Media->MediaId, 8191, 1024, Buffer);
  Check (L"out-of-range BlockIO read rejected", Status == EFI_INVALID_PARAMETER);
  Status = Block->WriteBlocks (Block, Block->Media->MediaId, 0, 512, Buffer);
  Check (L"BlockIO write rejected", Status == EFI_WRITE_PROTECTED);
  Status = gBS->HandleProtocol (Loaded->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
  if (EFI_ERROR (Status)) { Check (L"SimpleFileSystem", FALSE); goto Done; }
  Status = Fs->OpenVolume (Fs, &Root);
  if (EFI_ERROR (Status)) { Check (L"open FAT root", FALSE); goto Done; }
  Status = Root->Open (Root, &File, L"\\PATTERN.BIN", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) { Check (L"open pattern file", FALSE); goto Done; }
  Size = sizeof (Buffer);
  Status = File->Read (File, &Size, Buffer);
  Good = !EFI_ERROR (Status) && Size == 8193;
  for (I = 0; Good && I < Size; ++I) { Good = Buffer[I] == (UINT8)((I * 37 + 11) & 255); }
  Check (L"8193-byte file read and byte comparison", Good);
  Status = File->SetPosition (File, 509);
  if (!EFI_ERROR (Status)) { Size = 37; Status = File->Read (File, &Size, Buffer); }
  Good = !EFI_ERROR (Status) && Size == 37;
  for (I = 0; Good && I < Size; ++I) { Good = Buffer[I] == (UINT8)(((I + 509) * 37 + 11) & 255); }
  Check (L"seek across sector boundary", Good);
  Status = File->SetPosition (File, MAX_UINT64);
  if (!EFI_ERROR (Status)) { Status = File->GetPosition (File, &Position); }
  Good = !EFI_ERROR (Status) && Position == 8193;
  if (Good) { Size = 1; Status = File->Read (File, &Size, Buffer); Good = !EFI_ERROR (Status) && Size == 0; }
  Check (L"seek EOF and zero-byte read", Good);
  File->Close (File); File = NULL;
  Status = Root->Open (Root, &File, L"\\PATTERN.BIN", EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
  if (!EFI_ERROR (Status)) {
    Size = 1; Status = File->Write (File, &Size, Buffer);
    File->Close (File); File = NULL;
  }
  Check (L"filesystem write rejected", Status == EFI_WRITE_PROTECTED || Status == EFI_ACCESS_DENIED);
Done:
  if (File != NULL) { File->Close (File); }
  if (Root != NULL) { Root->Close (Root); }
  Print (L"\r\nV9 RESULT: %s (%u failures)\r\n", mFailed == 0 ? L"PASS" : L"FAIL", (UINT32)mFailed);
  Pause ();
  return mFailed == 0 ? EFI_SUCCESS : EFI_ABORTED;
}
