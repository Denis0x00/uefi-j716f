STATIC VOID Line (UINTN Row, CONST CHAR16 *Text)
{
  UINTN Width, Height, Count, I, X, Y, Sx, Sy, Pixel, Scale, Cell, GlyphHeight, Step;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
  CONST UINT8 *Glyph;
  Width = mGop->Mode->Info->HorizontalResolution;
  Height = mGop->Mode->Info->VerticalResolution;
  Scale = Height < 1856 ? 2 : 3;
  Cell = 8 * Scale; GlyphHeight = 19 * Scale;
  Step = MIN ((Height > 80 ? Height - 80 : 0) / 28, 64);
  if (Width < 96 || Step < GlyphHeight || 40 + Row * Step + GlyphHeight > Height) { return; }
  Count = MIN (StrLen (Text), (Width - 80) / Cell);
  if (!Count) { return; }
  Pixels = AllocatePool (Count * Cell * GlyphHeight * sizeof (*Pixels));
  if (Pixels == NULL) { return; }
  for (I = 0; I < Count * Cell * GlyphHeight; ++I) { Pixels[I] = mBg; }
  for (I = 0; I < Count; ++I) {
    Glyph = gCaFontGlyphs[Text[I] >= 32 && Text[I] <= 126 ? Text[I] - 32 : '?' - 32];
    for (Y = 0; Y < 19; ++Y) {
      for (X = 0; X < 8; ++X) {
        if ((Glyph[Y] & (0x80 >> X)) == 0) { continue; }
        for (Sy = 0; Sy < Scale; ++Sy) {
          for (Sx = 0; Sx < Scale; ++Sx) {
            Pixel = (Y * Scale + Sy) * Count * Cell + I * Cell + X * Scale + Sx;
            Pixels[Pixel] = mFg;
          }
        }
      }
    }
  }
  mGop->Blt (mGop, Pixels, EfiBltBufferToVideo, 0, 0, 40, 40 + Row * Step, Count * Cell, GlyphHeight, 0);
  FreePool (Pixels);
}
