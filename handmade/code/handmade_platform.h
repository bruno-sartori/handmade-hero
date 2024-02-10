#if !defined(HANDMADE_PLATFORM_H)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef size_t memory_index;

typedef float real32;
typedef double real64;


typedef struct ThreadContext {
  int Placeholder;
} ThreadContext;

// ========================================================= SERVICES THAT THE PLATFORM LAYER PROVIDES TO THE GAME
#if INTERNAL
/*
  IMPORTANT:
  These are NOT for doing anything in the shipping game - they are
  blocking and the write doesn't protect against lost data
*/
struct DEBUGReadFileResult {
  uint32 ContentsSize;
  void *Contents;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(ThreadContext *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUG_Platform_Free_File_Memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name(ThreadContext *Thread, char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUG_Platform_Read_Entire_File);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(ThreadContext *Thread, char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUG_Platform_Write_Entire_File);

#endif

// ========================================================== SERVICES THAT THE GAME PROVIDES TO THE PLATFORM LAYER

typedef struct GameOffscreenBuffer {
  void* Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
} GameOffscreenBuffer;

typedef struct GameSoundOutputBuffer {
  int SamplesPerSecond;
  int SampleCount;
  int16 *Samples;
} GameSoundOutputBuffer;

typedef struct GameButtonState {
  int HalfTransitionCount;
  bool32 EndedDown;
} GameButtonState;

typedef struct GameControllerInput {
  bool32 IsConnected;
  bool32 IsAnalog;
  real32 StickAverageX;
  real32 StickAverageY;

  union {
    GameButtonState Buttons[16]; // ignore Terminator button
    struct {
      GameButtonState MoveUp;
      GameButtonState MoveDown;
      GameButtonState MoveLeft;
      GameButtonState MoveRight;

      GameButtonState ActionUp;
      GameButtonState ActionDown;
      GameButtonState ActionLeft;
      GameButtonState ActionRight;

      GameButtonState LeftShoulder;
      GameButtonState RightShoulder;
      GameButtonState AButton;
      GameButtonState BButton;
      GameButtonState XButton;
      GameButtonState YButton;
      GameButtonState Back;
      GameButtonState Start;

      GameButtonState Terminator;
      // IMPORTANT: Do not add buttons below this line
    };
  };
} GameControllerInput;

typedef struct GameInput {
  GameButtonState MouseButtons[5];
  int32 MouseX;
  int32 MouseY;
  int32 MouseZ;
  // delta time for the frame
  real32 dtForFrame;
  GameControllerInput Controllers[5];
} GameInput;

typedef struct GameMemory {
  bool32 IsInitialized;
  uint64 PermanentStorageSize;
  void *PermanentStorage; // NOTE: Required to be cleared to zero at startup

  uint64 TransientStorageSize;
  void *TransientStorage; // NOTE: Required to be cleared to zero at startup

  DEBUG_Platform_Free_File_Memory *DEBUGPlatformFreeFileMemory;
  DEBUG_Platform_Read_Entire_File *DEBUGPlatformReadEntireFile;
  DEBUG_Platform_Write_Entire_File *DEBUGPlatformWriteEntireFile;
} GameMemory;

#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext *Thread, GameMemory *Memory, GameInput *Input, GameOffscreenBuffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(Game_Update_And_Render);

#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext *Thread, GameMemory *Memory, GameSoundOutputBuffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(Game_Get_Sound_Samples);


#ifdef __cplusplus
}
#endif

#define HANDMADE_PLATFORM_H
#endif
