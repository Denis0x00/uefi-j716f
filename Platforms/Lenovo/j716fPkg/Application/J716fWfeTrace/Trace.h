// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_TRACE_H
#define J716F_TRACE_H
#include "Ring.h"

#define TRACE_BYTES 64U
#define TRACE_INIT 1U
#define TRACE_BASELINE 2U
#define TRACE_PRE_RESET 3U
#define TRACE_RESET_RETURN 4U
#define TRACE_PRE_VERSION 5U
#define TRACE_VERSION_RETURN 6U
#define TRACE_PRE_FEATURE 7U
#define TRACE_FEATURE_RETURN 8U
#define TRACE_ARM_KERNEL 9U
#define TRACE_KERNEL_ENTRY 10U
#define TRACE_PRE_MMU 11U
#define TRACE_POST_MMU 12U
#define TRACE_MAP_FAILURE 13U
#define TRACE_KERNEL_RESET_RETURN 14U
#define TRACE_ARM_COOKIE 0x5031364b45524e4cULL

typedef struct {
  UINT32 Count, Id, Phase, Crc;
} TRACE_SCAN;

static UINT32 Read32 (const UINT8 *P) {
  return (UINT32)P[0] | ((UINT32)P[1]<<8) | ((UINT32)P[2]<<16) | ((UINT32)P[3]<<24);
}
static void Write32 (UINT8 *P, UINT32 V) {
  UINTN I; for (I=0; I<4; ++I) { P[I]=(UINT8)(V>>(I*8)); }
}
// IEEE CRC32; volatile reads reach the existing uncached reserved mapping.
static UINT32 TraceCrc (const volatile UINT8 *P, UINTN Bytes) {
  UINT32 C=0xffffffffU; UINTN I, J;
  for (I=0; I<Bytes; ++I) {
    C ^= P[I];
    for (J=0; J<8; ++J) { C=(C>>1) ^ (0xedb88320U & (0U-(C&1U))); }
  }
  return ~C;
}
static void TraceMake (UINT8 *P, UINT32 Id, UINT32 Phase, UINT32 Low, UINT32 High,
                       UINT32 Sig, UINT32 Start, UINT32 Size) {
  const UINT8 Magic[8]={'J','7','1','6','F','P','8','!'};
  const UINT8 End[8]={'P','8','D','O','N','E','!','\n'};
  UINTN I; UINT32 C;
  for (I=0; I<TRACE_BYTES; ++I) { P[I]=0; }
  for (I=0; I<8; ++I) { P[I]=Magic[I]; P[56+I]=End[I]; }
  Write32(P+8,1); Write32(P+12,TRACE_BYTES); Write32(P+16,Id); Write32(P+20,Phase);
  Write32(P+24,Low); Write32(P+28,High);
  Write32(P+32,Sig); Write32(P+36,Start); Write32(P+40,Size);
  C=TraceCrc(P,48); Write32(P+48,C); Write32(P+52,~C);
}
static int TraceValid (const UINT8 *P) {
  const UINT8 Magic[8]={'J','7','1','6','F','P','8','!'};
  const UINT8 End[8]={'P','8','D','O','N','E','!','\n'};
  UINTN I; UINT32 C;
  for (I=0; I<8; ++I) { if (P[I]!=Magic[I] || P[56+I]!=End[I]) { return 0; } }
  C=TraceCrc(P,48);
  return Read32(P+8)==1 && Read32(P+12)==TRACE_BYTES && Read32(P+16)!=0 &&
         Read32(P+20)>=TRACE_INIT && Read32(P+20)<=21U &&
         Read32(P+44)==0 && Read32(P+48)==C && Read32(P+52)==~C;
}
// Raw mode searches stale payload even when the header is damaged. It does not
// count those records as logical retained logs, and never accesses other zones.
static TRACE_SCAN TraceScan (volatile J716F_RING *Ring, int Raw) {
  TRACE_SCAN Result={0,0,0,0};
  UINT32 Size, Oldest, At; UINTN I; UINT8 P[TRACE_BYTES];
  if (!Raw && !RingValid(Ring)) { return Result; }
  Size=Raw ? J716F_RING_CAPACITY : Ring->Size;
  Oldest=Raw ? 0 : (Size==J716F_RING_CAPACITY ? Ring->Start : 0);
  if (Size<TRACE_BYTES) { return Result; }
  // Raw mode also considers records that cross the physical ring seam.
  for (At=0; At < (Raw ? Size : Size-TRACE_BYTES+1); ++At) {
    if (Ring->Data[(Oldest+At)%J716F_RING_CAPACITY]!='J') { continue; }
    for (I=0; I<TRACE_BYTES; ++I) { P[I]=Ring->Data[(Oldest+At+I)%J716F_RING_CAPACITY]; }
    if (!TraceValid(P)) { continue; }
    ++Result.Count;
    // A raw physical scan has no reliable chronological origin after header
    // loss. Within its highest ID, prefer PRE_RESET for photographic matching.
    // Logical mode still returns the chronologically last record of that ID.
    if (Read32(P+16)>Result.Id || (Read32(P+16)==Result.Id &&
        (!Raw || Read32(P+20)==TRACE_PRE_RESET || Result.Phase!=TRACE_PRE_RESET))) {
      Result.Id=Read32(P+16); Result.Phase=Read32(P+20); Result.Crc=Read32(P+48);
    }
  }
  return Result;
}
typedef struct { UINT32 Count, Crc; UINT64 Value; } TRACE_DETAIL;
static TRACE_DETAIL TraceFind (volatile J716F_RING *Ring, int Raw, UINT32 Id, UINT32 Phase) {
  TRACE_DETAIL R={0,0,0}; UINT32 Size, Oldest, At; UINTN I; UINT8 P[TRACE_BYTES];
  if (!Raw && !RingValid(Ring)) { return R; }
  Size=Raw?J716F_RING_CAPACITY:Ring->Size;
  Oldest=Raw?0:(Size==J716F_RING_CAPACITY?Ring->Start:0);
  if(Size<TRACE_BYTES) { return R; }
  for(At=0; At<(Raw?Size:Size-TRACE_BYTES+1); ++At) {
    if(Ring->Data[(Oldest+At)%J716F_RING_CAPACITY]!='J') { continue; }
    for(I=0; I<TRACE_BYTES; ++I) { P[I]=Ring->Data[(Oldest+At+I)%J716F_RING_CAPACITY]; }
    if(!TraceValid(P) || Read32(P+16)!=Id || Read32(P+20)!=Phase) { continue; }
    ++R.Count; R.Crc=Read32(P+48); R.Value=(UINT64)Read32(P+24)|((UINT64)Read32(P+28)<<32);
  }
  return R;
}

// P16 uses unused linear tail only; 64-byte ENTRY, six slots, 64-byte return.
#define P16_SLOTS 6U
#define P16_TAG 0x5031365300000000ULL
static int P16Room (volatile J716F_RING *R, UINT32 Extra) {
  return RingValid(R) && R->Start==R->Size && R->Size<=J716F_RING_CAPACITY-(576U+Extra);
}
static void P16Copy (volatile J716F_RING *R, UINT32 At, UINT8 *P) {
  UINTN I; for(I=0;I<TRACE_BYTES;++I) P[I]=R->Data[At+I];
}
// Require one exact ARM anchor, not only matching ID or a stale raw tag.
static int P16Anchor (volatile J716F_RING *R, UINT32 Id, UINT32 *Base, UINT32 *Crc) {
  UINT32 At,Hits=0; UINT8 P[TRACE_BYTES];
  for(At=0;At+TRACE_BYTES<=J716F_RING_CAPACITY;++At) {
    if(R->Data[At]!='J') continue;
    P16Copy(R,At,P);
    if(!TraceValid(P) || Read32(P+16)!=Id || Read32(P+20)!=TRACE_ARM_KERNEL ||
       ((UINT64)Read32(P+24)|((UINT64)Read32(P+28)<<32))!=TRACE_ARM_COOKIE ||
       Read32(P+32)!=0x43474244U || Read32(P+36)!=At || Read32(P+40)!=At ||
       At>J716F_RING_CAPACITY-576U) continue;
    ++Hits; *Base=At+128U; *Crc=Read32(P+48);
  }
  return Hits==1;
}
static TRACE_DETAIL P16Find (volatile J716F_RING *R, int Raw, UINT32 Id, UINT32 Phase) {
  TRACE_DETAIL D={0,0,0}; UINT32 Base,Crc,At; UINT8 P[TRACE_BYTES];
  if(Phase<16U || Phase>21U || !P16Anchor(R,Id,&Base,&Crc)) return D;
  // ENTRY must consume the same ARM and bind its full CRC in the upper word.
  P16Copy(R,Base-64U,P);
  if(!TraceValid(P) || Read32(P+16)!=Id || Read32(P+20)!=TRACE_KERNEL_ENTRY ||
     Read32(P+28)!=Crc || (Read32(P+24)&1U) || Read32(P+32)!=0x43474244U ||
     Read32(P+36)!=Base-64U || Read32(P+40)!=Base-64U) return D;
  At=Base+(Phase-16U)*64U;
  if(!Raw && (!RingValid(R) || R->Start!=R->Size || At+64U>R->Size)) return D;
  P16Copy(R,At,P);
  if(!TraceValid(P) || Read32(P+16)!=Id || Read32(P+20)!=Phase ||
     Read32(P+24)!=Crc || Read32(P+28)!=0x50313653U ||
     Read32(P+32)!=0x43474244U || Read32(P+36)!=At || Read32(P+40)!=At) return D;
  D.Count=1; D.Crc=Read32(P+48); D.Value=(UINT64)Read32(P+24)|((UINT64)Read32(P+28)<<32);
  return D;
}

// Full terminal records must follow the bound P16 slot at the exact offset.
// Generic phase11/12 records from older diagnostics must not become P16 results.
static TRACE_DETAIL P16Final (volatile J716F_RING *R, UINT32 Id, UINT32 Phase) {
  TRACE_DETAIL D={0,0,0},Witness; UINT32 Base,Crc,At,Prior,I; UINT8 P[TRACE_BYTES];
  if(Phase==TRACE_KERNEL_RESET_RETURN) {
    for(I=TRACE_PRE_MMU;I<=TRACE_MAP_FAILURE;++I) {
      Witness=P16Final(R,Id,I); if(!Witness.Count) continue;
      if(!P16Anchor(R,Id,&Base,&Crc)) return D;
      At=Base+(I==TRACE_PRE_MMU?256U:I==TRACE_POST_MMU?384U:64U)+64U;
      if(At>J716F_RING_CAPACITY-TRACE_BYTES) continue;
      P16Copy(R,At,P);
      if(TraceValid(P) && Read32(P+16)==Id && Read32(P+20)==Phase &&
         Read32(P+32)==0x43474244U && Read32(P+36)==At && Read32(P+40)==At) {
        ++D.Count; D.Crc=Read32(P+48); D.Value=(UINT64)Read32(P+24)|((UINT64)Read32(P+28)<<32);
      }
    }
    return D;
  }
  if(Phase==TRACE_PRE_MMU) return D;
  if(Phase<TRACE_PRE_MMU || Phase>TRACE_MAP_FAILURE || !P16Anchor(R,Id,&Base,&Crc)) return D;
  Prior=Phase==TRACE_PRE_MMU?19U:Phase==TRACE_POST_MMU?21U:16U;
  Witness=P16Find(R,1,Id,Prior); if(!Witness.Count) return D;
  At=Base+(Phase==TRACE_PRE_MMU?256U:Phase==TRACE_POST_MMU?384U:64U);
  if(At>J716F_RING_CAPACITY-TRACE_BYTES) return D;
  P16Copy(R,At,P);
  if(!TraceValid(P) || Read32(P+16)!=Id || Read32(P+20)!=Phase ||
     Read32(P+32)!=0x43474244U || Read32(P+36)!=At || Read32(P+40)!=At ||
     (Phase==TRACE_PRE_MMU && (Read32(P+24)&1U))) return D;
  D.Count=1; D.Crc=Read32(P+48); D.Value=(UINT64)Read32(P+24)|((UINT64)Read32(P+28)<<32);
  return D;
}
#endif
