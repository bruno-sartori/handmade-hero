#if !defined(HANDMADE_H)

// Services that the polatform layer provides to the game


// Services that the game provides to the platform layer
struct GameOffscreenBuffer {
  // BITMAPINFO Info;
  void* Memory;
  int Width;
  int Height;
  int Pitch;
};

internal void GameUpdateAndRender(GameOffscreenBuffer *Buffer, int BlueOffset, int GreenOffset); // timing, controller/keyboard input, bitmap buffer, sound buffer



#define HANDMADE_H
#endif
