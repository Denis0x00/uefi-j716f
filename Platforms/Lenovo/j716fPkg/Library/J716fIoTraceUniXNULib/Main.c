/** @file
  UniXNURun() and logging.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Internal.h"

#include <Protocol/SerialIo.h>

CONST CHAR16 *gJ716fBootStage = L"NOT ENTERED";
UINTN gJ716fBootFileBytes;
STATIC UNIXNU_UI *mJ716fTraceUi;
VOID J716fBootTrace (CONST CHAR16 *Stage)
{
  CHAR16 Line[96];
  gJ716fBootStage = Stage;
  UnicodeSPrint (Line, sizeof (Line), L"V10 stage: %s", Stage);
  if (mJ716fTraceUi != NULL) { UniXNUUiDrawStatus (mJ716fTraceUi, 2, Line); }
}

STATIC VOID J716fShowBootReturn (UNIXNU_UI *Ui, UNIXNU_CONFIG *Config, EFI_STATUS Result)
{
  UNIXNU_CONFIG Screen = *Config;
  UNIXNU_ENTRY Rows[5];
  CHAR16 Text[5][96];
  EFI_INPUT_KEY Key;
  EFI_EVENT Timer;
  UINTN I, Signaled;
  Screen.Title = L"J716F V10 - EFI return diagnosis";
  Screen.ShowLogo = FALSE;
  Screen.Entries = Rows;
  Screen.EntryCount = ARRAY_SIZE (Rows);
  Screen.Error[0] = 0; Screen.Notice[0] = 0;
  ZeroMem (Rows, sizeof (Rows));
  UnicodeSPrint (Text[0], sizeof (Text[0]), L"Last stage: %s", gJ716fBootStage);
  UnicodeSPrint (Text[1], sizeof (Text[1]), L"Status: %r", Result);
  UnicodeSPrint (Text[2], sizeof (Text[2]), L"Code: 0x%016lx", (UINT64)Result);
  UnicodeSPrint (Text[3], sizeof (Text[3]), L"EFI file read: %u bytes", (UINT32)gJ716fBootFileBytes);
  UnicodeSPrint (Text[4], sizeof (Text[4]), L"Photo this screen; short power returns");
  for (I = 0; I < ARRAY_SIZE (Rows); ++I) { Rows[I].Title = Text[I]; Rows[I].Type = UniXNUEntryEfi; }
  Ui->Info[0] = 0; Ui->Notice[0] = 0;
  UniXNUUiDrawFrame (Ui, &Screen);
  for (I = 0; I < ARRAY_SIZE (Rows); ++I) { UniXNUUiDrawEntry (Ui, &Screen, I, I == 0); }
  UniXNUUiDrawStatus (Ui, 0, L"V10 BOOT ATTEMPT RETURNED - SCREEN HELD");
  // Let the key that initiated the attempt fully release; no automatic reboot.
  if (gST->ConIn != NULL) { gST->ConIn->Reset (gST->ConIn, FALSE); }
  if (EFI_ERROR (gBS->CreateEvent (EVT_TIMER, TPL_CALLBACK, NULL, NULL, &Timer))) {
    for (;;) { gBS->Stall (100000); }
  }
  gBS->SetTimer (Timer, TimerPeriodic, EFI_TIMER_PERIOD_MILLISECONDS (100));
  for (;;) {
    gBS->WaitForEvent (1, &Timer, &Signaled);
    if (gST->ConIn != NULL && !EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key)) &&
        Key.UnicodeChar == CHAR_CARRIAGE_RETURN) { break; }
  }
  gBS->CloseEvent (Timer);
}

STATIC EFI_SERIAL_IO_PROTOCOL  *mSerial;

VOID
UniXNULogInit (
  VOID
  )
{
  if (EFI_ERROR (gBS->LocateProtocol (&gEfiSerialIoProtocolGuid, NULL, (VOID **)&mSerial))) {
    mSerial = NULL;
  }
}

VOID
EFIAPI
UniXNULog (
  IN CONST CHAR8  *Format,
  ...
  )
{
  VA_LIST  Args;
  CHAR8    Line[256];
  UINTN    Prefix;
  UINTN    Size;

  Prefix = AsciiSPrint (Line, sizeof (Line), "UNIXNU: ");
  VA_START (Args, Format);
  AsciiVSPrint (Line + Prefix, sizeof (Line) - Prefix - 2, Format, Args);
  VA_END (Args);
  AsciiStrCatS (Line, sizeof (Line), "\r\n");

  DEBUG ((PcdGet32 (PcdUniXNUDebugLevel), "%a", Line));
  if (mSerial != NULL) {
    Size = AsciiStrLen (Line);
    mSerial->Write (mSerial, &Size, Line);
  }
}

EFI_STATUS
EFIAPI
UniXNURun (
  IN EFI_HANDLE      ImageHandle,
  IN UNIXNU_HOST  Host
  )
{
  EFI_STATUS       Status;
  UNIXNU_CONFIG        Config;
  UNIXNU_UI            Ui;
  UNIXNU_INJECT_STATS  Stats;
  UINTN            Index;
  BOOLEAN          Countdown;
  UNIXNU_ENTRY         *Entry;
  UNIXNU_ENTRY         BootCopy;

  //
  // A boot manager waits for people; the 5 minute BDS watchdog does not.
  //
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  UniXNULogInit ();
  UniXNULog (
    "UniXNU %a starting as %a, firmware %s rev 0x%x",
    UNIXNU_VERSION_STRING,
    (Host == UniXNUHostFirmware) ? "firmware boot option" : "application",
    (gST->FirmwareVendor != NULL) ? gST->FirmwareVendor : L"?",
    gST->FirmwareRevision
    );

  //
  // Started straight from BDS, storage may not be connected yet.
  //
  if (Host == UniXNUHostFirmware) {
    UniXNUConnectAll ();
  }

  UniXNUConfigLoad (ImageHandle, &Config);
  UniXNULog (
    "config: %u entries, %u drivers, %u dt patches, %u acpi tables, %u slices, %u kits loaded, %u skipped",
    (UINT32)Config.EntryCount,
    (UINT32)Config.DriverCount,
    (UINT32)Config.PatchCount,
    (UINT32)Config.AcpiCount,
    (UINT32)Config.SliceCount,
    (UINT32)Config.KitsLoaded,
    (UINT32)Config.KitsSkipped
    );

  Config.Title = L"J716F V10 - EFI stage test";
  UniXNUPublishSlices (&Config);

  //
  // Drivers before the UI: an injected driver may provide the display or
  // the input device the menu needs.
  //
  ZeroMem (&Stats, sizeof (Stats));
  UniXNUInjectDrivers (ImageHandle, &Config, &Stats);
  UniXNUInjectAcpi (&Config, &Stats);

  UniXNUUiInit (&Config, &Ui);
  UnicodeSPrint (
    Ui.Info,
    sizeof (Ui.Info),
    L"drivers %u loaded / %u failed    kits %u    acpi %u",
    (UINT32)Stats.DriversLoaded,
    (UINT32)Stats.DriversFailed,
    (UINT32)Config.KitsLoaded,
    (UINT32)Stats.AcpiInstalled
    );

  Countdown = TRUE;
  for ( ; ;) {
    Index  = UniXNUMenuRun (&Ui, &Config, Countdown);
    Entry  = &Config.Entries[Index];
    BootCopy = *Entry;
    if (Entry->Confirm != NULL) {
      UINTN  Choice;

      if (!UniXNUMenuConfirm (&Ui, Entry, &Choice)) {
        UniXNULog ("menu: entry %u not confirmed", (UINT32)Index);
        UnicodeSPrint (Ui.Notice, sizeof (Ui.Notice), L"'%s' cancelled", Entry->Title);
        Countdown = FALSE;
        continue;
      }

      if (Entry->ConfirmCount > 0) {
        //
        // The chosen option's arguments follow the entry's own, on a copy: the entry itself stays as configured.
        //
        UINTN  Length = (Entry->Args != NULL ? StrLen (Entry->Args) : 0) + 1 + StrLen (Entry->ConfirmArgs[Choice]) + 1;

        BootCopy.Args = AllocateZeroPool (Length * sizeof (CHAR16));
        if (BootCopy.Args != NULL) {
          UnicodeSPrint (BootCopy.Args, Length * sizeof (CHAR16), L"%s %s", Entry->Args != NULL ? Entry->Args : L"", Entry->ConfirmArgs[Choice]);
        } else {
          BootCopy.Args = Entry->Args;
        }
      }
    }

    mJ716fTraceUi = &Ui;
    gJ716fBootFileBytes = 0;
    J716fBootTrace (L"ENTER BOOT");
    Status = UniXNUBootEntry (ImageHandle, &Config, &BootCopy);
    mJ716fTraceUi = NULL;
    J716fShowBootReturn (&Ui, &Config, Status);
    if ((Entry->Type == UniXNUEntryAction) && (Entry->Action == UniXNUActionExit)) {
      return EFI_SUCCESS;
    }

    //
    // The payload returned or failed: back to the menu, without the
    // countdown so a broken default cannot loop on a machine with input.
    //
    UnicodeSPrint (Ui.Notice, sizeof (Ui.Notice), L"'%s' returned: %r", Entry->Title, Status);
    UniXNULog ("menu: entry %u returned %r", (UINT32)Index, Status);
    gBS->SetWatchdogTimer (0, 0, 0, NULL);
    Countdown = FALSE;
  }
}
