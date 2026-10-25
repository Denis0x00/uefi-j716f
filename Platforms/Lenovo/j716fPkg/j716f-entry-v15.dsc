##
#  Copyright (c) 2011 - 2022, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.
#  Copyright (c) 2018, Bingxing Wang. All rights reserved.
#  Copyright (c) Microsoft Corporation.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = j716f
  PLATFORM_GUID                  = F8033398-C96B-45AC-B35D-10223A06A3A6
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/j716fEntryV15Pkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = j716fPkg/j716f-entry-v15.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  #
  # 0 = SM8250
  # 1 = SM8250-AB
  # 2 = SM8250-AC
  #
  SOC_TYPE                       = 2

!include KonaPkg/KonaPkg.dsc.inc

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  #
  # UEFI Stack
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0x9FF90000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Lenovo"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"Xiaoxin Pad Pro 2021"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"j716f"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"TB-J716F"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"Xiaoxin Pad Pro 2021"

  #
  # Simple Frame Buffer
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|2560
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|1600
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

  #
  # Platform PEI
  #
  gQcomPkgTokenSpaceGuid.PcdPlatformType|"LA"

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|j716fPkg/Library/J716fWindowMemoryMapLib/MemoryMapLib.inf
  SerialPortLib|j716fPkg/Library/J716fFrameBufferSerialPortLib/FrameBufferSerialPortLib.inf

  #
  # QCOM Libraries
  #
  ConfigurationMapLib|j716fPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

[Components]
  #
  # ACPI Tables
  #


[LibraryClasses]
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf

[Components]
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf

# V8: firmware-volume selection is source-only. Inherited ABL display state.
[PcdsFixedAtBuild]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"J716F V15 XNU ENTRY"
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDebugLevel|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleInRamSupport|FALSE

[PcdsFeatureFlag]
  gEfiMdeModulePkgTokenSpaceGuid.PcdSupportUpdateCapsuleReset|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSupportProcessCapsuleAtRuntime|FALSE

[LibraryClasses]
  PlatformSecLib|j716fPkg/Library/J716fMinimalPlatformSecLib/J716fMinimalPlatformSecLib.inf
  PlatformPeiLib|SiliciumNullPkg/Library/PlatformPeiLibNull/PlatformPeiLibNull.inf
  UniXNULib|j716fPkg/Library/J716fEntryTraceUniXNULib/UniXNULib.inf
  SerialPortLib|SiliciumPkg/Library/FrameBufferSerialPortLib/FrameBufferSerialPortLib.inf
  DeviceBootManagerLib|SiliciumNullPkg/Library/PlatformBootManagerLibNull/PlatformBootManagerLibNull.inf
  PlatformBootOptionsLib|SiliciumNullPkg/Library/PlatformBootOptionsLibNull/PlatformBootOptionsLibNull.inf
  AcpiTableUpdateLib|SiliciumNullPkg/Library/AcpiTableUpdateLibNull/AcpiTableUpdateLibNull.inf
  RamManagerLib|SiliciumNullPkg/Library/RamManagerLibNull/RamManagerLibNull.inf

[Components]
  j716fPkg/Drivers/J716fInheritedFbDxe/J716fInheritedFbDxe.inf
  j716fPkg/Drivers/J716fIoConsoleBdsDxe/J716fIoConsoleBdsDxe.inf

[Components]
  j716fPkg/Drivers/J716fKeysDxe/J716fKeysDxe.inf
  j716fPkg/Drivers/J716fEntryFatDxe/J716fEntryFatDxe.inf
  # Compiled for FAT packaging; never selected as an executable in the FV.

[Components]
  DarwinBootPkg/Driver/DarwinBoot.inf
