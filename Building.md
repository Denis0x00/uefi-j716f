# Building TB-J716F UEFI Firmware

This repository builds `Platforms/Lenovo/j716fPkg/j716f.dsc` for `AARCH64` with
EDK II `CLANGPDB`, targeting the 7 MiB (`0x700000` / 7,340,032 bytes) firmware
volume at `0xCE000000`.

## Recommended Path: Official UniXNU Format 2 Flow

Place `UniXNU`, `UniXNU-j716f`, and `uefi-j716f` as sibling directories on a
Linux (Ubuntu 24.04 x86_64) build host:

```bash
git clone https://github.com/UniXNU/UniXNU.git
git clone https://github.com/Denis0x00/UniXNU-j716f.git
git clone https://github.com/Denis0x00/uefi-j716f.git

cd UniXNU
./unixnu defconfig j716f_defconfig -D ../UniXNU-j716f --os 17
./unixnu fetch edk2 --os 17
./unixnu build firmware --os 17 --uefi ../uefi-j716f
```

The output firmware volume is written to:

```text
../uefi-j716f/Build/j716fPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd
```

## Standalone EDK II Build (Same Path as CI)

On Ubuntu 24.04 with `clang-18`, `lld-18`, `llvm-18`, `nasm`, `acpica-tools`,
`uuid-dev`, `build-essential`, and `python3`:

```bash
git submodule update --init --depth 1 Common/Mu Common/Mu_OEM_Sample
# Clone UniXNU/UniXNU (with drivers/DarwinBoot) and microsoft/mu_basecore
# at the commit pinned in UniXNU/feeds.toml (with its 5 listed submodules).
make -C "$EDK2/BaseTools/Source/C" -j"$(nproc)"

export WORKSPACE="$PWD"
export EDK_TOOLS_PATH="$EDK2/BaseTools"
export CONF_PATH="$PWD/Conf"
export CLANG_BIN="/usr/lib/llvm-18/bin/"
export PYTHON_COMMAND="python3"
export PATH="$EDK_TOOLS_PATH/BinWrappers/PosixLike:$EDK_TOOLS_PATH/Source/C/bin:$PATH"
export PACKAGES_PATH="$PWD:$PWD/Platforms/Lenovo:$PWD/Common/Mu:$PWD/Common/Mu_OEM_Sample:$EDK2:$PWD/Silicon/Qualcomm:$PWD/Silicon/Silicium:$UNIXNU/package:$UNIXNU/drivers/DarwinBoot"

mkdir -p "$CONF_PATH"
for f in target tools_def build_rule; do
  cp "$EDK_TOOLS_PATH/Conf/${f}.template" "$CONF_PATH/${f}.txt"
done

build -p Platforms/Lenovo/j716fPkg/j716f.dsc \
  -a AARCH64 -b DEBUG -t CLANGPDB -n "$(nproc)" \
  -D FD_BASE=0xCE000000 -D FD_SIZE=0x700000 -D FD_BLOCKS=1792 \
  -D DEVICE_MODEL=j716f -D ENABLE_SECUREBOOT=0
```

## Optional Local Diagnostic Menu Hold

If you apply `Resources/UniXNUPatches/j716f-menu-hold.patch` to your local
`UniXNU` checkout, you can additionally pass `-D J716F_MENU_DIAGNOSTIC=1` to
enable `PcdUniXNUBuiltinMenuWaitForInput` and `PcdUniXNUDiagnosticMenuHold`.
Without that local core patch, omit `J716F_MENU_DIAGNOSTIC` so the DSC builds
cleanly against unmodified upstream `UniXNU/UniXNU`.
