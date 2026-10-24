#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
#include <Library/TimerLib.h>

#include "FrameBuffer.h"
#include "Font.h"

inline
STATIC
UINT8*
DrawPixel (
  IN UINT8  *Pixels,
  IN UINT32  PixelColor,
  IN UINT32  FontScale)
{
  // Repeat Action
  for (UINT8 i = 0; i < FontScale; i++) {
    // Save Pixel Color
    UINT32 Color = PixelColor;

    // Go theu each Byte
    for (UINT8 j = 0; j < FB_BPP; j++) {
      // Update Pixel Color
      *Pixels++ = (UINT8)Color;

      // Shift Pixel Color
      Color >>= 8;
    }
  }

  return Pixels;
}

inline
STATIC
UINT8*
DrawRow (
  IN UINT8  *Pixels,
  IN UINT32  RowData,
  IN UINT32  FontScale,
  IN UINTN   Stride)
{
  // Repeat Action
  for (UINT8 i = 0; i < FontScale; i++) {
    // Save Row Data
    UINT32 Data = RowData;

    // Go thru each Pixel
    for (UINT8 j = 0; j < FONT_WIDTH; j++) {
      // Set Pixel Color
      UINT32 PixelColor = (Data & 1) ? 0xFFFFFFFF : 0x00000000;
      
      // Draw Pixel Color
      Pixels = DrawPixel (Pixels, PixelColor, FontScale);

      // Shift Row
      Data >>= 1;
    }

    // Append Stride
    Pixels += Stride;
  }
  
  return Pixels;
}

VOID
DrawGlyph (
  IN UINT8  *Pixels,
  IN UINT64  Glyph,
  IN UINT32  FontScale)
{
  // Calculate Stride
  UINTN Stride = (FB_WIDTH - (FONT_WIDTH * FontScale)) * FB_BPP;

  // Split Glyph
  UINT32 TopGlyph    = (UINT32)(Glyph >> 32);
  UINT32 BottomGlyph = (UINT32)Glyph;

  // Set Inital Row Data
  UINT32 RowData = TopGlyph;

  // Go thru each Row
  for (UINT8 Row = 0; Row < 12; Row++) {
    // Switch Row Data
    if (Row == 6) {
      RowData = BottomGlyph;
    }

    // Draw Row
    Pixels = DrawRow (Pixels, RowData, FontScale, Stride);

    // Shift to next Row
    RowData >>= FONT_WIDTH;
  }
}

VOID
AdvanceNewLine (
  IN EFI_PHYSICAL_ADDRESS       FbBase,
  IN UINT64                     FbLength,
  IN EFI_FRAME_BUFFER_POSITION *CurrentPosition,
  IN EFI_FRAME_BUFFER_POSITION  MaxPosition)
{
  // Wait
  MicroSecondDelay (FB_DELAY_US);

  // Update Position
  CurrentPosition->XPos  = 0;
  CurrentPosition->YPos += 1;

  // Verify Y Position
  if (CurrentPosition->YPos >= MaxPosition.YPos) {
    // Clear Frame Buffer
    ZeroMem ((VOID *)FbBase, FB_WIDTH * FB_HEIGHT * FB_BPP);

    // Reset X & Y Position
    CurrentPosition->XPos = 0;
    CurrentPosition->YPos = 0;
  }
}

UINT8
GetFontScale ()
{
  // Get Smaller Display Dimension
  UINT32 ShorterDimension = (FB_WIDTH < FB_HEIGHT) ? FB_WIDTH : FB_HEIGHT;

  // Return Calculated Font Scale
  return (ShorterDimension < 426) ? 1 : (UINT8)(ShorterDimension / 426);
}

VOID
GetFbPositions (
  IN  EFI_PHYSICAL_ADDRESS        FbBase,
  IN  UINT64                      FbLength,
  IN  UINT8                       FontScale,
  OUT EFI_FRAME_BUFFER_POSITION **CurrentPosition,
  OUT EFI_FRAME_BUFFER_POSITION  *MaxPosition)
{
  EFI_FRAME_BUFFER_POSITION LocalMaxPosition;

  // Get Current Position
  *CurrentPosition = (EFI_FRAME_BUFFER_POSITION *)(FbBase + FbLength - sizeof (EFI_FRAME_BUFFER_POSITION));

  // Calculate Max Positions
  LocalMaxPosition.XPos = FB_WIDTH  / ((FONT_WIDTH  + 1) * FontScale);
  LocalMaxPosition.YPos = FB_HEIGHT / ((FONT_HEIGHT - 4) * FontScale);

  // Pass Max Position
  *MaxPosition = LocalMaxPosition;
}

VOID
WriteFrameBuffer (
  IN EFI_PHYSICAL_ADDRESS FbBase,
  IN UINT64               FbLength,
  IN UINT8                Character)
{
  EFI_FRAME_BUFFER_POSITION *CurrentPosition;
  EFI_FRAME_BUFFER_POSITION  MaxPosition;
  UINT8                     *Pixels;

  // Set Glyph Font
  STATIC CONST UINT64 GlyphFont[] = GLYPH_FONT;

  // Set Glyph Font Scale
  UINT8 GlyphFontScale = GetFontScale ();

  // Get Frame Buffer Positions
  GetFbPositions (FbBase, FbLength, GlyphFontScale, &CurrentPosition, &MaxPosition);

  // Skip Invalid ACSII Characters
  if (Character >= 127) {
    return;
  }

  // Handle Control Characters
  if (Character < 32) {
    if (Character == '\f') {
      // Start a diagnostic page without losing its first lines at page wrap.
      ZeroMem ((VOID *)FbBase, FB_WIDTH * FB_HEIGHT * FB_BPP);
      CurrentPosition->XPos = 0;
      CurrentPosition->YPos = 0;
    } else if (Character == '\n') {
      AdvanceNewLine (FbBase, FbLength, CurrentPosition, MaxPosition);
    } else if (Character == '\r') {
      CurrentPosition->XPos = 0;
    }

    return;
  }

  // Skip leading Spaces
  if (CurrentPosition->XPos == 0 && Character == ' ') {
    return;
  }

  // Translate ACSII Character
  UINT64 Glyph = GlyphFont[Character - 32];

  // Calculate Glyph Dimension
  UINTN GlyphWidth  = (FONT_WIDTH  + 1) * GlyphFontScale;
  UINTN GlyphHeight = (FONT_HEIGHT - 4) * GlyphFontScale;

  // Set Glyph Pixels
  Pixels  = (VOID *)FbBase;
  Pixels += (UINTN)CurrentPosition->YPos * GlyphHeight * (FB_WIDTH * FB_BPP);
  Pixels += (UINTN)CurrentPosition->XPos * GlyphWidth  * FB_BPP;

  // Draw Glyph
  DrawGlyph (Pixels, Glyph, GlyphFontScale);

  // Increase X Position
  CurrentPosition->XPos++;

  // Check Max X Position
  if (CurrentPosition->XPos >= MaxPosition.XPos) {
    AdvanceNewLine (FbBase, FbLength, CurrentPosition, MaxPosition);
  }
}

EFI_STATUS
GetFrameBufferMemory (
  OUT EFI_PHYSICAL_ADDRESS *Base,
  OUT UINT64               *Length)
{
  EFI_MEMORY_REGION_DESCRIPTOR FbRegion;

  // Locate "Display Reserved" Memory Region
  LocateMemoryRegionByName ("Display Reserved", &FbRegion);
  LocateMemoryRegionByName ("Display_Reserved", &FbRegion);
  if (!FbRegion.Address && !FbRegion.Length) {
    return EFI_NOT_FOUND;
  }

  // Pass Frame Buffer Memory
  *Base   = FbRegion.Address;
  *Length = FbRegion.Length;

  return EFI_SUCCESS;
}

// Board-only diagnostic for the stock TzDxe's status-only assert report.
// The vendor report has Data=NULL, so the normal status handler has no line
// number to decode. Capture frame records while that report is still active.
STATIC
BOOLEAN
J716fBufferContains (
  IN CONST UINT8  *Buffer,
  IN UINTN        Length,
  IN CONST CHAR8  *Needle,
  IN UINTN        NeedleLength)
{
  UINTN Index;

  if (Length < NeedleLength) {
    return FALSE;
  }

  for (Index = 0; Index <= Length - NeedleLength; Index++) {
    if (CompareMem (Buffer + Index, Needle, NeedleLength) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
VOID
J716fTraceText (
  IN EFI_PHYSICAL_ADDRESS  FbBase,
  IN UINT64                FbLength,
  IN CONST CHAR8           *Text,
  IN UINTN                 Length)
{
  UINTN Index;

  // Use the renderer directly; never recurse through SerialPortWrite/DEBUG.
  for (Index = 0; Index < Length; Index++) {
    WriteFrameBuffer (FbBase, FbLength, (UINT8)Text[Index]);
  }
}

STATIC
VOID
J716fTzAssertTrace (
  IN EFI_PHYSICAL_ADDRESS  FbBase,
  IN UINT64                FbLength,
  IN CONST UINT8           *Message,
  IN UINTN                 MessageLength)
{
  EFI_MEMORY_REGION_DESCRIPTOR Stack;
  UINTN                        Frame;
  UINTN                        StackPointer;
  UINTN                        Start;
  UINTN                        End;
  UINTN                        Next;
  UINTN                        Count;
  UINTN                        Index;
  UINTN                        Frames[20];
  UINTN                        Returns[20];
  UINTN                        Length;
  CHAR8                        Line[160];
  CONST CHAR8                  *Stop;

  __asm__ __volatile__ ("mov %0, x29" : "=r" (Frame));
  __asm__ __volatile__ ("mov %0, sp" : "=r" (StackPointer));
  Count = 0;
  Stop  = "limit (20 frames)";

  J716fTraceText (FbBase, FbLength, "\fJ716F TZ ASSERT TRACE V7\n", sizeof ("\fJ716F TZ ASSERT TRACE V7\n") - 1);
  J716fTraceText (FbBase, FbLength, (CONST CHAR8 *)Message, MessageLength);
  // Preserve the identifying report even if later frame capture faults.
  WriteBackInvalidateDataCacheRange ((VOID *)FbBase, FbLength);

  ZeroMem (&Stack, sizeof (Stack));
  if (EFI_ERROR (LocateMemoryRegionByName ("UEFI Stack", &Stack))) {
    Stop = "UEFI Stack descriptor missing";
  } else {
    Start = (UINTN)Stack.Address;
    End   = Start + (UINTN)Stack.Length;
    if ((Stack.Length < 2 * sizeof (UINTN)) || (End < Start) ||
        (StackPointer < Start) || (StackPointer >= End)) {
      Stop = "current SP outside declared UEFI Stack";
    } else {
      // Follow only aligned, ascending frame records within this stack.
      // No raw stack scan, module/header probe, heap read or allocation.
      for (Index = 0; Index < ARRAY_SIZE (Frames); Index++) {
        if (((Frame & 15) != 0) || (Frame < StackPointer) ||
            (Frame < Start) || (Frame > End - 2 * sizeof (UINTN))) {
          Stop = "frame outside aligned stack bounds";
          break;
        }

        Frames[Count]  = Frame;
        Returns[Count] = ((CONST volatile UINTN *)Frame)[1];
        Next           = ((CONST volatile UINTN *)Frame)[0];
        Count++;
        if (Next == 0) {
          Stop = "end of frame chain";
          break;
        }

        if (Next <= Frame) {
          Stop = "nonascending frame chain";
          break;
        }

        Frame = Next;
      }
    }
  }

  Length = AsciiSPrint (Line, sizeof (Line), "SP=%016lx STACK=%016lx+%08lx\n",
                       StackPointer, Stack.Address, Stack.Length);
  J716fTraceText (FbBase, FbLength, Line, Length);
  for (Index = 0; Index < Count; Index++) {
    Length = AsciiSPrint (Line, sizeof (Line), "#%02u FP=%016lx LR=%016lx\n",
                         (UINT32)Index, Frames[Index], Returns[Index]);
    J716fTraceText (FbBase, FbLength, Line, Length);
  }

  Length = AsciiSPrint (Line, sizeof (Line), "V7 TRACE END: %a\n", Stop);
  J716fTraceText (FbBase, FbLength, Line, Length);
}

UINTN
EFIAPI
SerialPortWrite (
  IN UINT8 *Buffer,
  IN UINTN  NumberOfBytes)
{
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS FbBase;
  UINT64               FbLength;
  STATIC BOOLEAN       TzAssertTraced = FALSE;
  BOOLEAN              TraceTzAssert;

  // Get Frame Buffer Memory
  Status = GetFrameBufferMemory (&FbBase, &FbLength);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  // Get Interrupts State
  BOOLEAN InterruptState = ArmGetInterruptState ();

  // Disable Interrupts
  if (InterruptState) {
    ArmDisableInterrupts ();
  }

  TraceTzAssert = !TzAssertTraced &&
    J716fBufferContains (Buffer, NumberOfBytes, "ERROR: C90000002:V03000007", sizeof ("ERROR: C90000002:V03000007") - 1) &&
    J716fBufferContains (Buffer, NumberOfBytes, "6925FAD3-E243-4B69-AD61-3F978C8ABC60", sizeof ("6925FAD3-E243-4B69-AD61-3F978C8ABC60") - 1);
  if (TraceTzAssert) {
    TzAssertTraced = TRUE;
    J716fTzAssertTrace (FbBase, FbLength, Buffer, NumberOfBytes);
  } else {
    // Write Debug Message to Frame Buffer
    for (UINTN i = 0; i < NumberOfBytes; i++) {
      WriteFrameBuffer (FbBase, FbLength, *Buffer++);
    }
  }

  // Enable Interrupts
  if (InterruptState) {
    ArmEnableInterrupts ();
  }

  // Flush Frame Buffer
  WriteBackInvalidateDataCacheRange ((VOID *)FbBase, FbLength);

  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortRead (
  OUT UINT8 *Buffer,
  IN  UINTN  NumberOfBytes)
{
  return 0;
}

BOOLEAN
EFIAPI
SerialPortPoll ()
{
  return FALSE;
}

EFI_STATUS
EFIAPI
SerialPortSetControl (IN UINT32 Control)
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
SerialPortGetControl (OUT UINT32 *Control)
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64             *BaudRate,
  IN OUT UINT32             *ReceiveFifoDepth,
  IN OUT UINT32             *Timeout,
  IN OUT EFI_PARITY_TYPE    *Parity,
  IN OUT UINT8              *DataBits,
  IN OUT EFI_STOP_BITS_TYPE *StopBits)
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
SerialPortInitialize ()
{
  return EFI_SUCCESS;
}
