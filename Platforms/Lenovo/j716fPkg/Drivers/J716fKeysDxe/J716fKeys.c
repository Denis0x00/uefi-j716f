// SPDX-License-Identifier: BSD-2-Clause-Patent
// Debounced single-key events. Power confirms on release and never repeats.
#include "J716fInput.h"

J716F_INPUT_U32 J716fKeySample (J716F_INPUT_U32 Pon, J716F_INPUT_U32 Gpio) {
  return ((Pon & 1) ? J716fRawPower : 0) | ((Pon & 2) ? J716fRawDown : 0) | ((Gpio & 1) ? 0 : J716fRawUp);
}

J716F_INPUT_U32 J716fKeysUpdate (J716F_KEY_STATE *S, J716F_INPUT_U32 Raw, J716F_INPUT_U64 Now) {
  J716F_INPUT_U32 Old;
  if (S == 0) { return J716fKeyFlush; }
  if (Raw > 7) { S->Initialized = 0; S->Armed = 0; return J716fKeyFlush; }
  if (!S->Initialized || Now < S->LastNow) {
    S->Initialized = 1; S->Armed = 0; S->Raw = Raw; S->Stable = Raw;
    S->PowerLong = 0; S->Repeating = 0;
    S->LastNow = Now; S->RawSince = Now; S->PressSince = Now; S->RepeatSince = Now;
    return J716fKeyNone;
  }
  S->LastNow = Now;
  if (Raw && (Raw & (Raw - 1))) {
    S->Armed = 0; S->Raw = Raw; S->Stable = Raw; S->RawSince = Now;
    return J716fKeyFlush;
  }
  if (Raw != S->Raw) { S->Raw = Raw; S->RawSince = Now; return J716fKeyNone; }
  if (Now - S->RawSince < J716F_KEY_DEBOUNCE_MS) { return J716fKeyNone; }
  if (!S->Armed) {
    if (Raw == 0) { S->Armed = 1; S->Stable = 0; }
    return J716fKeyNone;
  }
  if (Raw != S->Stable) {
    Old = S->Stable; S->Stable = Raw; S->PressSince = Now; S->RepeatSince = Now; S->Repeating = 0;
    if (Raw == J716fRawPower) { S->PowerLong = 0; return J716fKeyNone; }
    if (Raw == J716fRawUp) { return J716fKeyUp; }
    if (Raw == J716fRawDown) { return J716fKeyDown; }
    if (Old == J716fRawPower && !S->PowerLong) { return J716fKeyEnter; }
    return J716fKeyNone;
  }
  if (Raw == J716fRawPower && !S->PowerLong && Now - S->PressSince >= J716F_KEY_POWER_LONG_MS) {
    S->PowerLong = 1; return J716fKeyEscape;
  }
  if ((Raw == J716fRawUp || Raw == J716fRawDown) &&
      Now - S->RepeatSince >= (S->Repeating ? J716F_KEY_REPEAT_MS : J716F_KEY_REPEAT_DELAY_MS)) {
    S->Repeating = 1; S->RepeatSince = Now;
    return Raw == J716fRawUp ? J716fKeyUp : J716fKeyDown;
  }
  return J716fKeyNone;
}
