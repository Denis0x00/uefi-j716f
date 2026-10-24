// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_IO_RESULT_H
#define J716F_IO_RESULT_H
#define J716F_IO_RESULT_GUID {0xb61c4081,0x3e3b,0x424a,{0xb5,0x93,0xa9,0xf7,0xf3,0x22,0x1b,0xdd}}
#define J716F_IO_RESULT_MAGIC 0x4A37313646494F31ULL
typedef struct {
  UINT64 Magic;
  UINT32 Version, Size, Checks, Passed, Failed, Complete;
  EFI_STATUS GopStatus;
} J716F_IO_RESULT;
#endif
