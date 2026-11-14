// Host-only ABI shims. Algorithms and board descriptors come from production.
#ifndef J716F_TEST_HOST_SHIM_H
#define J716F_TEST_HOST_SHIM_H
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <errno.h>
#include <libfdt.h>

typedef uint8_t UINT8;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int32_t INT32;
typedef uintptr_t UINTN;
typedef char CHAR8;
typedef void VOID;
typedef int BOOLEAN;
typedef uint64_t EFI_STATUS;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint32_t EFI_RESOURCE_TYPE;
typedef uint64_t EFI_RESOURCE_ATTRIBUTE_TYPE;
typedef uint32_t EFI_MEMORY_TYPE;
typedef uint32_t ARM_MEMORY_REGION_ATTRIBUTES;
typedef struct fdt_header FDT_HEADER;
#define STATIC static
#define CONST const
#define IN
#define OUT
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_NOT_READY 6
#define EFI_COMPROMISED_DATA 33
#define MAX_UINT64 UINT64_MAX
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define DEBUG(a) ((void)0)
#define DEBUG_ERROR 1
enum { EfiReservedMemoryType, EfiLoaderCode, EfiLoaderData,
  EfiBootServicesCode, EfiBootServicesData, EfiRuntimeServicesCode,
  EfiRuntimeServicesData, EfiConventionalMemory, EfiUnusableMemory,
  EfiACPIReclaimMemory, EfiACPIMemoryNVS, EfiMemoryMappedIO,
  EfiMemoryMappedIOPortSpace, EfiPalCode, EfiMaxMemoryType };
enum { EFI_RESOURCE_SYSTEM_MEMORY, EFI_RESOURCE_MEMORY_MAPPED_IO,
  EFI_RESOURCE_IO, EFI_RESOURCE_FIRMWARE_DEVICE, EFI_RESOURCE_MEMORY_MAPPED_IO_PORT,
  EFI_RESOURCE_MEMORY_RESERVED, EFI_RESOURCE_IO_RESERVED };
enum { ARM_MEMORY_REGION_ATTRIBUTE_DEVICE, ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED,
  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK, ARM_MEMORY_REGION_ATTRIBUTE_WRITE_THROUGH };
#define EFI_RESOURCE_ATTRIBUTE_PRESENT 0x1
#define EFI_RESOURCE_ATTRIBUTE_INITIALIZED 0x2
#define EFI_RESOURCE_ATTRIBUTE_TESTED 0x4
#define EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE 0x8
#define EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE 0x10
#define EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE 0x20
#define EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE 0x40
#define EFI_RESOURCE_ATTRIBUTE_EXECUTION_PROTECTABLE 0x80
#define EFI_RESOURCE_ATTRIBUTE_READ_PROTECTABLE 0x100
#define EFI_RESOURCE_ATTRIBUTE_WRITE_PROTECTABLE 0x200
#define EFI_RESOURCE_ATTRIBUTE_EXECUTION_PROTECTED 0x400
#define EFI_RESOURCE_ATTRIBUTE_READ_PROTECTED 0x800
#define EFI_RESOURCE_ATTRIBUTE_WRITE_PROTECTED 0x1000
#define AsciiStrCmp strcmp
static int AsciiStrCpyS(char *dest, size_t size, const char *src) {
  if (strlen(src) >= size) abort();
  strcpy(dest, src);
  return 0;
}
static uint64_t ReadUnaligned64(const void *p) { uint64_t v; memcpy(&v,p,8); return v; }
static uint32_t ReadUnaligned32(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
#define Fdt64ToCpu fdt64_to_cpu
#define Fdt32ToCpu fdt32_to_cpu
#define FdtCheckHeader fdt_check_header
#define FdtTotalSize fdt_totalsize
#define FdtVersion fdt_version
#define FdtOffsetDtStruct fdt_off_dt_struct
#define FdtSizeDtStruct fdt_size_dt_struct
#define FdtOffsetDtStrings fdt_off_dt_strings
#define FdtSizeDtStrings fdt_size_dt_strings
#define FdtOffsetMemRsvmap fdt_off_mem_rsvmap
#define FdtPathOffset fdt_path_offset
#define FdtGetProp fdt_getprop
#endif
