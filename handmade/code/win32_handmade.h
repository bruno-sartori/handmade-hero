#if !defined(WIN32_HANDMADE_H)

struct win32_offscreen_buffer {
  // NOTE: Pixels are alwasy 32-bits wide, Memory Order BB GG RR XX
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
};

struct win32_window_dimension {
  int Width;
  int Height;
};

struct win32_sound_output {
  int SamplesPerSecond;
  uint32 RunningSampleIndex;
  int BytesPerSample;
  DWORD SecondaryBufferSize;
  DWORD SafetyBytes;
  real32 tSine;
  // TODO: Should running sample index be in bytes as well
  // TODO: Math gets simpler if we add a "bytes per second" field?
};

struct win32_debug_time_marker {
  DWORD OutputPlayCursor;
  DWORD OutputWriteCursor;
  DWORD OutputLocation;
  DWORD OutputByteCount;
  DWORD ExpectedFlipPlayCursor;

  DWORD FlipPlayCursor;
  DWORD FlipWriteCursor;
};

struct win32_game_code {
  HMODULE GameCodeDLL;
  FILETIME DLLLastWriteTime;

  // IMPORTANT: Either of the callbacks can be 0!  You must check before calling.
  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;

  bool32 IsValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer {
  HANDLE FileHandle;
  HANDLE MemoryMap;
  char FileName[WIN32_STATE_FILE_NAME_COUNT];
  void *MemoryBlock;
};
struct win32_state {
  uint64 TotalSize;
  void *GameMemoryBlock;
  win32_replay_buffer ReplayBuffers[4];

  HANDLE RecordingHandle;
  int InputRecordingIndex;

  HANDLE PlaybackHandle;
  int InputPlayingIndex;

  char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
  char *OnePastLastEXEFileNameSlash;
};

#define WIN32_HANDMADE_H
#endif
