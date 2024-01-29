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

internal void GameUpdateAndRender(GameOffscreenBuffer *Buffer, int BlueOffset, int GreenOffset, GameSoundOutputBuffer *SoundBuffer, int ToneHz); // timing, controller/keyboard input, bitmap buffer, sound buffer



#define HANDMADE_H
#endif
