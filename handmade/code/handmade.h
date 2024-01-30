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

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// Services that the polatform layer provides to the game


// Services that the game provides to the platform layer
struct GameOffscreenBuffer {
  // BITMAPINFO Info;
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
  bool32 IsAnalog;

  real32 StartX;
  real32 StartY;
  real32 MinX;
  real32 MinY;
  real32 MaxX;
  real32 MaxY;
  real32 EndX;
  real32 EndY;

  union {
    GameButtonState Buttons[13];
    struct {
      GameButtonState Up;
      GameButtonState Down;
      GameButtonState Left;
      GameButtonState Right;
      GameButtonState Start;
      GameButtonState Back;
      GameButtonState LeftShoulder;
      GameButtonState RightShoulder;
      GameButtonState AButton;
      GameButtonState BButton;
      GameButtonState XButton;
      GameButtonState YButton;

      bool32 AButtonEndedDown;
    };
  };
};

struct GameInput {
  GameControllerInput Controllers[4];
};

internal void GameUpdateAndRender(GameInput *Input, GameOffscreenBuffer *Buffer, GameSoundOutputBuffer *SoundBuffer); // timing, controller/keyboard input, bitmap buffer, sound buffer



#define HANDMADE_H
#endif
