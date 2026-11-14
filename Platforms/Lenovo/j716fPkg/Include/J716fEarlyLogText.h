// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef J716F_EARLY_LOG_TEXT_H
#define J716F_EARLY_LOG_TEXT_H
#include <J716fEarlyLog.h>

typedef int (*J716F_EL_TEXT_WRITE)(void *, const char *, J716F_EL_U32);
typedef struct {
  J716F_EL_TEXT_WRITE write;
  void *context;
} J716F_EL_TEXT_CONTEXT;

// All strings here are fixed or validated header fields. Maximum line <512 B.
static inline void J716fElTextString(char *line, J716F_EL_U32 *n, const char *s)
{ while (*s) line[(*n)++] = *s++; }
static inline void J716fElTextNumber(char *line, J716F_EL_U32 *n, J716F_EL_U64 v)
{
  char digits[20]; J716F_EL_U32 count = 0;
  do { digits[count++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (count) line[(*n)++] = digits[--count];
}
static inline int J716fElTextRecord(void *context, J716F_EL_U64 sequence,
  const J716F_EL_U8 *payload, J716F_EL_U32 length)
{
  const char hex[] = "0123456789abcdef";
  J716F_EL_TEXT_CONTEXT *out = (J716F_EL_TEXT_CONTEXT *)context;
  char line[512]; J716F_EL_U32 n = 0, i;
  J716fElTextString(line, &n, "JXLG TEXT seq=");
  J716fElTextNumber(line, &n, sequence);
  J716fElTextString(line, &n, " bytes=");
  J716fElTextNumber(line, &n, length);
  J716fElTextString(line, &n, " data=\"");
  for (i = 0; i < length; ++i) {
    J716F_EL_U8 c = payload[i];
    if (c >= 0x20 && c <= 0x7e && c != '\\' && c != '"') line[n++] = (char)c;
    else {
      line[n++] = '\\'; line[n++] = 'x';
      line[n++] = hex[c >> 4]; line[n++] = hex[c & 15];
    }
  }
  line[n++] = '"'; line[n++] = '\n';
  return out->write(out->context, line, n);
}

static inline int J716fEarlyLogExportText(const volatile J716F_EL_U8 *snapshot,
  J716F_EL_U64 size, J716F_EL_TEXT_WRITE write, void *context)
{
  J716F_EL_REPORT report;
  J716F_EL_TEXT_CONTEXT output;
  char line[512]; J716F_EL_U32 n;
  int result;
  if (!write) return J716F_EL_BAD_ARGUMENT;
  // Validate and count before emitting BEGIN; no raw arbitrary memory is output.
  result = J716fEarlyLogSnapshot(snapshot, size, 0, 0, &report);
  if (result != J716F_EL_OK) return result;
  n = 0;
  J716fElTextString(line, &n, "JXLG BEGIN v=1 source=");
  J716fElTextNumber(line, &n, report.header.source);
  J716fElTextString(line, &n, " boot_seq=");
  J716fElTextNumber(line, &n, report.header.boot_sequence);
  J716fElTextString(line, &n, " boot_id=");
  J716fElTextNumber(line, &n, report.header.boot_id);
  J716fElTextString(line, &n, " build=");
  J716fElTextString(line, &n, report.header.build_tag);
  J716fElTextString(line, &n, " snapshot_crc=");
  J716fElTextNumber(line, &n, J716fEarlyLogCrc32(snapshot, J716F_EL_TOTAL_BYTES));
  line[n++] = '\n';
  if (write(context, line, n)) return J716F_EL_CALLBACK_STOP;
  n = 0;
  J716fElTextString(line, &n, "JXLG RANGE first=");
  J716fElTextNumber(line, &n, report.first_sequence);
  J716fElTextString(line, &n, " highest=");
  J716fElTextNumber(line, &n, report.highest_sequence);
  J716fElTextString(line, &n, " gaps=");
  J716fElTextNumber(line, &n, report.gaps);
  J716fElTextString(line, &n, " corrupt=");
  J716fElTextNumber(line, &n, report.invalid_records);
  J716fElTextString(line, &n, " empty=");
  J716fElTextNumber(line, &n, report.empty_records);
  line[n++] = '\n';
  if (write(context, line, n)) return J716F_EL_CALLBACK_STOP;
  output.write = write; output.context = context;
  result = J716fEarlyLogSnapshot(snapshot, size, J716fElTextRecord, &output, &report);
  if (result != J716F_EL_OK) return result;
  n = 0;
  J716fElTextString(line, &n, "JXLG END records=");
  J716fElTextNumber(line, &n, report.exported_records);
  J716fElTextString(line, &n, " bytes=");
  J716fElTextNumber(line, &n, report.exported_bytes);
  line[n++] = '\n';
  return write(context, line, n) ? J716F_EL_CALLBACK_STOP : J716F_EL_OK;
}
#endif
