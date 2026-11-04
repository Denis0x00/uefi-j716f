#ifndef _RAM_LOG_LIB_H_
#define _RAM_LOG_LIB_H_

//
// A copy of the debug output in memory that survives a warm reset, for a platform without a serial port. The Null
// instance does nothing; a platform instance may also restart the machine when the output is an ASSERT.
//

//
// Starts an empty log. Sec calls it once per boot, before the first debug output.
//
VOID
EFIAPI
RamLogReset (VOID);

//
// Appends debug output; FrameBufferSerialPortLib calls it for every write, with interrupts disabled.
//
VOID
EFIAPI
RamLogWrite (
  IN CONST UINT8 *Buffer,
  IN UINTN        NumberOfBytes);

#endif /* _RAM_LOG_LIB_H_ */
