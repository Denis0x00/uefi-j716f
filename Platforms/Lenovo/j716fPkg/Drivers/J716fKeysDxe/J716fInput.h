// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_INPUT_H
#define J716F_INPUT_H

#define J716F_SPMI_CORE 0x0c440000
#define J716F_SPMI_CORE_SIZE 0x1100
#define J716F_SPMI_OBSERVER 0x0e600000
#define J716F_SPMI_OBSERVER_SIZE 0x100000
#define J716F_SPMI_CONFIG 0x0c40a000
#define J716F_SPMI_CONFIG_SIZE 0x26000
#define J716F_SPMI_MAP_OFFSET 0x900
#define J716F_SPMI_OWNER_OFFSET 0x700
#define J716F_SPMI_APID_COUNT 512
#define J716F_SPMI_EE 0
#define J716F_SPMI_TIMEOUT_US 1000
#define J716F_PON_RT_STS 0x0810
#define J716F_PON_PERPH_SUBTYPE 0x0805
#define J716F_PON_REASON1_GEN1 0x0808
#define J716F_PON_WARM_RESET_REASON1_GEN1 0x080a
#define J716F_PON_WARM_RESET_REASON2 0x080b
#define J716F_POFF_REASON1_GEN1 0x080c
#define J716F_PON_REASON1_GEN2 0x08c0
#define J716F_PON_WARM_RESET_REASON1_GEN2 0x08c2
#define J716F_POFF_REASON1_GEN2 0x08c5
#define J716F_PON_OFF_REASON 0x08c7
#define J716F_PON_FAULT_REASON1 0x08c8
#define J716F_PON_S3_RESET_REASON 0x08ca
#define J716F_PON_SOFT_RB_SPARE 0x088f
#define J716F_GPIO6_RT_STS 0xc510
#define J716F_KEY_DEBOUNCE_MS 30
#define J716F_KEY_REPEAT_DELAY_MS 500
#define J716F_KEY_REPEAT_MS 120
#define J716F_KEY_POWER_LONG_MS 700
#define J716F_KEY_POLL_100NS 100000
typedef unsigned int J716F_INPUT_U32;
typedef unsigned long long J716F_INPUT_U64;

enum { J716fSpmiOk, J716fSpmiVersion, J716fSpmiMapping, J716fSpmiAddress,
       J716fSpmiDenied, J716fSpmiFailure, J716fSpmiTimeout, J716fSpmiBadBus };
enum { J716fKeyNone, J716fKeyUp, J716fKeyDown, J716fKeyEnter, J716fKeyEscape, J716fKeyFlush };
enum { J716fRawUp = 1, J716fRawDown = 2, J716fRawPower = 4 };

typedef struct {
  J716F_INPUT_U32 (*Read32)(void *Context, J716F_INPUT_U64 Address);
  void (*Write32)(void *Context, J716F_INPUT_U64 Address, J716F_INPUT_U32 Value);
  void (*DelayUs)(void *Context, J716F_INPUT_U32 Us);
  void *Context;
} J716F_SPMI_BUS;

typedef struct {
  J716F_INPUT_U32 Version, Ready, PonApid, GpioApid, LastStatus, LastAddress;
} J716F_SPMI_STATE;

typedef struct {
  J716F_INPUT_U32 Initialized, Armed, Raw, Stable, PowerLong, Repeating;
  J716F_INPUT_U64 LastNow, RawSince, PressSince, RepeatSince;
} J716F_KEY_STATE;

J716F_INPUT_U32 J716fSpmiInitialize (const J716F_SPMI_BUS *Bus, J716F_SPMI_STATE *State);
J716F_INPUT_U32 J716fSpmiReadByte (const J716F_SPMI_BUS *Bus, J716F_SPMI_STATE *State,
                             J716F_INPUT_U32 Address, J716F_INPUT_U32 *Value);
J716F_INPUT_U32 J716fKeySample (J716F_INPUT_U32 Pon, J716F_INPUT_U32 Gpio);
J716F_INPUT_U32 J716fKeysUpdate (J716F_KEY_STATE *State, J716F_INPUT_U32 Raw, J716F_INPUT_U64 NowMs);
#endif
