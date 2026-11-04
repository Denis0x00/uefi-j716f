##
#  Copyright (c) 2011 - 2022, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.
#  Copyright (c) 2018, Bingxing Wang. All rights reserved.
#  Copyright (c) Microsoft Corporation.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = j716f
  PLATFORM_GUID                  = F8033398-C96B-45AC-B35D-10223A06A3A6
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/j716fPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = j716fPkg/j716f.fdf
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

  #
  # Debug and Version
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"J716F UniXNU"
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDebugLevel|0
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDeviceModel|"j716f"
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerPages|0x100
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerBase|0x00000000
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleInRamSupport|FALSE

[PcdsFeatureFlag]
  gEfiMdeModulePkgTokenSpaceGuid.PcdSupportUpdateCapsuleReset|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSupportProcessCapsuleAtRuntime|FALSE

[LibraryClasses.common]
  MemoryMapLib|j716fPkg/Library/MemoryMapLib/MemoryMapLib.inf
  SerialPortLib|j716fPkg/Library/J716fFrameBufferSerialPortLib/FrameBufferSerialPortLib.inf
  ConfigurationMapLib|j716fPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  AdvancedLoggerHdwPortLib|AdvLoggerPkg/Library/AdvancedLoggerHdwPortLib/AdvancedLoggerHdwPortLib.inf
  AdvancedLoggerAccessLib|AdvLoggerPkg/Library/AdvancedLoggerAccessLib/AdvancedLoggerAccessLib.inf
  QrEncoderLib|MsGraphicsPkg/Library/QrEncoderLib/QrEncoderLib.inf
  AssertLib|AdvLoggerPkg/Library/AssertLib/AssertLib.inf

[LibraryClasses.common.SEC]
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf

[LibraryClasses.common.DXE_CORE]
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf

[LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.UEFI_DRIVER]
  # AdvancedLoggerLib removed

[LibraryClasses.common.DXE_RUNTIME_DRIVER]
  # AdvancedLoggerLib runtime removed

[Components]
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf
  UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf
  DarwinBootPkg/Driver/DarwinBoot.inf
  j716fPkg/Drivers/J716fInheritedFbDxe/J716fInheritedFbDxe.inf
  j716fPkg/Drivers/J716fKeysDxe/J716fKeysDxe.inf
  j716fPkg/Drivers/J716fIoConsoleBdsDxe/J716fIoConsoleBdsDxe.inf
