STATIC VOID Line (UINTN Row, CONST CHAR16 *Text)
{
  UINTN Width, Height, Count, I, X, Y, Sx, Sy, Pixel;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
  CONST UINT8 *Glyph;
  Width = mGop->Mode->Info->HorizontalResolution;
  Height = mGop->Mode->Info->VerticalResolution;
  if (Width < 96 || 40 + Row * 64 + 57 > Height) { return; }
  Count = MIN (StrLen (Text), (Width - 80) / 24);
  if (!Count) { return; }
  Pixels = AllocatePool (Count * 24 * 57 * sizeof (*Pixels));
  if (Pixels == NULL) { return; }
  for (I = 0; I < Count * 24 * 57; ++I) { Pixels[I] = mBg; }
  for (I = 0; I < Count; ++I) {
    Glyph = gCaFontGlyphs[Text[I] >= 32 && Text[I] <= 126 ? Text[I] - 32 : '?' - 32];
    for (Y = 0; Y < 19; ++Y) {
      for (X = 0; X < 8; ++X) {
        if ((Glyph[Y] & (0x80 >> X)) == 0) { continue; }
        for (Sy = 0; Sy < 3; ++Sy) {
          for (Sx = 0; Sx < 3; ++Sx) {
            Pixel = (Y * 3 + Sy) * Count * 24 + I * 24 + X * 3 + Sx;
            Pixels[Pixel] = mFg;
          }
        }
      }
    }
  }
  mGop->Blt (mGop, Pixels, EfiBltBufferToVideo, 0, 0, 40, 40 + Row * 64, Count * 24, 57, 0);
  FreePool (Pixels);
}
