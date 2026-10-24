APRIORI DXE {
  INF MdeModulePkg/Core/Dxe/DxeMain.inf
  INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf

  INF Binaries/j716f/Drivers/EnvDxe/EnvDxe.inf

  INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  INF MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf
  INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
  INF EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf

  INF Binaries/j716f/Drivers/SmemDxe/SmemDxe.inf
  INF Binaries/j716f/Drivers/DALSys/DALSys.inf
  INF Binaries/j716f/Drivers/HWIODxeDriver/HWIODxeDriver.inf
  INF Binaries/j716f/Drivers/ChipInfo/ChipInfo.inf
  INF Binaries/j716f/Drivers/PlatformInfoDxeDriver/PlatformInfoDxeDriver.inf
  INF Binaries/j716f/Drivers/HALIOMMU/HALIOMMU.inf
  INF Binaries/j716f/Drivers/ULogDxe/ULogDxe.inf
  INF Binaries/j716f/Drivers/CmdDbDxe/CmdDbDxe.inf
  INF Binaries/j716f/Drivers/PwrUtilsDxe/PwrUtilsDxe.inf
  INF Binaries/j716f/Drivers/NpaDxe/NpaDxe.inf
  INF Binaries/j716f/Drivers/RpmhDxe/RpmhDxe.inf
  INF Binaries/j716f/Drivers/PdcDxe/PdcDxe.inf
  INF Binaries/j716f/Drivers/VcsDxe/VcsDxe.inf
  INF Binaries/j716f/Drivers/ClockDxe/ClockDxe.inf
  INF Binaries/j716f/Drivers/SPMI/SPMI.inf
  INF Binaries/j716f/Drivers/ShmBridgeDxe/ShmBridgeDxe.inf
  INF Binaries/j716f/Drivers/ScmDxe/ScmDxe.inf
  INF j716fPkg/Drivers/J716fScmCompatDxe/J716fScmCompatDxe.inf
  INF Binaries/j716f/Drivers/DALTLMM/DALTLMM.inf
  INF Binaries/j716f/Drivers/PmicDxe/PmicDxe.inf

  INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf

  INF Binaries/j716f/Drivers/SdccDxe/SdccDxe.inf
  INF Binaries/j716f/Drivers/UFSDxe/UFSDxe.inf

  INF FatPkg/EnhancedFatDxe/Fat.inf

  INF Binaries/j716f/Drivers/TzDxe/TzDxe.inf

  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf

  INF Binaries/j716f/Drivers/FeatureEnablerDxe/FeatureEnablerDxe.inf
  INF Binaries/j716f/Drivers/DDRInfoDxe/DDRInfoDxe.inf
  INF Binaries/j716f/Drivers/I2C/I2C.inf

!if $(USE_CUSTOM_DISPLAY_DRIVER) == 1
  INF Binaries/j716f/Drivers/DisplayDxe/DisplayDxe.inf
!else
  INF SiliciumPkg/Drivers/SimpleFbDxe/SimpleFbDxe.inf
!endif

  INF Binaries/j716f/Drivers/PILProxyDxe/PILProxyDxe.inf
  INF Binaries/j716f/Drivers/PILDxe/PILDxe.inf
  INF Binaries/j716f/Drivers/SPSSDxe/SPSSDxe.inf
!if $(USE_CUSTOM_DISPLAY_DRIVER) == 1
  INF Binaries/j716f/Drivers/CPRDxe/CPRDxe.inf
!endif

  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
  INF MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  INF EmbeddedPkg/EmbeddedMonotonicCounter/EmbeddedMonotonicCounter.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  INF MdeModulePkg/Universal/PrintDxe/PrintDxe.inf
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf

  INF Binaries/j716f/Drivers/SPI/SPI.inf
  INF Binaries/j716f/Drivers/AdcDxe/AdcDxe.inf
  INF Binaries/j716f/Drivers/UsbPwrCtrlDxe/UsbPwrCtrlDxe.inf
  INF Binaries/j716f/Drivers/QcomChargerDxeLA/QcomChargerDxeLA.inf
  INF Binaries/j716f/Drivers/ChargerExDxe/ChargerExDxe.inf
  INF Binaries/j716f/Drivers/UsbfnDwc3Dxe/UsbfnDwc3Dxe.inf
  INF Binaries/j716f/Drivers/XhciPciEmulation/XhciPciEmulation.inf
  INF Binaries/j716f/Drivers/XhciDxe/XhciDxe.inf

  INF MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf

  INF Binaries/j716f/Drivers/UsbMsdDxe/UsbMsdDxe.inf
  INF Binaries/j716f/Drivers/UsbDeviceDxe/UsbDeviceDxe.inf
  INF Binaries/j716f/Drivers/UsbConfigDxe/UsbConfigDxe.inf
  INF Binaries/j716f/Drivers/UsbInitDxe/UsbInitDxe.inf
  INF Binaries/j716f/Drivers/ButtonsDxe/ButtonsDxe.inf
  INF Binaries/j716f/Drivers/TsensDxe/TsensDxe.inf
  INF Binaries/j716f/Drivers/LimitsDxe/LimitsDxe.inf
  INF Binaries/j716f/Drivers/GpiDxe/GpiDxe.inf
  INF Binaries/j716f/Drivers/HashDxe/HashDxe.inf
  INF Binaries/j716f/Drivers/CipherDxe/CipherDxe.inf
  INF Binaries/j716f/Drivers/MacDxe/MacDxe.inf
  INF Binaries/j716f/Drivers/RngDxe/RngDxe.inf

  INF EmbeddedPkg/SimpleTextInOutSerial/SimpleTextInOutSerial.inf
  INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
}
