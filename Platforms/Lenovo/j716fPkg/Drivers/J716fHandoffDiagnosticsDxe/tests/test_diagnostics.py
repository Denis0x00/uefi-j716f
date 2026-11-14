#!/usr/bin/env python3
"""Compile the actual driver's map safety functions against fault-injected UEFI.

No firmware/device access. The C function bodies are extracted unmodified from
the driver so these tests exercise its implementation, not a Python rewrite.
Run: python3 test_diagnostics.py --cc cc --sanitize
"""
from pathlib import Path
import argparse
import re
import shutil
import subprocess
import tempfile


HARNESS_PREFIX = r'''
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint8_t UINT8;
typedef size_t UINTN;
typedef int EFI_STATUS;
typedef int BOOLEAN;
#define VOID void
#define STATIC static
#define CONST const
#define IN
#define OUT
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_BUFFER_TOO_SMALL 1
#define EFI_COMPROMISED_DATA 2
#define EFI_OUT_OF_RESOURCES 3
#define EFI_DEVICE_ERROR 4
#define EFI_ERROR(Status) ((Status) != EFI_SUCCESS)
#define EFI_PAGE_SIZE 4096
#define MAX_UINT64 UINT64_MAX
#define MAX_UINTN SIZE_MAX
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define ZeroMem(Dest, Size) memset((Dest), 0, (Size))
#define CopyMem(Dest, Src, Size) memcpy((Dest), (Src), (Size))
#define EFI_MEMORY_RP UINT64_C(0x2000)
enum { EfiReservedMemoryType, EfiLoaderCode, EfiLoaderData,
  EfiBootServicesCode, EfiBootServicesData, EfiRuntimeServicesCode,
  EfiRuntimeServicesData, EfiConventionalMemory, EfiUnusableMemory,
  EfiACPIReclaimMemory, EfiACPIMemoryNVS, EfiMemoryMappedIO,
  EfiMemoryMappedIOPortSpace };
typedef struct {
  UINT32 Type, Pad;
  UINT64 PhysicalStart, VirtualStart, NumberOfPages, Attribute;
} EFI_MEMORY_DESCRIPTOR;
_Static_assert(sizeof(EFI_MEMORY_DESCRIPTOR) == 40, "UEFI descriptor layout");
enum { NoHob, AddMem, AddDev, HobOnlyNoCacheSetting };
enum { EFI_RESOURCE_SYSTEM_MEMORY, EFI_RESOURCE_MEMORY_RESERVED };
enum { ARM_MEMORY_REGION_ATTRIBUTE_DEVICE,
  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK,
  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_THROUGH,
  ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED };
#define RAM_WRITE_BACK ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK
typedef struct {
  UINT64 Address, Length;
  UINT32 ResourceType, HobOption, ArmAttributes;
} EFI_MEMORY_REGION_DESCRIPTOR;

typedef struct { EFI_STATUS Status; UINTN Size, Stride; } STEP;
static STEP Steps[10];
static UINTN StepCount, StepIndex, AllocCount, FreeCount, FailAllocation;
static VOID *LiveAllocation;
static UINTN LastCapacity;
static EFI_MEMORY_REGION_DESCRIPTOR Regions[2];
static UINT8 RegionCount;
static UINTN CrcCalls, CrcBeginLogs, CrcOkLogs, CrcSkipLogs, CrcFailureLogs;
static UINTN ExpectedCrcSize;
static VOID *ExpectedCrcBuffer;
static EFI_STATUS CrcStatus;
static UINT32 ReturnedCrc, LoggedCrc;
static UINT64 LoggedBase, LoggedSize;
static BOOLEAN ReadCrcBuffer;
#define DEBUG_INFO 1
#define DEBUG_WARN 2
#define DEBUG(Args) MockDebug Args
static VOID MockDebug(UINTN Level, const char *Format, ...) {
  (void)Level;
  va_list Args;
  va_start(Args, Format);
  if (strstr(Format, "CRC32 begin")) ++CrcBeginLogs;
  else if (strstr(Format, "CRC32 skipped")) ++CrcSkipLogs;
  else if (strstr(Format, "CRC32 failed")) ++CrcFailureLogs;
  else if (strstr(Format, "CRC32 ok")) {
    ++CrcOkLogs;
    LoggedBase = va_arg(Args, UINT64);
    UINT64 End = va_arg(Args, UINT64);
    LoggedSize = va_arg(Args, UINT64);
    LoggedCrc = va_arg(Args, UINT32);
    assert(LoggedBase + LoggedSize == End);
  } else assert(!"unexpected diagnostic log");
  va_end(Args);
}
static EFI_STATUS MockCalculateCrc32(VOID *Data, UINTN Size, UINT32 *Crc) {
  ++CrcCalls;
  assert(Data == ExpectedCrcBuffer && Size == ExpectedCrcSize && Crc != NULL);
  if (CrcStatus != EFI_SUCCESS) return CrcStatus;
  if (ReadCrcBuffer) {
    // Independent reference used only for an actual, bounded host buffer.
    UINT32 Value = UINT32_C(0xffffffff);
    for (UINTN Index = 0; Index < Size; ++Index) {
      Value ^= ((const UINT8 *)Data)[Index];
      for (unsigned Bit = 0; Bit < 8; ++Bit)
        Value = (Value >> 1) ^ ((Value & 1) ? UINT32_C(0xedb88320) : 0);
    }
    *Crc = ~Value;
  } else *Crc = ReturnedCrc;
  return EFI_SUCCESS;
}

static VOID *AllocatePool(UINTN Size) {
  assert(LiveAllocation == NULL);
  ++AllocCount;
  LastCapacity = Size;
  if (AllocCount == FailAllocation) return NULL;
  LiveAllocation = malloc(Size);
  assert(LiveAllocation != NULL);
  return LiveAllocation;
}
static VOID FreePool(VOID *Buffer) {
  assert(Buffer != NULL && Buffer == LiveAllocation);
  free(Buffer);
  LiveAllocation = NULL;
  ++FreeCount;
}
static EFI_STATUS MockGetMemoryMap(UINTN *Size, EFI_MEMORY_DESCRIPTOR *Buffer,
                                 UINTN *Key, UINTN *Stride, UINT32 *Version) {
  assert(StepIndex < StepCount);
  if (Buffer == NULL) assert(*Size == 0);
  else assert(Buffer == LiveAllocation && *Size == LastCapacity);
  STEP Step = Steps[StepIndex++];
  *Size = Step.Size;
  *Stride = Step.Stride;
  *Version = 1;
  *Key = 0x1234;
  return Step.Status;
}
static struct {
  EFI_STATUS (*GetMemoryMap)(UINTN *, EFI_MEMORY_DESCRIPTOR *, UINTN *, UINTN *, UINT32 *);
  EFI_STATUS (*CalculateCrc32)(VOID *, UINTN, UINT32 *);
} BootServices = { MockGetMemoryMap, MockCalculateCrc32 }, *gBS = &BootServices;
static VOID GetMemoryMap(EFI_MEMORY_REGION_DESCRIPTOR **Out, UINT8 *Count) {
  *Out = Regions;
  *Count = RegionCount;
}
'''


HARNESS_TESTS = r'''
static VOID Reset(VOID) {
  assert(LiveAllocation == NULL);
  StepCount = StepIndex = AllocCount = FreeCount = FailAllocation = 0;
}
static VOID Push(EFI_STATUS Status, UINTN Size, UINTN Stride) {
  assert(StepCount < sizeof(Steps) / sizeof(Steps[0]));
  Steps[StepCount++] = (STEP){Status, Size, Stride};
}
static VOID Failed(EFI_STATUS Expected, MAP_SNAPSHOT *Map) {
  assert(CaptureMap(Map) == Expected);
  assert(Map->Buffer == NULL && LiveAllocation == NULL);
  assert(StepCount == StepIndex);
}
static VOID TestCapture(VOID) {
  MAP_SNAPSHOT Map;
  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 120, 40);
  Push(EFI_SUCCESS, 80, 40);
  assert(CaptureMap(&Map) == EFI_SUCCESS);
  assert(Map.Size == 80 && Map.DescriptorSize == 40 && Map.Key == 0x1234);
  assert(Map.Version == 1 && Map.Capacity >= 120 && AllocCount == 1 && FreeCount == 0);
  FreePool(Map.Buffer);

  // The returned map grows between queries; the descriptor ABI stride changes.
  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 120, 40);
  Push(EFI_BUFFER_TOO_SMALL, 768, 48);
  Push(EFI_SUCCESS, 192, 48);
  assert(CaptureMap(&Map) == EFI_SUCCESS);
  assert(Map.Size == 192 && Map.DescriptorSize == 48 && Map.Capacity >= 768);
  assert(AllocCount == 2 && FreeCount == 1);
  FreePool(Map.Buffer);

  Reset();
  for (unsigned i = 0; i < MAX_MAP_TRIES; ++i)
    Push(EFI_BUFFER_TOO_SMALL, 4096 * (i + 1), 40);
  Failed(EFI_BUFFER_TOO_SMALL, &Map);
  assert(AllocCount == MAX_MAP_TRIES && FreeCount == AllocCount);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, MAX_MAP_BYTES, 40);
  Failed(EFI_OUT_OF_RESOURCES, &Map);
  assert(AllocCount == 0);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 80, 39);
  Failed(EFI_COMPROMISED_DATA, &Map);
  assert(AllocCount == 0);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 80, MAX_MAP_BYTES);
  Failed(EFI_COMPROMISED_DATA, &Map);
  assert(AllocCount == 0);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 80, 40);
  FailAllocation = 1;
  Failed(EFI_OUT_OF_RESOURCES, &Map);
  assert(AllocCount == 1 && FreeCount == 0);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 80, 40);
  Push(EFI_DEVICE_ERROR, 80, 40);
  Failed(EFI_DEVICE_ERROR, &Map);
  assert(AllocCount == 1 && FreeCount == 1);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 80, 40);
  Push(EFI_SUCCESS, 79, 40);
  Failed(EFI_COMPROMISED_DATA, &Map);
  assert(FreeCount == 1);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 80, 40);
  Push(EFI_SUCCESS, 4000, 40);
  Failed(EFI_COMPROMISED_DATA, &Map);

  Reset();
  Push(EFI_BUFFER_TOO_SMALL, 80, 40);
  Push(EFI_SUCCESS, 80, 0);
  Failed(EFI_COMPROMISED_DATA, &Map);

  Reset();
  Push(EFI_SUCCESS, 80, 40);
  Failed(EFI_COMPROMISED_DATA, &Map);
  puts("CaptureMap: 12 success/fault-injection cases passed");
}

static VOID TestDescriptorEnd(VOID) {
  EFI_MEMORY_DESCRIPTOR D = { .PhysicalStart=0x1000, .NumberOfPages=2 };
  UINT64 End = 0;
  assert(DescriptorEnd(&D, &End) && End == 0x3000);
  D.NumberOfPages = 0;
  assert(!DescriptorEnd(&D, &End));
  D.PhysicalStart = UINT64_MAX - 4095;
  D.NumberOfPages = 1;
  assert(!DescriptorEnd(&D, &End));
  D.PhysicalStart = 0;
  D.NumberOfPages = UINT64_MAX;
  assert(!DescriptorEnd(&D, &End));
  D.NumberOfPages = UINT64_MAX / 4096;
  assert(DescriptorEnd(&D, &End) && End == UINT64_MAX - 4095);
  puts("DescriptorEnd: 5 range/overflow cases passed");
}

static VOID TestFdtGuard(VOID) {
  // 48-byte firmware stride includes bytes unknown to our descriptor struct.
  UINT8 Raw[96] = {0};
  EFI_MEMORY_DESCRIPTOR D = { .Type=EfiReservedMemoryType,
    .PhysicalStart=0x1000, .NumberOfPages=1 };
  MAP_SNAPSHOT Map = { .Buffer=(EFI_MEMORY_DESCRIPTOR *)Raw, .Size=sizeof(Raw),
    .Capacity=sizeof(Raw), .DescriptorSize=48 };
  Regions[0] = (EFI_MEMORY_REGION_DESCRIPTOR){ .Address=0x1000, .Length=0x4000,
    .ResourceType=EFI_RESOURCE_SYSTEM_MEMORY, .HobOption=AddMem, .ArmAttributes=RAM_WRITE_BACK };
  RegionCount = 1;
  memcpy(Raw, &D, sizeof(D));
  D.PhysicalStart = 0x2000;
  memcpy(Raw + 48, &D, sizeof(D));
  assert(CanReadFdt(&Map, 0x1ff0, 32)); // Across adjacent descriptors.
  assert(!CanReadFdt(&Map, 0, 40));
  assert(!CanReadFdt(&Map, 0x1000, 0));
  assert(!CanReadFdt(&Map, UINT64_MAX - 8, 40));
  assert(!CanReadFdt(&Map, 0x0ff0, 40));
  assert(!CanReadFdt(&Map, 0x2ff0, 40)); // Missing tail coverage.
  D.PhysicalStart = 0x3000;
  memcpy(Raw + 48, &D, sizeof(D));
  assert(!CanReadFdt(&Map, 0x1ff0, 0x1020)); // Hole is not readable.
  D.PhysicalStart = 0x2000;
  D.Attribute = EFI_MEMORY_RP;
  memcpy(Raw + 48, &D, sizeof(D));
  assert(!CanReadFdt(&Map, 0x1ff0, 32));
  D.Attribute = 0;
  D.Type = EfiMemoryMappedIO;
  memcpy(Raw + 48, &D, sizeof(D));
  assert(!CanReadFdt(&Map, 0x1ff0, 32));
  D.Type = EfiUnusableMemory;
  memcpy(Raw + 48, &D, sizeof(D));
  assert(!CanReadFdt(&Map, 0x1ff0, 32));
  D.Type = EfiConventionalMemory;
  memcpy(Raw + 48, &D, sizeof(D));
  assert(CanReadFdt(&Map, 0x1ff0, 32)); // Readability does not imply reservation.
  Regions[0].HobOption = NoHob;
  assert(!CanReadFdt(&Map, 0x1ff0, 32));
  Regions[0].HobOption = AddMem;
  Regions[0].ArmAttributes = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
  assert(!CanReadFdt(&Map, 0x1ff0, 32));
  Regions[0].ArmAttributes = RAM_WRITE_BACK;
  Regions[0].ResourceType = EFI_RESOURCE_MEMORY_RESERVED;
  assert(!CanReadFdt(&Map, 0x1ff0, 32)); // Do not probe secure device reservations.
  puts("CanReadFdt: 14 coverage/protection cases passed");
}

static VOID ResetCrcLogs(VOID) {
  CrcBeginLogs = CrcOkLogs = CrcSkipLogs = CrcFailureLogs = 0;
}
static VOID RejectInitrd(const MAP_SNAPSHOT *Map, UINT64 Base, UINT64 Size) {
  UINTN PreviousCalls = CrcCalls;
  assert(!CanReadInitrd(Map, Base, Size));
  ResetCrcLogs();
  ReportInitrdCrc32(Map, Base, Size);
  assert(CrcCalls == PreviousCalls && CrcSkipLogs == 1);
  assert(CrcBeginLogs == 0 && CrcOkLogs == 0 && CrcFailureLogs == 0);
}
static VOID TestInitrdGuard(VOID) {
  UINT8 Raw[96] = {0};
  EFI_MEMORY_DESCRIPTOR D[2] = {
    { .Type=EfiReservedMemoryType, .PhysicalStart=0x1000, .NumberOfPages=2 },
    { .Type=EfiReservedMemoryType, .PhysicalStart=0x3000, .NumberOfPages=1 }
  };
  MAP_SNAPSHOT Map = { .Buffer=(EFI_MEMORY_DESCRIPTOR *)Raw, .Size=sizeof(Raw),
    .Capacity=sizeof(Raw), .DescriptorSize=48 };
  Regions[0] = (EFI_MEMORY_REGION_DESCRIPTOR){ .Address=0x1000, .Length=MAX_INITRD_CRC_BYTES,
    .ResourceType=EFI_RESOURCE_SYSTEM_MEMORY, .HobOption=AddMem, .ArmAttributes=RAM_WRITE_BACK };
  RegionCount = 1;
#define UPDATE_MAP() do { memcpy(Raw, &D[0], sizeof(D[0])); memcpy(Raw + 48, &D[1], sizeof(D[1])); } while (0)
  UPDATE_MAP();
  assert(CanReadInitrd(&Map, 0x1000, 0x3000));
  assert(CanReadInitrd(&Map, 0x2ff0, 32));
  // UEFI permits unordered descriptors. Stride extensions must be respected.
  memcpy(Raw, &D[1], sizeof(D[1]));
  memcpy(Raw + 48, &D[0], sizeof(D[0]));
  assert(CanReadInitrd(&Map, 0x1000, 0x3000));
  UPDATE_MAP();
  RejectInitrd(&Map, 0, 512);
  RejectInitrd(&Map, 0x1000, 0);
  RejectInitrd(&Map, UINT64_MAX - 8, 40);
  RejectInitrd(&Map, 0x1000, MAX_INITRD_CRC_BYTES + 1);
  RejectInitrd(&Map, 0x3ff0, 32); // Tail missing from EFI map.
  RejectInitrd(&Map, 0x0ff0, 32); // Head outside board RAM.
  D[1].PhysicalStart = 0x4000;
  UPDATE_MAP();
  RejectInitrd(&Map, 0x1000, 0x4000); // Coverage gap.
  D[1].PhysicalStart = 0x2000;
  UPDATE_MAP();
  RejectInitrd(&Map, 0x1000, 0x2000); // Overlap starts after the initial cursor.
  D[1].PhysicalStart = 0x1000;
  UPDATE_MAP();
  RejectInitrd(&Map, 0x1000, 0x1000); // Duplicate coverage at initial cursor.
  D[1].PhysicalStart = 0x3000;
  for (UINT32 Type = 1; Type < 20; ++Type) {
    D[1].Type = Type;
    UPDATE_MAP();
    RejectInitrd(&Map, 0x1000, 0x3000); // Every non-Reserved type, including unknown.
  }
  D[1].Type = EfiReservedMemoryType;
  D[1].Attribute = EFI_MEMORY_RP;
  UPDATE_MAP();
  RejectInitrd(&Map, 0x1000, 0x3000);
  D[1].Attribute = 0;
  D[1].NumberOfPages = 0;
  UPDATE_MAP();
  RejectInitrd(&Map, 0x1000, 0x1000);
  D[1].NumberOfPages = UINT64_MAX;
  UPDATE_MAP();
  RejectInitrd(&Map, 0x1000, 0x1000);
  D[1].NumberOfPages = 1;
  D[1].PhysicalStart = 0x3001;
  UPDATE_MAP();
  RejectInitrd(&Map, 0x1000, 0x1000);
  D[1].PhysicalStart = 0x3000;
  UPDATE_MAP();
  Regions[0].ResourceType = EFI_RESOURCE_MEMORY_RESERVED;
  RejectInitrd(&Map, 0x1000, 0x1000); // Secure/device board reservation.
  Regions[0].ResourceType = EFI_RESOURCE_SYSTEM_MEMORY;
  for (UINT32 Hob = NoHob; Hob <= HobOnlyNoCacheSetting; ++Hob) {
    if (Hob == AddMem) continue;
    Regions[0].HobOption = Hob;
    RejectInitrd(&Map, 0x1000, 0x1000);
  }
  Regions[0].HobOption = AddMem;
  Regions[0].ArmAttributes = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
  RejectInitrd(&Map, 0x1000, 0x1000);
  Regions[0].ArmAttributes = 1000;
  RejectInitrd(&Map, 0x1000, 0x1000); // Unknown attributes are not presumed readable.
  Regions[0].ArmAttributes = RAM_WRITE_BACK;
  Regions[0].Length = UINT64_MAX;
  RejectInitrd(&Map, 0x1000, 0x1000);
  Regions[0].Length = 0x1000;
  RejectInitrd(&Map, 0x1000, 0x3000);
  Regions[0].Length = MAX_INITRD_CRC_BYTES;
  RegionCount = 0;
  RejectInitrd(&Map, 0x1000, 0x1000);
  RegionCount = 1;
  RejectInitrd(NULL, 0x1000, 0x1000);
  MAP_SNAPSHOT Bad = Map;
  Bad.Buffer = NULL;
  RejectInitrd(&Bad, 0x1000, 0x1000);
  Bad = Map; Bad.Size = 0;
  RejectInitrd(&Bad, 0x1000, 0x1000);
  Bad = Map; Bad.Size = MAX_MAP_BYTES + 1;
  RejectInitrd(&Bad, 0x1000, 0x1000);
  Bad = Map; Bad.Capacity = Bad.Size - 1;
  RejectInitrd(&Bad, 0x1000, 0x1000);
  Bad = Map; Bad.Size -= 1;
  RejectInitrd(&Bad, 0x1000, 0x1000);
  Bad = Map; Bad.DescriptorSize = 0;
  RejectInitrd(&Bad, 0x1000, 0x1000);
  Bad = Map; Bad.DescriptorSize = sizeof(EFI_MEMORY_DESCRIPTOR) - 1;
  RejectInitrd(&Bad, 0x1000, 0x1000);
  // An 80 MiB request is accepted exactly at the limit, with one callback
  // receiving the whole interval (the mock does not dereference physical RAM).
  D[0].NumberOfPages = MAX_INITRD_CRC_BYTES / EFI_PAGE_SIZE;
  memcpy(Raw, &D[0], sizeof(D[0]));
  Map.Size = 48;
  assert(CanReadInitrd(&Map, 0x1000, MAX_INITRD_CRC_BYTES));
  ExpectedCrcBuffer = (VOID *)(uintptr_t)0x1000;
  ExpectedCrcSize = MAX_INITRD_CRC_BYTES;
  ReturnedCrc = UINT32_C(0x89abcdef);
  CrcStatus = EFI_SUCCESS;
  ReadCrcBuffer = FALSE;
  ResetCrcLogs();
  UINTN PreviousCalls = CrcCalls;
  ReportInitrdCrc32(&Map, 0x1000, MAX_INITRD_CRC_BYTES);
  assert(CrcCalls == PreviousCalls + 1 && CrcBeginLogs == 1 && CrcOkLogs == 1);
  assert(CrcSkipLogs == 0 && CrcFailureLogs == 0);
  assert(LoggedBase == 0x1000 && LoggedSize == MAX_INITRD_CRC_BYTES && LoggedCrc == ReturnedCrc);
  puts("CanReadInitrd: bounds, map stride/shape, complete coverage, overlaps, RAM attributes and Reserved-only refusals passed; no refused range reached CRC service");
#undef UPDATE_MAP
}

static VOID TestInitrdChecksum(VOID) {
  UINT8 *Buffer = malloc(8192);
  assert(Buffer != NULL);
  const UINT64 Base = (UINT64)(uintptr_t)Buffer;
  const UINT64 Page = Base & ~UINT64_C(4095);
  EFI_MEMORY_DESCRIPTOR D = { .Type=EfiReservedMemoryType,
    .PhysicalStart=Page, .NumberOfPages=3 };
  MAP_SNAPSHOT Map = { .Buffer=&D, .Size=sizeof(D),
    .Capacity=sizeof(D), .DescriptorSize=sizeof(D) };
  Regions[0] = (EFI_MEMORY_REGION_DESCRIPTOR){ .Address=Page, .Length=3 * 4096,
    .ResourceType=EFI_RESOURCE_SYSTEM_MEMORY, .HobOption=AddMem, .ArmAttributes=RAM_WRITE_BACK };
  RegionCount = 1;
  memcpy(Buffer, "123456789", 9);
  ExpectedCrcBuffer = Buffer;
  ExpectedCrcSize = 9;
  CrcStatus = EFI_SUCCESS;
  ReadCrcBuffer = TRUE;
  ResetCrcLogs();
  UINTN PreviousCalls = CrcCalls;
  ReportInitrdCrc32(&Map, Base, 9);
  assert(CrcCalls == PreviousCalls + 1 && CrcBeginLogs == 1 && CrcOkLogs == 1);
  assert(LoggedBase == Base && LoggedSize == 9 && LoggedCrc == UINT32_C(0xcbf43926));
  assert(CrcSkipLogs == 0 && CrcFailureLogs == 0);
  // A CRC service failure must not print a successful or uninitialized value.
  CrcStatus = EFI_DEVICE_ERROR;
  ResetCrcLogs();
  ReportInitrdCrc32(&Map, Base, 9);
  assert(CrcBeginLogs == 1 && CrcFailureLogs == 1 && CrcOkLogs == 0 && CrcSkipLogs == 0);
  free(Buffer);
  puts("ReportInitrdCrc32: exact buffer/length, standard CRC32 vector and service error logging passed");
}
int main(void) {
  TestCapture();
  TestDescriptorEnd();
  TestFdtGuard();
  TestInitrdGuard();
  TestInitrdChecksum();
  assert(LiveAllocation == NULL);
  puts("Diagnostic safety tests passed, including original 31 cases and initrd CRC guards; no device accessed");
  return 0;
}
'''


def function(source: str, return_type: str, name: str) -> str:
    start = source.index(f"STATIC\n{return_type}\n{name} (")
    end = source.index("\nSTATIC\n", start + 1)
    return source[start:end]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    cc = shutil.which(args.cc)
    if cc is None:
        raise SystemExit(f"host C compiler not found: {args.cc}")
    source_path = Path(__file__).resolve().parents[1] / "J716fHandoffDiagnostics.c"
    source = source_path.read_text(encoding="utf-8")
    definitions = "\n".join(re.findall(r"^#define MAX_(?:MAP_|INITRD_CRC_).*$", source, re.MULTILINE))
    start = source.index("typedef struct {")
    end = source.index("} MAP_SNAPSHOT;", start) + len("} MAP_SNAPSHOT;")
    actual = "\n".join([
        definitions, source[start:end],
        function(source, "EFI_STATUS", "CaptureMap"),
        function(source, "BOOLEAN", "DescriptorEnd"),
        function(source, "BOOLEAN", "CanReadFdt"),
        function(source, "BOOLEAN", "CanReadInitrd"),
        function(source, "VOID", "ReportInitrdCrc32"),
    ])
    with tempfile.TemporaryDirectory(prefix="j716f-diag-test-") as directory:
        path = Path(directory)
        program = path / "diagnostics-test.c"
        binary = path / "diagnostics-test"
        program.write_text(HARNESS_PREFIX + actual + HARNESS_TESTS, encoding="utf-8")
        command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-g"]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run(command + [str(program), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print(f"Source exercised: {source_path}")


if __name__ == "__main__":
    main()
