#if !defined(HANDMADE_H)

#include "handmade_platform.h"

//
//
//

struct memory_arena {
  memory_index Size;
  uint8 *Base;
  memory_index Used;
};

internal void InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base) {
  Arena->Size = Size;
  Arena->Base = Base;
  Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count) * sizeof(type))
void * PushSize_(memory_arena *Arena, memory_index Size) {
  Assert((Arena->Used + Size) <= Arena->Size);
  void *Result = Arena->Base + Arena->Used;
  Arena->Used += Size;

  return (Result);
}

#include "handmade_math.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct world {
  tile_map *TileMap;
};

struct loaded_bitmap {
  int32 Width;
  int32 Height;
  uint32 *Pixels;
};

struct hero_bitmaps {
  uint32 AlignX;
  uint32 AlignY;
  loaded_bitmap Head;
  loaded_bitmap Cape;
  loaded_bitmap Torso;
};

struct entity {
  bool32 Exists;
  tile_map_position P;
  v2 dP; // velocity
  uint32 FacingDirection;
  real32 Width;
  real32 Height;
};

struct game_state {
  memory_arena WorldArena;
  world *World;

  // TODO: Should we allow split-screen
  uint32 CameraFollowingEntityIndex;
  tile_map_position CameraP;

  uint32 PlayerIndexForController[ArrayCount(((game_input *)0)->Controllers)];
  uint32 EntityCount;
  entity Entities[256];

  loaded_bitmap Backdrop;
  hero_bitmaps HeroBitmaps[4];

};

#define HANDMADE_H
#endif
