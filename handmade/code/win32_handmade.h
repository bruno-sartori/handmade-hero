#if !defined(WIN32_HANDMADE_H)

struct Win32OffscreenBuffer {
  BITMAPINFO Info;
  void* Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
};

struct Win32WindowDimension {
  int Width;
  int Height;
};

struct Win32SoundOutput {
  int SamplesPerSecond;
  int RunningSampleIndex;
  int BytesPerSample;
  DWORD SecondaryBufferSize;
  real32 tSine;
  int LatencySampleCount;
};

struct Win32DebugTimeMarker {
  DWORD PlayCursor;
  DWORD WriteCursor;
};

#define WIN32_HANDMADE_H
#endif
