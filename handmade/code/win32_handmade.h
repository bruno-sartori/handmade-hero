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
  DWORD SafetyBytes;
  real32 tSine;
  int LatencySampleCount;
};

struct Win32DebugTimeMarker {
  DWORD OutputPlayCursor;
  DWORD OutputWriteCursor;
  DWORD OutputLocation;
  DWORD OutputByteCount;
  DWORD ExpectedFlipPlayCursor;
  DWORD FlipPlayCursor;
  DWORD FlipWriteCursor;
};

struct Win32GameCode {
  HMODULE GameCodeDLL;
  FILETIME DLLLastWriteTime;
  Game_Update_And_Render* UpdateAndRender;
  Game_Get_Sound_Samples* GetSoundSamples;
  bool32 IsValid;
};

#define WIN32_HANDMADE_H
#endif
