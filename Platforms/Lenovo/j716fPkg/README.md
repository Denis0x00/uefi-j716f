# J716F Platform Package (`j716fPkg`)

Board-specific Mu-Silicium UEFI platform package for the Lenovo Xiaoxin Pad Pro 2021 (`TB-J716F` / `j716f`, Qualcomm SM8250-AC).

## Active Modules

- **`j716f.dsc` / `j716f.fdf`**: Platform build description and flash layout (`SILICIUM_UEFI.fd` at `0xce000000`, size `0x700000`).
- **`Drivers/J716fInheritedFbDxe`**: Publishes the bootloader-initialized `2560x1600` BGRX8888 GOP framebuffer at `0x9c000000`.
- **`Drivers/J716fKeysDxe`**: Polls PM8150/PM8150B SPMI PON keys (`Power`, `Volume Up`, `Volume Down`) for UEFI menu navigation.
- **`Drivers/J716fIoConsoleBdsDxe`**: Connects graphics console, RAM disk FAT volume (`UniXNURamDiskDxe`), `DarwinBoot`, and the `UniXNU` boot application.
- **`Library/MemoryMapLib`**: SM8250-AC / J716F physical memory map (`0x80000000..0x200000000`, including `0x9c000000` display, `0xb0000000` boot log, `0xce000000` UEFI FD, and `0xd0000000` XNU window).
- **`Library/J716fFrameBufferSerialPortLib`**: Early framebuffer text console for SEC/DXE diagnostics.
- **`Library/ConfigurationMapLib`**: Qualcomm platform configuration map descriptors.
