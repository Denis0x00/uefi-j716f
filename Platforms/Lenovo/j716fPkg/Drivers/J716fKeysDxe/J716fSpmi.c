// SPDX-License-Identifier: BSD-2-Clause-Patent
// Independent limited v5 observer reader. Never issues a PMIC write command.
#include "J716fInput.h"

J716F_INPUT_U32 J716fSpmiInitialize (const J716F_SPMI_BUS *B, J716F_SPMI_STATE *S) {
  J716F_INPUT_U32 I, Entry, Ppid, Owner, *Target;
  if (B == 0 || S == 0 || B->Read32 == 0 || B->Write32 == 0 || B->DelayUs == 0) { return J716fSpmiBadBus; }
  S->Ready = 0; S->LastStatus = 0; S->LastAddress = 0;
  S->PonApid = J716F_SPMI_APID_COUNT; S->GpioApid = J716F_SPMI_APID_COUNT;
  S->Version = B->Read32 (B->Context, J716F_SPMI_CORE);
  if ((S->Version >> 28) != 5) { return J716fSpmiVersion; }
  for (I = 0; I < J716F_SPMI_APID_COUNT; ++I) {
    Entry = B->Read32 (B->Context, J716F_SPMI_CORE + J716F_SPMI_MAP_OFFSET + I * 4);
    if (!Entry) { continue; }
    Ppid = (Entry >> 8) & 0xfff;
    if (Ppid == (J716F_PON_RT_STS >> 8)) { Target = &S->PonApid; }
    else if (Ppid == (J716F_GPIO6_RT_STS >> 8)) { Target = &S->GpioApid; }
    else { continue; }
    Owner = B->Read32 (B->Context, J716F_SPMI_CONFIG + J716F_SPMI_OWNER_OFFSET + I * 4) & 7;
    // Observer reads do not require write ownership. Prefer this EE's mapping;
    // if none exists use the first matching observer mapping, as v5 allows.
    if (*Target == J716F_SPMI_APID_COUNT || Owner == J716F_SPMI_EE) { *Target = I; }
  }
  if (S->PonApid == J716F_SPMI_APID_COUNT || S->GpioApid == J716F_SPMI_APID_COUNT) { return J716fSpmiMapping; }
  S->Ready = 1;
  return J716fSpmiOk;
}

J716F_INPUT_U32 J716fSpmiReadByte (const J716F_SPMI_BUS *B, J716F_SPMI_STATE *S, J716F_INPUT_U32 Address, J716F_INPUT_U32 *Value) {
  J716F_INPUT_U32 Apid, Offset, Attempt, Status;
  if (B == 0 || S == 0 || Value == 0 || B->Read32 == 0 || B->Write32 == 0 || B->DelayUs == 0) { return J716fSpmiBadBus; }
  *Value = 0;
  if (!S->Ready || (S->Version >> 28) != 5) { return J716fSpmiVersion; }
  // J716F: only SID0 PON and GPIO6 RT_STS bytes are readable.
  if (Address == J716F_PON_RT_STS) { Apid = S->PonApid; }
  else if (Address == J716F_GPIO6_RT_STS) { Apid = S->GpioApid; }
  else { return J716fSpmiAddress; }
  if (Apid >= J716F_SPMI_APID_COUNT) { return J716fSpmiMapping; }
  Offset = 0x10000 * J716F_SPMI_EE + 0x80 * Apid;
  if (Offset > J716F_SPMI_OBSERVER_SIZE - 0x20) { return J716fSpmiMapping; }
  S->LastAddress = Address; S->LastStatus = 0;
  B->Write32 (B->Context, J716F_SPMI_OBSERVER + Offset, (1u << 27) | ((Address & 255) << 4));
  for (Attempt = 0; Attempt < J716F_SPMI_TIMEOUT_US; ++Attempt) {
    Status = B->Read32 (B->Context, J716F_SPMI_OBSERVER + Offset + 8);
    S->LastStatus = Status;
    if (Status & 1) {
      if (Status & 4) { return J716fSpmiDenied; }
      if (Status & 10) { return J716fSpmiFailure; }
      *Value = B->Read32 (B->Context, J716F_SPMI_OBSERVER + Offset + 0x18) & 255;
      return J716fSpmiOk;
    }
    B->DelayUs (B->Context, 1);
  }
  return J716fSpmiTimeout;
}
