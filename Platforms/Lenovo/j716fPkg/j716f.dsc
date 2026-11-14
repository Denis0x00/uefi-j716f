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
!if $(TARGET) == DEBUG
  # Keep firmware warnings/errors and UniXNU's default DEBUG_INFO messages.
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000042
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDebugLevel|0x00000040
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDebugLevel|0
!endif
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDeviceModel|"j716f"
!ifdef J716F_MENU_DIAGNOSTIC
!if $(J716F_MENU_DIAGNOSTIC) == 1
  # Explicit diagnostic policy for both built-in and loaded configurations.
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUBuiltinMenuWaitForInput|TRUE
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDiagnosticMenuHold|TRUE
!endif
!endif
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerPages|0x100
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerBase|0x00000000
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdCapsuleInRamSupport|FALSE

[PcdsFeatureFlag]
  gEfiMdeModulePkgTokenSpaceGuid.PcdSupportUpdateCapsuleReset|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdSupportProcessCapsuleAtRuntime|FALSE

[LibraryClasses.common]
  MemoryMapLib|j716fPkg/Library/MemoryMapLib/MemoryMapLib.inf
  RamLogLib|j716fPkg/Library/J716fRamLogLib/J716fRamLogLib.inf
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
  EarlyLogSecLib|j716fPkg/Library/J716fEarlyLogSecLib/J716fEarlyLogSecLib.inf
  EarlyHandoffLib|j716fPkg/Library/J716fEarlyHandoffLib/J716fEarlyHandoffLib.inf
  # The menu stage only publishes its own FV; no inherited XBL protocols.
  PlatformPeiLib|j716fPkg/Library/J716fPlatformPeiLib/PlatformPeiLib.inf
  # Sec.c calls BaseDebugLibSerialPortConstructor directly before DXE.
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf

[LibraryClasses.common.DXE_CORE]
!if $(TARGET) == DEBUG
  # The DxeCore constructor allocates the log and installs its protocol.
  DebugLib|AdvLoggerPkg/Library/BaseDebugLibAdvancedLogger/BaseDebugLibAdvancedLogger.inf
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/DxeCore/AdvancedLoggerLib.inf
!else
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
!endif

[LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.UEFI_DRIVER]
!if $(TARGET) == DEBUG
  DebugLib|AdvLoggerPkg/Library/BaseDebugLibAdvancedLogger/BaseDebugLibAdvancedLogger.inf
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Dxe/AdvancedLoggerLib.inf
!endif

[LibraryClasses.common.DXE_RUNTIME_DRIVER]
!if $(TARGET) == DEBUG
  DebugLib|AdvLoggerPkg/Library/BaseDebugLibAdvancedLogger/BaseDebugLibAdvancedLogger.inf
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Runtime/AdvancedLoggerLib.inf
!endif

[Components]
  # Built for explicit manual EFI payload entries; never automatically dispatched.
  j716fPkg/Application/J716fEarlyLogProbe/J716fEarlyLogProbe.inf
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf
  UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf {
    <PcdsFixedAtBuild>
      # QrEncoderLib INFO traces share the panel with the QR image. Keep
      # exporter errors while other modules retain the full boot diagnostics.
      gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
  }
  DarwinBootPkg/Driver/DarwinBoot.inf
  j716fPkg/Drivers/J716fInheritedFbDxe/J716fInheritedFbDxe.inf
  j716fPkg/Drivers/J716fKeysDxe/J716fKeysDxe.inf
!if $(TARGET) == DEBUG
  j716fPkg/Drivers/J716fHandoffDiagnosticsDxe/J716fHandoffDiagnosticsDxe.inf
!endif
  j716fPkg/Drivers/J716fIoConsoleBdsDxe/J716fIoConsoleBdsDxe.inf
