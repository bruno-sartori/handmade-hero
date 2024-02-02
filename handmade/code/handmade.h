#if !defined(HANDMADE_H)

// static keywords can have multiple behaviours in C++ so we create macros to
// highlight them
#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int32_t bool32;
typedef float real32;
typedef double real64;

// NOTE:
// TODO:
// STUDY:
// IMPORTANT:
// FIXME:

/*
  INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  SLOW_PERFORMANCE:
    0 - No slow code allowed
    1 - Slow code allowed
*/

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

// ========================================================= SERVICES THAT THE PLATFORM LAYER PROVIDES TO THE GAME
#if INTERNAL
struct DEBUGReadFileResult {
  uint32 ContentsSize;
  void *Contents;
};

internal DEBUGReadFileResult DEBUGPlatformReadEntireFile(CHAR *Filename);
internal void DEBUGPlatformFreeFileMemory(void *Memory);
internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory);

#endif

// ========================================================== SERVICES THAT THE GAME PROVIDES TO THE PLATFORM LAYER

struct GameOffscreenBuffer {
  void* Memory;
  int Width;
  int Height;
  int Pitch;
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
  void *PermanentStorage; // REQUIRED to be cleared to zero at startup

  uint64 TransientStorageSize;
  void *TransientStorage; // REQUIRED to be cleared to zero at startup
};

internal void GameUpdateAndRender(GameMemory *Memory, GameInput *Input, GameOffscreenBuffer *Buffer); // timing, controller/keyboard input, bitmap buffer
// NOTE: At the moment, this has to be a very fast function, less than a millisecond
internal void GameGetSoundSamples(GameMemory *Memory, GameSoundOutputBuffer *SoundBuffer);

struct GameState {
  int BlueOffset;
  int GreenOffset;
  int ToneHz;
};

#define HANDMADE_H
#endif
