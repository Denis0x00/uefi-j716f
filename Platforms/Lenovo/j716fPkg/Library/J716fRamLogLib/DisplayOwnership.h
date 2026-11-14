// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_DISPLAY_OWNERSHIP_H_
#define J716F_DISPLAY_OWNERSHIP_H_

#include <Uefi.h>

// Board-private, shared across independently linked SerialPortLib instances.
// SEC RamLogReset initializes this boot's state. Missing/invalid state fails
// closed: no framebuffer serial output and no successful graphics lease.
BOOLEAN EFIAPI J716fSerialMayDraw (VOID);

// Claim immediately around the one configured UniXNU StartImage call. Token
// binds this boot sequence AND this acquisition; stale/double releases fail.
BOOLEAN EFIAPI J716fDisplayClaim (OUT UINT64 *Token);
BOOLEAN EFIAPI J716fDisplayRelease (IN UINT64 Token);

#endif
