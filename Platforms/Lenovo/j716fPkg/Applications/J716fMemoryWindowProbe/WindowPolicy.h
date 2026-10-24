// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_WINDOW_POLICY_H
#define J716F_WINDOW_POLICY_H
#define J716F_REGION_BASE 0xD0000000ULL
#define J716F_REGION_END  0xE8000000ULL
#define J716F_WINDOW_SIZE 0x10000000ULL
#define J716F_BASE_RESIDUE 0x7C000ULL
STATIC BOOLEAN J716fChooseWindow (UINT64 Start, UINT64 Pages, UINT64 *Address)
{
  UINT64 End, Aligned;
  if (Pages == 0 || Pages > (MAX_UINT64 >> 12)) { return FALSE; }
  if (Start > MAX_UINT64 - (Pages << 12)) { return FALSE; }
  End = Start + (Pages << 12);
  if (Start < J716F_REGION_BASE) { Start = J716F_REGION_BASE; }
  if (End > J716F_REGION_END) { End = J716F_REGION_END; }
  if (Start >= End || Start > MAX_UINT64 - 0x1fffff) { return FALSE; }
  Aligned = ((Start + 0x1fffff) & ~0x1fffffULL) + J716F_BASE_RESIDUE;
  if (Aligned < Start || Aligned > End || End - Aligned < J716F_WINDOW_SIZE) { return FALSE; }
  *Address = Aligned;
  return TRUE;
}
STATIC UINT64 J716fPattern (UINT64 Address, UINTN Round)
{
  UINT64 Value = Address ^ 0x4A37313646563134ULL;
  return Round == 0 ? Value : ~Value;
}
#endif
