#include "handmade.h"

internal void RenderWeirdGradient(GameOffscreenBuffer* Buffer, int BlueOffset, int GreenOffset) {
  // TODO: pass a pointer or by value? Lets see what the optimizer does

  uint8* Row = (uint8*)Buffer->Memory;

  for (int Y = 0; Y < Buffer->Height; Y++) {
    uint32_t* Pixel = (uint32_t*)Row;

    for (int X = 0; X < Buffer->Width; X++) {
      uint8 Blue = (X + BlueOffset);
      uint8 Green = (Y + GreenOffset);

      *Pixel++ = ((Green << 8) | Blue);  // understand << (shift) operator
    }

    Row += Buffer->Pitch;  // This could be Row += (uint8 *)Pixel because pixel
                           // will point to the end of the row after the for
                           // loop, but this is more explicit to understand
  }
}

internal void GameUpdateAndRender(GameOffscreenBuffer* Buffer, int BlueOffset, int GreenOffset) {
  RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}
