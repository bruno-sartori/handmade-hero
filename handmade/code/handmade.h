#if !defined(HANDMADE_H)

/*
  INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  SLOW_PERFORMANCE:
    0 - No slow code allowed
    1 - Slow code allowed
*/

#include <math.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#if SLOW_PERFORMANCE
#define Assert(Expression) if (!(Expression)) {*(int *)0 = 0;} // if expression is false, write to the null pointer, which crashes the program (Platform Independend DebugBreak())
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32 SafeTruncateUInt64(uint64 Value) {
  Assert(Value <= 0xFFFFFFFF); // uint32 max value
  uint32 Result = (uint32)Value;
  return Result;
}

struct ThreadContext {
  int Placeholder;
};

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

struct GameOffscreenBuffer {
  void* Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
};

struct GameSoundOutputBuffer {
  int SamplesPerSecond;
  int SampleCount;
  int16 *Samples;
};

struct GameButtonState {
  int HalfTransitionCount;
  bool32 EndedDown;
};

struct GameControllerInput {
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
};

struct GameInput {
  GameButtonState MouseButtons[5];
  int32 MouseX;
  int32 MouseY;
  int32 MouseZ;

  GameControllerInput Controllers[5];
};

inline GameControllerInput *GetController(GameInput *Input, unsigned int ControllerIndex) {
  Assert(ControllerIndex < ArrayCount(Input->Controllers));

  GameControllerInput *Controller = &Input->Controllers[ControllerIndex];
  return Controller;
}

struct GameMemory {
  bool32 IsInitialized;
  uint64 PermanentStorageSize;
  void *PermanentStorage; // NOTE: Required to be cleared to zero at startup

  uint64 TransientStorageSize;
  void *TransientStorage; // NOTE: Required to be cleared to zero at startup

  DEBUG_Platform_Free_File_Memory *DEBUGPlatformFreeFileMemory;
  DEBUG_Platform_Read_Entire_File *DEBUGPlatformReadEntireFile;
  DEBUG_Platform_Write_Entire_File *DEBUGPlatformWriteEntireFile;
};

#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext *Thread, GameMemory *Memory, GameInput *Input, GameOffscreenBuffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(Game_Update_And_Render);

#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext *Thread, GameMemory *Memory, GameSoundOutputBuffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(Game_Get_Sound_Samples);

struct GameState {
  int BlueOffset;
  int GreenOffset;
  int ToneHz;
  real32 tSine;

  int PlayerX;
  int PlayerY;
  real32 tJump;
};

#define HANDMADE_H
#endif
