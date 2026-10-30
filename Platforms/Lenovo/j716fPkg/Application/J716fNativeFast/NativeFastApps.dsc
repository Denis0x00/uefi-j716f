[Defines]
  PLATFORM_NAME = J716fNativeLogApps
  PLATFORM_GUID = da593c7b-de34-47cc-b8a3-0d991d607984
  PLATFORM_VERSION = 1.0
  DSC_SPECIFICATION = 0x00010005
  OUTPUT_DIRECTORY = Build/NativeLogApps
  SUPPORTED_ARCHITECTURES = AARCH64
  BUILD_TARGETS = RELEASE|DEBUG
  SKUID_IDENTIFIER = DEFAULT
[LibraryClasses]
  QrEncoderLib|MsGraphicsPkg/Library/QrEncoderLib/QrEncoderLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  ArmLib|MdePkg/Library/ArmLib/ArmBaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0
[Components]
  J716fNativeLog/Read.inf
  J716fNativeLog/Arm.inf
  J716fNativeLog/Power.inf
