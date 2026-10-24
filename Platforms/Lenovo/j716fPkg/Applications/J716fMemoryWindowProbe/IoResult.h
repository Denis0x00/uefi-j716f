// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_IO_RESULT_H
#define J716F_IO_RESULT_H
#define J716F_IO_RESULT_GUID {0x6ed49014,0x3e3b,0x424a,{0xb5,0x93,0xa9,0xf7,0xf3,0x22,0x1b,0xdd}}
#define J716F_IO_RESULT_MAGIC 0x4A37313646573134ULL
typedef struct {
  UINT64 Magic;
  UINT32 Version, Size, Checks, Passed, Failed, Complete;
  EFI_STATUS GopStatus;
  UINT64 WindowBase, WindowBytes, VerifiedBytes, FailureAddress;
} J716F_IO_RESULT;
#endif
