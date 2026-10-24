// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_IO_RESULT_H
#define J716F_IO_RESULT_H
#define J716F_IO_RESULT_GUID {0x547e2df1,0x3e3b,0x424a,{0xb5,0x93,0xa9,0xf7,0xf3,0x22,0x1b,0xdd}}
#define J716F_IO_RESULT_MAGIC 0x4A373136464C4432ULL
typedef struct {
  UINT64 Magic;
  UINT32 Version, Size, Checks, Passed, Failed, Complete;
  EFI_STATUS GopStatus;
} J716F_IO_RESULT;
#endif
