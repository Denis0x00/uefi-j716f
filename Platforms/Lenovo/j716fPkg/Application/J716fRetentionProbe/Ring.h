// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_RING_H
#define J716F_RING_H
#ifdef J716F_RING_HOST
#include <stdint.h>
#include <stddef.h>
typedef uint8_t UINT8;
typedef uint32_t UINT32;
typedef size_t UINTN;
#else
#include <Base.h>
#endif

#define J716F_CONSOLE_BASE  0xB00BE000ULL
#define J716F_CONSOLE_BYTES 0x40000U
#define J716F_RING_CAPACITY (J716F_CONSOLE_BYTES - 12U)
#define J716F_DBGC          0x43474244U

typedef struct {
  UINT32 Signature;
  UINT32 Start;  // next byte to write, not oldest byte
  UINT32 Size;
  UINT8 Data[J716F_RING_CAPACITY];
} J716F_RING;

static int
RingValid (volatile J716F_RING *Ring)
{
  return Ring->Signature == J716F_DBGC &&
         Ring->Start < J716F_RING_CAPACITY &&
         Ring->Size <= J716F_RING_CAPACITY &&
         Ring->Start <= Ring->Size;
}

// Single writer, Boot Services only. Never repairs/initializes a bad header.
// As with Linux console, a full ring replaces its oldest bytes.
static int
RingAppend (volatile J716F_RING *Ring, const UINT8 *Text, UINTN Length,
            void (*Barrier)(void))
{
  UINT32 Start, Size, Next;
  UINTN Index;
  if (!RingValid (Ring) || Length == 0 || Length > J716F_RING_CAPACITY) {
    return 0;
  }
  Start = Ring->Start;
  Size = Ring->Size;
  for (Index = 0; Index < Length; ++Index) {
    Next = (Start + (UINT32)Index) % J716F_RING_CAPACITY;
    Ring->Data[Next] = Text[Index];
  }
  Barrier ();
  Ring->Size = Size + Length > J716F_RING_CAPACITY ?
               J716F_RING_CAPACITY : Size + (UINT32)Length;
  Ring->Start = (Start + (UINT32)Length) % J716F_RING_CAPACITY;
  Barrier ();
  return 1;
}

// Match in chronological order, including across the physical ring wrap.
static UINT32
RingCount (volatile J716F_RING *Ring, const UINT8 *Text, UINTN Length)
{
  UINT32 Size, Oldest, At, Count = 0;
  UINTN Index;
  if (!RingValid (Ring) || Length == 0 || Length > Ring->Size) {
    return 0;
  }
  Size = Ring->Size;
  Oldest = Size == J716F_RING_CAPACITY ? Ring->Start : 0;
  for (At = 0; At <= Size - Length; ++At) {
    for (Index = 0; Index < Length; ++Index) {
      if (Ring->Data[(Oldest + At + Index) % J716F_RING_CAPACITY] != Text[Index]) {
        break;
      }
    }
    if (Index == Length) {
      ++Count;
    }
  }
  return Count;
}
#endif
