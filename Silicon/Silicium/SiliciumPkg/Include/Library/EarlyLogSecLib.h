// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef EARLY_LOG_SEC_LIB_H
#define EARLY_LOG_SEC_LIB_H
#include <Uefi.h>
// Capture is called exactly once before RamLogReset, with no allocation/logging.
VOID EFIAPI EarlyLogSecCapture (VOID);
// Publish is called after successful memory/HOB construction, at most once.
VOID EFIAPI EarlyLogSecPublish (VOID);
#endif
