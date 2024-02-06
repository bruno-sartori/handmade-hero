#if !defined(HANDMADE_H)

/*
  INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  SLOW_PERFORMANCE:
    0 - No slow code allowed
    1 - Slow code allowed
*/

#include "handmade_platform.h"

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

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

inline GameControllerInput *GetController(GameInput *Input, unsigned int ControllerIndex) {
  Assert(ControllerIndex < ArrayCount(Input->Controllers));

  GameControllerInput *Controller = &Input->Controllers[ControllerIndex];
  return Controller;
}

struct TileMap {
  int32 CountX;
  int32 CountY;
  real32 UpperLeftX;
  real32 UpperLeftY;
  real32 TileWidth;
  real32 TileHeight;
  uint32 *Tiles;
};

struct WorldMap {
  int32 TileMapCountX;
  int32 TileMapCountY;
  TileMap *TileMaps;
};

struct GameState {
  real32 PlayerX;
  real32 PlayerY;
};

#define HANDMADE_H
#endif
