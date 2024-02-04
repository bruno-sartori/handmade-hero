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

  // NOTE: Either of the callbacks can be 0
  // you must check before calling
  Game_Update_And_Render* UpdateAndRender;
  Game_Get_Sound_Samples* GetSoundSamples;
  bool32 IsValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct Win32ReplayBuffer {
  HANDLE FileHandle;
  HANDLE MemoryMap;
  char FileName[WIN32_STATE_FILE_NAME_COUNT];
  void* MemoryBlock;
};

struct Win32State {
  uint64 TotalSize;
  void* GameMemoryBlock;
  Win32ReplayBuffer ReplayBuffers[4];

  HANDLE RecordingHandle;
  int InputRecordingIndex;
  HANDLE PlaybackHandle;
  int InputPlayingIndex;

  char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
  char *OnePastLastEXEFileNameSlash;
};

#define WIN32_HANDMADE_H
#endif
