#if !defined(WIN32_HANDMADE_H)

struct Win32OffscreenBuffer {
  BITMAPINFO Info;
  void* Memory;
  int Width;
  int Height;
  int Pitch;
};

struct Win32WindowDimension {
  int Width;
  int Height;
};

struct Win32SoundOutput {
  int SamplesPerSecond;
  int RunningSampleIndex;
  int BytesPerSample;
  int SecondaryBufferSize;
  real32 tSine;
  int LatencySampleCount;
};

#define WIN32_HANDMADE_H
#endif
